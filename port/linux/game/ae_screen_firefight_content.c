/*
AE_SCREEN_FIREFIGHT_CONTENT.C

See ae_screen_firefight_content.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen), the map's counts in the panel at the right.
A choice is written to the Firefight copy (the content byte) and kept for the map by the engine's own file; an objectives setting is
written through the one allowed writer, which accepts Firefight's keys alone (ae_firefight_menus_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_firefight_menus_rules.h"
#include "ae_glue_firefight.h"
#include "ae_glue_setup_options.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_firefight_content.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
int config_write(char const *name, char const *value);

enum { CONTENT_HIT = 0x7B20, TAGS_HIT = 0x7B24, LOADOUT_HIT = 0x7B28, OBJECTIVES_HIT = 0x7B2C };

/* the map MAP CONTENT is for */
static char map_name[128], map_caption[64];

/* ---------- the one writer of Firefight's settings */

static void setting_write(const char *key, const char *value)
{
	if (!ae_ffm_key_allowed(key))
	{
		platform_log("ae firefight: %s is not one of Firefight's menu settings: not written", key ? key : "(none)");
		return;
	}
	config_write(key, value);
}

static int setting_on(const char *key)
{
	return config_boolean(key);
}

/* ---------- the map's content: what the copy holds, kept for the map */

static unsigned char content_now(void)
{
	return ae_setup_edit_firefight_content();
}

static void content_set(unsigned char content)
{
	ae_setup_edit_firefight_content_set(content);
	ae_ffg_content_save(map_name, content);
}

static int loadout_is_set(void)
{
	return ae_ffg_loadout_get(map_name, 0) != NULL || ae_ffg_loadout_get(map_name, 1) != NULL;
}

static int health_is_changed(void)
{
	unsigned char rules[3];

	ae_ffg_health_get(map_name, rules);
	return rules[0] || rules[1] || rules[2];
}

void ae_screen_firefight_content_value(const char *map, char *text, int size)
{
	char saved[96];
	unsigned char has, content;

	if (size <= 0)
		return;
	text[0] = 0;
	if (!map || !map[0] || !ae_setup_edit_is_firefight())
		return;
	snprintf(saved, sizeof(saved), "%s", map);
	if (strcmp(saved, map_name))
	{
		snprintf(map_name, sizeof(map_name), "%s", saved);
	}
	has = ae_ffg_map_kinds(map_name);
	content = content_now();
	ae_ffm_content_summary(has, content, ae_ffg_health_detect(map_name) != 0, health_is_changed(), loadout_is_set(), text, (size_t)size);
}

/* ---------- MAP CONTENT */

enum { M_MODE, M_KIND, M_LIST = M_KIND + AE_FFM_CONTENT_KINDS, M_LOADOUT = M_LIST + AE_FFM_CONTENT_KINDS, M_HEAD, M_HEALTH };
/* what each row is: its id and the kind or rule it is of */
static struct { short id, which; } row_of[AE_FLOW_ROWS];

static struct ae_pvem_rows content;

static int tags_open(int kind);
static int loadout_open(void);

static void content_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	unsigned char has = ae_ffg_map_kinds(map_name), bits = content_now(), rules[3];
	int vanilla = ae_ffm_content_mode(bits) == AE_FFM_MODE_VANILLA, kind, rule, n = 0;
	unsigned char health = ae_ffg_health_detect(map_name);

	ae_ffg_health_get(map_name, rules);
	if (has)
	{
		snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", ae_ffm_mode_name(ae_ffm_content_mode(bits)));
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "USE CONTENT", flow->value_texts[n], ae_ffm_mode_help(), NULL };
		row_of[n].id = M_MODE;
		row_of[n++].which = 0;
		for (kind = 0; kind < AE_FFM_CONTENT_KINDS; kind++)
		{
			int choice = ae_ffm_content_choice(bits, kind);

			if (!ae_ffm_has_kind(has, kind))
				continue;
			snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", ae_ffm_choice_name(choice));
			flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, ae_ffm_kind_row(kind), flow->value_texts[n], ae_ffm_kind_help(kind),
				vanilla ? ae_ffm_needs_content : NULL };
			row_of[n].id = M_KIND;
			row_of[n++].which = (short)kind;
			if (choice == AE_FFM_CHOICE_CUSTOM)
			{
				int count = ae_ffg_tag_count(map_name, kind), on = 0, each;

				for (each = 0; each < count; each++)
					on += ae_ffg_tag_on(map_name, kind, ae_ffg_tag_name(map_name, kind, each));
				snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%d OF %d ON", on, count);
				flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, ae_ffm_kind_list_row(kind), flow->value_texts[n],
					ae_ffm_kind_list_help(kind), vanilla ? ae_ffm_needs_content : NULL };
				flow->rows[n].subscreen = 1;
				row_of[n].id = M_LIST;
				row_of[n++].which = (short)kind;
			}
		}
	}
	if (ae_ffm_has_kind(has, AE_FFM_KIND_WEAPONS))
	{
		const char *primary = ae_ffg_loadout_get(map_name, 0), *secondary = ae_ffg_loadout_get(map_name, 1);
		char one[40], two[40];

		if (!primary && !secondary)
			snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "THE GAME'S");
		else
		{
			ae_ffm_tag_label(primary ? primary : "THE GAME'S", one, sizeof(one));
			ae_ffm_tag_label(secondary ? secondary : "THE GAME'S", two, sizeof(two));
			snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s / %s", one, two);
		}
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "PLAYER LOADOUT", flow->value_texts[n], ae_ffm_loadout_help(), NULL };
		flow->rows[n].subscreen = 1;
		row_of[n].id = M_LOADOUT;
		row_of[n++].which = 0;
	}
	if (health)
	{
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "HEALTH RULES", NULL, NULL, NULL };
		row_of[n].id = M_HEAD;
		row_of[n++].which = 0;
		for (rule = 0; rule < AE_FFM_HEALTH_RULES; rule++)
		{
			snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", ae_ffm_health_choice(rule, rules[rule]));
			flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, ae_ffm_health_row(rule), flow->value_texts[n], ae_ffm_health_help(rule), NULL };
			row_of[n].id = M_HEALTH;
			row_of[n++].which = (short)rule;
		}
	}
	if (!n)
	{
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NOTHING OF ITS OWN", NULL,
			"This map has no enemies, weapons or vehicles of its own, and its player has no rules of its own.", NULL };
		flow->rows[n].quiet = 1;
		row_of[n].id = M_HEAD;
		row_of[n++].which = 0;
	}
	flow->count = (short)n;
	if (flow->focus >= n)
		flow->focus = (short)(n - 1);
	if (flow->focus < 0)
		flow->focus = 0;
}

static void content_step(struct ae_pvem_rows *r, int row, int direction)
{
	unsigned char bits = content_now();
	int which, count;

	(void)r;
	if (row < 0 || row >= r->flow.count)
		return;
	which = row_of[row].which;
	switch (row_of[row].id)
	{
	case M_MODE:
		which = (ae_ffm_content_mode(bits) + direction + AE_FFM_MODES) % AE_FFM_MODES;
		content_set(ae_ffm_content_with_mode(bits, which));
		platform_log("ae firefight content: use content %s", ae_ffm_mode_name(which));
		break;
	case M_KIND:
		count = (ae_ffm_content_choice(bits, which) + direction + AE_FFM_CHOICES) % AE_FFM_CHOICES;
		content_set(ae_ffm_content_with_choice(bits, which, count));
		platform_log("ae firefight content: %s %s", ae_ffm_kind_row(which), ae_ffm_choice_name(count));
		break;
	case M_LIST:
		if (tags_open(which))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case M_LOADOUT:
		if (loadout_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case M_HEALTH:
	{
		unsigned char rules[3];

		ae_ffg_health_get(map_name, rules);
		count = ae_ffm_health_choices(which);
		rules[which] = (unsigned char)((rules[which] + direction + count) % count);
		ae_ffg_health_set(map_name, rules);
		platform_log("ae firefight content: %s %s", ae_ffm_health_row(which), ae_ffm_health_choice(which, rules[which]));
		break;
	}
	default:
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static const char *content_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	return row >= 0 && (row_of[row].id == M_LIST || row_of[row].id == M_LOADOUT) ? "Open" : "Change";
}

static void fact(struct ae_flow_screen const *flow, float x, float width, float y, const char *label, const char *value, const char *note)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + 150.0f * u, top + y * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, value);
	if (note && note[0])
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x + width, top + y * u, AE_ALIGN_RIGHT, AE_COLOR_MUTED, note);
}

static void content_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	unsigned char has = ae_ffg_map_kinds(map_name), bits = content_now(), health = ae_ffg_health_detect(map_name);
	static const char *const kind_words[AE_FFM_CONTENT_KINDS] = { "kinds", "weapons", "vehicles" };
	static const char *const kind_labels[AE_FFM_CONTENT_KINDS] = { "ENEMY AI", "WEAPONS", "VEHICLES" };
	char text[64], note[48];
	int kind, line = 0;

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 330.0f);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, map_caption[0] ? map_caption : "THIS MAP");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		has ? "THIS MAP'S OWN CONTENT \xE2\x80\xA2 KEPT FOR THIS MAP" : "THE GAME'S OWN CONTENT ONLY");
	for (kind = 0; kind < AE_FFM_CONTENT_KINDS; kind++)
	{
		int count, choice, on = 0, each;

		if (!ae_ffm_has_kind(has, kind))
			continue;
		count = ae_ffg_tag_count(map_name, kind);
		choice = ae_ffm_content_choice(bits, kind);
		for (each = 0; each < count; each++)
			on += ae_ffg_tag_on(map_name, kind, ae_ffg_tag_name(map_name, kind, each));
		snprintf(text, sizeof(text), "%d %s", count, kind_words[kind]);
		note[0] = 0;
		if (ae_ffm_content_mode(bits) == AE_FFM_MODE_VANILLA || choice == AE_FFM_CHOICE_NONE)
			snprintf(note, sizeof(note), "not used");
		else if (choice == AE_FFM_CHOICE_CUSTOM)
			snprintf(note, sizeof(note), "%d chosen", on);
		fact(flow, x, width, 290.0f + 36.0f * (float)line++, kind_labels[kind], text, note);
	}
	if (health)
	{
		snprintf(text, sizeof(text), "%s", (health & 1) ? "no shields" : "own shields");
		fact(flow, x, width, 290.0f + 36.0f * (float)line++, "PLAYERS", text, (health & 2) ? "never recharge" : "");
	}
}

static int content_open_over(void)
{
	if (ae_ui_holds(&content))
		ae_pve_menus_rows_close(&content);
	memset(&content, 0, sizeof(content));
	content.name = "firefight content";
	content.title = "MAP CONTENT";
	snprintf(content.crumb, sizeof(content.crumb), "FIREFIGHT \xE2\x80\xA2 %s", map_caption[0] ? map_caption : "THIS MAP");
	content.build = content_build;
	content.step = content_step;
	content.a_word = content_a_word;
	content.side = content_side;
	content.hit_base = CONTENT_HIT;
	return ae_pve_menus_rows_open(&content);
}

int ae_screen_firefight_content_open(const char *map, const char *caption)
{
	if (!ae_setup_edit_is_firefight() || !map || !map[0])
		return 0;
	snprintf(map_name, sizeof(map_name), "%s", map);
	snprintf(map_caption, sizeof(map_caption), "%s", caption ? caption : "");
	return content_open_over();
}

/* ---------- CHOOSE ...: the map's own of a kind, each on or off (pages: the rows hold 32) */

enum { PAGE_TAGS = 26 };
static struct ae_pvem_rows tags;
static int tags_kind, tags_page;
static char tags_label[AE_FLOW_ROWS][48];
static short tags_row_of[AE_FLOW_ROWS];

static int tags_pages(void)
{
	int count = ae_ffg_tag_count(map_name, tags_kind);

	return count <= 0 ? 1 : (count + PAGE_TAGS - 1) / PAGE_TAGS;
}

static void tags_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int count = ae_ffg_tag_count(map_name, tags_kind), first, each, n = 0, pages = tags_pages();

	if (tags_page >= pages)
		tags_page = pages - 1;
	first = tags_page * PAGE_TAGS;
	for (each = first; each < count && each < first + PAGE_TAGS; each++)
	{
		const char *name = ae_ffg_tag_name(map_name, tags_kind, each);

		ae_ffm_tag_label(name, tags_label[n], sizeof(tags_label[n]));
		snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", ae_ffg_tag_on(map_name, tags_kind, name) ? "ON" : "OFF");
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, tags_label[n], flow->value_texts[n], ae_ffm_kind_list_help(tags_kind), NULL };
		tags_row_of[n++] = (short)each;
	}
	if (pages > 1)
	{
		snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%d OF %d", tags_page + 1, pages);
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NEXT PAGE", flow->value_texts[n],
			"The map has more than this page holds. A shows the next ones, the last page back to the first.", NULL };
		tags_row_of[n++] = -1;
	}
	flow->count = (short)n;
	if (flow->focus >= n)
		flow->focus = (short)(n - 1);
	if (flow->focus < 0)
		flow->focus = 0;
}

static void tags_step(struct ae_pvem_rows *r, int row, int direction)
{
	const char *name;

	(void)direction;
	if (row < 0 || row >= r->flow.count)
		return;
	if (tags_row_of[row] < 0)
	{
		tags_page = (tags_page + 1) % tags_pages();
		r->flow.focus = 0;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	name = ae_ffg_tag_name(map_name, tags_kind, tags_row_of[row]);
	ae_ffg_tag_set(map_name, tags_kind, name, !ae_ffg_tag_on(map_name, tags_kind, name));
	platform_log("ae firefight content: %s %s %s", ae_ffm_kind_row(tags_kind), name ? name : "", ae_ffg_tag_on(map_name, tags_kind, name) ? "on" : "off");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void tags_all(struct ae_pvem_rows *r)
{
	int count = ae_ffg_tag_count(map_name, tags_kind), each, on = 0, want;

	(void)r;
	for (each = 0; each < count; each++)
		on += ae_ffg_tag_on(map_name, tags_kind, ae_ffg_tag_name(map_name, tags_kind, each));
	want = on < count;
	for (each = 0; each < count; each++)
		ae_ffg_tag_set(map_name, tags_kind, ae_ffg_tag_name(map_name, tags_kind, each), want);
	platform_log("ae firefight content: %s all %s", ae_ffm_kind_row(tags_kind), want ? "on" : "off");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static const char *tags_a_word(struct ae_pvem_rows *r, int row)
{
	return row >= 0 && row < r->flow.count && tags_row_of[row] < 0 ? "Next page" : "Turn on or off";
}

static void tags_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char text[64];
	int count = ae_ffg_tag_count(map_name, tags_kind), each, on = 0;

	for (each = 0; each < count; each++)
		on += ae_ffg_tag_on(map_name, tags_kind, ae_ffg_tag_name(map_name, tags_kind, each));
	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 180.0f);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, map_caption[0] ? map_caption : "THIS MAP");
	snprintf(text, sizeof(text), "%d OF %d ON", on, count);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x, top + 262.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, text);
	ae_pve_menus_ui_paragraph(flow, 16.0f, 22.0f, x, top + 296.0f * u, width, 2, AE_COLOR_MUTED, "Kept for this map for the next games.");
}

static int tags_open(int kind)
{
	if (kind < 0 || kind >= AE_FFM_CONTENT_KINDS)
		return 0;
	tags_kind = kind;
	tags_page = 0;
	memset(&tags, 0, sizeof(tags));
	tags.name = "firefight tags";
	tags.title = ae_ffm_kind_list_row(kind);
	snprintf(tags.crumb, sizeof(tags.crumb), "MAP CONTENT \xE2\x80\xA2 %s", map_caption[0] ? map_caption : "THIS MAP");
	tags.build = tags_build;
	tags.step = tags_step;
	tags.a_word = tags_a_word;
	tags.x_action = tags_all;
	tags.x_word = "All on / off";
	tags.side = tags_side;
	tags.hit_base = TAGS_HIT;
	return ae_pve_menus_rows_open(&tags);
}

/* ---------- PLAYER LOADOUT: the map's weapons the players spawn with, in the primary and secondary places */

static struct ae_pvem_rows loadout;
static char loadout_text[2][48];

static void loadout_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int slot;

	for (slot = 0; slot < 2; slot++)
	{
		const char *tag = ae_ffg_loadout_get(map_name, slot);

		if (tag)
			ae_ffm_tag_label(tag, loadout_text[slot], sizeof(loadout_text[slot]));
		else
			snprintf(loadout_text[slot], sizeof(loadout_text[slot]), "THE GAME'S");
		flow->rows[slot] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, slot ? "SECONDARY WEAPON" : "PRIMARY WEAPON", loadout_text[slot],
			"Spawn with one of the map's own weapons in place of the game's. Left and right step through them; THE GAME'S puts it back.", NULL };
	}
	flow->count = 2;
}

static void loadout_step(struct ae_pvem_rows *r, int row, int direction)
{
	int count = ae_ffg_tag_count(map_name, AE_FFM_KIND_WEAPONS), current = -1, each, next;
	const char *tag;

	if (row < 0 || row > 1)
		return;
	tag = ae_ffg_loadout_get(map_name, row);
	for (each = 0; tag && each < count; each++)
		if (!strcmp(ae_ffg_tag_name(map_name, AE_FFM_KIND_WEAPONS, each), tag))
			current = each;
	/* (THE GAME'S is the place before the first weapon) */
	next = (current + 1 + direction + count + 1) % (count + 1) - 1;
	ae_ffg_loadout_set(map_name, row, next < 0 ? NULL : ae_ffg_tag_name(map_name, AE_FFM_KIND_WEAPONS, next));
	platform_log("ae firefight content: %s %s", row ? "secondary" : "primary", next < 0 ? "the game's" : ae_ffg_tag_name(map_name, AE_FFM_KIND_WEAPONS, next));
	ae_sound_request(AE_SOUND_CURSOR, 0);
	(void)r;
}

static int loadout_open(void)
{
	memset(&loadout, 0, sizeof(loadout));
	loadout.name = "firefight loadout";
	loadout.title = "PLAYER LOADOUT";
	snprintf(loadout.crumb, sizeof(loadout.crumb), "MAP CONTENT \xE2\x80\xA2 %s", map_caption[0] ? map_caption : "THIS MAP");
	loadout.build = loadout_build;
	loadout.step = loadout_step;
	loadout.hit_base = LOADOUT_HIT;
	return ae_pve_menus_rows_open(&loadout);
}

/* ---------- OBJECTIVES */

static struct ae_pvem_rows objectives;
/* each row: -1 a header, else the section (0 wave, 1 round) * 100 + 0 switch, 1 chance, 2 + the kind */
static short objective_row[AE_FLOW_ROWS];
static char objective_help[AE_FLOW_ROWS][160];

static void objectives_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int round, kind, n = 0;

	for (round = 0; round < 2; round++)
	{
		int on = setting_on(ae_ffm_objectives_key(round));
		long chance = config_integer(ae_ffm_chance_key(round));

		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, round ? "ROUND OBJECTIVES" : "WAVE OBJECTIVES", NULL, NULL, NULL };
		objective_row[n++] = -1;
		snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", on ? "ON" : "OFF");
		snprintf(objective_help[n], sizeof(objective_help[n]), "%s", ae_ffm_objectives_help(round, on));
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, round ? "ROUND OBJECTIVES" : "WAVE OBJECTIVES", flow->value_texts[n],
			objective_help[n], NULL };
		objective_row[n++] = (short)(round * 100);
		snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%ld%%", ae_ffm_chance_percent(ae_ffm_chance_index(chance)));
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "CHANCE", flow->value_texts[n], ae_ffm_chance_help(round),
			on ? NULL : round ? "NEEDS ROUND OBJECTIVES" : "NEEDS WAVE OBJECTIVES" };
		objective_row[n++] = (short)(round * 100 + 1);
		for (kind = 0; kind < ae_ffm_objective_count(round); kind++)
		{
			struct ae_ffm_objective const *row = ae_ffm_objective(round, kind);

			snprintf(flow->value_texts[n], sizeof(flow->value_texts[n]), "%s", setting_on(row->key) ? "ON" : "OFF");
			flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, row->label, flow->value_texts[n], row->help,
				on ? NULL : round ? "NEEDS ROUND OBJECTIVES" : "NEEDS WAVE OBJECTIVES" };
			objective_row[n++] = (short)(round * 100 + 2 + kind);
		}
	}
	flow->count = (short)n;
}

static void objectives_step(struct ae_pvem_rows *r, int row, int direction)
{
	int code, round, part;

	if (row < 0 || row >= r->flow.count || objective_row[row] < 0)
		return;
	code = objective_row[row];
	round = code / 100;
	part = code % 100;
	if (part == 0)
		setting_write(ae_ffm_objectives_key(round), setting_on(ae_ffm_objectives_key(round)) ? "false" : "true");
	else if (part == 1)
	{
		char value[16];
		int index = ae_ffm_chance_index(config_integer(ae_ffm_chance_key(round)));

		index = (index + direction + AE_FFM_CHANCES) % AE_FFM_CHANCES;
		snprintf(value, sizeof(value), "%ld", ae_ffm_chance_percent(index));
		setting_write(ae_ffm_chance_key(round), value);
	}
	else
	{
		struct ae_ffm_objective const *kind = ae_ffm_objective(round, part - 2);

		if (!kind)
			return;
		setting_write(kind->key, setting_on(kind->key) ? "false" : "true");
	}
	platform_log("ae firefight objectives: wave %s, round %s", setting_on(ae_ffm_objectives_key(0)) ? "on" : "off",
		setting_on(ae_ffm_objectives_key(1)) ? "on" : "off");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

void ae_screen_firefight_objectives_value(char *text, int size)
{
	if (size > 0)
		snprintf(text, (size_t)size, "%s", ae_ffm_objectives_value(setting_on(ae_ffm_objectives_key(0)), setting_on(ae_ffm_objectives_key(1))));
}

static void objectives_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	int round, on, each, kinds;
	char text[64];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 330.0f);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "OBJECTIVES");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "KEPT FOR EVERY MAP");
	for (round = 0; round < 2; round++)
	{
		on = setting_on(ae_ffm_objectives_key(round));
		kinds = 0;
		for (each = 0; each < ae_ffm_objective_count(round); each++)
			kinds += setting_on(ae_ffm_objective(round, each)->key);
		snprintf(text, sizeof(text), "%s", on ? "ON" : "OFF");
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + (292.0f + 70.0f * (float)round) * u, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			round ? "ROUND OBJECTIVES" : "WAVE OBJECTIVES");
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 22.0f, x, top + (322.0f + 70.0f * (float)round) * u, AE_ALIGN_LEFT,
			on ? AE_COLOR_TITLE : AE_COLOR_MUTED, text);
		snprintf(text, sizeof(text), "%ld%% \xE2\x80\xA2 %d OF %d KINDS", on ? ae_ffm_chance_percent(ae_ffm_chance_index(config_integer(ae_ffm_chance_key(round)))) : 0L,
			kinds, ae_ffm_objective_count(round));
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x + width, top + (322.0f + 70.0f * (float)round) * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, text);
	}
}

int ae_screen_firefight_objectives_open(void)
{
	if (ae_ui_holds(&objectives))
		ae_pve_menus_rows_close(&objectives);
	memset(&objectives, 0, sizeof(objectives));
	objectives.name = "firefight objectives";
	objectives.title = "OBJECTIVES";
	snprintf(objectives.crumb, sizeof(objectives.crumb), "FIREFIGHT");
	objectives.build = objectives_build;
	objectives.step = objectives_step;
	objectives.side = objectives_side;
	objectives.hit_base = OBJECTIVES_HIT;
	return ae_pve_menus_rows_open(&objectives);
}

/* ---------- DEV MODE's staging */

int ae_screen_firefight_open_staged(const char *scene, const char *map, const char *caption)
{
	if (!scene)
		return 0;
	if (!strcmp(scene, "objectives"))
		return ae_screen_firefight_objectives_open();
	if (!ae_screen_firefight_content_open(map, caption))
		return 0;
	if (!strcmp(scene, "tags"))
		return tags_open(AE_FFM_KIND_WEAPONS);
	if (!strcmp(scene, "loadout"))
		return loadout_open();
	return !strcmp(scene, "content");
}
