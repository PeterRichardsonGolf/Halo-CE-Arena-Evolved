/*
AE_SERVER_HTTP_MOD.INC

The moderation depth on the web port (server_http.c includes this, after the keys'): an admin's name and secret as Basic
auth (and the RCON's auth message, in ae_server_http_keys.inc), the first-run claim (POST /api/claim), and the bans'
export and import (GET /api/bans/export, POST /api/bans/import). Part of server_http.c: its statics are used as they are.
*/

#include "../game/ae_server_mod_rules.h"
#include "ae_server_admin.h"

void ae_server_mod_bans_export(struct server_json *json);
void ae_server_mod_access_json(struct server_json *json);
int ae_server_mod_bans_import(const char *body, int length, int *added, int *skipped, int *refused, char *why, int why_size);

/* ---------- an admin's name and secret as a session */

static struct session ae_admin_scratch;

/* the session "Authorization: Basic base64(name:secret)" gives (for the request alone: no cookie, no CSRF); a wrong login
counts toward the address's lockout as a wrong password does */
static struct session *ae_admin_basic_session(struct connection *connection, const char *encoded)
{
	unsigned char bytes[160];
	char name[AE_KEY_NAME_MAXIMUM + 8], reason[160];
	int size, role = 0, immunity = 0;
	char *colon;

	if (address_locked(connection->real_address))
		return NULL;
	size = server_base64_decode(encoded, bytes, (int)sizeof(bytes) - 1);
	if (size <= 0)
	{
		address_login_failed(connection->real_address, connection->real_text);
		return NULL;
	}
	bytes[size] = 0;
	colon = strchr((char *)bytes, ':');
	if (!colon || colon - (char *)bytes > AE_KEY_NAME_MAXIMUM)
	{
		address_login_failed(connection->real_address, connection->real_text);
		memset(bytes, 0, sizeof(bytes));
		return NULL;
	}
	*colon = 0;
	snprintf(name, sizeof(name), "%s", (char *)bytes);
	if (!ae_ops_admin_login(name, colon + 1, connection->real_address, &role, &immunity, reason, sizeof(reason)))
	{
		address_login_failed(connection->real_address, connection->real_text);
		server_log(_server_log_warning, "server", "web: an admin login was refused (%s)", reason);
		memset(bytes, 0, sizeof(bytes));
		return NULL;
	}
	memset(bytes, 0, sizeof(bytes));
	memset(&ae_admin_scratch, 0, sizeof(ae_admin_scratch));
	ae_admin_scratch.used = 1;
	ae_admin_scratch.role = role;
	snprintf(ae_admin_scratch.by, sizeof(ae_admin_scratch.by), "admin:%s", name);
	ae_admin_scratch.created = ae_admin_scratch.used_at = now_seconds();
	return &ae_admin_scratch;
}

/* ---------- POST /api/claim {token, password} (no session: the claim is how the first password is made) */

static int ae_claim_request(struct connection *connection, const char *body, int length)
{
	char token[64] = "", password[160] = "";

	if (!ae_ops_claim_pending())
	{
		respond_error(connection, 404, "not_found", "this server is not waiting to be claimed", 0);
		return 1;
	}
	if (strcmp(connection->request.method, "POST"))
	{
		respond_error(connection, 405, "method", "/api/claim takes POST: {\"token\":..., \"password\":...}", 0);
		return 1;
	}
	if (address_locked(connection->real_address) || !address_allows_request(connection->real_address, 0))
	{
		respond_error(connection, 429, "locked", "too many tries: wait", 60);
		return 1;
	}
	body_string(body, length, "token", token, sizeof(token));
	body_string(body, length, "password", password, sizeof(password));
	if (!ae_ops_claim_check(token))
	{
		address_login_failed(connection->real_address, connection->real_text);
		server_log(_server_log_warning, "server", "web: a wrong claim token was given");
		respond_error(connection, 403, "denied", "the claim token is wrong", 0);
	}
	else if (!ae_claim_password_valid(password))
	{
		respond_error(connection, 400, "schema", "the password is 10 to 100 printable characters", 0);
	}
	else
	{
		/* (the setting is written on the game's thread, as every change of one is: a ticket) */
		struct server_ticket *ticket = ticket_new(_ticket_api, _server_role_viewer, "claim");

		if (!ticket)
		{
			respond_error(connection, 500, "internal", "out of memory", 0);
		}
		else
		{
			snprintf(ticket->method, sizeof(ticket->method), "POST");
			snprintf(ticket->path, sizeof(ticket->path), "/api/claim");
			ticket->body = malloc(sizeof(password));
			if (ticket->body)
			{
				snprintf(ticket->body, sizeof(password), "%s", password);
				ticket->body_length = (int)strlen(ticket->body);
			}
			connection->ticket = ticket;
			ticket_submit(ticket);
		}
	}
	memset(password, 0, sizeof(password));
	return 1;
}

/* ---------- the bans' export and import (on the game's thread) */

static int ae_mod_ticket(struct server_ticket *ticket, const char *path, const char *method, const char *body, int length)
{
	struct server_json json;

	if (!strcmp(path, "/api/claim") && !strcmp(method, "POST"))
	{
		if (!ae_ops_claim_pending())
		{
			ticket_error(ticket, 404, "not_found", "this server is not waiting to be claimed");
		}
		else if (!body || !ae_claim_password_valid(body))
		{
			ticket_error(ticket, 400, "schema", "the password is 10 to 100 printable characters");
		}
		else if (!config_write("server.web_password", body))
		{
			ticket_error(ticket, 500, "internal", "config.toml cannot be written");
		}
		else
		{
			ae_ops_claim_done();
			server_json_begin(&json);
			server_json_object_begin(&json, NULL);
			server_json_boolean(&json, "ok", 1);
			server_json_boolean(&json, "claimed", 1);
			server_json_object_end(&json);
			ticket_answer(ticket, 200, &json);
			server_json_free(&json);
		}
		return 1;
	}
	/* (GET /api/backups and GET /api/access: what the web page's Access tab and About's backups read) */
	if ((!strcmp(path, "/api/backups") || !strcmp(path, "/api/access")) && strcmp(method, "GET"))
	{
		ticket_error(ticket, 405, "method", "takes GET");
		return 1;
	}
	if (!strcmp(path, "/api/backups") || !strcmp(path, "/api/access"))
	{
		server_json_begin(&json);
		server_json_object_begin(&json, NULL);
		server_json_boolean(&json, "ok", 1);
		if (!strcmp(path, "/api/backups"))
			ae_server_backups_json(&json);
		else
			ae_server_mod_access_json(&json);
		server_json_object_end(&json);
		ticket_answer(ticket, 200, &json);
		server_json_free(&json);
		return 1;
	}
	if (!strcmp(path, "/api/bans/export") && !strcmp(method, "GET"))
	{
		server_json_begin(&json);
		server_json_object_begin(&json, NULL);
		server_json_boolean(&json, "ok", 1);
		ae_server_mod_bans_export(&json);
		server_json_object_end(&json);
		ticket_answer(ticket, 200, &json);
		server_json_free(&json);
		return 1;
	}
	if (!strcmp(path, "/api/bans/import") && !strcmp(method, "POST"))
	{
		int added = 0, skipped = 0, refused = 0;
		char why[200];

		if (!ae_server_mod_bans_import(body, length, &added, &skipped, &refused, why, sizeof(why)))
		{
			ticket_error(ticket, 400, "schema", why);
			return 1;
		}
		server_log(_server_log_info, "moderation", "bans imported by %s: %d added, %d skipped, %d refused", ticket->by, added,
			skipped, refused);
		server_json_begin(&json);
		server_json_object_begin(&json, NULL);
		server_json_boolean(&json, "ok", 1);
		server_json_integer(&json, "added", added);
		server_json_integer(&json, "skipped", skipped);
		server_json_integer(&json, "refused", refused);
		server_json_object_end(&json);
		ticket_answer(ticket, 200, &json);
		server_json_free(&json);
		return 1;
	}
	return 0;
}
