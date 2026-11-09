/*
AE_MARK_CALLS.C

CALLS: the wheel's calls and the answers to a mark (OK, NEVER MIND, DIBS, CANCEL), through the host. A client says what it
means on message 100; the host checks it (whose it is, the team, the range and line, the guard, the mute), keeps it and
sends it to the marker's team on 101. Every new mark also goes out on the plain messages 98 and 99, with the kind an older
build knows, so a team with an older build still sees it; a build with both makes the mark from the detail and lets the plain
one be. Ids and numbers cross, never text: each machine has the words.

The host and a local game take their own players' calls and answers through the same rules with no message: split-screen
players on one machine share the marks, so they see each other's answers. The rules are ae_mark_rules.c's (unit tested).
*/

#include "cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"

#include "ae_mark_calls.h"
#include "ae_mark_places.h"
#include "ae_mark_rules.h"
#include "ae_marks.h"
#include "ae_wheel_rules.h"
#include "network_chat.h"
#include "network_distributed.h"

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

struct call_message
{
	struct distributed_message_header header;
	struct distributed_call call;
};

static boolean calls_detail_seen;
static struct ae_mark_spam reply_spam[MARK_PLAYERS];

/* ---------- private code */

static boolean call_connected_as(short connection)
{
	return game_connection() == connection;
}

static boolean call_finite(struct distributed_call const *call)
{
	return ae_mark_spot_finite(call->x, call->y, call->z) != 0;
}

/* ae_bots_marks.inc */
void ae_bots_marks_heard(struct player_datum const *marker, short mark_kind, long object_index, real_point3d const *spot, short wheel_entry);
boolean ae_bots_marks_ride(struct player_datum const *marker);

static void call_fill(struct distributed_call *call, long absolute, int verb, struct mark const *mark)
{
	csmemset(call, 0, sizeof(*call));
	call->player_index = (byte)absolute;
	call->verb = (byte)verb;
	if (!mark)
		return;
	call->entry = (byte)(mark->entry < 0 ? AE_WHEEL_SMART : mark->entry);
	call->target_player = (byte)ae_mark_wire_counts(mark->repeats, mark->serial);
	call->timer_plus_one = (byte)mark->timer_plus_one;
	call->place = (byte)mark->place;
	call->kind = (byte)mark->kind;
	call->word = (byte)mark->word;
	call->x = mark->spot.x;
	call->y = mark->spot.y;
	call->z = mark->spot.z;
}

/* (a client) to the host */
static void call_send_up(struct distributed_call const *call)
{
	struct call_message message;

	csmemset(&message, 0, sizeof(message));
	message.call = *call;
	distributed_send(&message, _distributed_message_call_up, 1, (word)sizeof(message), _distributed_to_host_reliably);
}

/* (the host) to every client machine with a player on the team, and only those */
static void call_send_down(struct distributed_call const *call, long team_index)
{
	long machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short count = ae_marks_team_machines(team_index, machines, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	short index;

	for (index = 0; index < count; index++)
	{
		struct call_message message;

		csmemset(&message, 0, sizeof(message));
		message.call = *call;
		distributed_send_to_machine_reliably(machines[index], &message, _distributed_message_call_down, 1, (word)sizeof(message));
	}
}

/* whether a player of this machine has that absolute index */
static boolean call_player_is_local(long absolute)
{
	short local;

	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
	{
		long player_index = local_player_get_player_index(local);

		if (player_index != NONE && DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) == absolute)
			return TRUE;
	}
	return FALSE;
}

/* the life a mark has now, in ticks from its birth */
static long call_life(struct mark const *mark)
{
	return mark->ends ? mark->ends - mark->born : AE_MARK_LIFE_TICKS;
}

/* whether this player's answer of this verb may stand on that mark. trusted: the host has checked it (its object is there) */
static boolean call_reply_ok(struct mark const *mark, int verb, long owner, long replier, long replier_team, boolean trusted)
{
	boolean same_team;
	int already;

	if (verb == AE_CALL_CANCEL)
		same_team = replier == owner;
	else
		same_team = game_engine_has_teams() && replier != owner && mark->team_index == replier_team;
	already = verb == AE_CALL_DIBS ? mark->dibs != -1 : (mark->responders >> replier) & 1;
	return ae_mark_reply_allowed(verb, mark->active, same_team, mark->kind, trusted || mark->object_index != NONE, already) != 0;
}

/* an answer that stands: an OK extends the mark's life */
static void call_reply_apply(struct mark *mark, int verb, long replier)
{
	switch (verb)
	{
	case AE_CALL_OK:
		mark->responders |= (unsigned short)(1u << replier);
		mark->ends = mark->born + ae_mark_life_after_ok(call_life(mark), mark->born, game_time_get());
		break;
	case AE_CALL_NEVER_MIND:
		mark->responders &= (unsigned short)~(1u << replier);
		break;
	case AE_CALL_DIBS:
		mark->dibs = (signed char)replier;
		break;
	}
}

/* ---------- the host takes a client's call */

/* a mark's repeats, the slot's own mark being the one before */
static short call_repeats(struct mark const *own, struct mark const *next)
{
	boolean same = own->active && own->entry == next->entry && own->kind == next->kind &&
		(next->object_index != NONE ? own->object_index == next->object_index :
		own->object_index == NONE && distance_squared3d(&own->spot, &next->spot) < 4.0f);

	return (short)ae_mark_repeats_after(same, own->repeats);
}

static void call_host_mark(struct distributed_call const *call)
{
	struct player_datum *player = distributed_player(call->player_index);
	struct mark next, *own;
	real_point3d spot;
	long found = NONE;
	short kind;
	int entry = call->entry;
	boolean dead, double_tap = call->verb == AE_CALL_MARK && entry == AE_WHEEL_ENEMY;

	if (!player || entry >= AE_WHEEL_COUNT || !call_finite(call) || (call->verb == AE_CALL_MARK && entry != AE_WHEEL_ENEMY &&
		entry != AE_WHEEL_SMART))
	{
		return;
	}
	dead = player->unit_index == NONE;
	if (!ae_wheel_entry_shown(entry, dead))
		return;
	spot.x = call->x;
	spot.y = call->y;
	spot.z = call->z;
	kind = (short)ae_mark_old_build_kind(entry);
	if (kind < 0)
		kind = call->kind;
	if (kind >= AE_MARK_KIND_COUNT)
		return;
	ae_marks_clear_slot(&next);
	if (dead)
	{
		/* (a dead player does not cast, and the death camera must not leak: the spot is the body's, whatever was sent) */
		if (!ae_marks_death_spot(player, &spot))
			return;
		kind = entry == AE_WHEEL_ENEMY ? AE_MARK_KIND_ENEMY : AE_MARK_KIND_PLACE;
		if (kind == AE_MARK_KIND_ENEMY)
			spot.z += MARK_ABOVE;
	}
	else if (entry == AE_WHEEL_REGROUP || entry == AE_WHEEL_RIDE)
	{
		/* (asking for something to come to you: at your feet, the host's own sight of them) */
		object_get_origin(object_get_ultimate_parent(player->unit_index), &spot);
	}
	else
	{
		if (kind != AE_MARK_KIND_PLACE)
			found = ae_marks_find(player, kind, &spot);
		/* (an enemy called with ENEMY may be a point, where no one stands to be found; the rest need what they say) */
		if (found == NONE && (kind == AE_MARK_KIND_ITEM || (kind == AE_MARK_KIND_ENEMY && entry != AE_WHEEL_ENEMY)))
		{
			platform_log("marks: host: player %d's call %d has nothing near its spot", (int)call->player_index, entry);
			return;
		}
		if (!ae_marks_host_reach(player, kind, &spot, found))
		{
			platform_log("marks: host: player %d's call %d is out of range or sight", (int)call->player_index, entry);
			return;
		}
	}
	if (!ae_marks_host_guard(call->player_index, double_tap))
		return;
	next.kind = kind;
	next.spot = spot;
	next.object_index = NONE;
	next.word = kind == AE_MARK_KIND_ENEMY ? MARK_WORD_ENEMY : MARK_WORD_HERE;
	/* (a tap at a body, a hill, a teleporter, a bomb site: its word is the client's; only the words of places stand) */
	if (kind == AE_MARK_KIND_PLACE && entry == AE_WHEEL_SMART && !dead && call->word > MARK_WORD_HERE && call->word < MARK_WORD_COUNT)
		next.word = call->word;
	if (kind == AE_MARK_KIND_ITEM)
	{
		next.object_index = found;
		next.word = TEST_FLAG(_object_mask_vehicle, object_get(found)->object.type) ? MARK_WORD_VEHICLE :
			TEST_FLAG(_object_mask_weapon, object_get(found)->object.type) ? MARK_WORD_WEAPON : MARK_WORD_ITEM;
		ae_marks_item_name(&next, found);
	}
	next.active = TRUE;
	next.team_index = player->team_index;
	next.born = game_time_get();
	next.ends = next.born + AE_MARK_LIFE_TICKS;
	next.entry = (short)entry;
	/* (a tap at an empty power spawn carries its timer; the host reads it from the spot, not from what the client says) */
	next.timer_plus_one = !dead && entry == AE_WHEEL_SMART && kind == AE_MARK_KIND_PLACE && next.word == MARK_WORD_HERE ?
		ae_marks_timer_for(&spot) : 0;
	/* (a place the map names: a fact's row has already named a power spawn) */
	next.place = !next.timer_plus_one && next.kind == AE_MARK_KIND_PLACE && next.word == MARK_WORD_HERE ?
		(short)ae_mark_place_at(&next.spot, next.name, MARK_NAME_SIZE) : 0;
	own = ae_marks_slot(call->player_index);
	next.repeats = call->verb == AE_CALL_WHEEL ? call_repeats(own, &next) : 0;
	next.serial = (short)ae_mark_serial_next(own->serial);
	/* (muted on this machine: the mark is kept, as the team's state, and only its tone and its drawing are left out here) */
	*own = next;
	if (!double_tap && !ae_marks_muted_here(player) && ae_marks_heard_here(player->team_index))
		ae_marks_tone(next.kind);
	platform_log("marks: from player %d call %d kind %d team %ld", (int)call->player_index, entry, (int)kind, (long)player->team_index);
	if (entry != AE_WHEEL_RIDE || !ae_bots_marks_ride(player))
		ae_bots_marks_heard(player, next.kind, next.object_index, &next.spot, (short)entry);
	ae_marks_share_plain(call->player_index, next.kind, next.word, &next.spot, next.team_index);
	ae_mark_calls_send_detail(call->player_index, &next);
	ae_marks_feed(call->player_index, &next);
}

static void call_host_reply(struct distributed_call const *call)
{
	struct player_datum *replier = distributed_player(call->player_index);
	struct mark *mark;
	long owner = call->target_player;

	if (!replier || owner >= MARK_PLAYERS)
		return;
	mark = ae_marks_slot(owner);
	if (!call_reply_ok(mark, call->verb, owner, call->player_index, replier->team_index, FALSE))
		return;
	/* (a reply's entry byte holds the serial of the mark it answers) */
	if (!ae_mark_reply_is_for(call->verb, call->entry, mark->serial, mark->ends ? mark->ends : mark->born + AE_MARK_LIFE_TICKS,
		game_time_get()))
	{
		return;
	}
	/* (an answer is a call to the team: counted by a guard of its own) */
	if (call->verb != AE_CALL_CANCEL)
	{
		int said = ae_mark_spam_take(&reply_spam[call->player_index], game_time_get());

		if (said == AE_MARK_SPAM_NOW_BLOCKED || said == AE_MARK_SPAM_STILL_BLOCKED)
			return;
	}
	platform_log("marks: from player %d answer %d on player %ld's mark", (int)call->player_index, (int)call->verb, owner);
	if (call->verb == AE_CALL_CANCEL)
	{
		mark->active = FALSE;
		ae_marks_share_clear(owner, mark->team_index);
		return;
	}
	call_reply_apply(mark, call->verb, call->player_index);
	call_send_down(call, mark->team_index);
}

/* ---------- a client takes the host's */

static void call_down_mark(struct distributed_call const *call, struct player_datum const *player)
{
	struct mark *mark;

	if (call->kind >= AE_MARK_KIND_COUNT || call->word >= MARK_WORD_COUNT || call->entry >= AE_WHEEL_COUNT || !call_finite(call))
		return;
	/* (the marker's own machine has it from the call, and heard it then) */
	if (call_player_is_local(call->player_index))
		return;
	mark = ae_marks_slot(call->player_index);
	/* (the plain mark of this call came first: it is the same mark, and has its tone) */
	if (mark->active && mark->entry == NONE && mark->kind == call->kind && mark->team_index == player->team_index)
	{
		mark->entry = call->entry;
		mark->repeats = (short)ae_mark_wire_repeats(call->target_player);
		mark->serial = (short)ae_mark_wire_serial(call->target_player);
		mark->timer_plus_one = call->timer_plus_one;
		mark->place = call->place;
		if (call->kind == AE_MARK_KIND_PLACE)
			ae_mark_place_name(call->place, mark->name, MARK_NAME_SIZE);
		ae_marks_feed(call->player_index, mark);
		return;
	}
	ae_marks_clear_slot(mark);
	mark->active = TRUE;
	mark->kind = call->kind;
	mark->word = call->word;
	mark->object_index = NONE;
	mark->team_index = player->team_index;
	mark->born = game_time_get();
	mark->ends = mark->born + AE_MARK_LIFE_TICKS;
	mark->entry = call->entry;
	mark->repeats = (short)ae_mark_wire_repeats(call->target_player);
	mark->serial = (short)ae_mark_wire_serial(call->target_player);
	mark->timer_plus_one = call->timer_plus_one;
	mark->place = call->place;
	if (call->kind == AE_MARK_KIND_PLACE)
		ae_mark_place_name(call->place, mark->name, MARK_NAME_SIZE);
	mark->spot.x = call->x;
	mark->spot.y = call->y;
	mark->spot.z = call->z;
	if (call->kind == AE_MARK_KIND_ITEM)
	{
		/* (the name is not sent: this machine has the item too) */
		long found = ae_marks_find(player, AE_MARK_KIND_ITEM, &mark->spot);

		if (found != NONE)
			ae_marks_item_name(mark, found);
	}
	platform_log("marks: from player %d call %d kind %d team %ld, heard here %d", (int)call->player_index, (int)call->entry,
		(int)call->kind, (long)player->team_index, (int)ae_marks_heard_here(player->team_index));
	if (!ae_marks_muted_here(player) && ae_marks_heard_here(player->team_index))
		ae_marks_tone(mark->kind);
	ae_marks_feed(call->player_index, mark);
}

static void call_down_reply(struct distributed_call const *call, struct player_datum const *replier)
{
	struct mark *mark;
	long owner = call->target_player;

	if (owner >= MARK_PLAYERS)
		return;
	mark = ae_marks_slot(owner);
	/* (the answer of a machine's own player comes back too, and has stood already: then it is refused) */
	if (!call_reply_ok(mark, call->verb, owner, call->player_index, replier->team_index, TRUE))
		return;
	call_reply_apply(mark, call->verb, call->player_index);
}

/* ---------- public code */

unsigned short ae_mark_calls_entry_size(void)
{
	return (unsigned short)sizeof(struct distributed_call);
}

void ae_mark_calls_reset(void)
{
	calls_detail_seen = FALSE;
	csmemset(reply_spam, 0, sizeof(reply_spam));
}

boolean ae_mark_calls_detail_seen(void)
{
	return calls_detail_seen;
}

void ae_mark_calls_send_detail(long absolute, struct mark const *mark)
{
	struct distributed_call call;

	if (!call_connected_as(_game_connection_network_server))
		return;
	call_fill(&call, absolute, AE_CALL_MARK, mark);
	call_send_down(&call, mark->team_index);
}

void ae_mark_calls_share(long absolute, int verb, struct mark const *mark)
{
	if (call_connected_as(_game_connection_network_server))
	{
		ae_marks_share_plain(absolute, mark->kind, mark->word, &mark->spot, mark->team_index);
		ae_mark_calls_send_detail(absolute, mark);
	}
	else if (call_connected_as(_game_connection_network_client))
	{
		struct distributed_call call;

		/* (a tap goes as it always did, to a host of any build; one of the newer words, which an older host drops, on 100) */
		if (verb == AE_CALL_MARK && mark->entry == AE_WHEEL_SMART && mark->word <= MARK_WORD_HERE)
		{
			ae_marks_share_plain(absolute, mark->kind, mark->word, &mark->spot, mark->team_index);
			return;
		}
		call_fill(&call, absolute, verb, mark);
		call_send_up(&call);
	}
}

void ae_mark_calls_tick(void)
{
	long now = game_time_get();
	short index;

	for (index = 0; index < MARK_PLAYERS; index++)
	{
		struct mark *mark = ae_marks_slot(index);
		short bit;

		if (!mark->active)
			continue;
		if (now >= mark->born && now - mark->born >= call_life(mark))
		{
			mark->active = FALSE;
			continue;
		}
		/* (an answer from a player who left, or who is not on the marker's team now, is let go) */
		for (bit = 0; bit < MARK_PLAYERS; bit++)
		{
			boolean said = (mark->responders >> bit) & 1 || mark->dibs == bit;
			struct player_datum const *player;

			if (!said)
				continue;
			player = distributed_player(bit);
			if (player && !player->quit_out_of_game && game_engine_has_teams() && player->team_index == mark->team_index)
				continue;
			mark->responders &= (unsigned short)~(1u << bit);
			if (mark->dibs == bit)
				mark->dibs = -1;
		}
	}
}

int ae_marks_answer(short local_player_index, int verb, int mark_player)
{
	long player_index;
	struct player_datum *player;
	struct mark *mark;
	long replier;

	if (!VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS) || !game_engine_running() || !VALID_INDEX(mark_player, MARK_PLAYERS))
		return 0;
	player_index = local_player_get_player_index(local_player_index);
	player = player_index == NONE ? NULL : player_try_and_get(player_index);
	if (!player)
		return 0;
	replier = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	mark = ae_marks_slot(mark_player);
	if (!call_reply_ok(mark, verb, mark_player, replier, player->team_index, call_connected_as(_game_connection_network_client)))
		return 0;
	platform_log("marks: view %d: answer %d on player %d's mark", local_player_index, verb, mark_player);
	if (verb == AE_CALL_CANCEL)
	{
		struct distributed_call call;

		mark->active = FALSE;
		if (call_connected_as(_game_connection_network_server))
			ae_marks_share_clear(mark_player, player->team_index);
		else if (call_connected_as(_game_connection_network_client))
		{
			/* (the plain clear first, for a host without calls; a host with them clears twice to the same end) */
			ae_marks_share_clear(mark_player, player->team_index);
			call_fill(&call, replier, verb, NULL);
			call.target_player = (byte)mark_player;
			call_send_up(&call);
		}
		return 1;
	}
	if (call_connected_as(_game_connection_network_client))
	{
		/* (it shows when the host's comes back: a refusal there leaves nothing to take back) */
		struct distributed_call call;

		call_fill(&call, replier, verb, NULL);
		call.target_player = (byte)mark_player;
		/* (which mark: the host takes it for this one only if it is still there) */
		call.entry = (byte)mark->serial;
		call_send_up(&call);
		return 1;
	}
	call_reply_apply(mark, verb, replier);
	if (call_connected_as(_game_connection_network_server))
	{
		struct distributed_call call;

		call_fill(&call, replier, verb, NULL);
		call.target_player = (byte)mark_player;
		call_send_down(&call, mark->team_index);
	}
	return 1;
}

void ae_mark_calls_handle_up(long machine_index, void const *entries, int from_stream)
{
	struct distributed_call call;
	struct player_datum *player;

	if (machine_index == NONE || !from_stream || !game_engine_running() || !ae_marks_rule_on())
		return;
	csmemcpy(&call, entries, sizeof(call));
	if (call.player_index >= MARK_PLAYERS || !distributed_machine_has_player(machine_index, call.player_index) ||
		(player = distributed_player(call.player_index)) == NULL || player->quit_out_of_game)
	{
		return;
	}
	if (network_chat_host_name_muted(player->name))
	{
		platform_log("marks: host: player %d is muted", (int)call.player_index);
		return;
	}
	switch (call.verb)
	{
	case AE_CALL_MARK:
	case AE_CALL_WHEEL:
		call_host_mark(&call);
		break;
	case AE_CALL_OK:
	case AE_CALL_NEVER_MIND:
	case AE_CALL_DIBS:
	case AE_CALL_CANCEL:
		call_host_reply(&call);
		break;
	}
}

void ae_mark_calls_handle_down(void const *entries, int from_stream)
{
	struct distributed_call call;
	struct player_datum *player;

	if (!from_stream || !game_engine_running())
		return;
	csmemcpy(&call, entries, sizeof(call));
	if (call.player_index >= MARK_PLAYERS || (player = distributed_player(call.player_index)) == NULL)
		return;
	/* (the host has detail to send: the plain marks that follow are the same ones) */
	calls_detail_seen = TRUE;
	switch (call.verb)
	{
	case AE_CALL_MARK:
		call_down_mark(&call, player);
		break;
	case AE_CALL_OK:
	case AE_CALL_NEVER_MIND:
	case AE_CALL_DIBS:
		call_down_reply(&call, player);
		break;
	}
}
