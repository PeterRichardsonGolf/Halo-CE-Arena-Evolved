/*
AE_PARTY_FOLLOW.C

See ae_party_follow.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "ae_address_rules.h"
#include "ae_glue_lobby.h"
#include "ae_party_follow.h"
#include "ae_party_follow_rules.h"
#include "ae_screen_create_game.h"
#include "ae_ui.h"
#include "../src/ae_party_client.h"
#include "../src/ae_worker.h"
#include "../src/p2p.h"

void platform_log(const char *format, ...);
boolean main_menu_is_active(void);

enum { FOLLOW_IDLE, FOLLOW_JOINING };

static struct
{
	int mode;
	unsigned long generation;
	struct ae_follow_state seen;
	char target[AE_PC_ADDRESS];     /* what is being joined */
	char last_server[AE_PC_ADDRESS];
	char failed_server[AE_PC_ADDRESS]; /* a found server this PC could not join: the next QUICK PLAY tells the service */
	int target_is_server;
	char note[80];
	char announced[AE_PC_ADDRESS];  /* the invite of a game this PC hosts that the party was told */
	struct ae_follow_members members;   /* the party as last seen, to notice a member gone */
} follow;

/* in the menus (any screen of them) with no session of this PC's own: a follow pulls the player to the game, over whatever is up */
static int menus_free(void)
{
	return main_menu_is_active() && !ae_lobby_owns_session();
}

static void start(struct ae_follow_action const *action)
{
	struct ae_result result;
	struct ae_address address;
	unsigned now = ae_worker_milliseconds();

	/* (the service's word is not enough to move this PC: a listed public server or a real invite, one follow in 10 s, and a pause
	after three failures) */
	if (!ae_follow_may_start(&follow.seen, now))
	{
		snprintf(follow.note, sizeof(follow.note), follow.seen.paused ? "follow paused after failures: press any party control" : "follow waits: one a 10 s");
		platform_log("ae party: follow: %s", follow.note);
		return;
	}
	if (action->kind == AE_FOLLOW_SERVER ? !ae_follow_target_ok(action->address, ae_party_client_service_local()) :
		!ae_follow_invite_ok(action->invite))
	{
		snprintf(follow.note, sizeof(follow.note), "refused a follow to %s", action->kind == AE_FOLLOW_SERVER ? "an address that is not a public server" : "an invite that is not one");
		platform_log("ae party: follow: %s", follow.note);
		return;
	}
	ae_follow_started(&follow.seen, now);
	if (action->kind == AE_FOLLOW_SERVER)
	{
		if (ae_address_parse(action->address, ae_lobby_host_port(), &address) != AE_ADDRESS_OK)
		{
			snprintf(follow.note, sizeof(follow.note), "not an address: %s", action->address);
			platform_log("ae party: follow: %s", follow.note);
			return;
		}
		result = ae_lobby_join_host(address.host, address.port);
		snprintf(follow.target, sizeof(follow.target), "%s", action->address);
	}
	else
	{
		result = ae_lobby_join_address(action->invite);
		snprintf(follow.target, sizeof(follow.target), "invite");
	}
	follow.target_is_server = action->kind == AE_FOLLOW_SERVER;
	if (!result.ok)
	{
		snprintf(follow.note, sizeof(follow.note), "could not join %s: %s", follow.target, result.reason);
		platform_log("ae party: follow: %s", follow.note);
		ae_follow_failed(&follow.seen, now);
		if (follow.target_is_server)
			snprintf(follow.failed_server, sizeof(follow.failed_server), "%s", follow.target);
		return;
	}
	follow.mode = FOLLOW_JOINING;
	if (action->kind == AE_FOLLOW_SERVER)
		snprintf(follow.last_server, sizeof(follow.last_server), "%s", action->address);
	snprintf(follow.note, sizeof(follow.note), "joining %s", follow.target);
	platform_log("ae party: follow: joining %s", follow.target);
}

static void joining(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result failure;
	int state = ae_lobby_join_state(&failure);

	if (state == 0)
		return;
	if (state < 0)
	{
		snprintf(follow.note, sizeof(follow.note), "join of %s failed: %s", follow.target,
			failure.reason[0] ? failure.reason : "left");
		platform_log("ae party: follow: %s", follow.note);
		follow.mode = FOLLOW_IDLE;
		ae_follow_failed(&follow.seen, ae_worker_milliseconds());
		if (follow.target_is_server)
			snprintf(follow.failed_server, sizeof(follow.failed_server), "%s", follow.target);
		return;
	}
	if (!ae_lobby_roster(&roster).ok || !roster.joined)
		return;
	if (!ae_lobby_add_local_player(0).ok)
	{
		ae_lobby_leave();
		snprintf(follow.note, sizeof(follow.note), "%s was full when joined", follow.target);
		platform_log("ae party: follow: %s", follow.note);
		follow.mode = FOLLOW_IDLE;
		return;
	}
	platform_log("ae party: follow: joined %s", follow.target);
	snprintf(follow.note, sizeof(follow.note), "joined %s", follow.target);
	follow.mode = FOLLOW_IDLE;
	ae_screen_create_game_lobby_joined("PARTY");
}

void ae_party_follow_tick(void)
{
	struct ae_pc_view view;
	struct ae_follow_action action;
	char me[AE_FRIEND_CODE_SIZE];
	unsigned long generation;

	if (!ae_party_client_enabled())
		return;
	if (follow.mode == FOLLOW_JOINING)
	{
		if (game_engine_running() && !main_menu_is_active())
			follow.mode = FOLLOW_IDLE;
		else
			joining();
		return;
	}
	generation = ae_party_client_view(&view);
	if (!generation || !ae_party_client_code(me, sizeof(me)))
		return;
	follow.generation = generation;
	/* (the leader hosting an internet game: the party is told its invite once) */
#ifdef HALO_GAME_BROWSER
	{
		char invite[AE_PC_ADDRESS];

		/* (a member left or was removed: the invite they hold is replaced, and the rest are told the new one) */
		if (follow.announced[0] && !strcmp(view.leader, me) && ae_follow_member_left(&follow.members, &view) &&
			p2p_hosting_invite(invite, (int)sizeof(invite)))
		{
			p2p_hosting_rotate_invite();
			follow.announced[0] = 0;
			platform_log("ae party: a member left: the hosted game's invite is replaced");
		}
		/* (a public game with no password only: the party is never handed the invite of a private or locked game) */
		if (p2p_hosting_listable_invite(invite, (int)sizeof(invite)) && ae_follow_should_announce(&view, me, invite, follow.announced))
		{
			snprintf(follow.announced, sizeof(follow.announced), "%s", invite);
			ae_party_client_call("party_follow", "kind", "hosted", "invite", invite, NULL);
			/* (from here on a private game or a password makes a new token) */
			p2p_hosting_invite_announced(invite);
			platform_log("ae party: the party is asked to follow into the game this PC hosts");
		}
	}
#endif
	ae_follow_members_note(&follow.members, &view);
	ae_follow_decide(&view, me, !menus_free(), &follow.seen, &action);
	switch (action.kind)
	{
	case AE_FOLLOW_SERVER:
	case AE_FOLLOW_INVITE:
		start(&action);
		break;
	case AE_FOLLOW_BOTS_OFFER:
		snprintf(follow.note, sizeof(follow.note), "nothing has room: AE TOURNAMENT with bots is the way on%s", action.ask ? " (the leader chooses)" : "");
		platform_log("ae party: follow: %s", follow.note);
		break;
	case AE_FOLLOW_STOPPED:
		snprintf(follow.note, sizeof(follow.note), "the search stopped: %s", action.reason);
		platform_log("ae party: follow: %s", follow.note);
		break;
	default:
		break;
	}
}

int ae_party_follow_continue(const char *playlist)
{
	struct ae_pc_view view;
	char me[AE_FRIEND_CODE_SIZE];
	int ask;

	if (!ae_party_client_enabled() || ae_party_client_state() != AE_PC_UP || !ae_party_client_view(&view) ||
		!ae_party_client_code(me, sizeof(me)))
		return 0;
	if (!ae_follow_continue(&view, me, playlist, &ask))
		return 0;
	if (ask)
	{
		/* (the same server first when it has room: the party just played there) */
		if (follow.failed_server[0])
			ae_party_client_call("quick_play", "playlist", playlist, "again", "1", "prefer", follow.last_server, "failed", follow.failed_server, NULL);
		else
			ae_party_client_call("quick_play", "playlist", playlist, "again", "1", "prefer", follow.last_server, NULL);
		follow.failed_server[0] = 0;
		platform_log("ae party: continue: QUICK PLAY %s again (prefer %s)", playlist, follow.last_server[0] ? follow.last_server : "none");
	}
	else
		platform_log("ae party: continue: waiting for the leader's next game");
	return 1;
}

void ae_party_follow_resume(void)
{
	ae_follow_resume(&follow.seen);
}

void ae_party_follow_status(char *text, int size)
{
	snprintf(text, (size_t)size, "%s", follow.note[0] ? follow.note : "idle");
}
