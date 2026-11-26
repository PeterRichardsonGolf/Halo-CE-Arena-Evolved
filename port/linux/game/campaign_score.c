/*
CAMPAIGN_SCORE.C

The campaign's score (game.campaign_score, Settings > Gameplay > Scoring's
SCORING, off by default: the stock game has none), as Halo 3's and the Master
Chief Collection's. Its values are campaign_score_values.json's
(campaign_score_values.c, which tools/campaign_score_values.py writes from it
as the build is configured):

- a kill is worth its enemy's base points (by its actor's type and its
  variant's name, or a vehicle's by its name), times its style's multiplier
  (one shot one kill, a headshot, a melee hit, a backsmack: from behind),
  times the difficulty's, times the skulls on, rounded to a point;
- an ally killed (or a friendly vehicle destroyed), another player killed
  (a betrayal), a death and Revert to Checkpoint take points as they are
  (the only cost of a checkpoint: the score and the time go on through it);
- when the level is won, its points are multiplied by its time's multiplier,
  by its time over the level's par time.

Only the machine that runs the AI knows the kills: the host of a co-op
game, or a single player. The host sends each entry, and the level's win, to
the clients of a network co-op game (network_coop.c), which keep the same
log. A co-op game's score is the players' added together (network.coop_score_type
"cooperative") or each player's own ("competitive"); the host's choice counts.

The score is a log of entries, each with the game time it happened at and
its player's absolute index (the same on every machine of a co-op game); the
totals are summed from it. Going back to a checkpoint (a death, or Revert to
Checkpoint) takes nothing from the log, nor from the level's time, which is
counted on each tick the game time goes forward (campaign_score_tick), not
read from it.

On the HUD, the score and the level's time (a strip of the par time left
over them, and a row of the score's multipliers: the difficulty's, the
skulls', the time's and their total) are a panel at the bottom right, as the
Master Chief Collection's (campaign_score_draw_view); the skulls' icons are drawn
above it (skulls.c, campaign_score_hud_height). The scoreboard (hold BACK, or
tab: game_engine.c) lists every player's score.

In a multiplayer game (game.multiplayer_scoring, Settings > Gameplay >
Scoring's MULTIPLAYER) nothing is scored: each machine gives its own
players the campaign's medals (game.multiplayer_medals:
campaign_score_player_killed, campaign_medals.c), logged as entries here
for their history and popups, and draws its own players' panel
(game.multiplayer_score_hud): their kills, deaths and assists, and the game
type's score (captures, time on the hill). No other machine is told of
either.

When a level is won (main_won_map) its totals, its time against its par,
each player's score and the best variants are written to the log, and shown
over the level until A or B is pressed, before the main menu (or the co-op
lobby) loads.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/actor_types.h"
#include "cutscene/cinematics.h"
#include "input/input.h"
#include "interface/interface.h"
#include "interface/hud_messaging.h"
#include "main/console.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"
#include "units/units.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"

#include "campaign_score.h"
#include "ae_pve_glue.h" /* AE hook */
#include "cev/custom_edition_maps.h" /* AE hook */
#include "campaign_score_values.h"
#include "campaign_medals.h"
#include "community.h"
#include "social.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "halo_skulls.h"
#include "ae_pve_score_menus.h" /* AE hook */
#include "halo_port_limits.h"
#include "halo_firefight.h"
#include "interface/ui_widget.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_group_lookup.h"

#include <stdio.h>
#include <string.h>
#include <xtl.h>

/* the records' file, in each profile's folder */
#define CAMPAIGN_SCORE_RECORDS_FILE "campaign_records.txt"

/* ---------- constants */

enum
{
	/* the kills one level logs (a level places a few hundred enemies) */
	CAMPAIGN_SCORE_MAXIMUM_KILLS = 8192,
	/* the players a score is kept for (by absolute index: a multiplayer
	game's every player's medals) */
	CAMPAIGN_SCORE_MAXIMUM_PLAYERS = HALO_PORT_MAXIMUM_NETWORK_PLAYERS,
	/* the kinds of variant the summary tells apart */
	CAMPAIGN_SCORE_MAXIMUM_VARIANTS = 64,
	/* the lines of the summary, and their length */
	CAMPAIGN_SCORE_SUMMARY_LENGTH = 56,
	/* the players the summary lists (a co-op game's) */
	CAMPAIGN_SCORE_REPORT_PLAYERS = 4,
	/* the cards of this machine's players (a split screen game's) */
	CAMPAIGN_SCORE_LOCAL_CARDS = 4,
	/* the records' text, a profile's every level's (campaign_records.txt) */
	CAMPAIGN_SCORE_RECORDS_LENGTH = 1024,
	/* the units a level's one shot, one kill remembers as hurt */
	CAMPAIGN_SCORE_HURT_SLOTS = 2048,
	/* units.c's _unit_damage_part_head (its enum is its own): a body part
	object_damage_aftermath is told of */
	CAMPAIGN_SCORE_HEAD_BODY_PART = 2,
};

/* the HUD's panel, in the HUD's 480 lines (scaled with the view as the
skulls' icons are): its corner's margins (the skulls'), its width, its
strip of segments on top, and each row's padding */
enum
{
	CAMPAIGN_SCORE_PANEL_MARGIN = 12,
	CAMPAIGN_SCORE_PANEL_WIDTH = 216,
	CAMPAIGN_SCORE_STRIP_HEIGHT = 7,
	CAMPAIGN_SCORE_STRIP_SEGMENTS = 24,
	CAMPAIGN_SCORE_STRIP_GAP = 2,
	CAMPAIGN_SCORE_ROW_GAP = 2,
	CAMPAIGN_SCORE_ROW_PADDING = 3,
	/* (the shortest a row is) */
	CAMPAIGN_SCORE_ROW_MINIMUM = 22,
	/* (the least room between a row's name and its value) */
	CAMPAIGN_SCORE_ROW_TEXT_GAP = 20,
	/* (the skulls' icons this far above the panel) */
	CAMPAIGN_SCORE_SKULLS_GAP = 4,
	/* the medal history's icons, their gap, and the most it shows (the
	JSON's medal history, up to this) */
	CAMPAIGN_SCORE_HISTORY_SIZE = 18,
	CAMPAIGN_SCORE_HISTORY_GAP = 3,
	CAMPAIGN_SCORE_HISTORY_MAXIMUM = 16,
};

/* the panel's colours (the summary's cards' edge, CAMPAIGN_SCORE_PANEL_EDGE,
campaign_score.h's); the HUD's panel is in the player's own colour
(campaign_score_palette) */
#define CAMPAIGN_SCORE_PANEL_FILL campaign_score_palette.fill
#define CAMPAIGN_SCORE_HUD_EDGE campaign_score_palette.edge
#define CAMPAIGN_SCORE_STRIP_FILL campaign_score_palette.strip
#define CAMPAIGN_SCORE_STRIP_UNLIT campaign_score_palette.unlit

/* a melee hit from behind: the blow going this nearly the way the body faces */
#define CAMPAIGN_SCORE_BACKSMACK_DOT 0.5f

/* the summary's colour: a player's name in the chat (network_chat.c's
chat_color) */
#define CAMPAIGN_SCORE_BLUE_RED 0.34f
#define CAMPAIGN_SCORE_BLUE_GREEN 0.51f
#define CAMPAIGN_SCORE_BLUE_BLUE 0.86f

/* ---------- structures */

/* what an entry of the log is: a kill (its points the enemy's, as its style,
the difficulty and the skulls make them), or a penalty (as it is) */
enum
{
	_campaign_score_entry_kill = 0,
	/* an ally killed, or a friendly vehicle destroyed */
	_campaign_score_entry_ally,
	/* another player killed */
	_campaign_score_entry_betrayal,
	/* the player's death */
	_campaign_score_entry_death,
	/* Revert to Checkpoint, from the pause menu */
	_campaign_score_entry_revert,
	/* a medal (campaign_medals.c): variant_index its medal */
	_campaign_score_entry_medal,
	/* a Firefight objective's points (firefight_objectives.c) */
	_campaign_score_entry_objective,
	NUMBER_OF_CAMPAIGN_SCORE_ENTRIES
};

/* the records a level won beat (campaign_score_record_level) */
enum
{
	_campaign_score_record_score_bit = 0,
	_campaign_score_record_time_bit,
	_campaign_score_record_medals_bit,
	_campaign_score_record_skulls_bit,
};

/* a kill's style, its entry's style bits */
enum
{
	_campaign_score_style_one_shot_bit = 0,
	_campaign_score_style_headshot_bit,
	_campaign_score_style_melee_bit,
	_campaign_score_style_backsmack_bit,
	NUMBER_OF_CAMPAIGN_SCORE_STYLES
};

struct campaign_score_entry
{
	long game_time;
	/* (the enemy's actor variant, or the vehicle's definition; NONE) */
	long variant_index;
	long points;
	short player_index;
	byte kind;
	byte style;
};

struct campaign_score_variant
{
	long variant_index;
	long kills;
	long points;
};

/* a level won's summary, as it was won (campaign_score_report_fill), shown
over it and the next level's start until A or B is pressed (its best enemy
to the log only) */
struct campaign_score_report
{
	boolean valid;
	boolean competitive;
	/* whose card it is: this machine's player (its name; 1 of 2, a split
	screen game's cards shown in turn), and its online result's slot
	(community_result_text) */
	wchar_t player[24];
	short player_number;
	short player_total;
	short result_slot;
	/* (this machine's first player's profile's records beaten) */
	boolean record_score;
	boolean record_time;
	short difficulty;
	wchar_t level[48];
	real difficulty_multiplier;
	real skulls_multiplier;
	real time_multiplier;
	long seconds;
	long par_seconds;
	long kills;
	long headshots;
	long melee;
	long medals;
	long deaths;
	long betrayals;
	long allies;
	long points;
	long score;
	short medal_counts[NUMBER_OF_CAMPAIGN_MEDALS];
	short player_count;
	struct
	{
		char name[24];
		long score;
		long kills;
	} players[CAMPAIGN_SCORE_REPORT_PLAYERS];
	char best_enemy[40];
	long best_enemy_kills;
	long best_enemy_points;
};

/* ---------- prototypes */

/* port_config.c's, cinematics.c's / cseries_windows.c's, tag_files.c's */
int config_boolean(const char *name);
unsigned long config_changes(void);
/* input_abstraction.c: how many ticks the keyboard's jump key (A) has been held */
byte input_abstraction_port_accept(short controller_index);
char *tag_get_name(long tag_index);
/* sdl_platform.c's: the log (debug.txt) only, not the console on screen */
void platform_log(char const *format, ...);
/* network_coop.c's: co-op is a network game on a campaign level; the host's
entries and its win are sent to the clients (campaign_score_network_kill, ...) */
boolean network_coop_active(void);
void network_coop_note_score_kill(short player_index, long variant_index, long points, short kind, byte style,
	boolean competitive);
void network_coop_note_score_won(boolean competitive);
/* port_config.c's */
char const *config_string(char const *name);
/* cinematics.c's */
void draw_quad(rectangle2d *rectangle, pixel32 color);
/* player_profile.c's: a profile's folder (its blam.sav's), ending with \\ */
boolean player_profile_get_enclosing_directory_path(long profile, char *full_path);

/* ---------- globals */

static struct campaign_score_entry campaign_score_entries[CAMPAIGN_SCORE_MAXIMUM_KILLS];
static long campaign_score_entry_count = 0;
static boolean campaign_score_full_logged = FALSE;
/* the last game time seen (NONE: none yet; a smaller one is a checkpoint
reverted to), and the level's time, counted on through reverts */
static long campaign_score_last_time = NONE;
static long campaign_score_ticks = 0;
/* a level just won, for the next one, and the one shown now: a card for each
of this machine's players, shown in turn */
static struct campaign_score_report campaign_score_pending[CAMPAIGN_SCORE_LOCAL_CARDS];
static short campaign_score_pending_count = 0;
static struct campaign_score_report campaign_score_summary[CAMPAIGN_SCORE_LOCAL_CARDS];
static short campaign_score_summary_count = 0;
static short campaign_score_summary_shown = 0;
/* (a button held when the summary came up does not dismiss it: only a new press) */
static boolean campaign_score_button_primed = FALSE;
static boolean campaign_score_button_held = FALSE;
/* the summary is shown over the level just won, which waits for it */
static boolean campaign_score_postgame = FALSE;
/* whether each player keeps a score of their own in co-op (network.coop_score_type
"competitive"): the host's choice, which a client is told with the kills */
static boolean campaign_score_competitive = FALSE;
/* the HUD panel's colours, the player's of the view being drawn
(campaign_score_palette_set): the Master Chief Collection's blue until one
is known */
struct campaign_score_palette
{
	pixel32 fill;
	pixel32 edge;
	pixel32 strip;
	pixel32 unlit;
	real_rgb_color subline;
};

static struct campaign_score_palette const campaign_score_palette_blue =
	{ 0xA00A2246, 0xE04F9BE0, 0xC06EB4F0, 0x406EB4F0, { { 0.62f, 0.80f, 1.0f } } };
static struct campaign_score_palette campaign_score_palette =
	{ 0xA00A2246, 0xE04F9BE0, 0xC06EB4F0, 0x406EB4F0, { { 0.62f, 0.80f, 1.0f } } };

/* each local player's colour last seen (a campaign player with no unit, dead
or not yet spawned, keeps theirs) */
static real_rgb_color campaign_score_player_colors[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static boolean campaign_score_player_color_known[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* the height the panel took in the view drawn last, for the skulls' icons
above it (campaign_score_hud_height), in the HUD's 480 lines */
static short campaign_score_drawn_height = 0;
/* the units hurt by any blow since the level began (or a checkpoint was
reverted to): a kill of one not among them is one shot, one kill. A set of
their indices (salted: a unit made later in a freed slot is another) */
static long campaign_score_hurt[CAMPAIGN_SCORE_HURT_SLOTS];
/* the last killing blow to a unit (campaign_score_killing_blow), for the
multiplayer kill it is announced as in the same tick: NONE for none */
static struct
{
	long unit_index;
	long game_time;
	long damage_definition_index;
	boolean headshot;
	boolean melee;
	boolean backsmack;
} campaign_score_blow = { NONE, NONE, NONE, FALSE, FALSE, FALSE };

static char const *const campaign_score_difficulty_names[NUMBER_OF_GAME_DIFFICULTY_LEVELS] =
	{ "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

/* ---------- private code */

static boolean campaign_score_level(
	void)
{
	struct scenario *scenario;

	if (!game_in_progress())
		return FALSE;
	if (firefight_active())
		return TRUE;
	if (game_engine_running())
		return FALSE;
	scenario = global_scenario_try_and_get();
	return scenario != NULL && scenario->type == _scenario_type_solo;
}

/* a multiplayer game (a game engine's) being played, with
game.multiplayer_scoring on */
static boolean campaign_score_multiplayer(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = FALSE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("game.multiplayer_scoring") != 0;
	}
	return enabled && game_in_progress() && game_engine_running() && !firefight_active();
}

/* the difficulty, as an index of the tables */
static short campaign_score_difficulty(
	void)
{
	short difficulty = firefight_active() ?
		(short)global_variant.game_engine_variant.firefight.difficulty :
		game_difficulty_level_get();

	return PIN(difficulty, 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1);
}

/* the host's (or a single machine's) co-op scoring type, from its config */
static boolean campaign_score_type_competitive(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean competitive = FALSE;

	if (changes != config_changes())
	{
		char const *type = config_string("network.coop_score_type");

		changes = config_changes();
		competitive = type != NULL && !strcmp(type, "competitive");
	}
	return competitive;
}

/* ---------- the values (campaign_score_values.json) */

/* a tag's name in lower case ("" for none) */
static void campaign_score_lower_name(
	long tag_index,
	char *lower,
	long size)
{
	char const *name = tag_index != NONE ? tag_get_name(tag_index) : NULL;
	long index = 0;

	for (; name && index < size - 1 && name[index]; index++)
		lower[index] = (char)((name[index] >= 'A' && name[index] <= 'Z') ? name[index] + 32 : name[index]);
	lower[index] = '\0';

	return;
}

static boolean campaign_score_name_has(
	char const *lower,
	char const *const *words)
{
	for (; *words; words++)
	{
		if (strstr(lower, *words))
			return TRUE;
	}
	return FALSE;
}

/* the points of the first entry with a word in the name (0 for none) */
static long campaign_score_match_points(
	struct campaign_score_match_value const *values,
	short count,
	char const *lower)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (campaign_score_name_has(lower, values[index].words))
			return values[index].points;
	}
	return 0;
}

/* an actor variant's points by its actor's type, and its name's words (0 for
a type not in the table) */
static long campaign_score_actor_points(
	struct campaign_score_actor_value const *values,
	short count,
	long variant_index)
{
	struct actor_variant_definition *variant;
	short actor_type;
	char lower[128];
	short index;

	if (variant_index == NONE)
		return 0;
	variant = actor_variant_definition_get(variant_index);
	if (variant->actor_reference.index == NONE)
		return 0;
	actor_type = actor_definition_get(variant->actor_reference.index)->type;
	campaign_score_lower_name(variant_index, lower, sizeof(lower));
	for (index = 0; index < count; index++)
	{
		long points;

		if (values[index].actor_type != actor_type)
			continue;
		points = campaign_score_match_points(values[index].variants, values[index].variant_count, lower);
		return points ? points : values[index].points;
	}
	return 0;
}

/* the skulls on, their multiplier: each one's, multiplied together (or the
highest), 1 with none on */
real campaign_score_skulls_multiplier(
	void)
{
	struct campaign_score_values const *values = &campaign_score_values;
	boolean highest = values->skull_combine == _campaign_score_combine_highest;
	boolean any = FALSE;
	real multiplier = 1.0f;
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		real value = values->skull_default;
		short index;

		if (!skull_active(skull))
			continue;
		for (index = 0; index < values->skull_count; index++)
		{
			if (!strcmp(values->skulls[index].skull, skull_descriptions[skull].key))
			{
				value = values->skulls[index].multiplier;
				break;
			}
		}
		if (highest)
			multiplier = any ? MAX(multiplier, value) : value;
		else
			multiplier *= value;
		any = TRUE;
	}
	return multiplier;
}

/* a kill's style's multiplier: those of its styles multiplied together (or
the highest), the regular kill's with none */
static real campaign_score_style_multiplier(
	byte style)
{
	struct campaign_score_values const *values = &campaign_score_values;
	real const multipliers[NUMBER_OF_CAMPAIGN_SCORE_STYLES] =
	{
		values->style_one_shot_one_kill,
		values->style_headshot,
		values->style_melee,
		values->style_backsmack,
	};
	boolean highest = values->style_combine == _campaign_score_combine_highest;
	boolean any = FALSE;
	real multiplier = 1.0f;
	short bit;

	for (bit = 0; bit < NUMBER_OF_CAMPAIGN_SCORE_STYLES; bit++)
	{
		if (!TEST_FLAG(style, bit))
			continue;
		if (highest)
			multiplier = any ? MAX(multiplier, multipliers[bit]) : multipliers[bit];
		else
			multiplier *= multipliers[bit];
		any = TRUE;
	}
	return any ? multiplier : values->style_regular;
}

/* the level's par time, in seconds, by its name (a10, of levels\a10\a10),
times the difficulty's (and a daily challenge's) par multiplier: NONE for a
level with none */
static long campaign_score_par_seconds(
	void)
{
	char lower[128];
	char const *last;
	short index;

	campaign_score_lower_name(global_scenario_index, lower, sizeof(lower));
	last = lower;
	for (index = 0; lower[index]; index++)
	{
		if (lower[index] == '\\' || lower[index] == '/')
			last = lower + index + 1;
	}
	for (index = 0; index < campaign_score_values.par_time_count; index++)
	{
		if (!strcmp(campaign_score_values.par_times[index].level, last))
		{
			real seconds = (real)campaign_score_values.par_times[index].seconds *
				campaign_score_values.par_difficulty[campaign_score_difficulty()];

			if (community_daily_run())
				seconds *= campaign_score_values.par_daily_challenge;
			return (long)(seconds + 0.5f);
		}
	}
	return NONE;
}

/* ---------- the log */

/* whether A or B is down on a controller (or the keyboard's jump key, A) */
static boolean campaign_score_button_down(
	void)
{
	short controller_index;

	for (controller_index = 0; controller_index < MAXIMUM_GAMEPADS; controller_index++)
	{
		struct gamepad_state const *gamepad = input_get_gamepad_state(controller_index);

		if (gamepad &&
			(gamepad->buttons[FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_a] > 0 ||
				gamepad->buttons[FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_b] > 0))
			return TRUE;
		if (input_abstraction_port_accept(controller_index) > 0)
			return TRUE;
	}
	return FALSE;
}

/* the units hurt forgotten (a new level, or a checkpoint reverted to) */
static void campaign_score_hurt_reset(
	void)
{
	long index;

	for (index = 0; index < CAMPAIGN_SCORE_HURT_SLOTS; index++)
		campaign_score_hurt[index] = NONE;

	return;
}

/* the unit's slot: its own, or the free one it would take (NONE: full) */
static long campaign_score_hurt_slot(
	long unit_index)
{
	long index = (long)((((unsigned long)unit_index * 2654435761UL) >> 16) % CAMPAIGN_SCORE_HURT_SLOTS);
	long tries;

	for (tries = 0; tries < CAMPAIGN_SCORE_HURT_SLOTS; tries++, index = (index + 1) % CAMPAIGN_SCORE_HURT_SLOTS)
	{
		if (campaign_score_hurt[index] == unit_index || campaign_score_hurt[index] == NONE)
			return index;
	}
	return NONE;
}

/* the level's time counted on: each tick the game time goes forward adds to
it, and going back to a checkpoint (the game time back) takes nothing from
it, as it takes nothing from the log. The units hurt are forgotten then:
those the checkpoint has are whole again */
static void campaign_score_update(
	void)
{
	long time = game_time_get();

	/* (the level's first look: its time so far, a saved game's too) */
	if (campaign_score_last_time == NONE)
		campaign_score_ticks = MAX(time, 0);
	else if (time > campaign_score_last_time)
		campaign_score_ticks += time - campaign_score_last_time;
	else if (time < campaign_score_last_time)
		campaign_score_hurt_reset();
	campaign_score_last_time = time;

	return;
}

static long campaign_score_total(
	short player_index)
{
	long total = 0;
	long index;

	for (index = 0; index < campaign_score_entry_count; index++)
	{
		if (campaign_score_entries[index].player_index == player_index)
			total += campaign_score_entries[index].points;
	}
	return total;
}

/* a player's name, in plain letters (PLAYER 1 for one that has left) */
static void campaign_score_player_name(
	short player_index,
	char *name,
	long size)
{
	struct data_iterator iterator;
	struct player_datum *player;
	wchar_t wide[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
	long index = 0;

	wide[0] = 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == player_index)
		{
			campaign_score_player_display_name(iterator.datum_index, wide, NUMBEROF(wide));
			break;
		}
	}
	for (; index < size - 1 && wide[index]; index++)
		name[index] = (char)(wide[index] >= 32 && wide[index] < 127 ? wide[index] : '?');
	name[index] = '\0';
	if (index == 0)
		snprintf(name, size, "PLAYER %d", (int)player_index + 1);

	return;
}

/* how many of a player's entries are of a kind (NONE: any player's) */
static long campaign_score_count_of(
	short player_index,
	short kind)
{
	long total = 0;
	long index;

	for (index = 0; index < campaign_score_entry_count; index++)
	{
		if ((player_index == NONE || campaign_score_entries[index].player_index == player_index) &&
			campaign_score_entries[index].kind == kind)
		{
			total++;
		}
	}
	return total;
}

/* how many of a medal a player (absolute index; NONE: anyone's) won in this
level or game, as the log has them (social.c's records) */
long campaign_score_medal_count(
	short player_index,
	short medal)
{
	long total = 0;
	long index;

	for (index = 0; index < campaign_score_entry_count; index++)
	{
		if ((player_index == NONE || campaign_score_entries[index].player_index == player_index) &&
			campaign_score_entries[index].kind == _campaign_score_entry_medal &&
			campaign_score_entries[index].variant_index == medal)
		{
			total++;
		}
	}
	return total;
}

/* how many kills had a style (NONE: any player's) */
static long campaign_score_style_count(
	short player_index,
	short style_bit)
{
	long total = 0;
	long index;

	for (index = 0; index < campaign_score_entry_count; index++)
	{
		if ((player_index == NONE || campaign_score_entries[index].player_index == player_index) &&
			campaign_score_entries[index].kind == _campaign_score_entry_kill &&
			TEST_FLAG(campaign_score_entries[index].style, style_bit))
		{
			total++;
		}
	}
	return total;
}

/* the points of all the players' entries together (a co-op team's score) */
static long campaign_score_team_total(
	void)
{
	long total = 0;
	long index;

	for (index = 0; index < campaign_score_entry_count; index++)
		total += campaign_score_entries[index].points;
	return total;
}

/* a score at the level's end: the time's multiplier on it (none on less
than nothing) */
static long campaign_score_final(
	long points)
{
	return points > 0 ? (long)(points * campaign_score_time_multiplier() + 0.5f) : points;
}

/* a variant's name as the summary has it: the last of its path, in capitals,
with spaces for underscores */
static void campaign_score_variant_name(
	long variant_index,
	char *name,
	long size)
{
	char const *path = tag_get_name(variant_index);
	char const *last;
	long index;

	if (path == NULL)
		path = "enemy";
	last = path;
	for (index = 0; path[index]; index++)
	{
		if (path[index] == '\\' || path[index] == '/')
			last = path + index + 1;
	}
	for (index = 0; index < size - 1 && last[index]; index++)
	{
		char c = last[index];

		if (c == '_')
			c = ' ';
		else if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		name[index] = c;
	}
	name[index] = '\0';

	return;
}

/* a time, H:MM:SS, as the Master Chief Collection's */
static void campaign_score_seconds_string(
	long seconds,
	char *text,
	long size)
{
	seconds = MAX(seconds, 0);
	snprintf(text, size, "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);

	return;
}

/* the level's time */
static void campaign_score_time_string(
	char *text,
	long size)
{
	campaign_score_seconds_string(campaign_score_level_ticks() / TICKS_PER_SECOND, text, size);

	return;
}

/* a multiplier as the panel and the summary have it: x3, x0.5, x1.25 */
static void campaign_score_multiplier_string(
	real multiplier,
	char *text,
	long size)
{
	char *end;

	/* (to two places, its zeros after the point dropped: x7.92, x0.5, x3) */
	snprintf(text, size, "x%.2f", (double)multiplier);
	end = text + strlen(text);
	while (end > text && strchr(text, '.') && (end[-1] == '0' || end[-1] == '.'))
	{
		boolean point = end[-1] == '.';

		*--end = '\0';
		if (point)
			break;
	}

	return;
}

/* the level's summary: its totals, its time against its par time, its
medals, its players' (a co-op game's) and its best enemy, and the score's */
/* the summary of the level (player_index: a player's own, by absolute
index: their kills, medals and deaths, and their score as the scoring type
keeps it; NONE: the whole team's) */
static void campaign_score_report_fill(
	struct campaign_score_report *report,
	short player_index_shown)
{
	struct campaign_score_variant best = { NONE, 0, 0 };
	struct campaign_score_variant variants[CAMPAIGN_SCORE_MAXIMUM_VARIANTS];
	long variant_count = 0;
	short player_index;
	long index;

	csmemset(report, 0, sizeof(*report));
	if (campaign_score_entry_count == 0)
		return;
	report->valid = TRUE;
	report->competitive = campaign_score_competitive;
	report->difficulty = campaign_score_difficulty();
	campaign_score_level_title(report->level, NUMBEROF(report->level));
	report->difficulty_multiplier = campaign_score_difficulty_multiplier();
	report->skulls_multiplier = campaign_score_skulls_multiplier();
	report->time_multiplier = campaign_score_time_multiplier();
	report->seconds = campaign_score_level_ticks() / TICKS_PER_SECOND;
	report->par_seconds = campaign_score_par_seconds();
	report->kills = campaign_score_count_of(player_index_shown, _campaign_score_entry_kill);
	report->headshots = campaign_score_style_count(player_index_shown, _campaign_score_style_headshot_bit);
	report->melee = campaign_score_style_count(player_index_shown, _campaign_score_style_melee_bit) +
		campaign_score_style_count(player_index_shown, _campaign_score_style_backsmack_bit);
	report->deaths = campaign_score_count_of(player_index_shown, _campaign_score_entry_death);
	report->betrayals = campaign_score_count_of(player_index_shown, _campaign_score_entry_betrayal);
	report->allies = campaign_score_count_of(player_index_shown, _campaign_score_entry_ally);
	report->points = player_index_shown != NONE && campaign_score_competitive ?
		campaign_score_total(player_index_shown) : campaign_score_team_total();
	report->score = campaign_score_final(report->points);
	for (index = 0; index < campaign_score_entry_count; index++)
	{
		struct campaign_score_entry const *entry = &campaign_score_entries[index];
		long found;

		if (player_index_shown != NONE && entry->player_index != player_index_shown)
			continue;

		if (entry->kind == _campaign_score_entry_medal)
		{
			report->medals++;
			if (entry->variant_index >= 0 && entry->variant_index < NUMBER_OF_CAMPAIGN_MEDALS)
				report->medal_counts[entry->variant_index]++;
			continue;
		}
		if (entry->kind != _campaign_score_entry_kill)
			continue;
		for (found = 0; found < variant_count; found++)
		{
			if (variants[found].variant_index == entry->variant_index)
				break;
		}
		if (found == variant_count && variant_count < CAMPAIGN_SCORE_MAXIMUM_VARIANTS)
		{
			variants[found].variant_index = entry->variant_index;
			variants[found].kills = 0;
			variants[found].points = 0;
			variant_count++;
		}
		if (found < variant_count)
		{
			variants[found].kills++;
			variants[found].points += entry->points;
			if (variants[found].points > best.points)
				best = variants[found];
		}
	}
	if (best.variant_index != NONE)
	{
		campaign_score_variant_name(best.variant_index, report->best_enemy, sizeof(report->best_enemy));
		report->best_enemy_kills = best.kills;
		report->best_enemy_points = best.points;
	}
	/* (a co-op game's players, each with points) */
	for (player_index = 0; player_index < CAMPAIGN_SCORE_MAXIMUM_PLAYERS &&
		report->player_count < CAMPAIGN_SCORE_REPORT_PLAYERS; player_index++)
	{
		if (campaign_score_total(player_index) != 0)
		{
			campaign_score_player_name(player_index, report->players[report->player_count].name,
				sizeof(report->players[report->player_count].name));
			report->players[report->player_count].score = campaign_score_final(campaign_score_total(player_index));
			report->players[report->player_count].kills = campaign_score_count_of(player_index,
				_campaign_score_entry_kill);
			report->player_count++;
		}
	}

	return;
}

/* the summary, to the log */
static void campaign_score_report_log(
	struct campaign_score_report const *report)
{
	char time[16];
	char par[16];
	short index;

	if (!report->valid)
		return;
	campaign_score_seconds_string(report->seconds, time, sizeof(time));
	campaign_score_seconds_string(report->par_seconds, par, sizeof(par));
	platform_log("campaign score: level complete, %s x%.2f, skulls x%.2f, time %s (par %s) x%.2f",
		campaign_score_difficulty_names[report->difficulty], (double)report->difficulty_multiplier,
		(double)report->skulls_multiplier, time, report->par_seconds != NONE ? par : "-", (double)report->time_multiplier);
	platform_log("campaign score: kills %ld, headshots %ld, melee %ld, medals %ld, deaths %ld, betrayals %ld, allies %ld",
		report->kills, report->headshots, report->melee, report->medals, report->deaths, report->betrayals,
		report->allies);
	platform_log("campaign score: points %ld, score %ld%s%s", report->points, report->score,
		report->record_score ? " (a new record)" : "", report->record_time ? " (a new best time)" : "");
	for (index = 0; index < report->player_count; index++)
		platform_log("campaign score: %s %ld, kills %ld", report->players[index].name, report->players[index].score,
			report->players[index].kills);
	if (report->best_enemy[0])
		platform_log("campaign score: best enemy %s, x%ld for %ld", report->best_enemy, report->best_enemy_kills,
			report->best_enemy_points);

	return;
}

/* ---------- public code */

boolean campaign_score_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = FALSE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("game.campaign_score") != 0;
	}
	return enabled && campaign_score_level();
}

boolean campaign_score_multiplayer_medals(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = TRUE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("game.multiplayer_medals") != 0;
	}
	return enabled && campaign_score_multiplayer();
}

boolean campaign_score_multiplayer_hud(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled = TRUE;

	if (changes != config_changes())
	{
		changes = config_changes();
		enabled = config_boolean("game.multiplayer_score_hud") != 0;
	}
	return enabled && campaign_score_multiplayer();
}

/* (Firefight's report's numbers, kept once for the postgame) */
static boolean campaign_score_firefight_report_filled = FALSE;

void campaign_score_new_map(
	void)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	campaign_score_firefight_report_filled = FALSE;
	campaign_score_entry_count = 0;
	campaign_score_postgame = FALSE;
	campaign_score_full_logged = FALSE;
	campaign_score_last_time = NONE;
	campaign_score_ticks = 0;
	campaign_score_drawn_height = 0;
	campaign_score_hurt_reset();
	campaign_score_blow.unit_index = NONE;
	campaign_medals_new_map();
	/* (a level just won, shown now; else nothing) */
	csmemcpy(campaign_score_summary, campaign_score_pending, sizeof(campaign_score_summary));
	campaign_score_summary_count = campaign_score_pending_count;
	campaign_score_summary_shown = 0;
	campaign_score_pending_count = 0;
	campaign_score_pending[0].valid = FALSE;
	campaign_score_button_primed = FALSE;

	return;
}

/* an entry is logged (the host's own, or one it sent): FALSE if the log is full */
static boolean campaign_score_append(
	short kind,
	long variant_index,
	short player_index,
	long points,
	byte style)
{
	struct campaign_score_entry *entry;
	char const *what;

	campaign_score_update();
	if (campaign_score_entry_count >= CAMPAIGN_SCORE_MAXIMUM_KILLS)
	{
		if (!campaign_score_full_logged)
		{
			campaign_score_full_logged = TRUE;
			console_warning("campaign score: the level's %d entries are logged; no more are scored",
				(int)CAMPAIGN_SCORE_MAXIMUM_KILLS);
		}
		return FALSE;
	}
	entry = &campaign_score_entries[campaign_score_entry_count++];
	entry->game_time = game_time_get();
	entry->variant_index = variant_index;
	entry->player_index = player_index;
	entry->points = points;
	entry->kind = (byte)kind;
	entry->style = style;
	/* (a medal shown to its player, if this machine's) */
	if (kind == _campaign_score_entry_medal)
		campaign_medals_awarded(player_index, (short)variant_index);
	if (campaign_score_enabled() || campaign_score_multiplayer_medals())
	{
		switch (kind)
		{
		case _campaign_score_entry_kill: what = "killed"; break;
		case _campaign_score_entry_ally: what = "killed an ally"; break;
		case _campaign_score_entry_betrayal: what = "betrayed"; break;
		case _campaign_score_entry_death: what = "died"; break;
		case _campaign_score_entry_revert: what = "reverted"; break;
		case _campaign_score_entry_objective: what = "completed an objective"; break;
		default: what = "won"; break;
		}
		platform_log("campaign score: player %d, %s %s, %ld (style %d)", (int)player_index, what,
			kind == _campaign_score_entry_medal ?
				(variant_index >= 0 && variant_index < NUMBER_OF_CAMPAIGN_MEDALS ? campaign_medal_descriptions[variant_index].label : "?") :
				variant_index != NONE ? tag_get_name(variant_index) : "",
			points, (int)style);
	}

	return TRUE;
}

/* an entry of this machine's (the host's, or a single player's), sent to a
network co-op game's clients */
static void campaign_score_note(
	short kind,
	long variant_index,
	long player_index,
	long points,
	byte style)
{
	short absolute_index;

	/* (the player's datum index, salted; the log keeps its absolute index,
	which every machine of a co-op game gives the same player) */
	if (!player_try_and_get(player_index))
		return;
	absolute_index = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (absolute_index >= CAMPAIGN_SCORE_MAXIMUM_PLAYERS)
		return;
	campaign_score_competitive = campaign_score_type_competitive();
	if (campaign_score_append(kind, variant_index, absolute_index, points, style) && network_coop_active())
		network_coop_note_score_kill(absolute_index, variant_index, points, kind, style, campaign_score_competitive);

	return;
}

void campaign_score_unit_damaged(
	long unit_index,
	long killing_player_index,
	short body_part,
	boolean melee,
	void const *direction,
	boolean lethal,
	boolean no_statistics,
	long damage_definition_index,
	long killer_object_index)
{
	struct campaign_score_values const *values = &campaign_score_values;
	struct unit_datum *unit;
	long slot;
	long variant_index = NONE;
	long base = 0;
	byte style = 0;

	/* (a client's are the host's, which sends them: the AI is the host's) */
	if (!campaign_score_level() || game_connection() == _game_connection_network_client)
		return;
	campaign_score_update();
	slot = campaign_score_hurt_slot(unit_index);
	if (!lethal)
	{
		if (slot != NONE)
			campaign_score_hurt[slot] = unit_index;
		return;
	}
	if (no_statistics)
		return;
	unit = unit_get(unit_index);

	/* a player's death, and another player's betrayal */
	if (unit->object.type == _object_type_biped && unit->unit.player_index != NONE)
	{
		if (values->penalty_death)
			campaign_score_note(_campaign_score_entry_death, NONE, unit->unit.player_index, values->penalty_death, 0);
		if (values->penalty_betrayal && killing_player_index != NONE && killing_player_index != unit->unit.player_index)
			campaign_score_note(_campaign_score_entry_betrayal, NONE, killing_player_index, values->penalty_betrayal, 0);
		/* (its sprees end; its killer is remembered: campaign_medals.c) */
		campaign_medals_player_died(unit->unit.player_index, NONE, killer_object_index);
		return;
	}

	/* the enemy's (or the ally's) points: a vehicle's by its name, else its
	actor variant's */
	if (unit->object.type == _object_type_vehicle)
	{
		char lower[128];

		variant_index = unit->definition_index;
		campaign_score_lower_name(variant_index, lower, sizeof(lower));
		base = campaign_score_match_points(values->vehicles, values->vehicle_count, lower);
		if (!base)
			base = campaign_score_match_points(values->ally_vehicles, values->ally_vehicle_count, lower);
	}
	else
	{
		long actor_index = unit->unit.swarm_actor_index != NONE ? unit->unit.swarm_actor_index : unit->unit.actor_index;

		if (actor_index == NONE)
			return;
		variant_index = actor_get(actor_index)->meta.variant_definition_index;
		base = campaign_score_actor_points(values->enemies, values->enemy_count, variant_index);
		if (!base)
			base = campaign_score_actor_points(values->allies, values->ally_count, variant_index);
	}
	/* (the allies' tables go by type: in Firefight marines and their
	vehicles can be the wave's, worth half what killing a friendly one
	costs, about a Jackal's to an Elite's) */
	if (base < 0 && firefight_active() && firefight_team_is_hostile(unit->object.owner_team_index))
		base = -base / 2;
	if (base < 0)
	{
		if (killing_player_index != NONE)
			campaign_score_note(_campaign_score_entry_ally, variant_index, killing_player_index, base, 0);
		return;
	}
	if (base == 0)
		return;
	/* (an enemy no player killed scores nothing; its killer, an AI's
	gunner, may win the vehicle's driver Wheelman) */
	if (killing_player_index == NONE)
	{
		campaign_medals_enemy_killed(NONE, killer_object_index, unit_index, damage_definition_index, FALSE, FALSE,
			FALSE);
		return;
	}

	/* the kill's style: hurt by nothing before, a hit to the head (none
	with Leadhead), a melee hit, from behind (the blow going the way the
	body faces) */
	if (slot != NONE && campaign_score_hurt[slot] != unit_index)
		SET_FLAG(style, _campaign_score_style_one_shot_bit, TRUE);
	if (body_part == CAMPAIGN_SCORE_HEAD_BODY_PART && !skull_active(_skull_leadhead))
		SET_FLAG(style, _campaign_score_style_headshot_bit, TRUE);
	if (melee)
	{
		real_vector3d blow = { { 0.0f, 0.0f, 0.0f } }; /* AE hook */
		boolean behind;

		if (direction)
			blow = *(real_vector3d const *)direction;
		behind = direction && normalize3d(&blow) > 0.0f &&
			dot_product3d(&blow, &unit->object.forward) > CAMPAIGN_SCORE_BACKSMACK_DOT;

		SET_FLAG(style, behind ? _campaign_score_style_backsmack_bit : _campaign_score_style_melee_bit, TRUE);
	}
	if (slot != NONE)
		campaign_score_hurt[slot] = unit_index;
	{
		long scored_points = (long)(base * campaign_score_style_multiplier(style) *
			values->difficulty[campaign_score_difficulty()] * campaign_score_skulls_multiplier() + 0.5f);

		campaign_score_note(_campaign_score_entry_kill, variant_index, killing_player_index, scored_points, style);
		if (!firefight_active())
		{
			char const *dmg_name = damage_definition_index != NONE ? tag_get_name(damage_definition_index) : NULL;
			boolean is_grenade = (dmg_name && strstr(dmg_name, "grenade") != NULL);
			boolean is_heavy = (base >= 50);
			firefight_stats_notify_enemy_kill(
				killing_player_index,
				_ff_challenge_cat_campaign,
				TEST_FLAG(style, _campaign_score_style_headshot_bit),
				TEST_FLAG(style, _campaign_score_style_melee_bit) || TEST_FLAG(style, _campaign_score_style_backsmack_bit),
				is_grenade,
				is_heavy,
				scored_points);
		}
	}
	/* (and the medals it wins: campaign_medals.c) */
	campaign_medals_enemy_killed(killing_player_index, killer_object_index, unit_index, damage_definition_index,
		TEST_FLAG(style, _campaign_score_style_headshot_bit), TEST_FLAG(style, _campaign_score_style_melee_bit),
		TEST_FLAG(style, _campaign_score_style_backsmack_bit));

	return;
}

void campaign_score_award_medal(
	long player_index,
	short medal)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	struct campaign_score_values const *values = &campaign_score_values;
	real points = 0.0f;
	short index;

	if (medal < 0 || medal >= NUMBER_OF_CAMPAIGN_MEDALS)
		return;
	/* (a multiplayer game's: this machine's players' alone, worth nothing) */
	if (campaign_score_multiplayer())
	{
		struct player_datum const *player = player_try_and_get(player_index);

		firefight_stats_record_medal(player_index, medal);
		if (player && player->local_player_index != NONE)
			campaign_score_note(_campaign_score_entry_medal, medal, player_index, 0, 0);
		return;
	}
	for (index = 0; index < values->medal_count; index++)
	{
		if (!strcmp(values->medals[index].medal, campaign_medal_descriptions[medal].key))
		{
			points = (real)values->medals[index].points;
			break;
		}
	}
	/* (as a kill's, times the difficulty and the skulls, if the values say) */
	if (values->medal_scaled)
		points *= values->difficulty[campaign_score_difficulty()] * campaign_score_skulls_multiplier();
	campaign_score_note(_campaign_score_entry_medal, medal, player_index, (long)(points + 0.5f), 0);
	firefight_stats_record_medal(player_index, medal);

	return;
}

/* (firefight_objectives.c) points a player earned with an objective, as
they are */
void campaign_score_award_points(
	long player_index,
	long points)
{
	if (points <= 0)
		return;
	campaign_score_note(_campaign_score_entry_objective, NONE, player_index, points, 0);

	return;
}

void campaign_score_killing_blow(
	long unit_index,
	short body_part,
	boolean melee,
	void const *direction,
	long damage_definition_index)
{
	struct unit_datum *unit;

	if (!campaign_score_multiplayer_medals())
		return;
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);
	if (!unit)
		return;
	campaign_score_blow.unit_index = unit_index;
	campaign_score_blow.game_time = game_time_get();
	campaign_score_blow.damage_definition_index = damage_definition_index;
	campaign_score_blow.headshot = body_part == CAMPAIGN_SCORE_HEAD_BODY_PART;
	campaign_score_blow.melee = FALSE;
	campaign_score_blow.backsmack = FALSE;
	/* (a melee hit from behind: the blow going the way the body faces, as
	campaign_score_unit_damaged's) */
	if (melee)
	{
		real_vector3d blow = { { 0.0f, 0.0f, 0.0f } }; /* AE hook */

		if (direction)
			blow = *(real_vector3d const *)direction;
		if (direction && normalize3d(&blow) > 0.0f &&
			dot_product3d(&blow, &unit->object.forward) > CAMPAIGN_SCORE_BACKSMACK_DOT)
		{
			campaign_score_blow.backsmack = TRUE;
		}
		else
		{
			campaign_score_blow.melee = TRUE;
		}
	}

	return;
}

void campaign_score_player_killed(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire)
{
	struct player_datum *dead_player = player_try_and_get(dead_player_index);
	long damage_definition_index = NONE;
	boolean headshot = FALSE;
	boolean melee = FALSE;
	boolean backsmack = FALSE;

	if (!campaign_score_multiplayer_medals() || !dead_player)
		return;
	campaign_score_update();
	/* (its killing blow, if this tick's was to the player's unit: else it
	died with none, as a client's body the host's blow never came for) */
	if (campaign_score_blow.unit_index != NONE && campaign_score_blow.unit_index == dead_player->unit_index &&
		campaign_score_blow.game_time == game_time_get())
	{
		damage_definition_index = campaign_score_blow.damage_definition_index;
		headshot = campaign_score_blow.headshot;
		melee = campaign_score_blow.melee;
		backsmack = campaign_score_blow.backsmack;
	}
	campaign_score_blow.unit_index = NONE;
	/* (an enemy's kill: not a suicide, a betrayal, nor the map's) */
	if (killing_player_index != NONE && killing_player_index != dead_player_index && !friendly_fire)
	{
		char const *dmg_name = damage_definition_index != NONE ? tag_get_name(damage_definition_index) : NULL;
		boolean is_grenade = (dmg_name && strstr(dmg_name, "grenade") != NULL);
		campaign_medals_player_killed_player(killing_player_index, killing_object_index, dead_player_index,
			damage_definition_index, headshot, melee, backsmack);
		if (!firefight_active())
		{
			firefight_stats_notify_enemy_kill(
				killing_player_index,
				_ff_challenge_cat_multiplayer,
				headshot,
				melee || backsmack,
				is_grenade,
				FALSE,
				100);
		}
	}
	/* (and the dead player's sprees end; their killer is remembered) */
	campaign_medals_player_died(dead_player_index,
		killing_player_index != NONE && !friendly_fire ? killing_player_index : NONE, NONE);

	return;
}

void campaign_score_checkpoint_reverted(
	void)
{
	short local_player_index;
	long player_index;

	if (!campaign_score_level() || game_connection() == _game_connection_network_client ||
		!campaign_score_values.penalty_checkpoint_revert)
	{
		return;
	}
	/* (each of this machine's players: the game goes back for all of them) */
	for (local_player_index = local_player_get_next(NONE); local_player_index != NONE;
		local_player_index = local_player_get_next(local_player_index))
	{
		player_index = local_player_get_player_index(local_player_index);
		if (player_index != NONE)
		{
			campaign_score_note(_campaign_score_entry_revert, NONE, player_index,
				campaign_score_values.penalty_checkpoint_revert, 0);
		}
	}

	return;
}

/* (network_coop.c) an entry the host logged, on a client */
void campaign_score_network_kill(
	short player_index,
	long variant_index,
	long points,
	short kind,
	byte style,
	boolean competitive)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	if (!campaign_score_level() || player_index < 0 || player_index >= CAMPAIGN_SCORE_MAXIMUM_PLAYERS ||
		kind < 0 || kind >= NUMBER_OF_CAMPAIGN_SCORE_ENTRIES)
	{
		return;
	}
	campaign_score_competitive = competitive;
	campaign_score_append(kind, variant_index, player_index, points, style);

	return;
}

/* ---------- the records: each profile's, its campaign_records.txt beside its
blam.sav ("level:score:seconds;..."), each level's best score and best time,
written as a level is won */

/* a level's key in the records: its name's last part (levels\\a10\\a10: a10),
lower case, of letters, digits and underscores */
static void campaign_score_record_key(
	char const *level_name,
	char *key,
	long size)
{
	char const *last = level_name ? level_name : "";
	long index;
	long length = 0;

	for (index = 0; level_name && level_name[index]; index++)
	{
		if (level_name[index] == '\\' || level_name[index] == '/')
			last = level_name + index + 1;
	}
	for (index = 0; last[index] && length < size - 1; index++)
	{
		char c = last[index];

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			key[length++] = c;
	}
	key[length] = '\0';

	return;
}

/* the profile's records file's path: FALSE for no profile */
static boolean campaign_score_records_path(
	long profile_index,
	char *path,
	long size)
{
	char directory[256];

	if (profile_index == NONE || !player_profile_get_enclosing_directory_path(profile_index, directory) ||
		!directory[0])
	{
		return FALSE;
	}
	snprintf(path, size, "%s%s", directory, CAMPAIGN_SCORE_RECORDS_FILE);
	return TRUE;
}

/* the profile's records, as text ("" for none) */
static void campaign_score_records_read(
	long profile_index,
	char *text,
	long size)
{
	char path[300];
	HANDLE file;
	unsigned long read = 0;

	text[0] = '\0';
	if (!campaign_score_records_path(profile_index, path, sizeof(path)))
		return;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	if (!ReadFile(file, text, (unsigned long)(size - 1), &read, NULL))
		read = 0;
	CloseHandle(file);
	text[read] = '\0';

	return;
}

static boolean campaign_score_records_write(
	long profile_index,
	char const *text)
{
	char path[300];
	HANDLE file;
	unsigned long written = 0;
	boolean success;

	if (!campaign_score_records_path(profile_index, path, sizeof(path)))
		return FALSE;
	file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	success = WriteFile(file, text, (unsigned long)strlen(text), &written, NULL) && written == strlen(text);
	CloseHandle(file);
	return success;
}

/* the record of the key in the records' text ("level:score:seconds", and
since the leaderboard ":difficulty:skulls:skulls percent:medals": each the
best of its own, the difficulty the best score's; NONE for those an older
record has not): FALSE for none */
static boolean campaign_score_record_find(
	char const *records,
	char const *key,
	struct campaign_score_record *record)
{
	size_t key_length = strlen(key);
	char const *entry = records;

	while (entry && *entry && key_length)
	{
		char const *end = strchr(entry, ';');

		if (!strncmp(entry, key, key_length) && entry[key_length] == ':')
		{
			record->difficulty = record->skulls = record->skulls_percent = record->medals = NONE;
			if (sscanf(entry + key_length + 1, "%ld:%ld:%ld:%ld:%ld:%ld", &record->score, &record->seconds,
				&record->difficulty, &record->skulls, &record->skulls_percent, &record->medals) >= 2)
			{
				return TRUE;
			}
		}
		entry = end ? end + 1 : NULL;
	}
	return FALSE;
}

/* the level won (the run's record): kept in the profile's records where
better than its record's, each record on its own; which were beaten
(_campaign_score_record_*_bit) */
static long campaign_score_record_level(
	long profile_index,
	struct campaign_score_record const *run)
{
	char records[CAMPAIGN_SCORE_RECORDS_LENGTH];
	char key[32];
	char text[CAMPAIGN_SCORE_RECORDS_LENGTH];
	struct campaign_score_record best;
	long length = 0;
	long beaten = 0;
	char const *entry;

	campaign_score_record_key(global_scenario_index != NONE ? tag_get_name(global_scenario_index) : NULL, key,
		sizeof(key));
	if (!key[0] || profile_index == NONE)
		return 0;
	campaign_score_records_read(profile_index, records, sizeof(records));
	if (campaign_score_record_find(records, key, &best))
	{
		/* (each its own best, whichever run's: the most points, the
		shortest time, the most medals, the most skulls and the highest
		skulls' multiplier; the difficulty the best score's) */
		if (run->score > best.score)
			SET_FLAG(beaten, _campaign_score_record_score_bit, TRUE);
		if (run->seconds < best.seconds)
			SET_FLAG(beaten, _campaign_score_record_time_bit, TRUE);
		if (run->medals > best.medals)
			SET_FLAG(beaten, _campaign_score_record_medals_bit, TRUE);
		if (run->skulls > best.skulls || run->skulls_percent > best.skulls_percent)
			SET_FLAG(beaten, _campaign_score_record_skulls_bit, TRUE);
		if (!beaten)
			return 0;
		if (TEST_FLAG(beaten, _campaign_score_record_score_bit))
		{
			best.score = run->score;
			best.difficulty = run->difficulty;
		}
		best.seconds = MIN(best.seconds, run->seconds);
		best.medals = MAX(best.medals, run->medals);
		best.skulls = MAX(best.skulls, run->skulls);
		best.skulls_percent = MAX(best.skulls_percent, run->skulls_percent);
	}
	else
	{
		/* (the level's first: all its records) */
		beaten = FLAG(_campaign_score_record_score_bit) | FLAG(_campaign_score_record_time_bit) |
			FLAG(_campaign_score_record_medals_bit) | FLAG(_campaign_score_record_skulls_bit);
		best = *run;
	}
	/* (the others as they were, then this level's) */
	text[0] = '\0';
	for (entry = records; entry && *entry;)
	{
		char const *end = strchr(entry, ';');
		long entry_length = end ? (long)(end - entry) : (long)strlen(entry);

		if (!(strncmp(entry, key, strlen(key)) == 0 && entry[strlen(key)] == ':') && entry_length > 0 &&
			length + entry_length + 2 < (long)sizeof(text))
		{
			memcpy(text + length, entry, (size_t)entry_length);
			length += entry_length;
			text[length++] = ';';
			text[length] = '\0';
		}
		entry = end ? end + 1 : NULL;
	}
	snprintf(text + length, sizeof(text) - (size_t)length, "%s:%ld:%ld:%ld:%ld:%ld:%ld", key, best.score,
		best.seconds, best.difficulty, best.skulls, best.skulls_percent, best.medals);
	if (!campaign_score_records_write(profile_index, text))
		platform_log("campaign score: could not write the profile's %s", CAMPAIGN_SCORE_RECORDS_FILE);
	else
		platform_log("campaign score: %s's record now %ld points, %ld seconds", key, best.score, best.seconds);

	return beaten;
}

boolean campaign_score_level_record(
	char const *level_name,
	long profile_index,
	struct campaign_score_record *record)
{
	char records[CAMPAIGN_SCORE_RECORDS_LENGTH];
	char key[32];

	campaign_score_record_key(level_name, key, sizeof(key));
	if (!key[0] || profile_index == NONE)
		return FALSE;
	campaign_score_records_read(profile_index, records, sizeof(records));
	return campaign_score_record_find(records, key, record);
}

static void campaign_score_win_compose(
	void)
{
	short local_player_index;
	short local_players = 0;
	/* (a daily challenge's run: every player's sent, split screen and
	network co-op too; a level's, only one player's of a game of this
	machine's alone: community.c) */
	boolean alone;

	if (!campaign_score_enabled())
	{
		if (global_scenario_index != NONE && !firefight_active())
		{
			long player_idx = local_player_get_player_index(local_player_get_next(NONE));
			long m_kills = player_idx != NONE ? campaign_score_player_kills(player_idx) : 0;
			long m_deaths = player_idx != NONE ? campaign_score_player_deaths(player_idx) : 0;
			long m_score = campaign_score_final(campaign_score_team_total());
			long m_seconds = campaign_score_level_ticks() / TICKS_PER_SECOND;
			short m_diff = campaign_score_difficulty();
			short m_skulls = 0;
			long m_medals = campaign_score_count_of(NONE, _campaign_score_entry_medal);
			int sk;

			for (sk = 0; sk < NUMBER_OF_SKULLS; sk++)
				m_skulls += skull_active(sk) ? 1 : 0;

			firefight_stats_record_campaign_mission_end(
				tag_get_name(global_scenario_index),
				m_score,
				m_kills,
				m_deaths,
				m_seconds,
				m_diff,
				m_skulls,
				m_medals);
		}
		return;
	}
	campaign_score_update();
	community_result_clear();
	campaign_score_pending_count = 0;
	for (local_player_index = local_player_get_next(NONE); local_player_index != NONE;
		local_player_index = local_player_get_next(local_player_index))
	{
		local_players++;
	}
	alone = local_players == 1 && game_connection() == _game_connection_local;
	/* (each of this machine's players: their records of the level, in their
	profiles, a co-op team's score or in a competitive game their own; a
	daily challenge's run keeps none, its settings not the player's own;
	their online result; and their card) */
	for (local_player_index = local_player_get_next(NONE); local_player_index != NONE &&
		campaign_score_pending_count < CAMPAIGN_SCORE_LOCAL_CARDS;
		local_player_index = local_player_get_next(local_player_index))
	{
		long player_index = local_player_get_player_index(local_player_index);
		short absolute_index = player_index != NONE ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : NONE;
		long profile_index = player_ui_get_active_player_profile_index(local_player_index);
		struct campaign_score_report *report = &campaign_score_pending[campaign_score_pending_count];
		long points = campaign_score_team_total();
		struct campaign_score_record run;
		struct player_profile profile;
		long records;
		int skull;

		if (campaign_score_competitive && player_index != NONE)
			points = campaign_score_total(absolute_index);
		run.score = campaign_score_final(points);
		run.seconds = campaign_score_level_ticks() / TICKS_PER_SECOND;
		run.difficulty = campaign_score_difficulty();
		run.skulls = 0;
		for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
			run.skulls += skull_active(skull) ? 1 : 0;
		run.skulls_percent = (long)(campaign_score_skulls_multiplier() * 100.0f + 0.5f);
		/* (the player's own medals; the team's, as its score, in a
		cooperative game) */
		run.medals = campaign_score_count_of(campaign_score_competitive ? absolute_index : NONE,
			_campaign_score_entry_medal);
		records = community_daily_run() || firefight_stats_session_withheld() ? 0 :
			campaign_score_record_level(profile_index, &run);
		if (campaign_score_pending_count == 0 && global_scenario_index != NONE && !firefight_active())
		{
			long mission_kills = player_index != NONE ? campaign_score_player_kills(player_index) : 0;
			long mission_deaths = player_index != NONE ? campaign_score_player_deaths(player_index) : 0;
			firefight_stats_record_campaign_mission_end(
				tag_get_name(global_scenario_index),
				run.score,
				mission_kills,
				mission_deaths,
				run.seconds,
				run.difficulty,
				run.skulls,
				run.medals);
		}
		/* (the online leaderboard's, the level's or the daily challenge's) */
		if (profile_index != NONE && global_scenario_index != NONE && !firefight_stats_session_withheld() &&
			player_profile_get(profile_index, &profile))
		{
			struct community_run online;

			online.score = run.score;
			online.seconds = run.seconds;
			online.difficulty = run.difficulty;
			online.skulls = run.skulls;
			online.skulls_percent = run.skulls_percent;
			online.medals = run.medals;
			online.kills = player_index != NONE ? campaign_score_player_kills(player_index) : 0;
			online.deaths = player_index != NONE ? campaign_score_player_deaths(player_index) : 0;
			online.players = network_game_get_game() && game_connection() != _game_connection_local ?
				MAX(network_game_get_game()->player_count, 1) : local_players;
			community_level_won(tag_get_name(global_scenario_index), &online, profile.player_name, profile_index,
				alone, campaign_score_pending_count);
			/* (and the account's records: social.c) */
			social_level_won(absolute_index, &online, tag_get_name(global_scenario_index), profile.player_name);
		}
		campaign_score_report_fill(report, absolute_index);
		report->record_score = TEST_FLAG(records, _campaign_score_record_score_bit);
		report->record_time = TEST_FLAG(records, _campaign_score_record_time_bit);
		if (player_index != NONE)
			campaign_score_player_display_name(player_index, report->player, NUMBEROF(report->player));
		report->player_number = (short)(campaign_score_pending_count + 1);
		report->player_total = local_players;
		report->result_slot = campaign_score_pending_count;
		campaign_score_report_log(report);
		campaign_score_pending_count++;
	}
	/* (none of this machine's: the team's) */
	if (!campaign_score_pending_count)
	{
		campaign_score_report_fill(&campaign_score_pending[0], NONE);
		campaign_score_pending[0].result_slot = NONE;
		campaign_score_report_log(&campaign_score_pending[0]);
		campaign_score_pending_count = 1;
	}

	return;
}

/* the pending summary is shown over the level (in the level itself, which is
still running): FALSE if there is none to show */
static boolean campaign_score_postgame_begin(
	void)
{
	if (!campaign_score_pending_count || !campaign_score_pending[0].valid || !campaign_score_level() ||
		cinematic_in_progress())
	{
		return FALSE;
	}
	csmemcpy(campaign_score_summary, campaign_score_pending, sizeof(campaign_score_summary));
	campaign_score_summary_count = campaign_score_pending_count;
	campaign_score_summary_shown = 0;
	campaign_score_pending_count = 0;
	campaign_score_pending[0].valid = FALSE;
	campaign_score_button_primed = FALSE;

	return TRUE;
}

boolean campaign_score_postgame_wait(
	void)
{
	if (!campaign_score_postgame)
	{
		if (!campaign_score_postgame_begin())
			return FALSE;
		campaign_score_postgame = TRUE;
		return TRUE;
	}
	/* (until A or B is pressed on the last card: campaign_score_draw) */
	if (campaign_score_summary_shown < campaign_score_summary_count &&
		campaign_score_summary[campaign_score_summary_shown].valid && campaign_score_level())
	{
		return TRUE;
	}
	campaign_score_postgame = FALSE;
	campaign_score_summary_count = 0;

	return FALSE;
}

void campaign_score_level_won(
	void)
{
	if (!campaign_score_level())
		return;
	if (game_connection() != _game_connection_network_client)
		campaign_score_competitive = campaign_score_type_competitive();
	/* (the level's medals, before the clients are told it is won) */
	if (game_connection() != _game_connection_network_client)
		campaign_medals_level_won();
	/* (the clients write their own summaries from the entries they were sent) */
	if (game_connection() == _game_connection_network_server && network_coop_active())
		network_coop_note_score_won(campaign_score_competitive);
	campaign_score_win_compose();

	return;
}

/* (network_coop.c) the host won the level, on a client */
void campaign_score_network_won(
	boolean competitive)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	if (!campaign_score_level())
		return;
	campaign_score_competitive = competitive;
	campaign_score_win_compose();
	/* (shown at once: the host holds the level for its own, then the lobby
	takes everyone) */
	campaign_score_postgame_begin();

	return;
}

boolean campaign_score_is_competitive(
	void)
{
	if (game_connection() != _game_connection_network_client)
		campaign_score_competitive = campaign_score_type_competitive();
	return campaign_score_competitive;
}

long campaign_score_player_points(
	long player_index)
{
	campaign_score_update();
	return campaign_score_total((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
}

long campaign_score_player_kills(
	long player_index)
{
	campaign_score_update();
	return campaign_score_count_of((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), _campaign_score_entry_kill);
}

long campaign_score_player_deaths(
	long player_index)
{
	campaign_score_update();
	return campaign_score_count_of((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), _campaign_score_entry_death);
}

long campaign_score_team_points(
	void)
{
	campaign_score_update();
	return campaign_score_team_total();
}

long campaign_score_level_ticks(
	void)
{
	if (campaign_score_level() || campaign_score_multiplayer())
		campaign_score_update();
	return campaign_score_ticks;
}

void campaign_score_tick(
	void)
{
	if (!ae_pve_records_allowed()) return; /* AE hook */
	/* (each tick: a checkpoint reverted to between two looks would take the
	ticks played since it from the time) */
	if (campaign_score_level())
		campaign_score_update();

	return;
}

real campaign_score_time_multiplier(
	void)
{
	struct campaign_score_values const *values = &campaign_score_values;
	long par = campaign_score_par_seconds();
	real ratio;
	short index;

	if (par == NONE || par <= 0)
		return 1.0f;
	ratio = (real)campaign_score_level_ticks() / (real)(par * TICKS_PER_SECOND);
	for (index = 0; index < values->time_bracket_count; index++)
	{
		if (values->time_brackets[index].up_to_par < 0.0f || ratio <= values->time_brackets[index].up_to_par)
			return values->time_brackets[index].multiplier;
	}
	return 1.0f;
}

real campaign_score_difficulty_multiplier_for(
	short difficulty)
{
	return campaign_score_values.difficulty[PIN(difficulty, 0, NUMBER_OF_GAME_DIFFICULTY_LEVELS - 1)];
}

real campaign_score_difficulty_multiplier(
	void)
{
	return campaign_score_difficulty_multiplier_for(campaign_score_difficulty());
}

real campaign_score_total_multiplier(
	void)
{
	return campaign_score_difficulty_multiplier() * campaign_score_skulls_multiplier() *
		campaign_score_time_multiplier();
}

void campaign_score_multiplier_text(
	real multiplier,
	wchar_t *text,
	long count)
{
	char narrow[16];
	long index;

	campaign_score_multiplier_string(multiplier, narrow, sizeof(narrow));
	for (index = 0; index < count - 1 && narrow[index]; index++)
		text[index] = (wchar_t)narrow[index];
	text[index] = 0;

	return;
}

real campaign_score_par_fraction(
	void)
{
	long par = campaign_score_par_seconds();

	if (par == NONE || par <= 0)
		return -1.0f;
	return (real)campaign_score_level_ticks() / (real)(par * TICKS_PER_SECOND);
}

void campaign_score_time_text(
	wchar_t *text,
	long count)
{
	char time[16];
	long index;

	campaign_score_time_string(time, sizeof(time));
	for (index = 0; index < count - 1 && time[index]; index++)
		text[index] = (wchar_t)time[index];
	text[index] = 0;

	return;
}

void campaign_score_player_display_name(
	long player_index,
	wchar_t *name,
	long count)
{
	struct player_datum const *player = player_try_and_get(player_index);
	wchar_t const *source = player ? player->name : L"";
	long length = player ? (long)NUMBEROF(player->name) : 0;
	struct player_profile profile;
	long index;

	/* (a player of this machine's campaign has no name of their own: a
	network game's player is given it; theirs is their profile's) */
	if (player && !player->name[0] && player->local_player_index != NONE &&
		player->local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
		player_ui_get_active_player_profile_index(player->local_player_index) != NONE)
	{
		player_ui_get_active_player_profile(player->local_player_index, &profile);
		source = profile.player_name;
		length = NUMBEROF(profile.player_name);
	}
	for (index = 0; index < count - 1 && index < length && source[index]; index++)
		name[index] = source[index];
	name[index] = 0;
	if (index == 0 && player)
	{
		char text[16];

		snprintf(text, sizeof(text), "Player %d", (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1);
		for (index = 0; index < count - 1 && text[index]; index++)
			name[index] = (wchar_t)text[index];
		name[index] = 0;
	}

	return;
}

void campaign_score_level_title(
	wchar_t *text,
	long count)
{
	char const *level_name = global_scenario_index != NONE ? tag_get_name(global_scenario_index) : NULL;
	wchar_t const *title = custom_edition_maps_level_title(level_name);
	char const *last = level_name ? level_name : "";
	long index;

	if (title)
	{
		for (index = 0; index < count - 1 && title[index]; index++)
			text[index] = title[index];
		text[index] = 0;
		return;
	}
	/* (another level: the last of its name's path) */
	for (index = 0; level_name && level_name[index]; index++)
	{
		if (level_name[index] == '\\' || level_name[index] == '/')
			last = level_name + index + 1;
	}
	for (index = 0; index < count - 1 && last[index]; index++)
		text[index] = (wchar_t)last[index];
	text[index] = 0;

	return;
}

short campaign_score_hud_height(
	void)
{
	return campaign_score_drawn_height;
}

/* the player's last medals (as many as the JSON's medal history, and the
count), oldest first; none with the medals off */
static short campaign_score_medal_history(
	long player_index,
	short *medals,
	short count)
{
	short absolute_index = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	short wanted = (short)MIN(count, campaign_score_values.medal_history);
	short found = 0;
	long index;

	if (!campaign_medals_enabled() || wanted <= 0)
		return 0;
	for (index = campaign_score_entry_count - 1; index >= 0 && found < wanted; index--)
	{
		if (campaign_score_entries[index].kind == _campaign_score_entry_medal &&
			campaign_score_entries[index].player_index == absolute_index)
		{
			medals[found++] = (short)campaign_score_entries[index].variant_index;
		}
	}
	/* (the newest last) */
	for (index = 0; index < found / 2; index++)
	{
		short medal = medals[index];

		medals[index] = medals[found - 1 - index];
		medals[found - 1 - index] = medal;
	}
	return found;
}

/* ---------- the HUD's panel

the panel is laid out in the HUD's 480 lines from the view's bottom right
corner, and drawn scaled with the view (its height over 480), as the skulls'
icons are */

void campaign_score_panel_quad(
	struct campaign_score_panel const *panel,
	real x0,
	real y0,
	real x1,
	real y1,
	pixel32 color)
{
	rectangle2d rectangle;

	rectangle.x0 = (short)(panel->left + x0 * panel->scale);
	rectangle.y0 = (short)(panel->top + y0 * panel->scale);
	rectangle.x1 = (short)MAX(panel->left + x1 * panel->scale, rectangle.x0 + 1);
	rectangle.y1 = (short)MAX(panel->top + y1 * panel->scale, rectangle.y0 + 1);
	draw_quad(&rectangle, color);

	return;
}

void campaign_score_panel_frame(
	struct campaign_score_panel const *panel,
	real x0,
	real y0,
	real x1,
	real y1,
	pixel32 color)
{
	campaign_score_panel_quad(panel, x0, y0, x1, y0 + 1.0f, color);
	campaign_score_panel_quad(panel, x0, y1 - 1.0f, x1, y1, color);
	campaign_score_panel_quad(panel, x0, y0 + 1.0f, x0 + 1.0f, y1 - 1.0f, color);
	campaign_score_panel_quad(panel, x1 - 1.0f, y0 + 1.0f, x1, y1 - 1.0f, color);

	return;
}

void campaign_score_panel_text(
	struct campaign_score_panel const *panel,
	char const *text,
	wchar_t const *unicode_text,
	real x0,
	real x1,
	real y,
	real size,
	short justification,
	real_argb_color const *color)
{
	real scale = panel->scale * size;
	real origin_x = panel->left + x0 * panel->scale;
	real origin_y = panel->top + y * panel->scale;
	wchar_t wide[64];
	rectangle2d bounds;
	rectangle2d ink;
	rectangle2d cursor;
	short shift;
	long index;

	if (!unicode_text)
	{
		for (index = 0; index < (long)NUMBEROF(wide) - 1 && text && text[index]; index++)
			wide[index] = (wchar_t)(unsigned char)text[index];
		wide[index] = 0;
		unicode_text = wide;
	}
	bounds.x0 = (short)origin_x;
	bounds.y0 = (short)origin_y;
	bounds.x1 = (short)(origin_x + (x1 - x0) / size);
	bounds.y1 = (short)(origin_y + panel->line_height);
	draw_string_set_draw_mode(panel->font_index, NONE, justification, 0, color);
	/* (the ink, not the characters' cells, at x0 or x1: each character's
	side bearing scales with the text, so that lines of two sizes would
	otherwise start, or end, apart. Moved in the text's own units, about
	the scale's origin) */
	draw_unicode_string_compute_bounds(&bounds, unicode_text, &ink, &cursor);
	shift = 0;
	if (ink.x0 <= ink.x1)
	{
		if (justification == _campaign_score_text_left)
			shift = (short)(ink.x0 - bounds.x0);
		else if (justification == _campaign_score_text_right)
			shift = (short)(ink.x1 - bounds.x1);
	}
	rasterizer_text_set_scale(scale, origin_x, origin_y);
	bounds.x0 = (short)(bounds.x0 - shift);
	bounds.x1 = (short)(bounds.x1 - shift);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, unicode_text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);

	return;
}

/* how wide a line of text is in the panel's units, its ink's, at `size`
times the HUD's text (0 for none) */
static real campaign_score_panel_text_width(
	struct campaign_score_panel const *panel,
	wchar_t const *unicode_text,
	real size)
{
	rectangle2d bounds;
	rectangle2d ink;
	rectangle2d cursor;
	real_argb_color color;

	color.alpha = color.red = color.green = color.blue = 1.0f;
	bounds.x0 = 0;
	bounds.y0 = 0;
	bounds.x1 = 0x3fff;
	bounds.y1 = panel->line_height;
	draw_string_set_draw_mode(panel->font_index, NONE, _campaign_score_text_left, 0, &color);
	draw_unicode_string_compute_bounds(&bounds, unicode_text, &ink, &cursor);
	return ink.x0 <= ink.x1 ? (ink.x1 - ink.x0) * size : 0.0f;
}

/* a row's text: the name and its value on top, the subline and its
value beneath; the name's top at y (the row's padding below the row's) */
static void campaign_score_panel_row_text(
	struct campaign_score_panel const *panel,
	real y,
	char const *name,
	wchar_t const *unicode_name,
	char const *value,
	char const *subline,
	char const *subline_value)
{
	real_argb_color text_color;
	real_argb_color subline_color;
	real text_left = CAMPAIGN_SCORE_ROW_PADDING * 2;
	real text_right = panel->width - CAMPAIGN_SCORE_ROW_PADDING * 2;
	real subline_top = y + CAMPAIGN_SCORE_ROW_PADDING + panel->line_height * 0.92f;

	text_color.alpha = 1.0f;
	text_color.red = text_color.green = text_color.blue = 0.95f;
	subline_color.alpha = 1.0f;
	subline_color.red = campaign_score_palette.subline.red;
	subline_color.green = campaign_score_palette.subline.green;
	subline_color.blue = campaign_score_palette.subline.blue;
	campaign_score_panel_text(panel, name, unicode_name, text_left, text_right, y + CAMPAIGN_SCORE_ROW_PADDING, 1.0f,
		_campaign_score_text_left, &text_color);
	campaign_score_panel_text(panel, value, NULL, text_left, text_right, y + CAMPAIGN_SCORE_ROW_PADDING, 1.0f,
		_campaign_score_text_right, &text_color);
	if (subline)
	{
		campaign_score_panel_text(panel, subline, NULL, text_left, text_right, subline_top,
			CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &subline_color);
	}
	if (subline_value)
	{
		campaign_score_panel_text(panel, subline_value, NULL, text_left, text_right, subline_top,
			CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &subline_color);
	}

	return;
}

/* a row: its panel and edge, and its text (campaign_score_panel_row_text);
its top at y, `row_height` high */
static void campaign_score_panel_row(
	struct campaign_score_panel const *panel,
	real y,
	real row_height,
	char const *name,
	wchar_t const *unicode_name,
	char const *value,
	char const *subline,
	char const *subline_value)
{
	campaign_score_panel_quad(panel, 0.0f, y, panel->width, y + row_height, CAMPAIGN_SCORE_PANEL_FILL);
	campaign_score_panel_frame(panel, 0.0f, y, panel->width, y + row_height, CAMPAIGN_SCORE_HUD_EDGE);
	campaign_score_panel_row_text(panel, y, name, unicode_name, value, subline, subline_value);

	return;
}

/* the score and the level's time, as the Master Chief Collection's panel at
the bottom right of one player's view (hud.c's hud_draw_screen, before the
skulls' icons, which it lifts above itself): a strip of segments, then the
player's own row (the team's in a cooperative co-op game); the other
players' scores are the scoreboard's (hold BACK, or tab) */
/* the panel's font and scale, in the view being drawn: FALSE for none */
static boolean campaign_score_panel_begin(
	struct campaign_score_panel *panel,
	short *width,
	short *view_height)
{
	struct font_header *font;

	panel->font_index = hud_get_font_index();
	if (panel->font_index == NONE)
		return FALSE;
	font = font_definition_get(panel->font_index);
	panel->line_height = (short)(font->ascending_height + font->descending_height);
	panel->width = CAMPAIGN_SCORE_PANEL_WIDTH;
	if (panel->line_height <= 0)
		return FALSE;
	*width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	*view_height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	panel->scale = (real)*view_height / 480.0f;
	return panel->scale > 0.0f;
}

/* an ARGB pixel of a colour, its channels scaled */
static pixel32 campaign_score_pixel(
	byte alpha,
	real_rgb_color const *color,
	real scale)
{
	long red = (long)(PIN(color->red * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
	long green = (long)(PIN(color->green * scale, 0.0f, 1.0f) * 255.0f + 0.5f);
	long blue = (long)(PIN(color->blue * scale, 0.0f, 1.0f) * 255.0f + 0.5f);

	return ((pixel32)alpha << 24) | ((pixel32)red << 16) | ((pixel32)green << 8) | (pixel32)blue;
}

/* game.score_hud_color: the panel's own blue, the player's Spartan's
colour, or game.score_hud_custom_color's armor colour */
enum
{
	_campaign_score_hud_color_default = 0,
	_campaign_score_hud_color_spartan,
	_campaign_score_hud_color_custom,
};

/* the multiplayer armor colors, in the profile's order (player_profile.c) */
static char const *const campaign_score_hud_color_names[] =
{
	"white", "black", "red", "blue", "gray", "yellow", "green", "pink", "purple",
	"cyan", "cobalt", "orange", "teal", "sage", "brown", "tan", "maroon", "salmon",
};

static short campaign_score_hud_color(
	short *custom_index)
{
	static unsigned long changes = (unsigned long)-1;
	static short mode = _campaign_score_hud_color_default;
	static short custom = 3;

	if (changes != config_changes())
	{
		char const *value = config_string("game.score_hud_color");
		short index;

		changes = config_changes();
		mode = _campaign_score_hud_color_default;
		if (value && !strcmp(value, "spartan"))
			mode = _campaign_score_hud_color_spartan;
		else if (value && !strcmp(value, "custom"))
			mode = _campaign_score_hud_color_custom;
		value = config_string("game.score_hud_custom_color");
		custom = 3;
		for (index = 0; value && index < (short)NUMBEROF(campaign_score_hud_color_names); index++)
		{
			if (!strcmp(value, campaign_score_hud_color_names[index]))
				custom = index;
		}
	}
	*custom_index = custom;
	return mode;
}

/* the panel's colours for one player's view, as game.score_hud_color
chooses: the blue, a fixed armor colour, or their Spartan's, as the others
see it: a team game's team colour (red or blue), a free for all's armor
colour (game_engine_player_get_change_color); in the campaign, their unit's
own (the campaign Spartan's fixed green alone, a network co-op player's
profile colour: players.c's players_coop_color_unit) */
static void campaign_score_palette_set(
	long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	short local_player_index = player ? player->local_player_index : NONE;
	real_rgb_color color;
	real_rgb_color hue;
	real peak;
	boolean known = FALSE;
	short custom_index;
	short mode = campaign_score_hud_color(&custom_index);

	campaign_score_palette = campaign_score_palette_blue;
	if (mode == _campaign_score_hud_color_default)
		return;
	if (mode == _campaign_score_hud_color_custom)
	{
		color = player_profile_get_rgb_color(custom_index);
		known = TRUE;
	}
	else if (!player)
	{
		return;
	}
	else if (game_engine_running())
	{
		game_engine_player_get_change_color(&color, player_index);
		known = TRUE;
	}
	else if (player->unit_index != NONE)
	{
		struct object_datum *object = object_try_and_get_and_verify_type(player->unit_index, NONE);

		if (object)
		{
			color = object->object.base_change_colors[0];
			known = TRUE;
		}
	}
	if (mode == _campaign_score_hud_color_spartan &&
		local_player_index >= 0 && local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
	{
		if (known)
		{
			campaign_score_player_colors[local_player_index] = color;
			campaign_score_player_color_known[local_player_index] = TRUE;
		}
		else if (campaign_score_player_color_known[local_player_index])
		{
			color = campaign_score_player_colors[local_player_index];
			known = TRUE;
		}
	}
	if (!known)
		return;

	/* the colour's hue at its brightest (its brightest channel 1), as the
	blue's: the fill dark, the edge bright, the strip and the subline toward
	white; black, which would not show on the dark fill, a grey */
	peak = MAX(color.red, MAX(color.green, color.blue));
	if (peak < 0.08f)
	{
		hue.red = hue.green = hue.blue = 0.55f;
	}
	else
	{
		hue.red = color.red / peak;
		hue.green = color.green / peak;
		hue.blue = color.blue / peak;
	}
	campaign_score_palette.fill = campaign_score_pixel(0xA0, &hue, 0.27f);
	campaign_score_palette.edge = campaign_score_pixel(0xE0, &hue, 0.88f);
	{
		real_rgb_color pale;

		pale.red = hue.red * 0.8f + 0.2f;
		pale.green = hue.green * 0.8f + 0.2f;
		pale.blue = hue.blue * 0.8f + 0.2f;
		campaign_score_palette.strip = campaign_score_pixel(0xC0, &pale, 0.94f);
		campaign_score_palette.unlit = campaign_score_pixel(0x40, &pale, 0.94f);
	}
	campaign_score_palette.subline.red = hue.red * 0.6f + 0.4f;
	campaign_score_palette.subline.green = hue.green * 0.6f + 0.4f;
	campaign_score_palette.subline.blue = hue.blue * 0.6f + 0.4f;

	return;
}

/* the medal history's row: its icons' size, and the room the row takes
with the gap beneath it (0 with no medals) */
static real campaign_score_history_size(
	void)
{
	short slots = (short)PIN(campaign_score_values.medal_history, 1, CAMPAIGN_SCORE_HISTORY_MAXIMUM);

	/* (as many slots as the history has, across the whole row: the icons
	smaller, if they must be, to fit them) */
	return MIN((real)CAMPAIGN_SCORE_HISTORY_SIZE,
		(CAMPAIGN_SCORE_PANEL_WIDTH - CAMPAIGN_SCORE_ROW_PADDING * 4 - (slots - 1) * CAMPAIGN_SCORE_HISTORY_GAP) /
			(real)slots);
}

static real campaign_score_history_height(
	short history_count)
{
	if (history_count <= 0)
		return 0.0f;
	return campaign_score_history_size() + CAMPAIGN_SCORE_ROW_PADDING * 2 + CAMPAIGN_SCORE_ROW_GAP;
}

/* the medal history: the last medals the player won, the newest at the
right, as campaign_score_values.json's medal history counts; at the panel's
top, which it moves beneath itself */
static void campaign_score_draw_history(
	struct campaign_score_panel *panel,
	short const *history,
	short history_count)
{
	long bitmap_index = campaign_medals_bitmap();
	real size = campaign_score_history_size();
	real row = size + CAMPAIGN_SCORE_ROW_PADDING * 2;
	short index;

	if (history_count <= 0)
		return;
	campaign_score_panel_quad(panel, 0.0f, 0.0f, panel->width, row, CAMPAIGN_SCORE_PANEL_FILL);
	campaign_score_panel_frame(panel, 0.0f, 0.0f, panel->width, row, CAMPAIGN_SCORE_HUD_EDGE);
	for (index = 0; index < history_count && bitmap_index != NONE; index++)
	{
		struct bitmap_data *bitmap = bitmap_group_try_and_get_bitmap(bitmap_index, history[index]);
		real x1 = panel->width - CAMPAIGN_SCORE_ROW_PADDING * 2 -
			(history_count - 1 - index) * (size + CAMPAIGN_SCORE_HISTORY_GAP);
		rectangle2d bounds;

		if (!bitmap)
			continue;
		bounds.x0 = (short)(panel->left + (x1 - size) * panel->scale);
		bounds.x1 = (short)(panel->left + x1 * panel->scale);
		bounds.y0 = (short)(panel->top + CAMPAIGN_SCORE_ROW_PADDING * panel->scale);
		bounds.y1 = (short)(panel->top + (CAMPAIGN_SCORE_ROW_PADDING + size) * panel->scale);
		draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, TRUE);
	}
	/* (the rest beneath it) */
	panel->top += campaign_score_history_height(history_count) * panel->scale;

	return;
}

/* a wide string's plain letters (the panel's rows take them) */
static void campaign_score_narrow(
	wchar_t const *source,
	char *text,
	long size)
{
	long index;

	for (index = 0; index < size - 1 && source[index]; index++)
		text[index] = source[index] < 0x80 ? (char)source[index] : '?';
	text[index] = 0;

	return;
}

/* a multiplayer game's panel: the medal history (game.multiplayer_medals),
and (game.multiplayer_score_hud) a strip of the team's standing (a team
game's: winning, losing or tied) and the game's time (left, with a time
limit), and the player's row: their name and place, their kills / deaths /
assists, and beneath, the game's kind and its game type's score (the
row wider than CAMPAIGN_SCORE_PANEL_WIDTH when the name and numbers need it,
grown to the left; the history over it keeps its own width) */
static void campaign_score_draw_multiplayer_view(
	long player_index)
{
	struct campaign_score_panel panel;
	struct player_datum *player = player_try_and_get(player_index);
	boolean row_shown = campaign_score_multiplayer_hud();
	real standing_height = 0.0f;
	real row_height = 0.0f;
	real history_height;
	short width;
	short view_height;
	short history[CAMPAIGN_SCORE_HISTORY_MAXIMUM];
	short history_count;

	if (!player || !campaign_score_panel_begin(&panel, &width, &view_height))
		return;
	campaign_score_update();
	campaign_score_palette_set(player_index);
	history_count = campaign_score_medal_history(player_index, history, NUMBEROF(history));
	history_height = campaign_score_history_height(history_count);
	if (row_shown)
	{
		/* (one row: the standing's line, of the smaller text, over the
		name's and the subline's) */
		standing_height = panel.line_height * CAMPAIGN_SCORE_SUBLINE_SIZE;
		row_height = panel.line_height * (1.0f + CAMPAIGN_SCORE_SUBLINE_SIZE) + CAMPAIGN_SCORE_ROW_PADDING * 2;
		row_height = MAX(row_height, CAMPAIGN_SCORE_ROW_MINIMUM) + standing_height;
	}
	if (history_height + row_height > 0.0f)
	{
		struct game_engine_hud_score hud_score;
		wchar_t own_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
		wchar_t name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 16];
		wchar_t wide_numbers[32];
		char kills_deaths_assists[32];
		long index;
		real row_width = panel.width;

		/* (the history's gap beneath it with no row there) */
		if (!row_shown)
			history_height -= CAMPAIGN_SCORE_ROW_GAP;
		/* (the row's name and place, kills / deaths / assists: the row wider
		than the panel's own width if they need it) */
		if (row_shown)
		{
			game_engine_hud_score_get(player_index, &hud_score);
			campaign_score_player_display_name(player_index, own_name, NUMBEROF(own_name));
			own_name[NUMBEROF(own_name) - 1] = 0;
			usnprintf(name, NUMBEROF(name), L"%s #%d", own_name, (int)hud_score.place);
			name[NUMBEROF(name) - 1] = 0;
			snprintf(kills_deaths_assists, sizeof(kills_deaths_assists), "%d / %d / %d",
				(int)player->statistics.kills[0], (int)player->statistics.deaths, (int)player->statistics.assists[0]);
			for (index = 0; index < (long)NUMBEROF(wide_numbers) - 1 && kills_deaths_assists[index]; index++)
				wide_numbers[index] = (wchar_t)(unsigned char)kills_deaths_assists[index];
			wide_numbers[index] = 0;
			row_width = MAX(row_width, CAMPAIGN_SCORE_ROW_PADDING * 4 + CAMPAIGN_SCORE_ROW_TEXT_GAP +
				campaign_score_panel_text_width(&panel, name, 1.0f) +
				campaign_score_panel_text_width(&panel, wide_numbers, 1.0f));
		}
		/* (the history at the panel's own width, as many medals as it
		counts, over the row's right; the row beneath it, as wide as it is) */
		panel.left = width - (CAMPAIGN_SCORE_PANEL_MARGIN + panel.width) * panel.scale;
		panel.top = view_height - (CAMPAIGN_SCORE_PANEL_MARGIN + history_height + row_height) * panel.scale;
		campaign_score_draw_history(&panel, history, history_count);
		panel.width = row_width;
		panel.left = width - (CAMPAIGN_SCORE_PANEL_MARGIN + panel.width) * panel.scale;
		if (row_shown)
		{
			real text_left = CAMPAIGN_SCORE_ROW_PADDING * 2;
			real text_right = panel.width - CAMPAIGN_SCORE_ROW_PADDING * 2;
			char kind[48];
			char score[64];
			char time[32];
			real_argb_color color;

			campaign_score_narrow(hud_score.kind, kind, sizeof(kind));
			campaign_score_narrow(hud_score.score, score, sizeof(score));
			campaign_score_narrow(hud_score.time, time, sizeof(time));

			/* the row's panel; on top, the team's standing and the game's
			time */
			campaign_score_panel_quad(&panel, 0.0f, 0.0f, panel.width, row_height, CAMPAIGN_SCORE_PANEL_FILL);
			campaign_score_panel_frame(&panel, 0.0f, 0.0f, panel.width, row_height, CAMPAIGN_SCORE_HUD_EDGE);
			color.alpha = 1.0f;
			switch (hud_score.team_standing)
			{
			case _game_engine_hud_winning:
				color.red = 0.45f;
				color.green = 0.95f;
				color.blue = 0.45f;
				campaign_score_panel_text(&panel, "WINNING", NULL, text_left, text_right, CAMPAIGN_SCORE_ROW_PADDING,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
				break;
			case _game_engine_hud_losing:
				color.red = 1.0f;
				color.green = 0.42f;
				color.blue = 0.38f;
				campaign_score_panel_text(&panel, "LOSING", NULL, text_left, text_right, CAMPAIGN_SCORE_ROW_PADDING,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
				break;
			case _game_engine_hud_tied:
				color.red = 1.0f;
				color.green = 0.92f;
				color.blue = 0.55f;
				campaign_score_panel_text(&panel, "TIED", NULL, text_left, text_right, CAMPAIGN_SCORE_ROW_PADDING,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
				break;
			default:
				break;
			}
			color.red = campaign_score_palette.subline.red;
			color.green = campaign_score_palette.subline.green;
			color.blue = campaign_score_palette.subline.blue;
			campaign_score_panel_text(&panel, time, NULL, text_left, text_right, CAMPAIGN_SCORE_ROW_PADDING,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);

			/* beneath it, their name and place, kills / deaths / assists; the
			game's kind, its score */
			campaign_score_panel_row_text(&panel, standing_height, NULL, name, kills_deaths_assists, kind, score);
		}
		campaign_score_drawn_height = (short)(history_height + row_height + CAMPAIGN_SCORE_SKULLS_GAP + 0.5f);
	}
	/* the medals just won, at the center left (campaign_medals.c) */
	campaign_medals_draw_popups(player_index);

	return;
}

/* Firefight's text beside the score row's: its smaller lines' size */
#define CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE 0.66f

enum
{
	/* (the rounds' and the waves' marks: a round's width, the current round's
	waves' width, and their heights and gaps) */
	CAMPAIGN_SCORE_FIREFIGHT_ROUND_WIDTH = 6,
	CAMPAIGN_SCORE_FIREFIGHT_WAVE_WIDTH = 5,
	CAMPAIGN_SCORE_FIREFIGHT_MARK_GAP = 2
};

/* how wide the rounds' and waves' marks are: the current round's opens into
its waves, none with a bonus round's */
static real campaign_score_firefight_marks_width(
	void)
{
	short rounds = FIREFIGHT_ROUNDS_PER_SET;
	real width = (rounds - 1) * CAMPAIGN_SCORE_FIREFIGHT_MARK_GAP;

	if (ff_state.in_bonus_round)
		return width + rounds * CAMPAIGN_SCORE_FIREFIGHT_ROUND_WIDTH;
	return width + (rounds - 1) * CAMPAIGN_SCORE_FIREFIGHT_ROUND_WIDTH +
		FIREFIGHT_WAVES_PER_ROUND * CAMPAIGN_SCORE_FIREFIGHT_WAVE_WIDTH + (FIREFIGHT_WAVES_PER_ROUND - 1);
}

/* the marks, their right edge at x1: the rounds done lit, the current round
as its waves (those done lit, this one white, or orange for a boss's), those
to come dark */
static void campaign_score_firefight_marks_draw(
	struct campaign_score_panel const *panel,
	real x1,
	real y,
	real height)
{
	short round = PIN(ff_state.current_round, (short)1, (short)FIREFIGHT_ROUNDS_PER_SET);
	short wave = PIN(ff_state.current_wave, (short)1, (short)FIREFIGHT_WAVES_PER_ROUND);
	real x = x1 - campaign_score_firefight_marks_width();
	short index;

	for (index = 1; index <= FIREFIGHT_ROUNDS_PER_SET; index++)
	{
		if (index == round && !ff_state.in_bonus_round)
		{
			short wave_index;

			for (wave_index = 1; wave_index <= FIREFIGHT_WAVES_PER_ROUND; wave_index++)
			{
				pixel32 color = CAMPAIGN_SCORE_STRIP_UNLIT;

				if (wave_index < wave)
					color = CAMPAIGN_SCORE_STRIP_FILL;
				else if (wave_index == wave)
					color = wave == FIREFIGHT_WAVES_PER_ROUND ? 0xFFFF6040 : 0xFFFFFFFF;
				campaign_score_panel_quad(panel, x, y, x + CAMPAIGN_SCORE_FIREFIGHT_WAVE_WIDTH, y + height, color);
				x += CAMPAIGN_SCORE_FIREFIGHT_WAVE_WIDTH + 1.0f;
			}
			x += CAMPAIGN_SCORE_FIREFIGHT_MARK_GAP - 1.0f;
		}
		else
		{
			campaign_score_panel_quad(panel, x, y, x + CAMPAIGN_SCORE_FIREFIGHT_ROUND_WIDTH, y + height,
				index < round || ff_state.in_bonus_round ? CAMPAIGN_SCORE_STRIP_FILL : CAMPAIGN_SCORE_STRIP_UNLIT);
			x += CAMPAIGN_SCORE_FIREFIGHT_ROUND_WIDTH + CAMPAIGN_SCORE_FIREFIGHT_MARK_GAP;
		}
	}

	return;
}

/* the row of a Firefight game: the lives, the hostiles and the time on top;
the player and their score; the set, round and wave at the left and their
marks at the right */
static void campaign_score_firefight_row(
	struct campaign_score_panel const *panel,
	real y,
	real row_height,
	wchar_t const *name,
	char const *score,
	char const *time,
	char const *lives,
	char const *hostiles,
	char const *progress)
{
	real_argb_color text_color;
	real_argb_color subline_color;
	real_argb_color lives_color;
	real const small = panel->line_height * CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE;
	real const text_left = CAMPAIGN_SCORE_ROW_PADDING * 2;
	real const text_right = panel->width - CAMPAIGN_SCORE_ROW_PADDING * 2;
	real const name_top = y + CAMPAIGN_SCORE_ROW_PADDING + small;
	real const progress_top = name_top + panel->line_height * 0.92f;

	campaign_score_panel_quad(panel, 0.0f, y, panel->width, y + row_height, CAMPAIGN_SCORE_PANEL_FILL);
	campaign_score_panel_frame(panel, 0.0f, y, panel->width, y + row_height, CAMPAIGN_SCORE_HUD_EDGE);
	text_color.alpha = 1.0f;
	text_color.red = text_color.green = text_color.blue = 0.95f;
	subline_color.alpha = 1.0f;
	subline_color.red = campaign_score_palette.subline.red;
	subline_color.green = campaign_score_palette.subline.green;
	subline_color.blue = campaign_score_palette.subline.blue;
	lives_color = subline_color;
	/* (the last life, in red) */
	if (ff_state.shared_lives_remaining >= 0 && ff_state.shared_lives_remaining <= 1)
	{
		lives_color.red = 1.0f;
		lives_color.green = 0.35f;
		lives_color.blue = 0.35f;
	}
	campaign_score_panel_text(panel, lives, NULL, text_left, text_right, y + CAMPAIGN_SCORE_ROW_PADDING,
		CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE, _campaign_score_text_left, &lives_color);
	campaign_score_panel_text(panel, hostiles, NULL, text_left, text_right, y + CAMPAIGN_SCORE_ROW_PADDING,
		CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE, _campaign_score_text_center, &subline_color);
	campaign_score_panel_text(panel, time, NULL, text_left, text_right, y + CAMPAIGN_SCORE_ROW_PADDING,
		CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE, _campaign_score_text_right, &subline_color);
	campaign_score_panel_text(panel, NULL, name, text_left, text_right, name_top, 1.0f, _campaign_score_text_left,
		&text_color);
	campaign_score_panel_text(panel, score, NULL, text_left, text_right, name_top, 1.0f, _campaign_score_text_right,
		&text_color);
	campaign_score_panel_text(panel, progress, NULL, text_left, text_right, progress_top,
		CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE, _campaign_score_text_left, &subline_color);
	campaign_score_firefight_marks_draw(panel, text_right, progress_top + small * 0.2f, small * 0.6f);

	return;
}

void campaign_score_draw_view(
	long player_index)
{
	struct campaign_score_panel panel;
	boolean firefight = firefight_active();
	boolean competitive;
	boolean coop;
	real row_height;
	real multipliers_height;
	real rows_top;
	real height;
	short rows;
	short width;
	short view_height;
	short segment;
	char score[24];
	char time[32];
	wchar_t own_name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
	short history[CAMPAIGN_SCORE_HISTORY_MAXIMUM];
	short history_count;
	real history_height;

	campaign_score_drawn_height = 0;
	if (player_index == NONE)
		return;
	if (campaign_score_multiplayer())
	{
		campaign_score_draw_multiplayer_view(player_index);
		return;
	}
	/* (a Firefight game's panel is its HUD, whatever the campaign's says) */
	if ((!campaign_score_enabled() && !firefight) || !campaign_score_panel_begin(&panel, &width, &view_height))
		return;
	campaign_score_update();
	campaign_score_palette_set(player_index);
	competitive = campaign_score_is_competitive();
	coop = network_coop_active() || local_player_count() > 1;
	rows = 1;
	/* (each row a line of the HUD's text and a smaller one beneath) */
	row_height = panel.line_height * (1.0f + CAMPAIGN_SCORE_SUBLINE_SIZE) + CAMPAIGN_SCORE_ROW_PADDING * 2;
	row_height = MAX(row_height, CAMPAIGN_SCORE_ROW_MINIMUM);
	/* (Firefight's: a smaller line each side of the name's) */
	if (firefight)
	{
		row_height = panel.line_height * (0.92f + CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE * 2.0f) +
			CAMPAIGN_SCORE_ROW_PADDING * 2;
	}
	/* (and the multipliers' row, of the smaller text) */
	multipliers_height = panel.line_height * CAMPAIGN_SCORE_SUBLINE_SIZE + CAMPAIGN_SCORE_ROW_PADDING * 2;
	rows_top = CAMPAIGN_SCORE_STRIP_HEIGHT + CAMPAIGN_SCORE_STRIP_GAP + multipliers_height + CAMPAIGN_SCORE_ROW_GAP;
	height = rows_top + rows * row_height + (rows - 1) * CAMPAIGN_SCORE_ROW_GAP;
	/* (and over it all, the player's last medals: none before the first) */
	history_count = campaign_score_medal_history(player_index, history, NUMBEROF(history));
	history_height = campaign_score_history_height(history_count);
	panel.left = width - (CAMPAIGN_SCORE_PANEL_MARGIN + CAMPAIGN_SCORE_PANEL_WIDTH) * panel.scale;
	panel.top = view_height - (CAMPAIGN_SCORE_PANEL_MARGIN + history_height + height) * panel.scale;
	campaign_score_draw_history(&panel, history, history_count);

	/* the strip of segments over the rows: the time left until the time's
	multiplier is the last (x0: half again the par time), lit from the left,
	going out as it passes; all lit for a level with no par time */
	{
		real fraction = campaign_score_par_fraction();
		real limit = 0.0f;
		short lit = CAMPAIGN_SCORE_STRIP_SEGMENTS;
		short index;

		for (index = 0; index < campaign_score_values.time_bracket_count; index++)
			limit = MAX(limit, campaign_score_values.time_brackets[index].up_to_par);
		if (fraction >= 0.0f && limit > 0.0f)
			lit = (short)(CAMPAIGN_SCORE_STRIP_SEGMENTS * (1.0f - fraction / limit) + 0.999f);
		lit = PIN(lit, 0, CAMPAIGN_SCORE_STRIP_SEGMENTS);
		campaign_score_panel_quad(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_PANEL_WIDTH, CAMPAIGN_SCORE_STRIP_HEIGHT,
			CAMPAIGN_SCORE_PANEL_FILL);
		campaign_score_panel_frame(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_PANEL_WIDTH, CAMPAIGN_SCORE_STRIP_HEIGHT,
			CAMPAIGN_SCORE_HUD_EDGE);
		for (segment = 0; segment < CAMPAIGN_SCORE_STRIP_SEGMENTS; segment++)
		{
			real span = (CAMPAIGN_SCORE_PANEL_WIDTH - 4.0f) / CAMPAIGN_SCORE_STRIP_SEGMENTS;
			real x0 = 2.0f + segment * span;

			campaign_score_panel_quad(&panel, x0 + 0.5f, 2.0f, x0 + span - 0.5f, CAMPAIGN_SCORE_STRIP_HEIGHT - 2.0f,
				segment < lit ? CAMPAIGN_SCORE_STRIP_FILL : CAMPAIGN_SCORE_STRIP_UNLIT);
		}
	}

	/* the score's multiplier now, over the rows: the difficulty's and the
	skulls' together, which each kill is worth more by (a kill's style's is
	its own; the time's, which only the level's end applies, is the
	scoreboard's, which has each) */
	{
		char total_text[16];
		real y = CAMPAIGN_SCORE_STRIP_HEIGHT + CAMPAIGN_SCORE_STRIP_GAP;
		real_argb_color color;

		campaign_score_multiplier_string(campaign_score_difficulty_multiplier() * campaign_score_skulls_multiplier(),
			total_text, sizeof(total_text));
		campaign_score_panel_quad(&panel, 0.0f, y, CAMPAIGN_SCORE_PANEL_WIDTH, y + multipliers_height,
			CAMPAIGN_SCORE_PANEL_FILL);
		campaign_score_panel_frame(&panel, 0.0f, y, CAMPAIGN_SCORE_PANEL_WIDTH, y + multipliers_height,
			CAMPAIGN_SCORE_HUD_EDGE);
		color.alpha = 1.0f;
		color.red = campaign_score_palette.subline.red;
		color.green = campaign_score_palette.subline.green;
		color.blue = campaign_score_palette.subline.blue;
		campaign_score_panel_text(&panel, "SCORE MULTIPLIER:", NULL, CAMPAIGN_SCORE_ROW_PADDING * 2,
			CAMPAIGN_SCORE_PANEL_WIDTH - CAMPAIGN_SCORE_ROW_PADDING * 2, y + CAMPAIGN_SCORE_ROW_PADDING,
			CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
		campaign_score_panel_text(&panel, total_text, NULL, CAMPAIGN_SCORE_ROW_PADDING * 2,
			CAMPAIGN_SCORE_PANEL_WIDTH - CAMPAIGN_SCORE_ROW_PADDING * 2, y + CAMPAIGN_SCORE_ROW_PADDING,
			CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
	}

	/* (Firefight's: the match's, which stops with it) */
	if (firefight)
		campaign_score_seconds_string(ff_state.session_elapsed_ticks / TICKS_PER_SECOND, time, sizeof(time));
	else
		campaign_score_time_string(time, sizeof(time));
	snprintf(score, sizeof(score), "%ld",
		competitive || !coop ? campaign_score_player_points(player_index) : campaign_score_team_total());
	/* (the player's own name, their profile's; a cooperative co-op
	game's row is the team's) */
	campaign_score_player_display_name(player_index, own_name, NUMBEROF(own_name));
	if (firefight)
	{
		char lives[24];
		char hostiles[24];
		char progress[40];
		wchar_t wide[40];
		real needed;
		long index;

		if (ff_state.shared_lives_remaining < 0)
			snprintf(lives, sizeof(lives), "LIVES: INF");
		else
			snprintf(lives, sizeof(lives), "LIVES: %d", (int)ff_state.shared_lives_remaining);
		snprintf(hostiles, sizeof(hostiles), "HOSTILES: %d", (int)ff_state.enemies_alive);
		if (ff_state.in_bonus_round)
			snprintf(progress, sizeof(progress), "SET %d BONUS ROUND", (int)ff_state.current_set);
		else
		{
			snprintf(progress, sizeof(progress), "SET %d RND %d WAVE %d", (int)ff_state.current_set,
				(int)ff_state.current_round, (int)ff_state.current_wave);
		}
		/* (the row wider than the panel if its lines need it, leftward, as a
		multiplayer game's is) */
		for (index = 0; index < (long)NUMBEROF(wide) - 1 && progress[index]; index++)
			wide[index] = (wchar_t)(unsigned char)progress[index];
		wide[index] = 0;
		needed = CAMPAIGN_SCORE_ROW_PADDING * 4 + CAMPAIGN_SCORE_ROW_TEXT_GAP / 2 +
			campaign_score_panel_text_width(&panel, wide, CAMPAIGN_SCORE_FIREFIGHT_LINE_SIZE) +
			campaign_score_firefight_marks_width();
		if (needed > panel.width)
		{
			panel.left -= (needed - panel.width) * panel.scale;
			panel.width = needed;
		}
		campaign_score_firefight_row(&panel, rows_top, row_height, own_name, score, time, lives, hostiles, progress);
	}
	else
	{
		campaign_score_panel_row(&panel, rows_top, row_height,
			coop && !competitive ? "TEAM" : NULL, coop && !competitive ? NULL : own_name, score,
			campaign_score_difficulty_names[campaign_score_difficulty()], time);
	}

	campaign_score_drawn_height = (short)(history_height + height + CAMPAIGN_SCORE_SKULLS_GAP + 0.5f);
	/* the medals just won, at the center left (campaign_medals.c) */
	campaign_medals_draw_popups(player_index);

	return;
}

/* ---------- the level's summary

shown over the level just won (and the next one's start) on black, until A
or B is pressed: a card, as the HUD's panel is drawn (in the screen's 480
lines, centred), of the level's name and difficulty, its numbers in two
columns, its score, the medals won and the players' (a co-op game's) */

enum
{
	CAMPAIGN_SCORE_CARD_WIDTH = 440,
	CAMPAIGN_SCORE_CARD_PADDING = 14,
	CAMPAIGN_SCORE_CARD_GAP = 8,
	CAMPAIGN_SCORE_CARD_MEDAL_SIZE = 22,
	/* (the least a medal and its count take: the row's room shared out
	among as many as fit) */
	CAMPAIGN_SCORE_CARD_MEDAL_SPACE = 56,
};

/* (the summary's colours: campaign_score.h's CAMPAIGN_SCORE_CARD_*) */

static void campaign_score_card_color(
	real_argb_color *color,
	real red,
	real green,
	real blue)
{
	color->alpha = 1.0f;
	color->red = red;
	color->green = green;
	color->blue = blue;

	return;
}

/* a label at the left of the column and its value at its right */
static void campaign_score_card_pair(
	struct campaign_score_panel const *panel,
	boolean draw,
	real x0,
	real x1,
	real y,
	char const *label,
	char const *value,
	boolean highlight)
{
	real_argb_color label_color;
	real_argb_color value_color;

	if (!draw)
		return;
	campaign_score_card_color(&label_color, 0.55f, 0.72f, 1.0f);
	if (highlight)
		campaign_score_card_color(&value_color, 1.0f, 0.84f, 0.35f);
	else
		campaign_score_card_color(&value_color, 0.95f, 0.95f, 0.95f);
	campaign_score_panel_text(panel, label, NULL, x0, x1, y, CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left,
		&label_color);
	campaign_score_panel_text(panel, value, NULL, x0, x1, y, CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right,
		&value_color);

	return;
}

/* the card, from its top (panel's): its height, drawn or not */
static real campaign_score_card_layout(
	struct campaign_score_panel const *panel,
	struct campaign_score_report const *report,
	boolean draw)
{
	real const width = CAMPAIGN_SCORE_CARD_WIDTH;
	real const left = CAMPAIGN_SCORE_CARD_PADDING;
	real const right = width - CAMPAIGN_SCORE_CARD_PADDING;
	real const middle = width / 2.0f;
	real const line = panel->line_height;
	real const small = panel->line_height * CAMPAIGN_SCORE_SUBLINE_SIZE;
	real_argb_color color;
	char text[64];
	char value[48];
	char other[24];
	real y = CAMPAIGN_SCORE_CARD_PADDING;
	short index;

	/* the title, the level's name, its difficulty and skulls */
	if (draw)
	{
		wchar_t level[64];

		campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
		campaign_score_panel_text(panel, "LEVEL COMPLETE", NULL, left, right, y, 1.0f, _campaign_score_text_center,
			&color);
		campaign_score_card_color(&color, CAMPAIGN_SCORE_BLUE_RED + 0.2f, CAMPAIGN_SCORE_BLUE_GREEN + 0.2f, 1.0f);
		ustrncpy(level, report->level, NUMBEROF(level) - 1);
		level[NUMBEROF(level) - 1] = 0;
		campaign_score_panel_text(panel, NULL, level, left, right, y + line + 2.0f, 1.0f, _campaign_score_text_center,
			&color);
	}
	y += line * 2.0f + 4.0f;
	/* (whose card it is, in a game of more than one player: its name, and
	which of this machine's players' cards it is) */
	if (report->player[0] && (report->player_total > 1 || report->player_count > 1))
	{
		if (draw)
		{
			wchar_t whose[64];

			if (report->player_total > 1)
			{
				usnprintf(whose, NUMBEROF(whose), L"%s  (PLAYER %d OF %d)", report->player, (int)report->player_number,
					(int)report->player_total);
			}
			else
				ustrncpy(whose, report->player, NUMBEROF(whose) - 1);
			whose[NUMBEROF(whose) - 1] = 0;
			campaign_score_card_color(&color, 1.0f, 0.84f, 0.35f);
			campaign_score_panel_text(panel, NULL, whose, left, right, y, CAMPAIGN_SCORE_SUBLINE_SIZE,
				_campaign_score_text_center, &color);
		}
		y += small + 4.0f;
	}
	campaign_score_multiplier_string(report->difficulty_multiplier, value, sizeof(value));
	campaign_score_multiplier_string(report->skulls_multiplier, other, sizeof(other));
	snprintf(text, sizeof(text), "%s %s   SKULLS %s", campaign_score_difficulty_names[report->difficulty], value,
		other);
	if (draw)
	{
		campaign_score_card_color(&color, 0.6f, 0.66f, 0.75f);
		campaign_score_panel_text(panel, text, NULL, left, right, y, CAMPAIGN_SCORE_SUBLINE_SIZE,
			_campaign_score_text_center, &color);
	}
	y += small + CAMPAIGN_SCORE_CARD_GAP;
	if (draw)
		campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
	y += CAMPAIGN_SCORE_CARD_GAP;

	/* the numbers, two columns */
	{
		real const column_gap = 24.0f;
		real const left_x1 = middle - column_gap / 2.0f;
		real const right_x0 = middle + column_gap / 2.0f;
		real const row = small + 6.0f;
		char numbers[5][2][2][24];
		short row_index;

		campaign_score_seconds_string(report->seconds, numbers[0][0][1], sizeof(numbers[0][0][1]));
		snprintf(numbers[0][0][0], sizeof(numbers[0][0][0]), "%s", report->record_time ? "TIME (BEST!)" : "TIME");
		snprintf(numbers[0][1][0], sizeof(numbers[0][1][0]), "PAR");
		if (report->par_seconds != NONE)
			campaign_score_seconds_string(report->par_seconds, numbers[0][1][1], sizeof(numbers[0][1][1]));
		else
			snprintf(numbers[0][1][1], sizeof(numbers[0][1][1]), "-");
		snprintf(numbers[1][0][0], sizeof(numbers[1][0][0]), "KILLS");
		snprintf(numbers[1][0][1], sizeof(numbers[1][0][1]), "%ld", report->kills);
		snprintf(numbers[1][1][0], sizeof(numbers[1][1][0]), "HEADSHOTS");
		snprintf(numbers[1][1][1], sizeof(numbers[1][1][1]), "%ld", report->headshots);
		snprintf(numbers[2][0][0], sizeof(numbers[2][0][0]), "MELEE KILLS");
		snprintf(numbers[2][0][1], sizeof(numbers[2][0][1]), "%ld", report->melee);
		snprintf(numbers[2][1][0], sizeof(numbers[2][1][0]), "MEDALS");
		snprintf(numbers[2][1][1], sizeof(numbers[2][1][1]), "%ld", report->medals);
		snprintf(numbers[3][0][0], sizeof(numbers[3][0][0]), "DEATHS");
		snprintf(numbers[3][0][1], sizeof(numbers[3][0][1]), "%ld", report->deaths);
		snprintf(numbers[3][1][0], sizeof(numbers[3][1][0]), "BETRAYALS");
		snprintf(numbers[3][1][1], sizeof(numbers[3][1][1]), "%ld", report->betrayals);
		snprintf(numbers[4][0][0], sizeof(numbers[4][0][0]), "ALLIES KILLED");
		snprintf(numbers[4][0][1], sizeof(numbers[4][0][1]), "%ld", report->allies);
		snprintf(numbers[4][1][0], sizeof(numbers[4][1][0]), "TIME BONUS");
		campaign_score_multiplier_string(report->time_multiplier, numbers[4][1][1], sizeof(numbers[4][1][1]));
		for (row_index = 0; row_index < 5; row_index++)
		{
			campaign_score_card_pair(panel, draw, left, left_x1, y, numbers[row_index][0][0], numbers[row_index][0][1],
				row_index == 0 && report->record_time);
			campaign_score_card_pair(panel, draw, right_x0, right, y, numbers[row_index][1][0],
				numbers[row_index][1][1], FALSE);
			y += row;
		}
	}
	y += CAMPAIGN_SCORE_CARD_GAP - 6.0f;
	if (draw)
		campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
	y += CAMPAIGN_SCORE_CARD_GAP;

	/* the score: the points, times the time's bonus */
	snprintf(value, sizeof(value), "%ld", report->points);
	campaign_score_multiplier_string(report->time_multiplier, other, sizeof(other));
	snprintf(text, sizeof(text), "POINTS %s  x  TIME BONUS %s", value, other);
	if (draw)
	{
		campaign_score_card_color(&color, 0.6f, 0.66f, 0.75f);
		campaign_score_panel_text(panel, text, NULL, left, right, y, CAMPAIGN_SCORE_SUBLINE_SIZE,
			_campaign_score_text_center, &color);
	}
	y += small + 4.0f;
	if (!(report->competitive && report->player_count > 1))
	{
		snprintf(value, sizeof(value), "%ld", report->score);
		if (draw)
		{
			campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
			campaign_score_panel_text(panel, "SCORE", NULL, left, right, y, 1.0f, _campaign_score_text_left, &color);
			campaign_score_card_color(&color, 1.0f, 0.84f, 0.35f);
			campaign_score_panel_text(panel, value, NULL, left, right, y, 1.0f, _campaign_score_text_right, &color);
			if (report->record_score)
			{
				campaign_score_panel_text(panel, "NEW RECORD!", NULL, left, right, y + (line - small) / 2.0f,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_center, &color);
			}
		}
		y += line + 4.0f;
	}
	/* (what the online leaderboard made of it: community.c, as it answers) */
	if (report->result_slot != NONE && community_result_text(report->result_slot)[0])
	{
		if (draw)
		{
			campaign_score_card_color(&color, CAMPAIGN_SCORE_BLUE_RED + 0.2f, CAMPAIGN_SCORE_BLUE_GREEN + 0.2f, 1.0f);
			campaign_score_panel_text(panel, NULL, community_result_text(report->result_slot), left, right, y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_center, &color);
		}
		y += small + 6.0f;
	}
	/* (a co-op game's players: each one's score and kills) */
	if (report->player_count > 1)
	{
		for (index = 0; index < report->player_count; index++)
		{
			snprintf(value, sizeof(value), "%ld   KILLS %ld", report->players[index].score, report->players[index].kills);
			campaign_score_card_pair(panel, draw, left, right, y, report->players[index].name, value, FALSE);
			y += small + 6.0f;
		}
	}

	/* the medals won, each with how many */
	if (report->medals > 0)
	{
		long bitmap_index = campaign_medals_bitmap();
		short per_row = (short)MAX((right - left) / CAMPAIGN_SCORE_CARD_MEDAL_SPACE, 1);
		real space = (right - left) / per_row;
		short shown = 0;

		y += CAMPAIGN_SCORE_CARD_GAP - 4.0f;
		if (draw)
			campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
		y += CAMPAIGN_SCORE_CARD_GAP;
		for (index = 0; index < NUMBER_OF_CAMPAIGN_MEDALS; index++)
		{
			real x;

			if (report->medal_counts[index] <= 0)
				continue;
			/* (a row of as many as fit, spread over the whole width) */
			if (shown > 0 && shown % per_row == 0)
				y += CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f;
			x = left + (shown % per_row) * space;
			shown++;
			if (draw)
			{
				struct bitmap_data *bitmap = bitmap_index != NONE ?
					bitmap_group_try_and_get_bitmap(bitmap_index, index) : NULL;

				if (bitmap)
				{
					rectangle2d bounds;

					bounds.x0 = (short)(panel->left + x * panel->scale);
					bounds.y0 = (short)(panel->top + y * panel->scale);
					bounds.x1 = (short)(panel->left + (x + CAMPAIGN_SCORE_CARD_MEDAL_SIZE) * panel->scale);
					bounds.y1 = (short)(panel->top + (y + CAMPAIGN_SCORE_CARD_MEDAL_SIZE) * panel->scale);
					draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, TRUE);
				}
				snprintf(value, sizeof(value), "x%d", (int)report->medal_counts[index]);
				campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
				campaign_score_panel_text(panel, value, NULL, x + CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f,
					x + space, y + (CAMPAIGN_SCORE_CARD_MEDAL_SIZE - small) / 2.0f,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
			}
		}
		y += CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f;
	}

	return y + CAMPAIGN_SCORE_CARD_PADDING - 4.0f;
}

/* ---------- Firefight's report

the match's report, a card as the level's summary is, in the postgame's 640 by
480: the map and difficulty, how far the game got and the local player's
numbers, the players' scores, the team's score and the local player's medals */

enum
{
	CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH = 584,
	/* (the card's room above the prompt, at 410 of the 480) */
	CAMPAIGN_SCORE_FIREFIGHT_CARD_BOTTOM = 402,
	CAMPAIGN_SCORE_FIREFIGHT_CARD_PLAYERS = 8,
	CAMPAIGN_SCORE_FIREFIGHT_CARD_ICON = 14,
	/* (the numbers' column, and where the players' columns end from the
	card's right) */
	CAMPAIGN_SCORE_FIREFIGHT_NUMBERS_WIDTH = 216,
	CAMPAIGN_SCORE_FIREFIGHT_SCORE_COLUMN = 96,
	CAMPAIGN_SCORE_FIREFIGHT_KILLS_COLUMN = 60,
	CAMPAIGN_SCORE_FIREFIGHT_DEATHS_COLUMN = 30
};

static struct campaign_score_report campaign_score_firefight_report;

/* the card, from its top: its height, drawn or not */
static real campaign_score_firefight_card_layout(
	struct campaign_score_panel const *panel,
	struct campaign_score_report const *report,
	long const *players,
	short player_count,
	boolean draw)
{
	real const width = CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH;
	real const left = CAMPAIGN_SCORE_CARD_PADDING;
	real const right = width - CAMPAIGN_SCORE_CARD_PADDING;
	real const line = panel->line_height;
	real const small = panel->line_height * CAMPAIGN_SCORE_SUBLINE_SIZE;
	real const row = small + 5.0f;
	long team_score = campaign_score_team_points();
	real_argb_color color;
	char text[64];
	char value[48];
	char other[24];
	real y = CAMPAIGN_SCORE_CARD_PADDING;
	short index;

	/* the title, the map, its difficulty and skulls */
	if (draw)
	{
		wchar_t level[64];

		campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
		campaign_score_panel_text(panel, ff_state.phase_state == _ff_phase_victory ? "FIREFIGHT VICTORY" :
			(ff_state.phase_state == _ff_phase_defeat ? "FIREFIGHT OVER" : "FIREFIGHT COMPLETE"), NULL, left, right, y,
			1.0f, _campaign_score_text_center, &color);
		campaign_score_card_color(&color, CAMPAIGN_SCORE_BLUE_RED + 0.2f, CAMPAIGN_SCORE_BLUE_GREEN + 0.2f, 1.0f);
		ustrncpy(level, report->level, NUMBEROF(level) - 1);
		level[NUMBEROF(level) - 1] = 0;
		campaign_score_panel_text(panel, NULL, level, left, right, y + line + 2.0f, 1.0f, _campaign_score_text_center,
			&color);
	}
	y += line * 2.0f + 4.0f;
	campaign_score_multiplier_string(report->difficulty_multiplier, value, sizeof(value));
	campaign_score_multiplier_string(report->skulls_multiplier, other, sizeof(other));
	snprintf(text, sizeof(text), "%s %s   SKULLS %s", campaign_score_difficulty_names[report->difficulty], value,
		other);
	if (draw)
	{
		campaign_score_card_color(&color, 0.6f, 0.66f, 0.75f);
		campaign_score_panel_text(panel, text, NULL, left, right, y, CAMPAIGN_SCORE_SUBLINE_SIZE,
			_campaign_score_text_center, &color);
	}
	y += small + CAMPAIGN_SCORE_CARD_GAP;
	if (draw)
		campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
	y += CAMPAIGN_SCORE_CARD_GAP;

	/* the numbers at the left, the players at the right */
	{
		real const numbers_right = left + CAMPAIGN_SCORE_FIREFIGHT_NUMBERS_WIDTH;
		real const players_left = numbers_right + 20.0f;
		real const score_column = right - CAMPAIGN_SCORE_FIREFIGHT_SCORE_COLUMN;
		real const kills_column = right - CAMPAIGN_SCORE_FIREFIGHT_KILLS_COLUMN;
		real const deaths_column = right - CAMPAIGN_SCORE_FIREFIGHT_DEATHS_COLUMN;
		short shown = (short)MIN(player_count, CAMPAIGN_SCORE_FIREFIGHT_CARD_PLAYERS);
		real numbers_y = y;
		real players_y = y;
		char numbers[8][2][24];
		short row_index;

		if (ff_state.in_bonus_round)
			snprintf(numbers[0][1], sizeof(numbers[0][1]), "SET %d BONUS", (int)ff_state.current_set);
		else
		{
			snprintf(numbers[0][1], sizeof(numbers[0][1]), "SET %d RND %d WAVE %d", (int)ff_state.current_set,
				(int)ff_state.current_round, (int)ff_state.current_wave);
		}
		snprintf(numbers[0][0], sizeof(numbers[0][0]), "REACHED");
		snprintf(numbers[1][0], sizeof(numbers[1][0]), "TIME");
		campaign_score_seconds_string(ff_state.session_elapsed_ticks / TICKS_PER_SECOND, numbers[1][1],
			sizeof(numbers[1][1]));
		snprintf(numbers[2][0], sizeof(numbers[2][0]), "KILLS");
		snprintf(numbers[2][1], sizeof(numbers[2][1]), "%ld", report->kills);
		snprintf(numbers[3][0], sizeof(numbers[3][0]), "HEADSHOTS");
		snprintf(numbers[3][1], sizeof(numbers[3][1]), "%ld", report->headshots);
		snprintf(numbers[4][0], sizeof(numbers[4][0]), "MELEE KILLS");
		snprintf(numbers[4][1], sizeof(numbers[4][1]), "%ld", report->melee);
		snprintf(numbers[5][0], sizeof(numbers[5][0]), "MEDALS");
		snprintf(numbers[5][1], sizeof(numbers[5][1]), "%ld", report->medals);
		snprintf(numbers[6][0], sizeof(numbers[6][0]), "DEATHS");
		snprintf(numbers[6][1], sizeof(numbers[6][1]), "%ld", report->deaths);
		snprintf(numbers[7][0], sizeof(numbers[7][0]), "LIVES LEFT");
		if (ff_state.shared_lives_remaining < 0)
			snprintf(numbers[7][1], sizeof(numbers[7][1]), "-");
		else
			snprintf(numbers[7][1], sizeof(numbers[7][1]), "%d", (int)ff_state.shared_lives_remaining);
		for (row_index = 0; row_index < 8; row_index++)
		{
			campaign_score_card_pair(panel, draw, left, numbers_right, numbers_y, numbers[row_index][0],
				numbers[row_index][1], FALSE);
			numbers_y += row;
		}

		/* (the columns' names, then a row each: best first, the best marked) */
		if (draw)
		{
			campaign_score_card_color(&color, 0.55f, 0.72f, 1.0f);
			campaign_score_panel_text(panel, "PLAYER", NULL, players_left, right, players_y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
			campaign_score_panel_text(panel, "SCORE", NULL, players_left, score_column, players_y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
			campaign_score_panel_text(panel, "K", NULL, players_left, kills_column, players_y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
			campaign_score_panel_text(panel, "D", NULL, players_left, deaths_column, players_y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
			campaign_score_panel_text(panel, "M", NULL, players_left, right, players_y,
				CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
		}
		players_y += row;
		for (index = 0; index < shown; index++)
		{
			long player_index = players[index];
			struct player_datum *player = player_try_and_get(player_index);

			if (draw && player)
			{
				struct firefight_mcc_rank_info rank;
				struct firefight_emblem_info emblem;
				short icon_x = (short)(panel->left + players_left * panel->scale);
				short icon_y = (short)(panel->top + players_y * panel->scale);
				real name_left = players_left + (CAMPAIGN_SCORE_FIREFIGHT_CARD_ICON + 2.0f) * 2.0f + 2.0f;

				if (index == 0)
				{
					campaign_score_panel_quad(panel, players_left - 3.0f, players_y - 1.0f, right + 3.0f,
						players_y + row - 2.0f, 0x30FFD040);
				}
				game_engine_player_rank_and_emblem(player_index, &rank, &emblem);
				firefight_draw_mcc_rank_icon(icon_x, icon_y, CAMPAIGN_SCORE_FIREFIGHT_CARD_ICON, rank.tour_number,
					rank.rank_in_tour, 1.0f);
				firefight_draw_player_emblem((short)(icon_x + CAMPAIGN_SCORE_FIREFIGHT_CARD_ICON + 2), icon_y,
					CAMPAIGN_SCORE_FIREFIGHT_CARD_ICON, &emblem, 1.0f);
				if (player->local_player_index != NONE)
					campaign_score_card_color(&color, 1.0f, 0.84f, 0.35f);
				else
					campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
				campaign_score_panel_text(panel, NULL, player->name, name_left, score_column - 44.0f, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
				snprintf(value, sizeof(value), "%ld", campaign_score_player_points(player_index));
				campaign_score_panel_text(panel, value, NULL, players_left, score_column, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
				snprintf(value, sizeof(value), "%ld", firefight_player_kills(player_index));
				campaign_score_panel_text(panel, value, NULL, players_left, kills_column, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
				snprintf(value, sizeof(value), "%d", (int)player->statistics.deaths);
				campaign_score_panel_text(panel, value, NULL, players_left, deaths_column, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
				snprintf(value, sizeof(value), "%ld", firefight_stats_get_session_medals(player_index));
				campaign_score_panel_text(panel, value, NULL, players_left, right, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_right, &color);
			}
			players_y += row;
		}
		if (player_count > shown)
		{
			if (draw)
			{
				snprintf(text, sizeof(text), "+%d MORE", (int)(player_count - shown));
				campaign_score_card_color(&color, 0.6f, 0.66f, 0.75f);
				campaign_score_panel_text(panel, text, NULL, players_left, right, players_y,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
			}
			players_y += row;
		}
		y = MAX(numbers_y, players_y);
	}
	y += CAMPAIGN_SCORE_CARD_GAP - 5.0f;
	if (draw)
		campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
	y += CAMPAIGN_SCORE_CARD_GAP;

	/* the team's score, and how it stands to the best on this map */
	{
		struct firefight_map_record best;
		char const *map_name = global_scenario_index != NONE ? tag_get_name(global_scenario_index) : "map";
		boolean record = firefight_stats_get_map_record(map_name, 0, &best);

		if (draw)
		{
			snprintf(value, sizeof(value), "%ld", team_score);
			campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
			campaign_score_panel_text(panel, "TEAM SCORE", NULL, left, right, y, 1.0f, _campaign_score_text_left, &color);
			campaign_score_card_color(&color, 1.0f, 0.84f, 0.35f);
			campaign_score_panel_text(panel, value, NULL, left, right, y, 1.0f, _campaign_score_text_right, &color);
			if (record && best.best_score > 0)
			{
				if (team_score >= best.best_score)
					snprintf(text, sizeof(text), "NEW RECORD!");
				else
				{
					snprintf(text, sizeof(text), "BEST %ld (SET %d RND %d WAVE %d)", best.best_score,
						(int)best.best_set, (int)best.best_round, (int)best.best_wave);
					campaign_score_card_color(&color, 0.6f, 0.66f, 0.75f);
				}
				campaign_score_panel_text(panel, text, NULL, left, right, y + (line - small) / 2.0f,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_center, &color);
			}
		}
		y += line + 4.0f;
	}

	/* the medals won, each with how many */
	if (report->medals > 0)
	{
		long bitmap_index = campaign_medals_bitmap();
		short per_row = (short)MAX((right - left) / CAMPAIGN_SCORE_CARD_MEDAL_SPACE, 1);
		real space = (right - left) / per_row;
		short shown = 0;

		y += CAMPAIGN_SCORE_CARD_GAP - 4.0f;
		if (draw)
			campaign_score_panel_quad(panel, left, y, right, y + 1.0f, CAMPAIGN_SCORE_CARD_RULE);
		y += CAMPAIGN_SCORE_CARD_GAP;
		for (index = 0; index < NUMBER_OF_CAMPAIGN_MEDALS && shown < per_row * 2; index++)
		{
			real x;

			if (report->medal_counts[index] <= 0)
				continue;
			if (shown > 0 && shown % per_row == 0)
				y += CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f;
			x = left + (shown % per_row) * space;
			shown++;
			if (draw)
			{
				struct bitmap_data *bitmap = bitmap_index != NONE ?
					bitmap_group_try_and_get_bitmap(bitmap_index, index) : NULL;

				if (bitmap)
				{
					rectangle2d bounds;

					bounds.x0 = (short)(panel->left + x * panel->scale);
					bounds.y0 = (short)(panel->top + y * panel->scale);
					bounds.x1 = (short)(panel->left + (x + CAMPAIGN_SCORE_CARD_MEDAL_SIZE) * panel->scale);
					bounds.y1 = (short)(panel->top + (y + CAMPAIGN_SCORE_CARD_MEDAL_SIZE) * panel->scale);
					draw_bitmap_in_rect(bitmap, &bounds, NULL, NULL, 0xFFFFFFFF, NULL, TRUE);
				}
				snprintf(value, sizeof(value), "x%d", (int)report->medal_counts[index]);
				campaign_score_card_color(&color, 0.95f, 0.95f, 0.95f);
				campaign_score_panel_text(panel, value, NULL, x + CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f,
					x + space, y + (CAMPAIGN_SCORE_CARD_MEDAL_SIZE - small) / 2.0f,
					CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_left, &color);
			}
		}
		y += CAMPAIGN_SCORE_CARD_MEDAL_SIZE + 4.0f;
	}

	return y + CAMPAIGN_SCORE_CARD_PADDING - 4.0f;
}

void campaign_score_firefight_report_draw(
	long const *players,
	short player_count)
{
	struct campaign_score_panel panel;
	struct font_header *font;
	real vx0 = (real)(-render.camera.viewport_bounds.x0);
	real vy0 = (real)(-render.camera.viewport_bounds.y0);
	real height;

	panel.font_index = hud_get_font_index();
	if (panel.font_index == NONE)
		return;
	font = font_definition_get(panel.font_index);
	panel.line_height = (short)(font->ascending_height + font->descending_height);
	if (panel.line_height <= 0)
		return;
	panel.scale = 1.0f;
	panel.width = CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH;
	/* (the match's numbers, once: and its XP and records, which the card's
	best score reads, kept as it is first shown) */
	if (!campaign_score_firefight_report_filled)
	{
		struct firefight_postgame_xp_summary xp;
		long local_player = local_player_get_player_index(0);

		firefight_stats_get_postgame_xp_summary(&xp);
		campaign_score_report_fill(&campaign_score_firefight_report,
			local_player != NONE ? (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(local_player) : NONE);
		campaign_score_level_title(campaign_score_firefight_report.level,
			NUMBEROF(campaign_score_firefight_report.level));
		campaign_score_firefight_report.difficulty = campaign_score_difficulty();
		campaign_score_firefight_report.difficulty_multiplier = campaign_score_difficulty_multiplier();
		campaign_score_firefight_report.skulls_multiplier = campaign_score_skulls_multiplier();
		campaign_score_firefight_report_filled = TRUE;
	}
	panel.left = vx0;
	panel.top = vy0;
	campaign_score_panel_quad(&panel, 0.0f, 0.0f, 640.0f, 480.0f, 0xFF000000);
	height = campaign_score_firefight_card_layout(&panel, &campaign_score_firefight_report, players, player_count, FALSE);
	panel.left = vx0 + (640.0f - CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH) / 2.0f;
	panel.top = vy0 + MAX(6.0f, (CAMPAIGN_SCORE_FIREFIGHT_CARD_BOTTOM - height) / 2.0f);
	campaign_score_panel_quad(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH, height, CAMPAIGN_SCORE_CARD_FILL);
	campaign_score_panel_frame(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_FIREFIGHT_CARD_WIDTH, height,
		CAMPAIGN_SCORE_PANEL_EDGE);
	campaign_score_firefight_card_layout(&panel, &campaign_score_firefight_report, players, player_count, TRUE);

	return;
}

void campaign_score_draw(
	void)
{
	struct campaign_score_panel panel;
	struct font_header *font;
	rectangle2d screen;
	real_argb_color color;
	real height;
	boolean down;
	struct campaign_score_report const *summary;

	if (cinematic_in_progress() || !game_in_progress() || campaign_score_summary_shown >= campaign_score_summary_count ||
		!campaign_score_summary[campaign_score_summary_shown].valid || !campaign_score_level())
	{
		return;
	}
	if (ae_end_card_wanted()) return; /* AE hook */
	/* (the next player's card, or the end, when A or B is pressed anew) */
	down = campaign_score_button_down();
	if (!campaign_score_button_primed)
		campaign_score_button_primed = TRUE;
	else if (down && !campaign_score_button_held)
		campaign_score_summary_shown++;
	campaign_score_button_held = down;
	if (campaign_score_summary_shown >= campaign_score_summary_count)
		return;
	summary = &campaign_score_summary[campaign_score_summary_shown];
	panel.font_index = hud_get_font_index();
	if (panel.font_index == NONE)
		return;
	font = font_definition_get(panel.font_index);
	panel.line_height = (short)(font->ascending_height + font->descending_height);
	panel.width = CAMPAIGN_SCORE_PANEL_WIDTH;
	if (panel.line_height <= 0)
		return;
	/* (the whole screen black: the fullscreen overlays' camera is the
	screen's, as cinematic_render's letterbox has it) */
	screen = render.camera.viewport_bounds;
	draw_quad(&screen, 0xFF000000);
	panel.scale = (real)(screen.y1 - screen.y0) / 480.0f;
	if (panel.scale <= 0.0f)
		return;
	height = campaign_score_card_layout(&panel, summary, FALSE);
	panel.left = screen.x0 + ((screen.x1 - screen.x0) - CAMPAIGN_SCORE_CARD_WIDTH * panel.scale) / 2.0f;
	panel.top = screen.y0 + ((screen.y1 - screen.y0) - (height + panel.line_height * 2.0f) * panel.scale) / 2.0f;
	campaign_score_panel_quad(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_CARD_WIDTH, height, CAMPAIGN_SCORE_CARD_FILL);
	campaign_score_panel_frame(&panel, 0.0f, 0.0f, CAMPAIGN_SCORE_CARD_WIDTH, height, CAMPAIGN_SCORE_PANEL_EDGE);
	campaign_score_card_layout(&panel, summary, TRUE);
	campaign_score_card_color(&color, 0.55f, 0.55f, 0.55f);
	campaign_score_panel_text(&panel, campaign_score_summary_shown + 1 < campaign_score_summary_count ?
		"PRESS A OR B FOR THE NEXT PLAYER" : "PRESS A OR B TO CONTINUE", NULL, 0.0f, CAMPAIGN_SCORE_CARD_WIDTH,
		height + panel.line_height, CAMPAIGN_SCORE_SUBLINE_SIZE, _campaign_score_text_center, &color);

	return;
}

#include "ae_pve_score_menus.inc" /* AE hook */
