/*
AE_GLUE_POSTGAME.C

The AE post-game's contact with the game (ae_glue_postgame.h).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "ae_glue_pause.h"
#include "ae_glue_postgame.h"
#include "ae_header_lines.h"
#include "ae_scoreboard_header.h"
#include "ae_hooks.h"
#include "ae_postgame.h"
#include "ae_scoreboard.h"
#include "ae_strip_model.h"
#include "../src/ae_draw.h"

void platform_log(char const *format, ...);
/* game_engine.c's ranking (populate_statistic_buffer, _postgame_statistic_ranking 0): ae_scoreboard_stat's layout */
long populate_statistic_buffer(struct ae_scoreboard_stat *statistic_buffer, long statistic, boolean inverse);
#define RANKING 0
#define PLACE_TIED 0x80000000UL
#define MAXIMUM_RANKED 128 /* MULTIPLAYER_MAXIMUM_PLAYERS (halo_port_limits.h) */

/* the AE menus' colours: the teams (the gallery's and the scoreboard's), the players (SETTINGS' and the pause's) */
#define TEAM_RED 0xC0392BFFu
#define TEAM_BLUE 0x2E86DEFFu
#define TIE_COLOR 0xEEF3F6FFu      /* AE_COLOR_TITLE */
#define TIE_BANNER 0x10161CF2u     /* AE_COLOR_VIEW_PANEL */
#define WHITE 0xFFFFFFFFu
static unsigned int const player_colors[AE_POSTGAME_SEATS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

/* the game's time when it was last played (ticks), for TIME PLAYED */
static long played_ticks = 0;

/* ---------- colours */

static float channel_luminance(unsigned int byte)
{
	float c = (float)byte / 255.0f;

	return c <= 0.03928f ? c / 12.92f : (float)pow((c + 0.055f) / 1.055f, 2.4f);
}

static float luminance(unsigned int rgba)
{
	return 0.2126f * channel_luminance((rgba >> 24) & 0xFF) + 0.7152f * channel_luminance((rgba >> 16) & 0xFF) +
		0.0722f * channel_luminance((rgba >> 8) & 0xFF);
}

static unsigned int rgba_of(real red, real green, real blue)
{
	return ((unsigned int)(PIN(red, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) |
		((unsigned int)(PIN(green, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
		((unsigned int)(PIN(blue, 0.0f, 1.0f) * 255.0f + 0.5f) << 8) | 0xFF;
}

/* a colour mixed toward white until it reads on the post-game's dark screen (3:1 for its large words, with room) */
static unsigned int readable(unsigned int rgba)
{
	short step;

	for (step = 0; step < 10 && luminance(rgba) < 0.16f; step++)
	{
		unsigned int r = (rgba >> 24) & 0xFF, g = (rgba >> 16) & 0xFF, b = (rgba >> 8) & 0xFF;

		r += (255 - r) / 4;
		g += (255 - g) / 4;
		b += (255 - b) / 4;
		rgba = (r << 24) | (g << 16) | (b << 8) | 0xFF;
	}
	return rgba;
}

static unsigned int armour_color(long player_index)
{
	union real_rgb_color armour;

	if (player_index != NONE && game_engine_player_get_change_color(&armour, player_index))
		return rgba_of(armour.red, armour.green, armour.blue);
	return TIE_COLOR;
}

/* ---------- text */

static void narrow(wchar_t const *wide, char *text, int size)
{
	int at;

	for (at = 0; at < size - 1 && wide && wide[at]; at++)
		text[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
	text[at] = 0;
}

static void upper(char *text)
{
	for (; *text; text++)
	{
		if (*text >= 'a' && *text <= 'z')
			*text = (char)(*text - 'a' + 'A');
	}
}

static void place_text(long place, char *text, int size)
{
	wchar_t wide[16];

	ae_strip_format_place(place + 1, wide, NUMBEROF(wide));
	narrow(wide, text, size);
}

/* a score as the game counts it: a time for the timed modes (oddball, king, a timed race's), else a number */
/* a place spelled out (owner 10-25, title mockup C): "FIRST" .. "ONE HUNDRED TWENTY-EIGHTH" (place from 0) */
static void place_words(long place, char *text, int size)
{
	static const char *const units[20] = { "", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
		"TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN" };
	static const char *const unit_ordinals[20] = { "", "FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH", "SIXTH",
		"SEVENTH", "EIGHTH", "NINTH", "TENTH", "ELEVENTH", "TWELFTH", "THIRTEENTH", "FOURTEENTH", "FIFTEENTH",
		"SIXTEENTH", "SEVENTEENTH", "EIGHTEENTH", "NINETEENTH" };
	static const char *const tens[10] = { "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY",
		"NINETY" };
	static const char *const ten_ordinals[10] = { "", "", "TWENTIETH", "THIRTIETH", "FORTIETH", "FIFTIETH", "SIXTIETH",
		"SEVENTIETH", "EIGHTIETH", "NINETIETH" };
	long n = PIN(place + 1, 1, 999), rest = n % 100;
	char low[32];

	if (rest == 0)
		snprintf(low, sizeof(low), "%s", "");
	else if (rest < 20)
		snprintf(low, sizeof(low), "%s", unit_ordinals[rest]);
	else if (rest % 10 == 0)
		snprintf(low, sizeof(low), "%s", ten_ordinals[rest / 10]);
	else
		snprintf(low, sizeof(low), "%s-%s", tens[rest / 10], unit_ordinals[rest % 10]);
	if (n < 100)
		snprintf(text, size, "%s", low);
	else if (rest == 0)
		snprintf(text, size, "%s HUNDREDTH", units[n / 100]);
	else
		snprintf(text, size, "%s HUNDRED %s", units[n / 100], low);
}

static void score_text(long score, char *text, int size)
{
	struct game_variant *variant = game_engine_get_variant();
	short index = variant ? variant->game_engine_index : NONE;

	if (index == game_engine_oddball || index == game_engine_king)
	{
		long seconds = MAX(score, 0) / TICKS_PER_SECOND;

		snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
	}
	else
		snprintf(text, size, "%ld", score);
}

static void player_name(long player_index, char *text, int size)
{
	wchar_t buffer[48];
	const wchar_t *name = ae_game_engine_scoreboard_name(player_index, buffer, NUMBEROF(buffer));

	narrow(name, text, size);
	if (!text[0])
		snprintf(text, size, "Player");
}

/* ---------- facts */

/* the winning team (0 red, 1 blue), NONE for a tie: the engine's own call (game_engine_did_player_win) on a player of
each team in turn */
static long winning_team(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long won;

		if (player->team_index < 0 || player->team_index > 1)
			continue;
		won = game_engine_did_player_win(iterator.datum_index);
		if (won == NONE)
			return NONE;
		return won ? player->team_index : 1 - player->team_index;
	}
	return NONE;
}

static void team_score_text(long team, char *text, int size)
{
	score_text(game_engine_get_team_score(team), text, size);
}

void ae_glue_postgame_facts(struct ae_postgame_facts *facts)
{
	static struct ae_scoreboard_stat ranked[MAXIMUM_RANKED];
	struct game_variant *variant = game_engine_get_variant();
	char const *path = main_get_multiplayer_map_name();
	wchar_t map[48];
	long count, winner = NONE, i;
	short local;
	long seconds;

	memset(facts, 0, sizeof(*facts));
	if (!game_engine_running() || !variant)
		return;
	facts->teams = game_engine_has_teams() ? 1 : 0;
	facts->host = global_network_game_server_get() != NULL;
	facts->can_continue = game_engine_showing_postgame() ? 1 : 0;
	if (!path || !path[0])
		path = main_get_map_name();
	ae_scoreboard_map_name(path, map, NUMBEROF(map));
	narrow(map, facts->map, sizeof(facts->map));
	upper(facts->map);
	ae_glue_pause_gametype(facts->gametype, sizeof(facts->gametype));
	seconds = played_ticks / TICKS_PER_SECOND;
	snprintf(facts->played, sizeof(facts->played), "%ld:%02ld", seconds / 60, seconds % 60);
	count = populate_statistic_buffer(ranked, RANKING, FALSE);
	facts->players = (short)MIN(count, 0x7FFF);

	/* the title and the banner: the winning team, or FFA's winner */
	if (facts->teams)
	{
		char red[16], blue[16];

		winner = winning_team();
		team_score_text(0, red, sizeof(red));
		team_score_text(1, blue, sizeof(blue));
		if (winner == NONE)
		{
			snprintf(facts->banner, sizeof(facts->banner), "TIE GAME");
			facts->title_color = TIE_COLOR;
			facts->banner_color = TIE_BANNER;
			snprintf(facts->banner_score, sizeof(facts->banner_score), "%s - %s", red, blue);
		}
		else
		{
			snprintf(facts->banner, sizeof(facts->banner), "%s TEAM WINS", winner == 0 ? "RED" : "BLUE");
			facts->title_color = facts->banner_color = winner == 0 ? TEAM_RED : TEAM_BLUE;
			snprintf(facts->banner_score, sizeof(facts->banner_score), "%s - %s", winner == 0 ? red : blue,
				winner == 0 ? blue : red);
		}
	}
	else if (count > 0)
	{
		char name[48];
		long score = game_engine->get_player_score ? game_engine->get_player_score(ranked[0].player_index, FALSE) : 0;

		player_name(ranked[0].player_index, name, sizeof(name));
		upper(name);
		if (count > 1 && (ranked[0].place & PLACE_TIED))
		{
			snprintf(facts->banner, sizeof(facts->banner), "TIE FOR 1ST");
			facts->banner_color = TIE_BANNER;
			facts->title_color = TIE_COLOR;
		}
		else
		{
			snprintf(facts->banner, sizeof(facts->banner), "%s WINS", name);
			facts->banner_color = armour_color(ranked[0].player_index);
			facts->title_color = readable(facts->banner_color);
		}
		score_text(score, facts->banner_score, sizeof(facts->banner_score));
	}
	/* (white words on the banner; a light armour colour, where they would not read and the banner would look like the
	focus's white bar: the dark panel, the colour its stripe) */
	facts->banner_text_color = WHITE;
	if (luminance(facts->banner_color) > 0.3f)
	{
		facts->banner_stripe = facts->banner_color;
		facts->banner_color = TIE_BANNER;
	}
	/* (and a dark one, black armour, lost on the dark screen: the panel, its stripe the colour lightened) */
	else if (luminance(facts->banner_color) < 0.04f)
	{
		facts->banner_stripe = readable(facts->banner_color);
		facts->banner_color = TIE_BANNER;
	}

	/* each local player's own result */
	for (local = 0; local < AE_POSTGAME_SEATS; local++)
	{
		struct ae_postgame_seat *seat = &facts->seats[local];
		long player_index = local_player_exists(local) ? local_player_get_player_index(local) : NONE;
		struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
		long place = NONE;
		boolean tied = FALSE;

		if (!player)
			continue;
		for (i = 0; i < count; i++)
		{
			if (ranked[i].player_index == player_index)
			{
				place = (long)(ranked[i].place & ~PLACE_TIED);
				tied = (ranked[i].place & PLACE_TIED) != 0;
			}
		}
		seat->present = 1;
		seat->slot = (short)(local + 1);
		player_name(player_index, seat->name, sizeof(seat->name));
		seat->kills = player->statistics.kills[0];
		seat->deaths = player->statistics.deaths;
		seat->assists = player->statistics.assists[0];
		seat->spree = ae_postgame_best_spree(player_index);
		if (facts->teams)
		{
			long team = PIN(player->team_index, 0, 1);
			char mine[16], theirs[16];

			seat->chip = seat->stripe = team == 0 ? TEAM_RED : TEAM_BLUE;
			seat->title_color = facts->title_color;
			snprintf(seat->team, sizeof(seat->team), "%s TEAM", team == 0 ? "RED" : "BLUE");
			snprintf(seat->title, sizeof(seat->title), "%s", facts->banner);
			team_score_text(team, mine, sizeof(mine));
			team_score_text(1 - team, theirs, sizeof(theirs));
			snprintf(seat->result_sub, sizeof(seat->result_sub), "%s Team %s, %s Team %s", team == 0 ? "Red" : "Blue",
				mine, team == 0 ? "Blue" : "Red", theirs);
			seat->result = winner == NONE ? AE_POSTGAME_TIE : winner == team ? AE_POSTGAME_WIN : AE_POSTGAME_LOSS;
			snprintf(seat->result_text, sizeof(seat->result_text), "%s", seat->result == AE_POSTGAME_TIE ? "TIE" :
				seat->result == AE_POSTGAME_WIN ? "WIN" : "LOSS");
		}
		else
		{
			char where[16];

			seat->chip = player_colors[local];
			seat->title_color = seat->stripe = readable(armour_color(player_index));
			seat->result = AE_POSTGAME_PLACE;
			place_text(place == NONE ? count : place, where, sizeof(where));
			snprintf(seat->result_text, sizeof(seat->result_text), "%s", where);
			/* (spelled out, as the owner picked; a tie keeps its short form: "TIED 2ND PLACE") */
			if (tied)
				snprintf(seat->title, sizeof(seat->title), "TIED %s PLACE", where);
			else
			{
				char words[40];

				place_words(place == NONE ? count - 1 : place, words, sizeof(words));
				snprintf(seat->title, sizeof(seat->title), "%s PLACE", words);
			}
			snprintf(seat->result_sub, sizeof(seat->result_sub), "%sof %ld player%s", tied ? "tied, " : "", count,
				count == 1 ? "" : "s");
		}
	}
}

void ae_glue_postgame_card(short local_player, struct ae_pause_card *card)
{
	short row;

	ae_glue_pause_card(local_player, card);
	for (row = 0; row < card->rows; row++)
	{
		if (!strcmp(card->names[row], "RED") || !strcmp(card->names[row], "BLUE"))
			strncat(card->names[row], " TEAM", sizeof(card->names[row]) - strlen(card->names[row]) - 1);
	}
}

/* the board's lists' width (the mockup's: wider than the game's 900 under its banner) */
#define BOARD_WIDTH 1000.0f

void ae_glue_postgame_board(float layout_width, float layout_height, float top, float *bottom, float *x, float *width)
{
	static struct ae_scoreboard_stat ranked[MAXIMUM_RANKED];
	struct ae_scoreboard_options options;
	long count = populate_statistic_buffer(ranked, RANKING, FALSE);
	long viewer = NONE;
	short local;
	real board_x, board_w;

	for (local = 0; local < AE_POSTGAME_SEATS && viewer == NONE; local++)
	{
		if (local_player_exists(local))
			viewer = local_player_get_player_index(local);
	}
	csmemset(&options, 0, sizeof(options));
	options.full_screen = TRUE;
	options.all_local_me = TRUE;
	options.no_title = TRUE;
	options.best_spree = viewer != NONE && ae_postgame_best_spree(viewer) != NONE;
	options.fit_all = TRUE;
	options.lists_top = top;
	options.width = BOARD_WIDTH;
	options.scrim_alpha = 1.0f;
	options.layer_width = layout_width;
	options.layer_height = layout_height;
	ae_scoreboard_draw_with(viewer, 1.0f, ranked, count, NONE, 0, 0, &options);
	ae_scoreboard_postgame_place(layout_width, layout_height, options.side, BOARD_WIDTH, &board_x, &board_w);
	*bottom = options.bottom;
	*x = board_x;
	*width = board_w;
}

int ae_glue_postgame_view(short local_player, short *view, short *views, short *spans_two_columns)
{
	short index = 0, at;

	*view = 0;
	*views = local_player_count();
	*spans_two_columns = 0;
	if (!local_player_exists(local_player))
		return 0;
	if (*views <= 1)
	{
		*views = 1;
		return 1;
	}
	for (at = local_player_get_next(NONE); at != NONE && at != local_player; at = local_player_get_next(at))
		index++;
	*view = index;
	*spans_two_columns = *views == 3 && index == 0;
	return 1;
}

int ae_glue_postgame_due(void)
{
	return ae_menus_active() && ae_draw_available() && game_engine_running() && !main_menu_is_active() &&
		game_engine_force_single_screen();
}

void ae_glue_postgame_track(void)
{
	if (game_engine_running() && game_engine_can_score() && !main_menu_is_active())
		played_ticks = game_time_get();
}

int ae_glue_postgame_continue(void)
{
	struct network_game_server *server = global_network_game_server_get();

	if (!server || !game_engine_showing_postgame())
		return 0;
	network_game_server_reset_to_pregame(server);
	platform_log("ae postgame: continue: the server back to the pregame");
	return 1;
}

void ae_glue_postgame_leave(void)
{
	platform_log("ae postgame: leaving the game");
	network_game_abort();
}
