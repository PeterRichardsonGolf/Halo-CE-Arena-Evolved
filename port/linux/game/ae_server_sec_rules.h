/*
AE_SERVER_SEC_RULES.H

The dedicated server's web port, security wave 2 (audit M2, M3, L1, L3, L4, L5): the pure decisions, so that a test
can pin them (port/linux/tests/ae_server_sec_rules_test.c). server_http.c and server_mode.c call them.
*/

#ifndef __AE_SERVER_SEC_RULES_H
#define __AE_SERVER_SEC_RULES_H

/* ---------- sign-in budget (M2) */

enum
{
	/* passwords checked a minute from one address; from this machine's own loopback (a supervisor, a fleet agent) */
	AE_SEC_LOGIN_PER_ADDRESS = 10,
	AE_SEC_LOGIN_PER_ADDRESS_LOCAL = 60,
	/* ... and in all, from addresses other than this machine's own */
	AE_SEC_LOGIN_GLOBAL = 120
};

struct ae_sec_login_budget
{
	unsigned long minute;
	int used;
};

/* the check of one more password: 0 allowed, 1 the address has used its own budget this minute, 2 the global ceiling
is used. An address over its own budget does not use the global one (one client cannot drain what the others share);
a local connection never meets the global ceiling. The minutes are wall-clock minutes. */
int ae_sec_login_check(
	struct ae_sec_login_budget *address,
	struct ae_sec_login_budget *global,
	unsigned long minute,
	int local);

/* ---------- the Host a browser's request names (L1) */

/* whether a browser's request to this Host header (name or address, a port allowed) is one the server answers: an
IPv4 or IPv6 address, "localhost", or a name in the comma-separated list. Other names are a DNS rebinding's.
An empty Host is allowed (HTTP/1.0). */
int ae_sec_host_allowed(
	char const *host,
	char const *names);

/* ---------- the RCON (L3, L4, L5) */

enum
{
	/* sockets still to sign in, from one address, and in all */
	AE_SEC_WS_UNAUTHENTICATED_PER_ADDRESS = 2,
	AE_SEC_WS_TOTAL = 16
};

/* whether one more WebSocket may be taken: 0 yes, 1 this address has its unauthenticated ones, 2 too many sockets in
all, 3 too many signed in (when the new one is, from a session) */
int ae_sec_ws_admit(
	int total,
	int signed_in,
	int signed_in_maximum,
	int unauthenticated_from_address,
	int new_is_signed_in);

/* a command's word appended to a line: the word as it goes onto the line (quoted if it has a space or is empty),
written to out; the length, or -1 if the word has a double quote or a control character (it would split into
more arguments) or does not fit */
int ae_sec_command_word(
	char const *word,
	char *out,
	int size);

/* the web page's minimum password length: 8, but 12 when the port is bound beyond loopback with no trusted proxy (the
password then crosses the network in clear text) */
int ae_sec_password_minimum(
	int loopback,
	int has_trusted_proxy);

/* whether the web port's binding needs no warning: 1 on loopback, behind a trusted proxy (TLS ends there), or when the
owner said it is on purpose (server.web_insecure_http) */
int ae_sec_bind_allowed(
	int loopback,
	int has_trusted_proxy,
	int insecure_http);

#endif
