/*
AE_STRIP_MODEL.C

The score strip's model (ae_strip_model.h): the engine's numbers in, the
strip's words, colour roles and bar fills out, for every mode (spec 11.6,
11.10, 11.10b; plan notes/plans/2026-10-12-score-strip.md Task 2).

Colours follow the mockups (mock4.py): labels grey, values white, names in
their side's colour; FFA's own lines ("YOU 3RD") in my colour and the other
side's ("LEADER bot3") white, whole.
*/

#include <string.h>

#include "ae_strip_model.h"

#define COUNT(a) ((long)(sizeof(a) / sizeof((a)[0])))

/* ---------- a text, built a part at a time */

static void text_clear(struct ae_strip_text *text)
{
	memset(text, 0, sizeof(*text));
	text->color = _ae_strip_color_grey;
}

static long text_length(const struct ae_strip_text *text)
{
	long length = 0;

	while (length < AE_STRIP_TEXT_LENGTH && text->text[length])
		length++;
	return length;
}

/* appends at most `count` characters of a string (stopping at its end), in a colour; the text is cut, never
overrun, and keeps its terminator */
static void put_some(struct ae_strip_text *text, const wchar_t *string, long count, enum ae_strip_color color)
{
	long length = text_length(text);
	long i;

	for (i = 0; string && i < count && string[i] && length < AE_STRIP_TEXT_LENGTH - 1; i++, length++)
	{
		text->text[length] = string[i];
		text->colors[length] = (unsigned char)color;
		if (length == 0)
			text->color = color;
	}
	text->text[length] = 0;
}

static void put(struct ae_strip_text *text, const wchar_t *string, enum ae_strip_color color)
{
	put_some(text, string, AE_STRIP_TEXT_LENGTH, color);
}

/* a player's name: at most its array's characters, terminated or not */
static void put_name(struct ae_strip_text *text, const wchar_t name[AE_STRIP_NAME_LENGTH], enum ae_strip_color color)
{
	put_some(text, name, AE_STRIP_NAME_LENGTH - 1, color);
}

static void format_long(long value, wchar_t *out, long out_count)
{
	wchar_t digits[24];
	unsigned long magnitude;
	long count = 0, length = 0;

	if (out_count <= 0)
		return;
	magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1 : (unsigned long)value;
	do
	{
		digits[count++] = (wchar_t)(L'0' + (int)(magnitude % 10));
		magnitude /= 10;
	} while (magnitude && count < COUNT(digits));
	if (value < 0 && length < out_count - 1)
		out[length++] = L'-';
	while (count > 0 && length < out_count - 1)
		out[length++] = digits[--count];
	out[length] = 0;
}

static void put_long(struct ae_strip_text *text, long value, enum ae_strip_color color)
{
	wchar_t digits[24];

	format_long(value, digits, COUNT(digits));
	put(text, digits, color);
}

static void put_time(struct ae_strip_text *text, long seconds, enum ae_strip_color color)
{
	wchar_t time[24];

	ae_strip_format_time(seconds, time, COUNT(time));
	put(text, time, color);
}

/* ---------- formats */

void ae_strip_format_time(long seconds, wchar_t *out, long out_count)
{
	wchar_t minutes[24];
	wchar_t time[32];
	long length = 0, i;

	if (out_count <= 0)
		return;
	if (seconds < 0)
		seconds = 0;
	format_long(seconds / 60, minutes, COUNT(minutes));
	for (i = 0; minutes[i]; i++)
		time[length++] = minutes[i];
	time[length++] = L':';
	time[length++] = (wchar_t)(L'0' + (int)(seconds % 60 / 10));
	time[length++] = (wchar_t)(L'0' + (int)(seconds % 10));
	time[length] = 0;
	for (i = 0; time[i] && i < out_count - 1; i++)
		out[i] = time[i];
	out[i] = 0;
}

void ae_strip_format_place(long place, wchar_t *out, long out_count)
{
	const wchar_t *suffix = L"TH";
	wchar_t place_text[32];
	long length, i;

	if (out_count <= 0)
		return;
	if (place % 100 < 11 || place % 100 > 13)
	{
		if (place % 10 == 1)
			suffix = L"ST";
		else if (place % 10 == 2)
			suffix = L"ND";
		else if (place % 10 == 3)
			suffix = L"RD";
	}
	format_long(place, place_text, 24);
	for (length = 0; place_text[length]; length++)
		;
	for (i = 0; suffix[i]; i++)
		place_text[length++] = suffix[i];
	place_text[length] = 0;
	for (i = 0; place_text[i] && i < out_count - 1; i++)
		out[i] = place_text[i];
	out[i] = 0;
}

/* ---------- the rules */

static enum ae_strip_color team_color(long team_index)
{
	return team_index == 0 ? _ae_strip_color_team_red : _ae_strip_color_team_blue;
}

static real fill(long score, long limit)
{
	real value;

	if (limit <= 0 || score <= 0)
		return 0.0f;
	value = (real)score / (real)limit;
	return value > 1.0f ? 1.0f : value;
}

static long clamp(long value, long low, long high)
{
	return value < low ? low : value > high ? high : value;
}

/* the colour of the context line's player: mine or the other side's (white in FFA) */
static enum ae_strip_color context_color(const struct ae_strip_snapshot *snapshot, enum ae_strip_color left,
	enum ae_strip_color right)
{
	return snapshot->context_side == _ae_strip_side_me ? left : right;
}

/* CTF: one team's own flag under its score. Carried: the carrier's name in the carrier's (the other team's)
colour */
static void build_flag(const struct ae_strip_side_snapshot *side, enum ae_strip_color carrier_color,
	struct ae_strip_text *text)
{
	if (side->flag == _ae_flag_carried)
		put_name(text, side->flag_carrier, carrier_color);
	else if (side->flag == _ae_flag_dropped)
	{
		put(text, L"DROPPED ", _ae_strip_color_grey);
		put_time(text, side->flag_return_seconds, _ae_strip_color_white);
	}
	else if (side->flag == _ae_flag_home)
		put(text, L"HOME", _ae_strip_color_grey);
}

/* FFA: "YOU 3RD" in my colour; "LEADER bot3" (or "2ND bot1" when I lead) white */
static void build_ffa_subs(const struct ae_strip_snapshot *snapshot, struct ae_strip_model *model)
{
	wchar_t place[24];

	ae_strip_format_place(snapshot->me.place > 0 ? snapshot->me.place : 1, place, COUNT(place));
	put(&model->left_sub, L"YOU ", _ae_strip_color_me);
	put(&model->left_sub, place, _ae_strip_color_me);
	put(&model->right_sub, snapshot->i_lead ? L"2ND " : L"LEADER ", _ae_strip_color_white);
	put_name(&model->right_sub, snapshot->other.name, _ae_strip_color_white);
}

static void build_hill(const struct ae_strip_snapshot *snapshot, enum ae_strip_color left, enum ae_strip_color right,
	struct ae_strip_text *text)
{
	put(text, L"HILL ", _ae_strip_color_grey);
	switch (snapshot->hill)
	{
	case _ae_hill_yours: put(text, L"YOURS", left); break;
	case _ae_hill_contested: put(text, L"CONTESTED", _ae_strip_color_white); break;
	case _ae_hill_theirs: put(text, L"THEIRS", right); break;
	default: put(text, L"EMPTY", _ae_strip_color_white); break;
	}
	if (snapshot->hill_move_seconds >= 0)
	{
		put(text, L"  MOVES IN ", _ae_strip_color_grey);
		put_time(text, snapshot->hill_move_seconds, _ae_strip_color_white);
	}
}

/* race and rally (11.10b R2): my place as the hero, the leader (the second when I lead) by name; no bars */
static void build_race(const struct ae_strip_snapshot *snapshot, enum ae_strip_color left,
	struct ae_strip_model *model)
{
	wchar_t place[24];
	const wchar_t *other_label = snapshot->i_lead ? L"2ND " : L"LEADER ";

	ae_strip_format_place(snapshot->me.place > 0 ? snapshot->me.place : 1, place, COUNT(place));
	text_clear(&model->left_score);
	put(&model->left_score, place, left);
	text_clear(&model->right_score);
	put_name(&model->right_score, snapshot->other.name, _ae_strip_color_white);
	if (snapshot->mode == _ae_strip_race)
	{
		put(&model->left_sub, L"LAP ", _ae_strip_color_grey);
		put_long(&model->left_sub, snapshot->me.score, _ae_strip_color_white);
		if (snapshot->laps > 0)
		{
			put(&model->left_sub, L" OF ", _ae_strip_color_grey);
			put_long(&model->left_sub, snapshot->laps, _ae_strip_color_grey);
		}
		put(&model->right_sub, other_label, _ae_strip_color_grey);
		put(&model->right_sub, L"LAP ", _ae_strip_color_grey);
		put_long(&model->right_sub, snapshot->other.score, _ae_strip_color_white);
		put(&model->context, L"CHECKPOINT ", _ae_strip_color_grey);
		put_long(&model->context, snapshot->checkpoint, _ae_strip_color_white);
		if (snapshot->checkpoints > 0)
		{
			put(&model->context, L" OF ", _ae_strip_color_grey);
			put_long(&model->context, snapshot->checkpoints, _ae_strip_color_grey);
		}
	}
	else
	{
		put(&model->left_sub, L"POINTS ", _ae_strip_color_grey);
		put_long(&model->left_sub, snapshot->me.score, _ae_strip_color_white);
		if (snapshot->score_to_win > 0)
		{
			put(&model->left_sub, L" OF ", _ae_strip_color_grey);
			put_long(&model->left_sub, snapshot->score_to_win, _ae_strip_color_grey);
		}
		put(&model->right_sub, other_label, _ae_strip_color_grey);
		put_long(&model->right_sub, snapshot->other.score, _ae_strip_color_white);
		put(&model->right_sub, L" POINTS", _ae_strip_color_grey);
		put(&model->context, L"NEXT FLAG", _ae_strip_color_grey);
		if (snapshot->next_flag_meters >= 0)
		{
			put(&model->context, L" ", _ae_strip_color_grey);
			put_long(&model->context, snapshot->next_flag_meters, _ae_strip_color_white);
			put(&model->context, L" M", _ae_strip_color_white);
		}
	}
}

static void build_elimination(const struct ae_strip_snapshot *snapshot, struct ae_strip_text *text)
{
	if (snapshot->lives_left == 1)
		put(text, L"LAST LIFE", _ae_strip_color_white);
	else if (snapshot->lives_left > 1)
	{
		put(text, L"LIVES ", _ae_strip_color_grey);
		put_long(text, snapshot->lives_left, _ae_strip_color_white);
	}
	if (snapshot->players_total > 0)
	{
		if (text->text[0])
			put(text, L"  ", _ae_strip_color_grey);
		put_long(text, snapshot->players_left, _ae_strip_color_white);
		put(text, L" OF ", _ae_strip_color_grey);
		put_long(text, snapshot->players_total, _ae_strip_color_grey);
		put(text, L" LEFT", _ae_strip_color_grey);
	}
}

static void build_rounds(const struct ae_strip_snapshot *snapshot, struct ae_strip_model *model)
{
	model->pips_total = snapshot->rounds_to_win > 0 ? snapshot->rounds_to_win : 0;
	model->left_pips_lit = clamp(snapshot->me.rounds_won, 0, model->pips_total);
	model->right_pips_lit = clamp(snapshot->other.rounds_won, 0, model->pips_total);
	/* (decided per match: both sides cells, or both sides text) */
	if (snapshot->me.players <= 4 && snapshot->other.players <= 4)
	{
		model->round_cells = TRUE;
		model->left_cells = clamp(snapshot->me.players, 0, 4);
		model->left_lit = clamp(snapshot->me.alive, 0, model->left_cells);
		model->right_cells = clamp(snapshot->other.players, 0, 4);
		model->right_lit = clamp(snapshot->other.alive, 0, model->right_cells);
	}
	else
	{
		put_long(&model->left_alive, clamp(snapshot->me.alive, 0, snapshot->me.players), _ae_strip_color_white);
		put(&model->left_alive, L" ALIVE", _ae_strip_color_white);
		put_long(&model->right_alive, clamp(snapshot->other.alive, 0, snapshot->other.players),
			_ae_strip_color_white);
		put(&model->right_alive, L" ALIVE", _ae_strip_color_white);
	}
	/* (Bomb: the planted bomb, else the viewer's job this round) */
	if (snapshot->bomb_planted)
	{
		enum ae_strip_color planters = snapshot->bomb_planted_side == _ae_strip_side_me ?
			(snapshot->teams ? team_color(snapshot->me.team_index) : _ae_strip_color_me) :
			snapshot->teams ? team_color(snapshot->other.team_index) : _ae_strip_color_white;

		enum ae_strip_color defenders = snapshot->bomb_planted_side == _ae_strip_side_me ?
			(snapshot->teams ? team_color(snapshot->other.team_index) : _ae_strip_color_white) :
			snapshot->teams ? team_color(snapshot->me.team_index) : _ae_strip_color_me;

		put(&model->context, L"BOMB ", _ae_strip_color_grey);
		if (snapshot->bomb_defusing)
			put(&model->context, L"DEFUSING", defenders);
		else
			put(&model->context, L"PLANTED", planters);
	}
	else if (snapshot->bomb_carrying)
		put(&model->context, L"YOU HAVE THE BOMB",
			snapshot->teams ? team_color(snapshot->me.team_index) : _ae_strip_color_me);
	else if (snapshot->bomb_role != _ae_bomb_role_none)
		put(&model->context, snapshot->bomb_role == _ae_bomb_role_attack ? L"ATTACK" : L"DEFEND",
			snapshot->teams ? team_color(snapshot->me.team_index) : _ae_strip_color_me);
	else if (snapshot->respawns_left >= 0)
	{
		put(&model->context, L"RESPAWNS ", _ae_strip_color_grey);
		put_long(&model->context, snapshot->respawns_left, _ae_strip_color_white);
	}
}

void ae_strip_model_build(const struct ae_strip_snapshot *snapshot, struct ae_strip_model *model)
{
	enum ae_strip_color left, right;
	boolean timed, race;
	long left_value, right_value;

	if (!snapshot || !model)
		return;
	memset(model, 0, sizeof(*model));
	text_clear(&model->left_score); text_clear(&model->right_score); text_clear(&model->clock);
	text_clear(&model->context); text_clear(&model->lead); text_clear(&model->goal);
	text_clear(&model->left_sub); text_clear(&model->right_sub);
	text_clear(&model->left_alive); text_clear(&model->right_alive); text_clear(&model->played);

	left = snapshot->teams ? team_color(snapshot->me.team_index) : _ae_strip_color_me;
	right = snapshot->teams ? team_color(snapshot->other.team_index) : _ae_strip_color_white;
	timed = (boolean)(snapshot->timed || snapshot->mode == _ae_strip_oddball || snapshot->mode == _ae_strip_king ||
		snapshot->mode == _ae_strip_accumulation);
	race = (boolean)(snapshot->mode == _ae_strip_race || snapshot->mode == _ae_strip_rally);
	model->marker_left = TRUE;

	/* the clock: neutral, or a grey label in timed and race modes; nothing on NHE's maps */
	model->clock_small = (boolean)(timed || race);
	model->clock.color = model->clock_small ? _ae_strip_color_grey : _ae_strip_color_clock;
	if (!snapshot->clock_hidden)
		put_time(&model->clock, snapshot->clock_seconds, model->clock.color);
	/* MATCH CLOCK BOTH: the time played over the time left, as the corner clock has it: a grey line like the goal's,
	the value alone (an "ELAPSED" label ran into the my-side marker in small views); never without the clock */
	if (!snapshot->clock_hidden && snapshot->played_shown)
		put_time(&model->played, snapshot->played_seconds, _ae_strip_color_grey);

	/* the scores: rounds won, times, or numbers */
	left_value = snapshot->mode == _ae_strip_rounds ? snapshot->me.rounds_won : snapshot->me.score;
	right_value = snapshot->mode == _ae_strip_rounds ? snapshot->other.rounds_won : snapshot->other.score;
	model->left_score.color = left;
	model->right_score.color = right;
	if (timed)
	{
		put_time(&model->left_score, left_value, left);
		put_time(&model->right_score, right_value, right);
	}
	else
	{
		put_long(&model->left_score, left_value, left);
		put_long(&model->right_score, right_value, right);
	}

	/* bars and the goal line: only with a limit; never in race, rally or rounds (cells there) */
	if (snapshot->score_to_win > 0 && !race && snapshot->mode != _ae_strip_rounds)
	{
		model->bars = TRUE;
		model->left_fill = fill(snapshot->me.score, snapshot->score_to_win);
		model->right_fill = fill(snapshot->other.score, snapshot->score_to_win);
		put(&model->goal, L"TO ", _ae_strip_color_grey);
		if (timed)
			put_time(&model->goal, snapshot->score_to_win, _ae_strip_color_grey);
		else
			put_long(&model->goal, snapshot->score_to_win, _ae_strip_color_grey);
	}

	if (!snapshot->teams && !race)
		build_ffa_subs(snapshot, model);

	switch (snapshot->mode)
	{
	case _ae_strip_ctf:
		model->flag_glyphs = TRUE;
		build_flag(&snapshot->me, right, &model->left_sub);
		build_flag(&snapshot->other, left, &model->right_sub);
		break;
	case _ae_strip_oddball:
		if (snapshot->context_name[0])
		{
			put(&model->context, L"BALL ", _ae_strip_color_grey);
			put_name(&model->context, snapshot->context_name, context_color(snapshot, left, right));
		}
		if (snapshot->teams)
		{
			put(&model->left_sub, L"YOU ", _ae_strip_color_grey);
			put_time(&model->left_sub, snapshot->own_score, _ae_strip_color_white);
		}
		break;
	case _ae_strip_king:
		build_hill(snapshot, left, right, &model->context);
		break;
	case _ae_strip_race:
	case _ae_strip_rally:
		build_race(snapshot, left, model);
		break;
	case _ae_strip_juggernaut:
		if (snapshot->context_side == _ae_strip_side_me)
			put(&model->context, L"YOU ARE THE JUGGERNAUT", left);
		else if (snapshot->context_name[0])
		{
			put(&model->context, L"JUGGERNAUT ", _ae_strip_color_grey);
			put_name(&model->context, snapshot->context_name, right);
		}
		break;
	case _ae_strip_accumulation:
		if (snapshot->reverse_tag)
		{
			if (snapshot->context_name[0])
			{
				put(&model->context, L"IT ", _ae_strip_color_grey);
				put_name(&model->context, snapshot->context_name, context_color(snapshot, left, right));
			}
		}
		else if (snapshot->balls_total > 0)
		{
			put(&model->context, L"YOU HOLD ", _ae_strip_color_grey);
			put_long(&model->context, snapshot->balls_held, left);
			put(&model->context, L" OF ", _ae_strip_color_grey);
			put_long(&model->context, snapshot->balls_total, _ae_strip_color_grey);
		}
		break;
	case _ae_strip_elimination:
		build_elimination(snapshot, &model->context);
		break;
	case _ae_strip_phantoms:
		if (snapshot->context_name[0])
		{
			put(&model->context, L"TARGET ", _ae_strip_color_grey);
			put_name(&model->context, snapshot->context_name, context_color(snapshot, left, right));
		}
		break;
	case _ae_strip_rounds:
		build_rounds(snapshot, model);
		break;
	default:
		break;
	}

	/* the lead: over the clock when nothing else is; never in timed or race modes */
	if (!timed && !race && !model->context.text[0])
	{
		long difference = left_value - right_value;

		if (difference == 0)
			put(&model->lead, L"TIED", _ae_strip_color_grey);
		else
		{
			enum ae_strip_color leader = difference > 0 ? left : right;

			put(&model->lead, L"+", leader);
			put_long(&model->lead, difference > 0 ? difference : -difference, leader);
		}
	}
}
