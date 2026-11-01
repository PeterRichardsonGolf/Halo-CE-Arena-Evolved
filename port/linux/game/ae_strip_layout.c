/*
AE_STRIP_LAYOUT.C

The score strip's layout (ae_strip_layout.h): one bottom line, the gap rule,
every mode. Plan notes/plans/2026-10-12-score-strip.md Task 3.

The strip is laid out around the HUD area's centre at a scale k (the view
factor s, or less when the gap rule needs it). Its lowest drawn line (the
sub-lines, else the pips, else the goal line, else the scores) sits on the
shared bottom line, the motion sensor's "15m" baseline (11.12); "TO n" joins
the sub-lines' baseline when both are drawn (gate correction G5). Its right end
keeps 48 x s from the timers list: the bars shrink toward 60 first, then the
whole strip scales (never under half of s).
*/

#include <math.h>
#include <string.h>

#include "ae_strip_layout.h"

/* ---------- sizes */

real ae_strip_text_px(real base, real scale, real s)
{
	real floor = s < AE_STRIP_QUARTER_VIEW_FACTOR ? AE_STRIP_QUARTER_TEXT_FLOOR : AE_STRIP_TEXT_FLOOR;
	real px = base * scale;

	return px < floor ? floor : px;
}

real ae_strip_digits_px(real base, real scale)
{
	real px = base * scale;

	return px < AE_STRIP_DIGITS_FLOOR ? AE_STRIP_DIGITS_FLOOR : px;
}

static real maximum(real a, real b) { return a > b ? a : b; }

static boolean has_text(const struct ae_strip_text *text) { return (boolean)(text->text[0] != 0); }

static long text_count(const wchar_t *text)
{
	long count = 0;

	while (count < AE_STRIP_TEXT_LENGTH && text[count])
		count++;
	return count;
}

/* a score drawn in counter digits ("17", "1:12", "-2"), or a place ("2ND": digits, then letters), or a name */
static long digits_prefix(const wchar_t *text)
{
	long count = 0;

	while (count < AE_STRIP_TEXT_LENGTH && text[count] &&
		((text[count] >= L'0' && text[count] <= L'9') || text[count] == L':' || text[count] == L'-' ||
		text[count] == L'+'))
		count++;
	return count;
}

/* ---------- measuring at a scale */

struct measures {
	const struct ae_strip_model *model;
	real s;
	real (*digits)(const wchar_t *text, real cap_px);
	real (*text)(const wchar_t *text, real px);
};

static real measure_text(const struct measures *m, const struct ae_strip_text *text, real base, real k)
{
	return has_text(text) ? m->text(text->text, ae_strip_text_px(base, k, m->s)) : 0.0f;
}

/* a score's width: counter digits, then (race) the place's letters 2 px after them, or a name */
static real measure_score(const struct measures *m, const struct ae_strip_text *score, real k)
{
	wchar_t digits[AE_STRIP_TEXT_LENGTH];
	long count = digits_prefix(score->text), length = text_count(score->text);
	real width = 0.0f;

	if (!length)
		return 0.0f;
	if (count == 0)
		return m->text(score->text, ae_strip_text_px(AE_STRIP_NAME_PX, k, m->s));
	/* (room for the terminator, should a text fill its array unterminated) */
	if (count > AE_STRIP_TEXT_LENGTH - 1)
		count = AE_STRIP_TEXT_LENGTH - 1;
	memcpy(digits, score->text, (size_t)count * sizeof(wchar_t));
	digits[count] = 0;
	width = m->digits(digits, ae_strip_digits_px(AE_STRIP_SCORE_CAP, k));
	if (count < length)
		width += 2.0f * k + m->text(score->text + count, ae_strip_text_px(AE_STRIP_SUFFIX_PX, k, m->s));
	return width;
}

static boolean score_is_name(const struct ae_strip_text *score)
{
	return (boolean)(has_text(score) && digits_prefix(score->text) == 0);
}

/* the clock cell: the width of "0:00", or "00:00" past 9:59; none when the clock is hidden */
static real measure_clock(const struct measures *m, real k)
{
	const wchar_t *cell = text_count(m->model->clock.text) >= 5 ? L"00:00" : L"0:00";

	if (!has_text(&m->model->clock))
		return 0.0f;
	if (m->model->clock_small)
		return m->text(cell, ae_strip_text_px(AE_STRIP_SMALL_CLOCK_PX, k, m->s));
	return m->digits(cell, ae_strip_digits_px(AE_STRIP_CLOCK_CAP, k));
}

/* ---------- one side's extent from the centre */

struct side_widths { real score, sub, alive; long cells; boolean pips; };

static real side_extent(const struct measures *m, const struct side_widths *side, real inner, real bar, real k)
{
	const struct ae_strip_model *model = m->model;
	real outer = inner + side->score;
	real extent = outer;

	if (model->bars)
		extent = maximum(extent, outer + (AE_STRIP_BAR_GAP + bar) * k);
	if (model->round_cells && side->cells > 0)
		extent = maximum(extent, outer + (AE_STRIP_BAR_GAP + AE_STRIP_CELL_WIDTH * (real)side->cells) * k);
	if (side->alive > 0.0f)
		extent = maximum(extent, outer + AE_STRIP_BAR_GAP * k + side->alive);
	if (side->sub > 0.0f)
		extent = maximum(extent, side->pips ? inner + side->score / 2.0f + side->sub / 2.0f : inner + side->sub);
	return extent;
}

struct extents { real inner, clock, left, right, half; struct side_widths l, r; };

static void measure_all(const struct measures *m, real bar, real k, struct extents *e)
{
	const struct ae_strip_model *model = m->model;
	real centred;

	e->clock = measure_clock(m, k);
	e->inner = e->clock / 2.0f + AE_STRIP_GAP_TO_CLOCK * k;
	e->l.score = measure_score(m, &model->left_score, k);
	e->r.score = measure_score(m, &model->right_score, k);
	e->l.sub = measure_text(m, &model->left_sub, AE_STRIP_SUB_PX, k);
	e->r.sub = measure_text(m, &model->right_sub, AE_STRIP_SUB_PX, k);
	if (model->flag_glyphs)
	{
		real glyph = (AE_STRIP_FLAG_GLYPH + AE_STRIP_FLAG_GAP) * ae_strip_text_px(AE_STRIP_SUB_PX, k, m->s);

		if (e->l.sub > 0.0f)
			e->l.sub += glyph;
		if (e->r.sub > 0.0f)
			e->r.sub += glyph;
	}
	e->l.alive = measure_text(m, &model->left_alive, AE_STRIP_ALIVE_PX, k);
	e->r.alive = measure_text(m, &model->right_alive, AE_STRIP_ALIVE_PX, k);
	e->l.cells = model->left_cells;
	e->r.cells = model->right_cells;
	e->l.pips = e->r.pips = (boolean)(model->pips_total > 0);
	e->left = side_extent(m, &e->l, e->inner, bar, k);
	e->right = side_extent(m, &e->r, e->inner, bar, k);
	centred = maximum(maximum(measure_text(m, &model->context, AE_STRIP_CONTEXT_PX, k),
		measure_text(m, &model->lead, AE_STRIP_CONTEXT_PX, k)), measure_text(m, &model->goal, AE_STRIP_GOAL_PX, k));
	centred = maximum(centred, measure_text(m, &model->played, AE_STRIP_PLAYED_PX, k));
	centred = maximum(centred, e->clock) / 2.0f;
	e->left = maximum(e->left, centred);
	e->right = maximum(e->right, centred);
	e->half = maximum(e->left, e->right);
}

/* the gap rule: the largest bar (150 -> 60) at the view factor, then the largest scale, that keep the strip's
half width within the room; never under AE_STRIP_MINIMUM_SCALE of the view factor */
static void fit(const struct measures *m, real room, real *bar_out, real *k_out)
{
	struct extents e;
	real s = m->s, bar = AE_STRIP_BAR_LENGTH, low, high;
	int step;

	measure_all(m, bar, s, &e);
	if (e.half <= room)
	{
		*bar_out = bar; *k_out = s;
		return;
	}
	if (m->model->bars)
	{
		/* (the bars are the only part that shrinks: as long as the rest fits) */
		measure_all(m, 0.0f, s, &e);
		bar = (room - (e.inner + maximum(e.l.score, e.r.score) + AE_STRIP_BAR_GAP * s)) / s;
		bar = bar > AE_STRIP_BAR_LENGTH ? AE_STRIP_BAR_LENGTH : bar < AE_STRIP_BAR_MINIMUM ? AE_STRIP_BAR_MINIMUM : bar;
		measure_all(m, bar, s, &e);
		if (e.half <= room + 0.001f)
		{
			*bar_out = bar; *k_out = s;
			return;
		}
		bar = AE_STRIP_BAR_MINIMUM;
	}
	low = s * AE_STRIP_MINIMUM_SCALE;
	high = s;
	measure_all(m, bar, low, &e);
	if (e.half > room)
	{
		*bar_out = bar; *k_out = low;
		return;
	}
	/* (halved until a 2000th of the scale: under half a pixel on a strip a thousand pixels wide, in about 10 steps,
	each measuring every text: this runs every frame while the gap rule holds) */
	for (step = 0; step < 40 && high - low > s * 0.0005f; step++)
	{
		real middle = (low + high) / 2.0f;

		measure_all(m, bar, middle, &e);
		if (e.half <= room)
			low = middle;
		else
			high = middle;
	}
	*bar_out = bar; *k_out = low;
}

/* ---------- boxes */

static void text_box(struct ae_strip_box *box, real x, real baseline, real w, real px, boolean visible)
{
	box->x = x; box->y = baseline - px; box->w = w; box->h = px; box->visible = visible;
}

static void span_box(struct ae_strip_box *box, real x, real bottom, real w, real h, boolean visible)
{
	box->x = x; box->y = bottom - h; box->w = w; box->h = h; box->visible = visible;
}

static void lift_over_marker(struct ae_strip_box *line, const struct ae_strip_box *marker, real k)
{
	real lowest = marker->y - AE_STRIP_MARKER_CLEAR * k;

	real side = AE_STRIP_MARKER_SIDE_CLEAR * k;

	/* (a line reaching within the clearance's 8 px of the marker's sides counts: "BOMB PLANTED" ended 2 px off it) */
	if (line->visible && marker->visible && line->x < marker->x + marker->w + side && marker->x - side < line->x + line->w &&
		line->y + line->h > lowest)
		line->y = lowest - line->h;
}

void ae_strip_layout_compute(const struct ae_strip_model *model, const struct ae_strip_geometry *geometry,
	real (*measure_digits)(const wchar_t *text, real cap_px), real (*measure_text_function)(const wchar_t *text,
	real px), struct ae_strip_layout *layout)
{
	struct measures m;
	struct extents e;
	real k, bar, room, centre, base, inner, lowest, sub_drop, goal_drop, px, w, clock_px, context_y;
	boolean subs, pips;

	if (!layout)
		return;
	memset(layout, 0, sizeof(*layout));
	if (!model || !geometry || !measure_digits || !measure_text_function)
		return;
	m.model = model;
	m.s = geometry->s > 0.0f ? geometry->s : 1.0f;
	m.digits = measure_digits;
	m.text = measure_text_function;
	centre = geometry->centre_x;

	room = geometry->list_left_x - AE_STRIP_LIST_GAP * m.s - centre;
	fit(&m, room, &bar, &k);
	measure_all(&m, bar, k, &e);
	layout->strip_scale = k;
	layout->bar_length = model->bars ? bar * k : 0.0f;
	inner = e.inner;

	/* the lowest line on the shared line: sub-lines (under pips: lower), else pips, else the goal */
	subs = (boolean)(has_text(&model->left_sub) || has_text(&model->right_sub));
	pips = (boolean)(model->pips_total > 0);
	sub_drop = pips ? AE_STRIP_SUB_DROP_PIPS : AE_STRIP_SUB_DROP;
	goal_drop = subs ? sub_drop : model->clock_small ? AE_STRIP_TIMED_GOAL_DROP : AE_STRIP_GOAL_DROP;
	lowest = 0.0f;
	if (has_text(&model->goal))
		lowest = maximum(lowest, goal_drop);
	if (pips)
		lowest = maximum(lowest, AE_STRIP_PIPS_DROP);
	if (subs)
		lowest = maximum(lowest, sub_drop);
	base = geometry->shared_line_y - lowest * k;
	layout->score_baseline_y = base;
	layout->clock_baseline_y = model->clock_small ? base - AE_STRIP_TIMED_CLOCK_RISE * k : base;

	/* the clock, the scores */
	clock_px = model->clock_small ? ae_strip_text_px(AE_STRIP_SMALL_CLOCK_PX, k, m.s) :
		ae_strip_digits_px(AE_STRIP_CLOCK_CAP, k);
	text_box(&layout->clock, centre - e.clock / 2.0f, layout->clock_baseline_y, e.clock, clock_px,
		has_text(&model->clock));
	px = score_is_name(&model->left_score) ? ae_strip_text_px(AE_STRIP_NAME_PX, k, m.s) :
		ae_strip_digits_px(AE_STRIP_SCORE_CAP, k);
	text_box(&layout->left_score, centre - inner - e.l.score, base, e.l.score, px, has_text(&model->left_score));
	px = score_is_name(&model->right_score) ? ae_strip_text_px(AE_STRIP_NAME_PX, k, m.s) :
		ae_strip_digits_px(AE_STRIP_SCORE_CAP, k);
	text_box(&layout->right_score, centre + inner, base, e.r.score, px, has_text(&model->right_score));

	/* MATCH CLOCK BOTH: the time played over the clock, as the corner clock has it; the context / lead line clears
	it */
	context_y = base - AE_STRIP_CONTEXT_RISE * k;
	px = ae_strip_text_px(AE_STRIP_PLAYED_PX, k, m.s);
	w = measure_text(&m, &model->played, AE_STRIP_PLAYED_PX, k);
	text_box(&layout->played, centre - w / 2.0f, (model->clock_small ? layout->clock_baseline_y - AE_STRIP_TEXT_CAP *
		clock_px : layout->clock.y) - AE_STRIP_PLAYED_CLOCK_GAP * k, w, px, has_text(&model->played));
	if (layout->played.visible)
	{
		real context_px = ae_strip_text_px(AE_STRIP_CONTEXT_PX, k, m.s);
		real highest = layout->played.y + layout->played.h - AE_STRIP_TEXT_CAP * px - AE_STRIP_PLAYED_CONTEXT_GAP * k -
			AE_STRIP_TEXT_DESCENT * context_px;

		if (context_y > highest)
			context_y = highest;
	}

	/* the lines over and under the clock */
	px = ae_strip_text_px(AE_STRIP_CONTEXT_PX, k, m.s);
	w = measure_text(&m, &model->context, AE_STRIP_CONTEXT_PX, k);
	text_box(&layout->context, centre - w / 2.0f, context_y, w, px, has_text(&model->context));
	w = measure_text(&m, &model->lead, AE_STRIP_CONTEXT_PX, k);
	text_box(&layout->lead, centre - w / 2.0f, context_y, w, px, has_text(&model->lead));
	w = measure_text(&m, &model->goal, AE_STRIP_GOAL_PX, k);
	text_box(&layout->goal, centre - w / 2.0f, base + goal_drop * k, w, ae_strip_text_px(AE_STRIP_GOAL_PX, k, m.s),
		has_text(&model->goal));

	/* sub-lines: from each score's inner edge outward; under pips, centred under the score */
	px = ae_strip_text_px(AE_STRIP_SUB_PX, k, m.s);
	text_box(&layout->left_sub, pips ? layout->left_score.x + (e.l.score - e.l.sub) / 2.0f : centre - inner - e.l.sub,
		base + sub_drop * k, e.l.sub, px, has_text(&model->left_sub));
	text_box(&layout->right_sub, pips ? layout->right_score.x + (e.r.score - e.r.sub) / 2.0f : centre + inner,
		base + sub_drop * k, e.r.sub, px, has_text(&model->right_sub));

	/* bars, cells, "ALIVE": 16 beyond each score */
	span_box(&layout->left_bar, layout->left_score.x - AE_STRIP_BAR_GAP * k - layout->bar_length,
		base - AE_STRIP_BAR_BOTTOM * k, layout->bar_length, AE_STRIP_BAR_HEIGHT * k, model->bars);
	span_box(&layout->right_bar, layout->right_score.x + e.r.score + AE_STRIP_BAR_GAP * k,
		base - AE_STRIP_BAR_BOTTOM * k, layout->bar_length, AE_STRIP_BAR_HEIGHT * k, model->bars);
	w = AE_STRIP_CELL_WIDTH * (real)model->left_cells * k;
	span_box(&layout->left_cells, layout->left_score.x - AE_STRIP_BAR_GAP * k - w, base - AE_STRIP_BAR_BOTTOM * k, w,
		AE_STRIP_BAR_HEIGHT * k, (boolean)(model->round_cells && model->left_cells > 0));
	w = AE_STRIP_CELL_WIDTH * (real)model->right_cells * k;
	span_box(&layout->right_cells, layout->right_score.x + e.r.score + AE_STRIP_BAR_GAP * k,
		base - AE_STRIP_BAR_BOTTOM * k, w, AE_STRIP_BAR_HEIGHT * k,
		(boolean)(model->round_cells && model->right_cells > 0));
	px = ae_strip_text_px(AE_STRIP_ALIVE_PX, k, m.s);
	text_box(&layout->left_alive, layout->left_score.x - AE_STRIP_BAR_GAP * k - e.l.alive,
		base - AE_STRIP_ALIVE_DROP * k, e.l.alive, px, has_text(&model->left_alive));
	text_box(&layout->right_alive, layout->right_score.x + e.r.score + AE_STRIP_BAR_GAP * k,
		base - AE_STRIP_ALIVE_DROP * k, e.r.alive, px, has_text(&model->right_alive));

	/* the my-side marker: its tip 31 above the baseline, over my score's middle */
	span_box(&layout->marker, layout->left_score.x + e.l.score / 2.0f - AE_STRIP_MARKER_WIDTH * k / 2.0f,
		base - AE_STRIP_MARKER_RISE * k, AE_STRIP_MARKER_WIDTH * k, AE_STRIP_MARKER_HEIGHT * k,
		(boolean)(model->marker_left && has_text(&model->left_score)));
	/* a wide context / lead line ("YOU ARE THE JUGGERNAUT") reaches over the marker: lift it clear (the narrow ones
	stay 40 above the baseline) */
	lift_over_marker(&layout->context, &layout->marker, k);
	lift_over_marker(&layout->lead, &layout->marker, k);

	/* pips: rounds to win, centred under each score */
	w = pips ? (AE_STRIP_PIP_PITCH * (real)(model->pips_total - 1) + AE_STRIP_PIP_WIDTH) * k : 0.0f;
	span_box(&layout->left_pips, layout->left_score.x + (e.l.score - w) / 2.0f, base + AE_STRIP_PIPS_DROP * k, w,
		AE_STRIP_PIP_HEIGHT * k, pips);
	span_box(&layout->right_pips, layout->right_score.x + (e.r.score - w) / 2.0f, base + AE_STRIP_PIPS_DROP * k, w,
		AE_STRIP_PIP_HEIGHT * k, pips);

	layout->left_end_x = centre - e.left;
	layout->right_end_x = centre + e.right;
	/* the backing: an ellipse past both ends, centred 6 above the baseline (mock4.py backing) */
	layout->backing_cx = centre;
	layout->backing_cy = base - 6.0f * k;
	layout->backing_rx = e.half + 40.0f * k;
	layout->backing_ry = 46.0f * k;
}

/* ---------- FFA's YOU colour */

static real channel_linear(real c)
{
	return c <= 0.04045f ? c / 12.92f : (real)pow((c + 0.055f) / 1.055f, 2.4f);
}

static real relative_luminance(real r, real g, real b)
{
	return 0.2126f * channel_linear(r) + 0.7152f * channel_linear(g) + 0.0722f * channel_linear(b);
}

static real hue_channel(real p, real q, real t)
{
	if (t < 0.0f)
		t += 1.0f;
	if (t > 1.0f)
		t -= 1.0f;
	if (t < 1.0f / 6.0f)
		return p + (q - p) * 6.0f * t;
	if (t < 0.5f)
		return q;
	if (t < 2.0f / 3.0f)
		return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
	return p;
}

static void from_hsl(real h, real s, real l, struct ae_strip_rgb *out)
{
	real q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
	real p = 2.0f * l - q;

	out->red = hue_channel(p, q, h + 1.0f / 3.0f);
	out->green = hue_channel(p, q, h);
	out->blue = hue_channel(p, q, h - 1.0f / 3.0f);
}

boolean ae_score_strip_me_color(const struct ae_strip_rgb *armour, struct ae_strip_rgb *out)
{
	real r, g, b, high, low, l, s, h, d, low_l, high_l;
	int step;

	out->red = 229.0f / 255.0f;
	out->green = 240.0f / 255.0f;
	out->blue = 1.0f;
	if (!armour)
		return FALSE;
	r = armour->red < 0.0f ? 0.0f : armour->red > 1.0f ? 1.0f : armour->red;
	g = armour->green < 0.0f ? 0.0f : armour->green > 1.0f ? 1.0f : armour->green;
	b = armour->blue < 0.0f ? 0.0f : armour->blue > 1.0f ? 1.0f : armour->blue;
	high = r > g ? (r > b ? r : b) : (g > b ? g : b);
	low = r < g ? (r < b ? r : b) : (g < b ? g : b);
	l = (high + low) / 2.0f;
	d = high - low;
	s = d <= 0.0f ? 0.0f : d / (1.0f - fabsf(2.0f * l - 1.0f));
	if (s < 0.2f || l < 0.08f || l > 0.9f)
		return FALSE;
	if (relative_luminance(r, g, b) >= 0.33f)
	{
		out->red = r;
		out->green = g;
		out->blue = b;
		return TRUE;
	}
	if (high == r)
		h = (g - b) / d / 6.0f;
	else if (high == g)
		h = ((b - r) / d + 2.0f) / 6.0f;
	else
		h = ((r - g) / d + 4.0f) / 6.0f;
	if (h < 0.0f)
		h += 1.0f;
	/* (the lightness that just reaches the rule, no further than 0.9) */
	low_l = l;
	high_l = 0.9f;
	for (step = 0; step < 24; step++)
	{
		struct ae_strip_rgb probe;
		real middle = (low_l + high_l) / 2.0f;

		from_hsl(h, s, middle, &probe);
		if (relative_luminance(probe.red, probe.green, probe.blue) >= 0.33f)
			high_l = middle;
		else
			low_l = middle;
	}
	from_hsl(h, s, high_l, out);
	return TRUE;
}

