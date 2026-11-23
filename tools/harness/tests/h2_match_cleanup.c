#include "harness.h"
/* ae_h2_match.c's spawn hold while a round's map cleanup waits: held only in the engines whose update runs and
clears the cleanup, so one left from the previous game holds no spawn in a Race game */

#include "config.inc"
enum { _object_dead_bit };
struct object_fields { unsigned long damage_flags; };
struct unit_datum { struct object_fields object; };
struct player_datum { boolean quit_out_of_game; long unit_index, death_time; };
struct data_iterator { int index; long datum_index; };
struct ae_variant_ext { long max_active_players; };
struct game_variant { long game_engine_index; };
static struct game_variant variant;
static int engine_object;
static void *game_engine;
static boolean engine_running;
static int round_on;
static struct ae_variant_ext ext;
static void *player_data;

static boolean network_game_distributed_client(void) { return FALSE; }
static boolean game_engine_running(void) { return engine_running; }
static struct game_variant *game_engine_get_variant(void) { return &variant; }
static boolean ae_round_on(void) { return round_on; }
static boolean ae_round_holds_spawn(long player_index) { return FALSE; }
static struct ae_variant_ext const *ae_variant_ext_game(void) { return &ext; }
static struct player_datum *player_get(long index) { CHECK(FALSE, "no player read with no active-player limit"); return NULL; }
static void data_iterator_new(struct data_iterator *iterator, void *data) { iterator->index = 0; }
static void *data_iterator_next(struct data_iterator *iterator) { return NULL; }
static struct unit_datum *unit_try_and_get(long index) { return NULL; }
static boolean ae_h2_may_spawn(long max, int alive, int ahead) { return TRUE; }
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(index) ((index) & 0xFFFF)
#include "under_test.inc"

static void start(long engine)
{
	game_engine = &engine_object;
	engine_running = TRUE;
	round_on = 0;
	ext.max_active_players = 0;
	variant.game_engine_index = engine;
	st.cleanup_at = NONE;
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	CASE("race-after-round-reset")
	{
		/* a CTF round's reset 2 ticks before the game ended: its cleanup still waits when a Race game starts */
		start(game_engine_ctf);
		st.cleanup_at = 1000;
		start(game_engine_race);
		st.cleanup_at = 1000;
		CHECK(!ae_h2_match_holds_spawn(0), "a Race spawn held by the previous game's cleanup");
		return 0;
	}
	CASE("ctf-cleanup-waits")
	{
		start(game_engine_ctf);
		st.cleanup_at = 1000;
		CHECK(ae_h2_match_holds_spawn(0), "a spawn let through before the round's cleanup ran");
		return 0;
	}
	CASE("slayer-no-cleanup")
	{
		start(game_engine_slayer);
		CHECK(!ae_h2_match_holds_spawn(0), "a spawn held with no cleanup waiting");
		return 0;
	}
	CASE("no-engine")
	{
		start(game_engine_race);
		game_engine = NULL;
		st.cleanup_at = 1000;
		CHECK(!ae_h2_match_holds_spawn(0), "a spawn held with no game engine");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
