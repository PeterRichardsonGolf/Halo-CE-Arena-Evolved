#include "harness.h"
/* actor_stimulus.c's actor_stimulus_heard_shooting: an actor hears a friend shoot and takes the friend's aim-assist
target (kept on the player for 3 s after the shot) as an enemy. A target deleted inside those 3 s (a script's
object_destroy or ai_erase) is no unit any more: it is passed over, not read */

typedef unsigned short word;
enum { _actor_combat_status_certain = 3, _direction_specification_prop = 1, _secondary_look_shooting_prop = 2,
	_secondary_look_priority_default = 0 };
struct prop_datum { boolean enemy; long unit_index; };
struct unit_fields { long player_index, actor_index; };
struct object_fields { short owner_team_index; };
struct unit_datum { struct object_fields object; struct unit_fields unit; };
struct player_datum { long aim_assist_unit_index, aim_assist_timestamp; };
struct actor_meta { short team_index; };
struct actor_state { short combat_status; };
struct actor_datum { struct actor_meta meta; struct actor_state state; };
struct direction_specification { short type; long prop_index; };

static struct prop_datum prop;
static struct unit_datum units[4];
static boolean unit_live[4];
static struct player_datum player;
static struct actor_datum actor;
static int orphans;
static long now = 1000;

static struct prop_datum *prop_get(long index) { return &prop; }
static struct unit_datum *unit_get(long index)
{
	CHECK(index >= 0 && index < 4 && unit_live[index], "unit index %ld is unused or changed", index);
	return &units[index];
}
static struct unit_datum *unit_try_and_get(long index) { return index >= 0 && index < 4 && unit_live[index] ? &units[index] : NULL; }
static struct player_datum *player_get(long index) { return &player; }
static struct actor_datum *actor_get(long index) { return &actor; }
static long game_time_get(void) { return now; }
static boolean game_team_is_enemy(word a, word b) { return a != b; }
static void actor_stimulus_enter_combat_perceived_enemy(long actor_index, long prop_index) { }
static void actor_stimulus_enter_combat_friend_in_combat(long actor_index, long prop_index) { }
static void actor_perception_create_orphan_from_friend(long actor_index, long unit_index, long a, long b) { orphans++; }
static void actor_derive_target_information(long actor_index, long other) { }
static void actor_look_secondary(long actor_index, short look, short priority, struct direction_specification *direction) { }
#include "under_test.inc"

static void world(void)
{
	memset(units, 0, sizeof(units));
	unit_live[0] = unit_live[1] = TRUE;
	unit_live[2] = unit_live[3] = FALSE;
	prop.enemy = FALSE;
	prop.unit_index = 0;               /* the friend who shot */
	units[0].unit.player_index = 0;
	units[0].unit.actor_index = NONE;
	units[1].object.owner_team_index = 2; /* the friend's target, an enemy of the actor */
	actor.meta.team_index = 1;
	player.aim_assist_unit_index = 1;
	player.aim_assist_timestamp = now - 30;
	orphans = 0;
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	world();
	CASE("live-target")
	{
		actor_stimulus_heard_shooting(0, 0);
		CHECK(orphans == 1, "the live target not taken");
		return 0;
	}
	CASE("deleted-target")
	{
		unit_live[1] = FALSE;
		actor_stimulus_heard_shooting(0, 0);
		CHECK(orphans == 0, "a deleted target taken");
		return 0;
	}
	CASE("old-target")
	{
		player.aim_assist_timestamp = now - 91;
		unit_live[1] = FALSE;
		actor_stimulus_heard_shooting(0, 0);
		CHECK(orphans == 0, "a target older than 3 s taken");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
