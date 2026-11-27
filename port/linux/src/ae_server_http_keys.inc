/*
AE_SERVER_HTTP_KEYS.INC

API keys on the web port (server_http.c includes this, after its ticket helpers; ae_server_admin.c keeps the keys): a
key as a Bearer token on the API and in the RCON's auth message, the /api/keys routes, the stop route's delay, and
the check that closes the remote consoles of a revoked key. Part of server_http.c: its statics are used as they are.
*/

#include "ae_server_ops.h"

/* ---------- a key as a session */

static struct session ae_key_scratch;
static char ae_key_scratch_id[AE_KEY_ID_LENGTH + 1]; /* (the id of the key that made it) */

/* the session a Bearer key gives (one for the request alone: a key has no cookie, no CSRF, no idle time); a wrong
key counts toward the address's lockout as a wrong password does */
static struct session *ae_key_session(struct connection *connection, const char *token)
{
	char name[AE_KEY_NAME_MAXIMUM + 1], id[AE_KEY_ID_LENGTH + 1], reason[160];
	int role = 0;

	if (address_locked(connection->real_address))
		return NULL;
	if (!ae_server_key_check(token, connection->real_address, name, sizeof(name), &role, id, reason, sizeof(reason)))
	{
		address_login_failed(connection->real_address, connection->real_text);
		server_log(_server_log_warning, "server", "web: a key was refused (%s)", reason);
		return NULL;
	}
	memset(&ae_key_scratch, 0, sizeof(ae_key_scratch));
	ae_key_scratch.used = 1;
	ae_key_scratch.role = role;
	snprintf(ae_key_scratch.by, sizeof(ae_key_scratch.by), "key:%s", name);
	ae_key_scratch.created = ae_key_scratch.used_at = now_seconds();
	snprintf(ae_key_scratch_id, sizeof(ae_key_scratch_id), "%s", id);
	return &ae_key_scratch;
}

/* a socket upgraded with a Bearer key keeps that key's id, so ae_keys_check closes it when the key is revoked or expires */
static void ae_key_ws_bind(struct connection *connection, const struct session *session)
{
	connection->ae_key_id[0] = 0;
	if (session == &ae_key_scratch)
		snprintf(connection->ae_key_id, sizeof(connection->ae_key_id), "%s", ae_key_scratch_id);
}

/* the RCON's {"type":"auth","name":...,"secret":...} (an admin row's login): 1 when the message carries them (answered here) */
static int ae_ws_auth_admin(struct connection *connection, struct server_json_document *document)
{
	char name[AE_KEY_NAME_MAXIMUM + 8] = "", secret[120] = "", reason[160];
	int role = 0, immunity = 0, locked;

	if (server_json_member(document, 0, "name") < 0 || server_json_member(document, 0, "secret") < 0)
		return 0;
	locked = address_locked(connection->real_address);
	if (locked)
	{
		ws_auth_answer(connection, 0, "locked", "too many tries: wait", locked);
		return 1;
	}
	server_json_get_string(document, server_json_member(document, 0, "name"), name, sizeof(name));
	server_json_get_string(document, server_json_member(document, 0, "secret"), secret, sizeof(secret));
	if (!ae_ops_admin_login(name, secret, connection->real_address, &role, &immunity, reason, sizeof(reason)))
	{
		address_login_failed(connection->real_address, connection->real_text);
		server_log(_server_log_warning, "server", "rcon: an admin login was refused (%s)", reason);
		ws_auth_answer(connection, 0, "denied", "wrong name or secret", 0);
	}
	else
	{
		connection->role = role;
		snprintf(connection->by, sizeof(connection->by), "admin:%s", name);
		server_log(_server_log_info, "server", "rcon: %s signed in (%s)", connection->by, server_role_name(role));
		ws_auth_answer(connection, 1, NULL, NULL, 0);
	}
	memset(secret, 0, sizeof(secret));
	return 1;
}

/* the RCON's {"type":"auth","key":...}: 1 when the message carries a key (answered here) */
static int ae_ws_auth_key(struct connection *connection, struct server_json_document *document)
{
	char key[AE_KEY_TOKEN_LENGTH + 32] = "";
	char name[AE_KEY_NAME_MAXIMUM + 1], id[AE_KEY_ID_LENGTH + 1], reason[160];
	int token = server_json_member(document, 0, "key");
	int role = 0, locked;

	if (token < 0)
		return ae_ws_auth_admin(connection, document);
	locked = address_locked(connection->real_address);
	if (locked)
	{
		ws_auth_answer(connection, 0, "locked", "too many tries: wait", locked);
		return 1;
	}
	if (!server_json_get_string(document, token, key, sizeof(key)) ||
		!ae_server_key_check(key, connection->real_address, name, sizeof(name), &role, id, reason, sizeof(reason)))
	{
		address_login_failed(connection->real_address, connection->real_text);
		server_log(_server_log_warning, "server", "rcon: a key was refused");
		ws_auth_answer(connection, 0, "denied", "wrong key", 0);
	}
	else
	{
		connection->role = role;
		snprintf(connection->by, sizeof(connection->by), "key:%s", name);
		snprintf(connection->ae_key_id, sizeof(connection->ae_key_id), "%s", id);
		server_log(_server_log_info, "server", "rcon: %s signed in (%s)", connection->by, server_role_name(role));
		ws_auth_answer(connection, 1, NULL, NULL, 0);
	}
	memset(key, 0, sizeof(key));
	return 1;
}

/* the remote consoles of a key revoked or expired closed (checked when the keys change, and every second) */
static void ae_keys_check(void)
{
	static unsigned long revision = ~0UL, second;
	unsigned long now = now_seconds(), current = ae_server_key_revision();
	int index;

	if (revision == current && second == now)
		return;
	revision = current;
	second = now;
	ae_sec_sessions_check();
	for (index = 0; index < MAXIMUM_CONNECTIONS; index++)
	{
		struct connection *connection = &http.connections[index];

		if (connection->socket >= 0 && connection->websocket && connection->role != _server_role_none &&
			connection->ae_key_id[0] && !ae_server_key_alive(connection->ae_key_id))
		{
			connection->role = _server_role_none;
			ws_close(connection, 1008, "the key was revoked or expired");
		}
		/* (an admin row's login, Basic or the RCON's own: closed when its row is gone or has expired) */
		else if (connection->socket >= 0 && connection->websocket && connection->role != _server_role_none &&
			!strncmp(connection->by, "admin:", 6))
		{
			int row_role, row_immunity;

			if (!ae_ops_admin_by_name(connection->by + 6, &row_role, &row_immunity))
			{
				connection->role = _server_role_none;
				ws_close(connection, 1008, "the admin row was removed or expired");
			}
		}
	}
}

/* ---------- /api/keys (on the game's thread) */

static void ae_keys_ticket(struct server_ticket *ticket, const char *path, const char *method, const char *body,
	int length)
{
	struct server_json json;

	server_json_begin(&json);
	if (!strcmp(path, "/api/keys") && !strcmp(method, "GET"))
	{
		server_json_object_begin(&json, NULL);
		server_json_boolean(&json, "ok", 1);
		ae_server_keys_json(&json);
		server_json_object_end(&json);
		ticket_answer(ticket, 200, &json);
	}
	else if (!strcmp(path, "/api/keys") && !strcmp(method, "POST"))
	{
		struct server_json_document document;
		char name[AE_KEY_NAME_MAXIMUM + 8] = "", role_text[24] = "", allow[AE_KEY_ALLOW_MAXIMUM + 8] = "";
		char token[AE_KEY_TOKEN_LENGTH + 1], message[200];
		double days = 0;
		int member;

		if (!server_json_parse(&document, body, length, 8) || document.tokens[0].type != _server_json_object)
		{
			ticket_error(ticket, 400, "bad_request", "the body is a JSON object: name, role, days, allow");
		}
		else
		{
			server_json_get_string(&document, server_json_member(&document, 0, "name"), name, sizeof(name));
			server_json_get_string(&document, server_json_member(&document, 0, "role"), role_text, sizeof(role_text));
			server_json_get_string(&document, server_json_member(&document, 0, "allow"), allow, sizeof(allow));
			member = server_json_member(&document, 0, "days");
			if (member >= 0 && !server_json_get_number(&document, member, &days))
				days = -1;
			server_json_document_free(&document);
			if (days != (double)(long)days)
				days = -1;
			if (!name[0] || !role_text[0])
			{
				ticket_error(ticket, 400, "schema", "name and role are needed");
			}
			else if (!ae_server_key_create(name, ae_key_role_parse(role_text), (long)days, allow, ticket->role,
				ticket->by, token, sizeof(token), message, sizeof(message)))
			{
				ticket_error(ticket, strstr(message, "exists") ? 409 : 400, strstr(message, "exists") ? "exists" : "schema",
					message);
			}
			else
			{
				server_json_object_begin(&json, NULL);
				server_json_boolean(&json, "ok", 1);
				server_json_string(&json, "name", name);
				server_json_string(&json, "role", role_text);
				/* (the secret, once: it is kept as a hash alone) */
				server_json_string(&json, "key", token);
				server_json_object_end(&json);
				ticket_answer(ticket, 200, &json);
			}
			memset(token, 0, sizeof(token));
		}
	}
	else if (!strncmp(path, "/api/keys/", 10) && !strcmp(method, "DELETE"))
	{
		if (!ae_key_name_valid(path + 10) || !ae_server_key_remove(path + 10, ticket->by))
		{
			ticket_error(ticket, 404, "not_found", "no key of that name");
		}
		else
		{
			server_json_object_begin(&json, NULL);
			server_json_boolean(&json, "ok", 1);
			server_json_object_end(&json);
			ticket_answer(ticket, 200, &json);
		}
	}
	else
	{
		ticket_error(ticket, 405, "method", "GET and POST /api/keys, DELETE /api/keys/<name>");
	}
	server_json_free(&json);
}

/* POST /api/stop with a delay ("seconds": 0 to 3600, or "after": "delay" for server.restart_warning_seconds):
sv_stop or sv_restart <seconds> <notice>, warned as the command is; 0 when the body has none (the route goes on) */
static int ae_stop_delayed(struct server_ticket *ticket, const char *body, int length)
{
	struct server_json_document document;
	char after[16] = "", notice[256] = "", kept[160], line[SERVER_COMMAND_LINE_SIZE];
	double seconds = -1;
	int restart = 0, cancel = 0, member, handled = 0, ok = 1;

	if (!server_json_parse(&document, body, length, 8) || document.tokens[0].type != _server_json_object)
		return 0;
	member = server_json_member(&document, 0, "seconds");
	if (member >= 0 && !server_json_get_number(&document, member, &seconds))
		ok = 0;
	server_json_get_string(&document, server_json_member(&document, 0, "after"), after, sizeof(after));
	member = server_json_member(&document, 0, "cancel");
	if (member >= 0)
		server_json_get_boolean(&document, member, &cancel);
	member = server_json_member(&document, 0, "restart");
	if (member >= 0)
		server_json_get_boolean(&document, member, &restart);
	server_json_get_string(&document, server_json_member(&document, 0, "notice"), notice, sizeof(notice));
	server_json_document_free(&document);
	if (cancel || (seconds < 0 && ok && strcmp(after, "delay")))
		return 0;
	handled = 1;
	if (!strcmp(after, "delay") && seconds < 0 && ok)
		seconds = (double)config_integer("server.restart_warning_seconds");
	if (!ok || seconds < 0 || seconds > 3600 || seconds != (double)(long)seconds ||
		(after[0] && strcmp(after, "delay") && strcmp(after, "now")))
	{
		ticket_error(ticket, 400, "schema", "seconds is a whole number from 0 to 3600 (after is delay or now)");
		return handled;
	}
	line_text(notice, kept, sizeof(kept));
	if (seconds == 0)
		snprintf(line, sizeof(line), "%s now %s", restart ? "sv_restart" : "sv_stop", kept);
	else
		snprintf(line, sizeof(line), "%s %ld %s", restart ? "sv_restart" : "sv_stop", (long)seconds, kept);
	ticket_command(ticket, line);
	return handled;
}
