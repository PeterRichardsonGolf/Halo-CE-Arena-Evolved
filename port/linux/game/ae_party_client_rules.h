/*
AE_PARTY_CLIENT_RULES.H

The game's side of the party service's wire (tools/ae_party_service, wire version 2): the text the hello signs, a call's URL
and JSON body, and the service's answers read into one record. Pure but for server_json (the JSON reader): port/linux/tests/
ae_party_client_rules_test.c runs it. The thread that makes the calls is src/ae_party_client.c.
*/

#ifndef __AE_PARTY_CLIENT_RULES_H
#define __AE_PARTY_CLIENT_RULES_H

#include "ae_friend_code.h"
#include "ae_party_net_rules.h"

enum
{
	AE_PC_NAME = 24,
	AE_PC_ID = 16,
	AE_PC_ADDRESS = 80,
	AE_PC_TICKET = 320,
	AE_PC_INVITES = 4,
	AE_PC_WORD = 12,
	AE_PC_ORIGIN = 200,
	AE_PC_POLL_MIN = 1000,
	AE_PC_POLL_MAX = 15000,
	AE_PC_POLL_IDLE = 5000,
	AE_PC_WIRE = 2
};

struct ae_pc_member
{
	char code[AE_FRIEND_CODE_SIZE];
	char name[AE_PC_NAME];
	int seats;
	char state[AE_PC_WORD];       /* menus, party, searching, game, offline */
};

struct ae_pc_invite
{
	char party[AE_PC_ID];
	char from_code[AE_FRIEND_CODE_SIZE];
	char from_name[AE_PC_NAME];
	int seats, expires_s;
};

/* QUICK PLAY's answer: state "found" (a server holds the seats; ticket), "bots" (nothing with room: AE TOURNAMENT is offered;
ask 1 when the leader must choose) or "none" (cancelled or stopped; reason says why) */
struct ae_pc_search
{
	int present;
	char state[AE_PC_WORD];
	char playlist[AE_PC_WORD + 4];
	long n;
	int seats;
	char server_address[AE_PC_ADDRESS];
	char server_name[48];
	int server_players, server_max, ping_ms;    /* ping -1: none measured */
	char ticket[AE_PC_TICKET];
	int ask, room_for, again;
	char reason[24];
};

struct ae_pc_follow
{
	int present;
	char kind[AE_PC_WORD];        /* server, hosted, lan */
	char address[AE_PC_ADDRESS];
	char invite[AE_PC_ADDRESS];
	long n;
};

/* a Steam friend who runs AE and asked about this player too (the service's `steam` list) */
struct ae_pc_steam
{
	char hash[16];
	char code[AE_FRIEND_CODE_SIZE];
	char name[AE_PC_NAME];
	char state[AE_PC_WORD];
	int verified;
};

enum { AE_PC_STEAM = 16 };

/* one answer of the service: an error reply (ok 0, error = its code, detail, room for party_full), a poll that changed nothing
(same 1) or a view of this player's party, invites and search */
struct ae_pc_view
{
	int ok, same;
	char error[24], detail[64];
	int room;
	long rev;
	char code[AE_FRIEND_CODE_SIZE];             /* this player's own friend code, from a hello or a poll */
	char sid[40];                               /* a hello's session */
	char nonce[40];                             /* a challenge's nonce */
	int poll_ms;                                /* a hello's poll interval, 0 when none */
	int has_party;
	char party_id[AE_PC_ID];
	char leader[AE_FRIEND_CODE_SIZE];
	int privacy;                                /* enum ae_pnet_privacy */
	int seats;
	char link[28];                              /* the invite link's text, for the leader only */
	int member_count;
	struct ae_pc_member member[AE_PNET_MAX];
	int invite_count;
	struct ae_pc_invite invite[AE_PC_INVITES];
	struct ae_pc_search search;
	struct ae_pc_follow follow;
	int steam_count;
	struct ae_pc_steam steam[AE_PC_STEAM];
	char notice[64];
};

/* the text the hello signs: "ae-party-hello-2\n<origin>\n<nonce>\n<public key in lower-case hex>": the service's own address and the
single-use nonce its challenge gave, so a hello made for one service is worth nothing at another */
void ae_pc_hello_text(const char *origin, const char *nonce, const char *pub_hex, char *out, int size);
/* a service address as the hello signs it: scheme and host in lower case, the port kept, the path as written without a trailing
slash, no query or fragment ("HTTPS://Host/party/" is "https://host/party"); 1 when it is an address that fits */
int ae_pc_origin(const char *base, char *out, int size);
/* a call's URL: base (any trailing slash dropped) + "/v1/" + op; 1 when it fits */
int ae_pc_url(char *url, int size, const char *base, const char *op);
/* adds "key":"value" to the JSON object in body (empty: starts "{...}"); both are JSON-escaped, text that is not UTF-8 reads as '?'.
1 when it fits (body is left as it was when not). Every value on the wire is a string */
int ae_pc_body_add(char *body, int size, const char *key, const char *value);

/* the wait between polls: the service's poll_ms held to 1000-15000 ms, and at least 5000 ms while there is no party and no search (active 0) */
int ae_pc_poll_ms(int service_ms, int active);

/* the service's answer read into out (cleared first): 1 when it was a JSON object, ok set by the absence of "error"; 0 for anything else */
int ae_pc_view_parse(const char *json, int length, struct ae_pc_view *out);
/* the party of a view as the rules module holds it (a view with no party: an empty record); leader is where the leader sits */
void ae_pc_party(const struct ae_pc_view *view, struct ae_pnet *party);
/* a view's one-line summary for the log: "party 3 seats leader K7QM rev 12" */
void ae_pc_view_summary(const struct ae_pc_view *view, char *text, int size);

#endif
