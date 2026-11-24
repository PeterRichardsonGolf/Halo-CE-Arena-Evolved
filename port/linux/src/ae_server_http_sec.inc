/*
AE_SERVER_HTTP_SEC.INC

Security wave 2 of the web port (server_http.c includes this, before password_role; the pure decisions
are port/linux/game/ae_server_sec_rules.c): the per-address sign-in budget, a request that names a Host the server
does not answer to, the RCON's sockets (unauthenticated ones capped by address, signed-in ones counted apart) and its
sessions' end. Part of server_http.c: its statics are used as they are.
*/

#include "../game/ae_server_sec_rules.h"

/* ---------- M2: the sign-in budget */

static struct ae_sec_login_budget ae_sec_global_login;

/* whether one more password from the address is over its budget (or the ceiling the other addresses share) */
static int ae_sec_login_over(unsigned long address, int local)
{
	return ae_sec_login_check(&address_record(address)->ae_login, &ae_sec_global_login, now_seconds() / 60, local) != 0;
}

/* a request that carries X-Forwarded-* headers from a peer that is no trusted proxy: a proxy that was not told so
(server.web_trusted_proxy), or a client faking one. Its address is the peer's, and it is no local connection (the
owner's password is not taken through a proxy of the machine itself). Said once. */
static int ae_sec_spoofed(const struct http_request *request, int forwarded)
{
	static int said;
	int spoofed = !forwarded && (request->forwarded_for[0] || request->forwarded_proto[0] || request->forwarded_host[0]);

	if (spoofed && !said)
	{
		said = 1;
		server_log(_server_log_warning, "server", "web: a request carries X-Forwarded-* headers from an address that is "
			"no trusted proxy: they are ignored, and the owner's password is not taken from it. Behind a reverse proxy "
			"set server.web_trusted_proxy");
	}
	return spoofed;
}

/* ---------- L1: the Host of a browser's request */

/* 1 if the request was answered (refused): a request a browser makes (it has an Origin or a Sec-Fetch-Site) to a
Host name that is no address, localhost or one of server.web_hosts is a DNS rebinding's. A trusted proxy's requests
and a script's (curl) are not asked. */
static int ae_sec_host_check(struct connection *connection)
{
	const struct http_request *request = &connection->request;

	if (connection->forwarded || (!request->origin[0] && !request->fetch_site[0]))
		return 0;
	if (ae_sec_host_allowed(request->host, config_string("server.web_hosts")))
		return 0;
	respond_error(connection, 403, "host", "this host name is not one of the server's (server.web_hosts)", 0);
	return 1;
}

/* ---------- L3: the RCON's sockets */

/* whether the new socket (from a session, or not) finds no place: the 503 is answered here */
static int ae_sec_ws_full(struct connection *connection, int total, int new_is_signed_in)
{
	int index, signed_in = 0, unauthenticated = 0, verdict;

	for (index = 0; index < MAXIMUM_CONNECTIONS; index++)
	{
		const struct connection *other = &http.connections[index];

		if (other->socket < 0 || !other->websocket)
			continue;
		if (other->role != _server_role_none)
			signed_in++;
		else if (other->real_address == connection->real_address)
			unauthenticated++;
	}
	verdict = ae_sec_ws_admit(total, signed_in, MAXIMUM_WEBSOCKETS, unauthenticated, new_is_signed_in);
	return verdict;
}

/* whether the signed-in sockets have all their places (the sign-in of one more answers "busy") */
static int ae_sec_ws_signed_full(const struct connection *connection)
{
	int index, signed_in = 0;

	for (index = 0; index < MAXIMUM_CONNECTIONS; index++)
		signed_in += http.connections[index].socket >= 0 && http.connections[index].websocket &&
			http.connections[index].role != _server_role_none && &http.connections[index] != connection;
	return signed_in >= MAXIMUM_WEBSOCKETS;
}

/* ---------- L2: a socket signed in by a session ends with it */

/* the socket signed in by this session (one of the table's: a key's has no place in it) */
static void ae_sec_ws_bind(struct connection *connection, const struct session *session)
{
	connection->ae_session_bound = session >= http.sessions && session < http.sessions + MAXIMUM_SESSIONS;
	if (connection->ae_session_bound)
		memcpy(connection->ae_session_hash, session->hash, sizeof(connection->ae_session_hash));
}

static int ae_sec_session_alive(const unsigned char *hash)
{
	unsigned long now = now_seconds();
	int index;

	for (index = 0; index < MAXIMUM_SESSIONS; index++)
	{
		const struct session *session = &http.sessions[index];

		if (session->used && p2p_equal(session->hash, hash, P2P_SHA256_SIZE))
			return now - session->created <= SESSION_SECONDS;
	}
	return 0;
}

/* 1 if the socket's session ended (the socket is closed): checked at every message */
static int ae_sec_ws_signed_out(struct connection *connection)
{
	if (!connection->ae_session_bound || connection->role == _server_role_none ||
		ae_sec_session_alive(connection->ae_session_hash))
		return 0;
	connection->role = _server_role_none;
	ws_close(connection, 1008, "signed out");
	return 1;
}

/* the sockets signed in by the session that logs out close at once */
static void ae_sec_ws_close_session(const struct session *session)
{
	int index;

	for (index = 0; index < MAXIMUM_CONNECTIONS; index++)
	{
		struct connection *connection = &http.connections[index];

		if (connection->socket >= 0 && connection->websocket && connection->ae_session_bound &&
			connection->role != _server_role_none &&
			p2p_equal(connection->ae_session_hash, session->hash, P2P_SHA256_SIZE))
		{
			connection->role = _server_role_none;
			ws_close(connection, 1008, "signed out");
		}
	}
}

/* every second: the sockets of a session that expired close */
static void ae_sec_sessions_check(void)
{
	int index;

	for (index = 0; index < MAXIMUM_CONNECTIONS; index++)
		if (http.connections[index].socket >= 0 && http.connections[index].websocket)
			ae_sec_ws_signed_out(&http.connections[index]);
}
