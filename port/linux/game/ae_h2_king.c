/* The second-generation game's King of the Hill settings and TERRITORIES (see the header).
TERRITORIES: every hill the count asks for is in play at once, neutral at the start. A team standing in a territory
alone for CONTROL TIME (neutral) or CONTEST TIME (an enemy's) takes it, straight to that team; a territory is neutral
only before its first capture. Both teams in it: the progress holds and it scores nothing. The owner alone in it takes
the attacker's progress back at the speed it was made; nobody in it, the progress is lost. A team holding two of the
territories scores a tick of score each tick, all three two (the King engine's score is in ticks: SCORE TO WIN's
minutes are seconds held). */
#include "cseries.h"
#include "game/game_engine.h"
#include "game/game_engine_king.h"
#include "game/players.h"
#include "memory/data.h"
#include "ae_variant_ext.h"
#include "ae_h2_king.h"

#include <string.h>

boolean network_game_distributed_client(void);
void platform_log(char const *format, ...);
short hud_nav_point_index(char const *name);
short ae_king_hill_count(void);
long ae_king_hill_id(short hill_index);
boolean ae_king_player_in_hill(long player_index, short hill_index);
/* (game_engine_multiplayer_sounds.c's enum multiplayer_information_sound) */
enum { AE_SOUND_HILL_CONTESTED = 39, AE_SOUND_HILL_CONTROLLED = 40 };

static signed char owners[AE_H2_TERRITORIES_MAXIMUM];
static signed char capturing[AE_H2_TERRITORIES_MAXIMUM];
static long progress[AE_H2_TERRITORIES_MAXIMUM];
static long owned_hill_ids[AE_H2_TERRITORIES_MAXIMUM];
static long log_ticks;
static signed char contested[AE_H2_TERRITORIES_MAXIMUM];
static long majority_team_scores[2];

int ae_h2_territories_on(void)
{
	struct game_variant const *variant = game_engine_get_variant();

	return variant && variant->game_engine_index == game_engine_king && variant->universal_variant.teams &&
		ae_variant_ext_game()->mode == AE_EXT_MODE_TERRITORIES;
}

long ae_h2_king_hills_wanted(long multi_hill_wanted, long map_hills, long maximum)
{
	if (ae_h2_territories_on())
	{
		long count = ae_ext_territories_count(ae_variant_ext_game(), (int)map_hills);
		static long logged = -1;

		if (logged != map_hills * 100 + count)
			platform_log("ae territories: %ld of the map's %ld hills in play", count, map_hills), logged = map_hills * 100 + count;

		return count < 1 ? 1 : count > maximum ? maximum : count;
	}
	return multi_hill_wanted;
}

int ae_h2_king_moving(int moving_hill)
{
	if (ae_h2_territories_on())
		return 0;
	return ae_variant_ext_game()->king_move_10s ? 1 : moving_hill;
}

long ae_h2_king_move_ticks(long default_ticks)
{
	long seconds = ae_ext_king_move_seconds(ae_variant_ext_game());

	return seconds > 0 ? seconds * TICKS_PER_SECOND : default_ticks;
}

long ae_h2_king_score_step(int contested)
{
	if (ae_h2_territories_on())
		return 0;
	if (contested && (ae_variant_ext_game()->king_flags & AE_EXT_KING_UNCONTESTED))
		return 0;
	return 1;
}

int ae_h2_king_every_player_scores(void)
{
	return !ae_h2_territories_on() && (ae_variant_ext_game()->king_flags & AE_EXT_KING_TEAM_TIME_MULTIPLIER) != 0;
}

void ae_h2_territories_reset(void)
{
	short index;

	log_ticks = 0;
	for (index = 0; index < AE_H2_TERRITORIES_MAXIMUM; index++)
	{
		owners[index] = -1;
		capturing[index] = -1;
		progress[index] = 0;
		contested[index] = 0;
		owned_hill_ids[index] = NONE;
	}
}

int ae_h2_territories_tick(short hill_count, long const *hill_ids, long const (*counts)[2], long *team_scores,
	long score_to_win)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	long control = ae_ext_control_seconds(ext) * TICKS_PER_SECOND;
	long contest = ae_ext_contest_seconds(ext) * TICKS_PER_SECOND;
	long scoring[2] = { 0, 0 };
	short hill;
	int won = 0, team;

	if (!ae_h2_territories_on() || network_game_distributed_client())
		return 0;
	for (hill = 0; hill < hill_count && hill < AE_H2_TERRITORIES_MAXIMUM; hill++)
	{
		long red = counts[hill][0], blue = counts[hill][1];
		signed char alone = red && !blue ? 0 : blue && !red ? 1 : -1;

		/* (a hill the engine put in another place: a new territory) */
		if (owned_hill_ids[hill] != hill_ids[hill])
		{
			owned_hill_ids[hill] = hill_ids[hill];
			owners[hill] = capturing[hill] = -1;
			progress[hill] = 0;
		}
		contested[hill] = red && blue;
		if (!red && !blue)
			progress[hill] = 0, capturing[hill] = -1;
		else if (red && blue)
		{
			/* (both teams in it: the progress holds, and the territory scores nothing) */
		}
		else if (alone != owners[hill])
		{
			/* (a neutral territory or an enemy's goes straight to the team standing alone in it) */
			if (capturing[hill] != alone)
				capturing[hill] = alone, progress[hill] = 0;
			if (++progress[hill] >= (owners[hill] < 0 ? control : contest))
			{
				owners[hill] = alone;
				platform_log("ae territories: territory %d (hill %ld) taken by team %d; scores %ld %ld", hill, hill_ids[hill],
					alone, team_scores[0], team_scores[1]);
				progress[hill] = 0;
				capturing[hill] = -1;
				game_engine_play_multiplayer_sound(AE_SOUND_HILL_CONTROLLED);
			}
		}
		else if (progress[hill] > 0)
		{
			/* (the owner alone in it takes the attacker's progress back at the speed it was made) */
			if (--progress[hill] <= 0)
				progress[hill] = 0, capturing[hill] = -1;
		}
		if (owners[hill] >= 0 && !contested[hill])
			scoring[owners[hill]]++;
	}
	/* (a team scores when it holds a majority of the territories in play (more than half: one of one, two of three, three
	of five), one point a second, and two when it holds them all; a contested territory is not counted) */
	{
		long in_play = hill_count < AE_H2_TERRITORIES_MAXIMUM ? hill_count : AE_H2_TERRITORIES_MAXIMUM;

		for (team = 0; team < 2; team++)
		{
			long points = in_play > 0 && scoring[team] * 2 > in_play ? (scoring[team] == in_play ? 2 : 1) : 0;

			team_scores[team] += points;
			if (points && team_scores[team] >= score_to_win)
				won = 1;
			majority_team_scores[team] = points;
		}
	}
	return won;
}

long ae_h2_territories_hill_state(short hill_index, long engine_state)
{
	if (!ae_h2_territories_on() || hill_index < 0 || hill_index >= AE_H2_TERRITORIES_MAXIMUM)
		return engine_state;
	return owners[hill_index] == 0 ? king_hill_controlled_red :
		owners[hill_index] == 1 ? king_hill_controlled_blue : king_hill_uncontrolled;
}

long ae_h2_territories_write(unsigned char *buffer, long size, short hill_count, long const *hill_ids)
{
	struct ae_h2_territories_network_state state;
	short hill;

	if (!ae_h2_territories_on() || size < (long)sizeof(state))
		return 0;
	memset(&state, 0, sizeof(state));
	state.magic = AE_H2_TERRITORIES_MAGIC;
	state.count = hill_count > AE_H2_TERRITORIES_MAXIMUM ? AE_H2_TERRITORIES_MAXIMUM : hill_count;
	for (hill = 0; hill < AE_H2_TERRITORIES_MAXIMUM; hill++)
	{
		state.hill_ids[hill] = hill < state.count ? hill_ids[hill] : NONE;
		state.owners[hill] = hill < state.count ? owners[hill] : -1;
	}
	memcpy(buffer, &state, sizeof(state));
	return sizeof(state);
}

int ae_h2_territories_read(unsigned char const *buffer, long size)
{
	struct ae_h2_territories_network_state state;
	short hill;

	if (size != (long)sizeof(state))
		return 0;
	memcpy(&state, buffer, sizeof(state));
	if (state.magic != AE_H2_TERRITORIES_MAGIC || state.count < 0 || state.count > AE_H2_TERRITORIES_MAXIMUM)
		return 0;
	for (hill = 0; hill < AE_H2_TERRITORIES_MAXIMUM; hill++)
	{
		owners[hill] = hill < state.count && state.owners[hill] >= -1 && state.owners[hill] <= 1 ? state.owners[hill] : -1;
		owned_hill_ids[hill] = hill < state.count ? state.hill_ids[hill] : NONE;
	}
	return 1;
}

int ae_h2_territories_engine_tick(long *team_scores, long score_to_win)
{
	long counts[AE_H2_TERRITORIES_MAXIMUM][2];
	long ids[AE_H2_TERRITORIES_MAXIMUM];
	short hill_count = ae_king_hill_count(), hill;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!ae_h2_territories_on() || network_game_distributed_client())
		return 0;
	if (hill_count > AE_H2_TERRITORIES_MAXIMUM)
		hill_count = AE_H2_TERRITORIES_MAXIMUM;
	memset(counts, 0, sizeof(counts));
	for (hill = 0; hill < hill_count; hill++)
		ids[hill] = ae_king_hill_id(hill);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == NONE || player->team_index < 0 || player->team_index > 1)
			continue;
		for (hill = 0; hill < hill_count; hill++)
		{
			if (ae_king_player_in_hill(iterator.datum_index, hill))
				counts[hill][player->team_index]++;
		}
	}
	/* (the telemetry: every 10 s of game time, who owns what) */
	if (++log_ticks >= 10 * TICKS_PER_SECOND)
	{
		long red, blue, neutral, total;

		log_ticks = 0;
		ae_h2_territories_held(&red, &blue, &neutral, &total);
		platform_log("ae territories: owners r/b/n %ld/%ld/%ld of %ld; scores %ld %ld; owner by territory %d %d %d; "
			"majority %s; contested %d%d%d; progress %ld %ld %ld", red, blue, neutral, total, team_scores[0], team_scores[1],
			(int)owners[0], (int)owners[1], (int)owners[2],
			majority_team_scores[0] > 0 ? "r" : majority_team_scores[1] > 0 ? "b" : "none",
			(int)contested[0], (int)contested[1], (int)contested[2], progress[0], progress[1], progress[2]);
	}
	return ae_h2_territories_tick(hill_count, ids, (long const (*)[2])counts, team_scores, score_to_win);
}

long ae_h2_territories_hill_id(short hill_index)
{
	return hill_index >= 0 && hill_index < AE_H2_TERRITORIES_MAXIMUM ? owned_hill_ids[hill_index] : NONE;
}

/* the territories' owners as counts: red, blue, neutral of those in play (0: not a TERRITORIES game) */
int ae_h2_territories_held(long *red, long *blue, long *neutral, long *total)
{
	short hill_count = ae_king_hill_count(), hill;

	*red = *blue = *neutral = *total = 0;
	if (!ae_h2_territories_on())
		return 0;
	if (hill_count > AE_H2_TERRITORIES_MAXIMUM)
		hill_count = AE_H2_TERRITORIES_MAXIMUM;
	for (hill = 0; hill < hill_count; hill++)
	{
		if (!game_engine_get_goal_in_use(hill))
			continue;
		(*total)++;
		if (owners[hill] == 0)
			(*red)++;
		else if (owners[hill] == 1)
			(*blue)++;
		else
			(*neutral)++;
	}
	return *total > 0;
}

/* a territory's waypoint: the game's own crown arrows, red or blue by owner, the plain arrow while neutral; the hill's
own (blue) crown outside TERRITORIES. A map's HUD globals may lack a name (stock king only ever placed "crown_blue"):
the first of the names the map has, so find_nav_point never gets one it lacks */
char const *ae_h2_territories_icon(short hill_index)
{
	char const *name;

	if (!ae_h2_territories_on() || hill_index < 0 || hill_index >= AE_H2_TERRITORIES_MAXIMUM)
		name = "crown_blue";
	else
		name = owners[hill_index] == 0 ? "crown_red" : owners[hill_index] == 1 ? "crown_blue" : "default"; /* (drawn white: ae_h2_territories_nav_tint) */
	if (hud_nav_point_index(name) == NONE)
	{
		if (hud_nav_point_index("crown_red") != NONE)
			name = "crown_red";
		else if (hud_nav_point_index("crown_blue") != NONE)
			name = "crown_blue";
		else
			name = "default"; /* none at all: NONE, drawn as no nav point */
	}
	return name;
}

/* the waypoint being drawn is a neutral territory's: its arrow white (the game's arrows are red or blue only) */
static int nav_neutral;

void ae_h2_territories_nav_begin(long goal_index)
{
	nav_neutral = goal_index >= 0 && goal_index < AE_H2_TERRITORIES_MAXIMUM && ae_h2_territories_on() &&
		owners[goal_index] < 0;
}

void ae_h2_territories_nav_tint(real_rgb_color *color)
{
	if (nav_neutral)
		color->red = color->green = color->blue = 1.0f;
}

/* a bot's territory: the team's players take the hills its team does not hold, one each in the hills' order (the team
rank of a player is its index among the team's), the ones beyond them hold what the team has; with every one held
they spread over them all. in_target: the bot stands in its territory */
int ae_h2_territories_bot_target(long team_index, long player_index, real_point3d const *from, real_point3d *target,
	int *in_target)
{
	static short assigned[32];
	short hill_count = ae_king_hill_count(), hill;
	short list[AE_H2_TERRITORIES_MAXIMUM], list_count = 0;
	long rank = 0;
	short pick;
	struct data_iterator iterator;
	struct player_datum *player;

	if (!ae_h2_territories_on() || team_index < 0 || team_index > 1 || !from || player_index == NONE)
		return 0;
	if (hill_count > AE_H2_TERRITORIES_MAXIMUM)
		hill_count = AE_H2_TERRITORIES_MAXIMUM;
	for (hill = 0; hill < hill_count; hill++)
	{
		if (game_engine_get_goal_in_use(hill) && owners[hill] != (signed char)team_index)
			list[list_count++] = hill;
	}
	for (hill = 0; hill < hill_count; hill++)
	{
		if (game_engine_get_goal_in_use(hill) && owners[hill] == (signed char)team_index)
			list[list_count++] = hill;
	}
	if (list_count == 0)
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->team_index == team_index && iterator.datum_index < player_index)
			rank++;
	}
	pick = list[rank % list_count];
	if (assigned[player_index & 31] != pick + 1)
	{
		assigned[player_index & 31] = pick + 1;
		platform_log("ae territories: player %ld (team %ld) goes for territory %d (%s)", player_index & 0xFFFF, team_index,
			pick, owners[pick] < 0 ? "neutral" : owners[pick] == team_index ? "held, defends" : "enemy's");
	}
	game_engine_get_goal_position(target, pick);
	/* (the marker's height: the ground under it) */
	target->z -= 0.63f;
	if (in_target)
		*in_target = ae_king_player_in_hill(player_index, pick);
	return 1;
}
