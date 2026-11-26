/*
AE_SCREEN_RANDOMIZER.C

See ae_screen_randomizer.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen) with the mission's facts in the panel on the
right. The pages are one screen driven by the rows table of ae_randomizer_menus_rules.c: a row shows only while what it depends
on is chosen, so a family of dependent rows is not drawn dimmed.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_randomizer_glue.h"
#include "ae_glue_pve_menus.h"
#include "ae_motion.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_randomizer_menus_rules.h"
#include "ae_screen_randomizer.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DOT "\xE2\x80\xA2"
enum { HUB_HIT = 0x7D40, PAGE_HIT = 0x7D60, SETUPS_HIT = 0x7D80, ITEMS_HIT = 0x7DA0, NAME_HIT = 0x7DC0 };
enum { NAME_ROWS = NAME_HIT + 1, NAME_PROMPTS, NAME_FIELD };
enum { CUSTOM_TYPES = 64 };

/* ---------- what the rows read */

static const char *read_setting(void *context, const char *key)
{
	return ae_pve_menus_ui_read(context, key);
}

/* the item set's custom types (actors, weapons), found again each second */
static struct
{
	unsigned long checked;
	int counts[2];
	char types[2][CUSTOM_TYPES][AE_PVE_ITEM_TYPE];
	int has_set;
} cache;

static struct ae_rnd_facts facts_now(void)
{
	struct ae_rnd_facts facts;
	unsigned long now = ae_motion_now();

	if (!cache.checked || now - cache.checked > 1000)
	{
		cache.checked = now;
		cache.counts[0] = ae_pve_item_set_types(0, cache.types[0], CUSTOM_TYPES);
		cache.counts[1] = ae_pve_item_set_types(1, cache.types[1], CUSTOM_TYPES);
		cache.has_set = config_boolean("game.tag_injection") && ae_pve_item_set_played() >= 0;
	}
	facts.get = read_setting;
	facts.context = NULL;
	facts.actor_types = cache.counts[0];
	facts.weapon_types = cache.counts[1];
	facts.has_item_set = cache.has_set;
	return facts;
}

static void write_value(const char *key, const char *value)
{
	ae_pve_menus_ui_write(key, value);
}

/* ---------- the saved setups */

static const char *standard_value(const char *key)
{
	static char text[4][64];
	static int next;
	char *buffer = text[next++ & 3];

	buffer[0] = 0;
	return config_default(key, buffer, sizeof(text[0])) ? buffer : "";
}

static void slot_key(int slot, char *key, size_t size)
{
	snprintf(key, size, "game.randomizer_profile_%d", slot + 1);
}

static void setups_read(struct ae_rnd_setups *setups)
{
	int slot;

	memset(setups, 0, sizeof(*setups));
	for (slot = 0; slot < AE_RND_SETUPS; slot++)
	{
		char key[40], text[1200];

		slot_key(slot, key, sizeof(key));
		if (!config_text(key, text, sizeof(text)) || !text[0])
			continue;
		if (ae_rnd_setup_parse(text, &setups->setup[setups->count], standard_value))
			setups->count++;
	}
}

/* the setups, in order, into the slots (an emptied slot written empty, a slot unchanged not written again) */
static void setups_write(struct ae_rnd_setups const *setups)
{
	int slot;

	for (slot = 0; slot < AE_RND_SETUPS; slot++)
	{
		char key[40], text[1200] = "", current[1200] = "";

		slot_key(slot, key, sizeof(key));
		if (slot < setups->count && !ae_rnd_setup_format(&setups->setup[slot], text, sizeof(text)))
			continue;
		if (config_text(key, current, sizeof(current)) && !strcmp(current, text))
			continue;
		write_value(key, text);
	}
}

/* the setup of the pages as they are now, under a name */
static void setup_capture(const char *name, struct ae_rnd_setup *setup)
{
	int key;

	memset(setup, 0, sizeof(*setup));
	snprintf(setup->name, sizeof(setup->name), "%.*s", AE_RND_SETUP_NAME, name);
	for (key = 0; key < AE_RND_SETUP_KEYS; key++)
	{
		if (!ae_rnd_setup_key(key)[0] || !config_text(ae_rnd_setup_key(key), setup->value[key], sizeof(setup->value[key])))
			setup->value[key][0] = 0;
	}
}

static const char *setup_get(void *context, const char *key)
{
	struct ae_rnd_setup const *setup = context;
	const char *value;

	if (!strcmp(key, "game.randomizer"))
		return "true";
	value = ae_rnd_setup_value(setup, key);
	return value ? value : "";
}

/* a saved setup in the hub's words */
static void setup_summary(struct ae_rnd_setup const *setup, char *text, size_t size)
{
	struct ae_rnd_facts facts = { setup_get, NULL, 0, 0, 0 };

	facts.context = (void *)setup;
	ae_rnd_lobby_summary(&facts, text, size);
}

/* ---------- the hub */

/* whether this PC cannot write the campaign pack at all: none of the campaign maps the pack reads is there (the missions
come from other files), so the randomizer would never have its data */
static int pack_unavailable(void)
{
	struct randomizer_pack_state pack;

	randomizer_pack_state(&pack);
	return pack.failed && !pack.writing && pack.maps_wanted == 0;
}

enum { H_SWITCH, H_HEAD_MISSION, H_AI, H_WEAPONS, H_PICKUPS, H_LOADOUT, H_ITEMS, H_HEAD_SETUPS, H_SAVE, H_LOAD, H_ROWS };

static struct ae_pvem_rows hub;
static int hub_id[AE_FLOW_ROWS];
static char hub_value[H_ROWS][48], hub_help[H_ROWS][200];
static int sets_known;

static int page_open(int page);
static int setups_open(void);
static int name_open(void);
static int items_open(void);

static void hub_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	struct ae_rnd_facts facts = facts_now();
	struct ae_rnd_setups setups;
	struct ae_pve_item_set sets[AE_PVE_ITEM_SETS];
	int on = config_boolean("game.randomizer"), sets_count = ae_pve_item_sets(sets, AE_PVE_ITEM_SETS), n = 0, id;
	const char *wait = on ? NULL : "NEEDS RANDOMIZER";
	int unavailable = pack_unavailable();
	char text[48];

	setups_read(&setups);
	sets_known = sets_count;
	snprintf(hub_value[H_SWITCH], sizeof(hub_value[0]), "%s", on ? "ON" : "OFF");
	ae_rnd_ai_summary(&facts, hub_value[H_AI], sizeof(hub_value[0]));
	ae_rnd_weapon_summary(&facts, 0, hub_value[H_WEAPONS], sizeof(hub_value[0]));
	ae_rnd_weapon_summary(&facts, 1, hub_value[H_PICKUPS], sizeof(hub_value[0]));
	snprintf(hub_value[H_LOADOUT], sizeof(hub_value[0]), "%s", ae_rnd_loadout_summary(&facts));
	snprintf(hub_value[H_ITEMS], sizeof(hub_value[0]), "%s", facts.has_item_set ? "IN USE" : "NONE");
	snprintf(hub_value[H_LOAD], sizeof(hub_value[0]), "%d SAVED", setups.count);
	snprintf(hub_help[H_SWITCH], sizeof(hub_help[0]), "The randomizer swaps the mission's enemies, the weapons they carry and the weapons lying in it. "
		"Each start rolls a new layout; a checkpoint keeps the one you have.");
	if (unavailable)
		snprintf(hub_help[H_SWITCH], sizeof(hub_help[0]), "This PC's campaign maps cannot be packed for the randomizer: the missions are read from "
			"files the pack does not take yet.");
	snprintf(hub_help[H_AI], sizeof(hub_help[0]), "Which actors the enemies and your allies become, and how large the enemy squads are. {A} opens the page.");
	snprintf(hub_help[H_WEAPONS], sizeof(hub_help[0]), "The weapons the AI is given. {A} opens the page.");
	snprintf(hub_help[H_PICKUPS], sizeof(hub_help[0]), "The weapons lying in the mission. {A} opens the page.");
	snprintf(hub_help[H_LOADOUT], sizeof(hub_help[0]), "The weapons you start each mission with. Works with every swap off too. {A} opens the page.");
	snprintf(hub_help[H_ITEMS], sizeof(hub_help[0]), "Play the mission with a set's items: its weapons and actors join the pools. {A} chooses a set.");
	if (ae_rnd_setups_would_drop(&setups, "") == 0)
		snprintf(text, sizeof(text), "%s", setups.setup[0].name);
	else
		text[0] = 0;
	if (text[0])
		snprintf(hub_help[H_SAVE], sizeof(hub_help[0]), "Saving a ninth replaces the oldest: %s. {A} names the setup.", text);
	else
		snprintf(hub_help[H_SAVE], sizeof(hub_help[0]), "Keeps every page's settings under a name, up to eight on this PC. {A} names the setup.");
	snprintf(hub_help[H_LOAD], sizeof(hub_help[0]), "%s", setups.count ? "Puts a saved setup's settings on the pages. {A} opens the list." : "No setup is saved yet.");
	for (id = 0; id < H_ROWS; id++)
	{
		struct ae_flow_row *row = &flow->rows[n];
		int door = 0;

		if (id == H_ITEMS && sets_count < 1)
			continue;
		switch (id)
		{
		case H_SWITCH: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "RANDOMIZER", hub_value[id], hub_help[id],
			unavailable ? "NOT AVAILABLE" : NULL }; break;
		case H_HEAD_MISSION: *row = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "THE MISSION", NULL, NULL, NULL }; break;
		case H_AI: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "AI REPLACEMENT", hub_value[id], hub_help[id], wait }; door = 1; break;
		case H_WEAPONS: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "AI WEAPONS", hub_value[id], hub_help[id], wait }; door = 1; break;
		case H_PICKUPS: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "WEAPON PICKUPS", hub_value[id], hub_help[id], wait }; door = 1; break;
		case H_LOADOUT: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "PLAYER LOADOUT", hub_value[id], hub_help[id], wait }; door = 1; break;
		case H_ITEMS: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "ITEM SET", hub_value[id], hub_help[id], wait }; door = 1; break;
		case H_HEAD_SETUPS: *row = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "SETUPS", NULL, NULL, NULL }; break;
		case H_SAVE: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "SAVE THIS SETUP...", NULL, hub_help[id], wait }; break;
		default: *row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "LOAD A SETUP", hub_value[id], hub_help[id],
			on ? (setups.count ? NULL : "NONE SAVED") : wait }; door = 1; break;
		}
		row->subscreen = (short)door;
		hub_id[n++] = id;
	}
	flow->count = (short)n;
}

static void hub_step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)r;
	(void)direction;
	if (row < 0 || row >= r->flow.count)
		return;
	switch (hub_id[row])
	{
	case H_SWITCH:
		write_value("game.randomizer", config_boolean("game.randomizer") ? "false" : "true");
		ae_sound_request(AE_SOUND_CURSOR, 0);
		break;
	case H_AI: page_open(AE_RND_PAGE_AI); break;
	case H_WEAPONS: page_open(AE_RND_PAGE_WEAPONS); break;
	case H_PICKUPS: page_open(AE_RND_PAGE_PICKUPS); break;
	case H_LOADOUT: page_open(AE_RND_PAGE_LOADOUT); break;
	case H_ITEMS: items_open(); break;
	case H_SAVE: name_open(); break;
	case H_LOAD: setups_open(); break;
	default: return;
	}
	if (hub_id[row] != H_SWITCH)
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static const char *hub_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	return hub_id[row] == H_SWITCH ? (config_boolean("game.randomizer") ? "Turn off" : "Turn on") : hub_id[row] == H_SAVE ? "Name" : "Open";
}

/* a fact line of a panel: a muted label at the left, the value at the right */
static void fact(struct ae_flow_screen const *flow, float x, float width, float y, const char *label, const char *value, unsigned int color)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, top + y * u, AE_ALIGN_RIGHT, color, value);
}

static void hub_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	struct ae_rnd_facts facts = facts_now();
	struct randomizer_pack_state pack;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char text[96], number[48];
	int enemies = !strcmp(read_setting(NULL, "game.randomizer_enemies"), "true"), allies = !strcmp(read_setting(NULL, "game.randomizer_allies"), "true");

	randomizer_pack_state(&pack);
	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 700.0f);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "THE MISSION");
	ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	fact(flow, x, width, 282.0f, "ENEMIES", enemies ? "SWAPPED" : "AS MADE", AE_COLOR_TEXT);
	fact(flow, x, width, 320.0f, "ALLIES", allies ? "SWAPPED" : "AS MADE", AE_COLOR_TEXT);
	ae_rnd_ai_summary(&facts, text, sizeof(text));
	snprintf(number, sizeof(number), "%s", strstr(text, DOT) ? strstr(text, DOT) + 4 : "1X");
	fact(flow, x, width, 358.0f, "SQUADS", number, AE_COLOR_TEXT);
	ae_rnd_weapon_summary(&facts, 0, text, sizeof(text));
	fact(flow, x, width, 396.0f, "AI WEAPONS", text, AE_COLOR_TEXT);
	ae_rnd_weapon_summary(&facts, 1, text, sizeof(text));
	fact(flow, x, width, 434.0f, "PICKUPS", text, AE_COLOR_TEXT);
	fact(flow, x, width, 472.0f, "GO CRAZY", !strcmp(read_setting(NULL, "game.randomizer_crazy"), "true") ? "ON" : "OFF", AE_COLOR_TEXT);
	ae_draw_rect(x, top + 500.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	if (pack.done)
		snprintf(text, sizeof(text), "READY  %d of %d missions", pack.maps_done, pack.maps_wanted);
	else if (pack.failed)
		snprintf(text, sizeof(text), "NOT WRITTEN");
	else
		snprintf(text, sizeof(text), "PREPARING  %d of %d missions", pack.maps_done, pack.maps_wanted);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 528.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "CAMPAIGN PACK");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x, top + 560.0f * u, AE_ALIGN_LEFT, pack.done ? AE_COLOR_ACCENT : AE_COLOR_TITLE, text);
	if (!pack.done && !pack.failed && pack.maps_wanted > 0)
	{
		ae_draw_rect(x, top + 584.0f * u, width, 8.0f * u, 4.0f * u, AE_COLOR_TRACK);
		ae_draw_rect(x, top + 584.0f * u, width * (float)pack.maps_done / (float)pack.maps_wanted, 8.0f * u, 4.0f * u, AE_COLOR_ACCENT);
	}
	if (pack.failed)
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 600.0f * u, width, 3, AE_COLOR_WARNING, pack.reason[0] ? pack.reason : "The pack could not be written.");
	else if (!pack.done)
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 616.0f * u, width, 4, AE_COLOR_MUTED, "The randomizer copies enemies and weapons from the other "
			"missions, so it first packs them once from this PC's campaign maps, in the background. A mission starts plain until the pack is ready.");
	ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 748.0f * u, width, 2, AE_COLOR_MUTED, "Alone and in split screen only: online games play the campaign's own.");
}

static int hub_open(void)
{
	if (ae_ui_holds(&hub))
		ae_pve_menus_rows_close(&hub);
	memset(&hub, 0, sizeof(hub));
	hub.name = "randomizer";
	hub.title = "RANDOMIZER";
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(hub.crumb, sizeof(hub.crumb), "CAMPAIGN \xC2\xB7 %s", name);
	}
	hub.build = hub_build;
	hub.step = hub_step;
	hub.a_word = hub_a_word;
	hub.side = hub_side;
	hub.hit_base = HUB_HIT;
	return ae_pve_menus_rows_open(&hub);
}

int ae_screen_randomizer_open(void)
{
	return hub_open();
}

void ae_screen_randomizer_lobby_value(char *text, size_t size)
{
	struct ae_rnd_facts facts = facts_now();

	if (pack_unavailable())
	{
		snprintf(text, size, "NOT AVAILABLE");
		return;
	}
	ae_rnd_lobby_summary(&facts, text, size);
}

/* ---------- a page */

static struct ae_pvem_rows page;
static int page_which, page_map[AE_FLOW_ROWS];
static char page_value[AE_FLOW_ROWS][48], page_help[AE_FLOW_ROWS][220];

static const char *const page_titles[AE_RND_PAGES] = { "AI REPLACEMENT", "AI WEAPONS", "WEAPON PICKUPS", "PLAYER LOADOUT" };

/* the item set's custom types a row picks from, and the one its setting names (the first when it names none, as the setting is then written) */
static int custom_current(struct ae_rnd_row const *row, int *count)
{
	int each, kind = row->weapons ? 1 : 0;
	const char *now = read_setting(NULL, row->key);

	*count = cache.counts[kind];
	for (each = 0; each < *count; each++)
		if (!_stricmp(cache.types[kind][each], now))
			return each;
	return *count > 0 ? 0 : -1;
}

static void page_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	struct ae_rnd_facts facts = facts_now();
	int each, n = 0;

	for (each = 0; each < ae_rnd_row_count() && n < AE_FLOW_ROWS; each++)
	{
		struct ae_rnd_row const *row = ae_rnd_row(each);

		if (row->page != page_which || !ae_rnd_row_shown(row, &facts))
			continue;
		page_map[n] = each;
		if (row->kind == AE_RND_ROW_HEAD)
		{
			flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, row->label, NULL, NULL, NULL };
			continue;
		}
		if (row->kind == AE_RND_ROW_CUSTOM)
		{
			int count, current = custom_current(row, &count);

			snprintf(page_value[n], sizeof(page_value[0]), "%s", current < 0 ? "NONE" : cache.types[row->weapons ? 1 : 0][current]);
		}
		else
			snprintf(page_value[n], sizeof(page_value[0]), "%s", ae_rnd_row_value_text(row, &facts));
		snprintf(page_help[n], sizeof(page_help[0]), "%s", row->help);
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, row->label, page_value[n], page_help[n], NULL };
		n++;
	}
	flow->count = (short)n;
	if (flow->focus >= flow->count)
		flow->focus = (short)(flow->count - 1);
	if (flow->focus >= 0 && flow->rows[flow->focus].kind == AE_FLOW_ROW_HEADER)
		flow->focus = (short)(flow->focus + 1 < flow->count ? flow->focus + 1 : 0);
}

static void page_step(struct ae_pvem_rows *r, int row, int direction)
{
	struct ae_rnd_facts facts = facts_now();
	struct ae_rnd_row const *def;

	if (row < 0 || row >= r->flow.count)
		return;
	def = ae_rnd_row(page_map[row]);
	if (def->kind == AE_RND_ROW_SWITCH)
		write_value(def->key, !strcmp(read_setting(NULL, def->key), "true") ? "false" : "true");
	else if (def->kind == AE_RND_ROW_CHOICE)
	{
		int now = ae_rnd_choice_index(def, read_setting(NULL, def->key));

		write_value(def->key, def->choices[ae_rnd_choice_step(def, now, direction < 0 ? -1 : 1, &facts)].value);
	}
	else if (def->kind == AE_RND_ROW_CUSTOM)
	{
		int count, current = custom_current(def, &count);

		if (count < 1)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		current = (current + (direction < 0 ? -1 : 1) + count) % count;
		write_value(def->key, cache.types[def->weapons ? 1 : 0][current]);
	}
	else
		return;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void page_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	struct ae_rnd_facts facts = facts_now();
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char text[96];
	int enemies = !strcmp(read_setting(NULL, "game.randomizer_enemies"), "true");

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 560.0f);
	if (page_which == AE_RND_PAGE_AI)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "WHAT THEY BECOME");
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		snprintf(text, sizeof(text), "%s", enemies ? ae_rnd_row_value_text(ae_rnd_row(3), &facts) : "THEIR OWN");
		fact(flow, x, width, 282.0f, "ENEMY SQUADS", text, AE_COLOR_TEXT);
		ae_rnd_ai_summary(&facts, text, sizeof(text));
		fact(flow, x, width, 320.0f, "SQUAD SIZE", strstr(text, DOT) ? strstr(text, DOT) + 4 : "1X", AE_COLOR_TEXT);
		fact(flow, x, width, 358.0f, "YOUR ALLIES", !strcmp(read_setting(NULL, "game.randomizer_allies"), "true") ? "SWAPPED" : "THEIR OWN", AE_COLOR_TEXT);
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 410.0f * u, width, 3, AE_COLOR_MUTED,
			"A row shows only while what it needs is chosen: a pool's type with ONE TYPE, a type's rank.");
	}
	else if (page_which == AE_RND_PAGE_LOADOUT)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "YOU START WITH");
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		fact(flow, x, width, 282.0f, "WEAPONS", ae_rnd_loadout_summary(&facts), AE_COLOR_TEXT);
		fact(flow, x, width, 320.0f, "GRENADES", "THE MISSION'S", AE_COLOR_TEXT);
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 372.0f * u, width, 4, AE_COLOR_MUTED,
			"DEFAULT: the mission's own. RANDOM: two for each player, from the pickups' pool. SET: these two. Works with every swap off too.");
	}
	else
	{
		int pickups = page_which == AE_RND_PAGE_PICKUPS;

		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, pickups ? "WHAT LIES IN THE MISSION" : "WHAT THEY CARRY");
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		ae_rnd_weapon_summary(&facts, pickups, text, sizeof(text));
		fact(flow, x, width, 282.0f, pickups ? "PICKUPS" : "AI WEAPONS", text, AE_COLOR_TEXT);
		ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 334.0f * u, width, 4, AE_COLOR_MUTED, pickups ?
			"A weapon the mission lacks is copied from another mission. One a script names stays." :
			"Only enemies that can hold a weapon get it; the others keep theirs.");
	}
}

/* PLAYER LOADOUT's picker: the playing set's weapons fill its last slots (a staged scene brings two made-up ones) */
static int staged_set_weapons;

static void loadout_choices_refresh(void)
{
	char labels[AE_RND_LOADOUT_SET_SLOTS][48], values[AE_RND_LOADOUT_SET_SLOTS][24];
	int count;

	if (staged_set_weapons)
	{
		snprintf(labels[0], sizeof(labels[0]), "SPIRIT PISTOL");
		snprintf(values[0], sizeof(values[0]), "tag:5eed0001");
		snprintf(labels[1], sizeof(labels[1]), "BRUTE CARBINE");
		snprintf(values[1], sizeof(values[1]), "tag:5eed0002");
		count = 2;
	}
	else
		count = ae_pve_item_set_weapons(labels, values, AE_RND_LOADOUT_SET_SLOTS);
	ae_rnd_loadout_set_weapons((char const (*)[48])labels, (char const (*)[24])values, count);
}

static int page_open(int which)
{
	if (ae_ui_holds(&page))
		ae_pve_menus_rows_close(&page);
	memset(&page, 0, sizeof(page));
	page_which = which;
	if (which == AE_RND_PAGE_LOADOUT)
		loadout_choices_refresh();
	page.name = "randomizer page";
	page.title = page_titles[which];
	{
		char name[48];

		ae_pve_menus_ui_profile(name, sizeof(name));
		snprintf(page.crumb, sizeof(page.crumb), "RANDOMIZER \xC2\xB7 %s", name);
	}
	page.build = page_build;
	page.step = page_step;
	page.side = page_side;
	page.hit_base = PAGE_HIT;
	cache.checked = 0;
	return ae_pve_menus_rows_open(&page);
}

/* ---------- LOAD A SETUP */

static struct ae_pvem_rows setups_screen;
static struct ae_rnd_setups listed;
static char setup_value[AE_FLOW_ROWS][48], setup_help[AE_FLOW_ROWS][120], setup_head[48];
static int delete_index = -1;

static void setups_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each, n = 0;

	setups_read(&listed);
	snprintf(setup_head, sizeof(setup_head), "SAVED ON THIS PC (%d OF %d)", listed.count, AE_RND_SETUPS);
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, setup_head, NULL, NULL, NULL };
	for (each = 0; each < listed.count; each++)
	{
		setup_summary(&listed.setup[each], setup_value[n], sizeof(setup_value[0]));
		snprintf(setup_help[n], sizeof(setup_help[0]), "{A} loads %s onto the pages. {X} deletes it.", listed.setup[each].name);
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, listed.setup[each].name, setup_value[n], setup_help[n], NULL };
		n++;
	}
	flow->count = (short)n;
	if (listed.count < 1)
		flow->count = 1;
}

static void setups_step(struct ae_pvem_rows *r, int row, int direction)
{
	int each;

	(void)direction;
	if (row < 1 || row > listed.count)
		return;
	for (each = 0; each < AE_RND_SETUP_KEYS; each++)
		if (ae_rnd_setup_key(each)[0] && listed.setup[row - 1].value[each][0])
			write_value(ae_rnd_setup_key(each), listed.setup[row - 1].value[each]);
	platform_log("ae randomizer: loaded setup %s", listed.setup[row - 1].name);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_pve_menus_rows_close(r);
	if (ae_ui_holds(&hub))
	{
		hub.flow.focus = 2;
		cache.checked = 0;
	}
}

static void delete_picked(short choice, void *context)
{
	(void)context;
	if (choice == 1 && delete_index >= 0)
	{
		struct ae_rnd_setups setups;

		setups_read(&setups);
		if (ae_rnd_setups_remove(&setups, delete_index))
		{
			setups_write(&setups);
			platform_log("ae randomizer: setup deleted");
		}
		ae_sound_request(AE_SOUND_BACK, 0);
	}
	delete_index = -1;
}

static void delete_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;
	static char title[64], body[200];

	if (delete_index < 0 || delete_index >= listed.count)
		return;
	snprintf(title, sizeof(title), "DELETE %s?", listed.setup[delete_index].name);
	snprintf(body, sizeof(body), "The setup is deleted from this PC. What the randomizer is set to now stays. This cannot be undone.");
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "DELETE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.picked = delete_picked;
	spec.context = r;
	platform_log("ae randomizer: delete dialog %s", ae_dialog_open(&spec, AE_OWNER_ANY) ? "opened" : "refused");
}

static void setups_x(struct ae_pvem_rows *r)
{
	short focus = r->flow.focus;

	if (focus >= 1 && focus <= listed.count)
	{
		delete_index = focus - 1;
		setups_screen.dialog_pending = 1;
	}
}

static void setups_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	short focus = flow->focus;
	char text[96];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 500.0f);
	if (focus < 1 || focus > listed.count)
	{
		ae_pve_menus_ui_paragraph(flow, 16.0f, 24.0f, x, top + 230.0f * u, width, 3, AE_COLOR_MUTED, "No setup is saved yet. SAVE THIS SETUP keeps the pages as they are.");
		return;
	}
	{
		struct ae_rnd_setup const *setup = &listed.setup[focus - 1];
		struct ae_rnd_facts facts = { setup_get, NULL, 0, 0, 0 };

		facts.context = (void *)setup;
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, setup->name);
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		ae_rnd_ai_summary(&facts, text, sizeof(text));
		fact(flow, x, width, 282.0f, "AI REPLACEMENT", text, AE_COLOR_TEXT);
		ae_rnd_weapon_summary(&facts, 0, text, sizeof(text));
		fact(flow, x, width, 320.0f, "AI WEAPONS", text, AE_COLOR_TEXT);
		ae_rnd_weapon_summary(&facts, 1, text, sizeof(text));
		fact(flow, x, width, 358.0f, "WEAPON PICKUPS", text, AE_COLOR_TEXT);
		fact(flow, x, width, 396.0f, "PLAYER LOADOUT", ae_rnd_loadout_summary(&facts), AE_COLOR_TEXT);
	}
}

static int setups_open_scene(int delete_dialog)
{
	if (ae_ui_holds(&setups_screen))
		ae_pve_menus_rows_close(&setups_screen);
	memset(&setups_screen, 0, sizeof(setups_screen));
	setups_screen.name = "randomizer setups";
	setups_screen.title = "LOAD A SETUP";
	snprintf(setups_screen.crumb, sizeof(setups_screen.crumb), "RANDOMIZER");
	setups_screen.build = setups_build;
	setups_screen.step = setups_step;
	setups_screen.x_action = setups_x;
	setups_screen.x_word = "Delete";
	setups_screen.side = setups_side;
	setups_screen.dialog_open = delete_open;
	setups_screen.hit_base = SETUPS_HIT;
	if (delete_dialog)
	{
		delete_index = 0;
		setups_screen.dialog_pending = 1;
	}
	if (!ae_pve_menus_rows_open(&setups_screen))
		return 0;
	if (setups_screen.flow.focus < 1 && listed.count > 0)
		setups_screen.flow.focus = 1;
	return 1;
}

static int setups_open(void)
{
	return setups_open_scene(0);
}

/* ---------- ITEM SET */

static struct ae_pvem_rows items_screen;
static struct ae_pve_item_set item_sets[AE_PVE_ITEM_SETS];
static int item_count;
static char item_value[AE_FLOW_ROWS][48], item_help[AE_FLOW_ROWS][200];

static void items_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each, played = ae_pve_item_set_played(), none_on = !config_boolean("game.tag_injection") || played < 0, n = 0;

	snprintf(item_value[0], sizeof(item_value[0]), "%s", none_on ? "IN USE" : "THE CAMPAIGN'S OWN");
	snprintf(item_help[0], sizeof(item_help[0]), "{A} plays the mission with its own enemies and weapons only.");
	flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NONE", item_value[0], item_help[0], NULL };
	for (each = 0; each < item_count && n < AE_FLOW_ROWS; each++)
	{
		snprintf(item_value[n], sizeof(item_value[0]), "%s%ld ITEMS ON", !none_on && played == each ? "IN USE " DOT " " : "", item_sets[each].items_on);
		snprintf(item_help[n], sizeof(item_help[0]), "{A} plays the mission with %s's items: its weapons and actors join the pools, and the pools get an ITEM SET choice.", item_sets[each].name);
		flow->rows[n] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, item_sets[each].name, item_value[n], item_help[n],
			item_sets[each].items_on > 0 ? NULL : "NO ITEMS ON" };
		n++;
	}
	flow->count = (short)n;
}

static void items_step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)direction;
	if (row == 0)
	{
		write_value("game.randomizer_tag_profile", "");
		write_value("game.tag_injection", "false");
	}
	else if (row >= 1 && row <= item_count)
	{
		write_value("game.randomizer_tag_profile", item_sets[row - 1].folder);
		write_value("game.tag_injection", "true");
	}
	else
		return;
	cache.checked = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_pve_menus_rows_close(r);
}

static void items_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	short focus = flow->focus;
	char text[48];

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 420.0f);
	if (focus >= 1 && focus <= item_count)
	{
		long weapons, actors;

		ae_pve_item_set_counts(focus - 1, &weapons, &actors);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, item_sets[focus - 1].name);
		ae_draw_rect(x, top + 250.0f * u, width, u, 0.0f, AE_COLOR_RULE);
		snprintf(text, sizeof(text), "%ld", weapons);
		fact(flow, x, width, 282.0f, "WEAPONS", text, AE_COLOR_TEXT);
		snprintf(text, sizeof(text), "%ld", actors);
		fact(flow, x, width, 320.0f, "ACTORS", text, AE_COLOR_TEXT);
	}
	else
		ae_pve_menus_ui_paragraph(flow, 16.0f, 24.0f, x, top + 230.0f * u, width, 4, AE_COLOR_MUTED, "The mission plays with its own enemies and weapons only.");
	ae_pve_menus_ui_paragraph(flow, 15.0f, 22.0f, x, top + 470.0f * u, width, 3, AE_COLOR_MUTED, "Alone and in split screen only.");
}

static int items_open(void)
{
	if (ae_ui_holds(&items_screen))
		ae_pve_menus_rows_close(&items_screen);
	memset(&items_screen, 0, sizeof(items_screen));
	item_count = ae_pve_item_sets(item_sets, AE_PVE_ITEM_SETS);
	items_screen.name = "randomizer item set";
	items_screen.title = "ITEM SET";
	snprintf(items_screen.crumb, sizeof(items_screen.crumb), "RANDOMIZER");
	items_screen.build = items_build;
	items_screen.step = items_step;
	items_screen.side = items_side;
	items_screen.hit_base = ITEMS_HIT;
	return ae_pve_menus_rows_open(&items_screen);
}

/* ---------- NAME THIS SETUP */

static struct
{
	struct ae_flow_screen flow;
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;
	char error[96], error_for[AE_TEXT_MAXIMUM];
} naming;

static struct ae_screen_class const naming_class;

static void naming_save(void)
{
	struct ae_rnd_setups setups;
	struct ae_rnd_setup setup;
	char name[AE_RND_SETUP_NAME + 1];

	snprintf(name, sizeof(name), "%.*s", AE_RND_SETUP_NAME, naming.text.text);
	if (!ae_rnd_setup_name_clean(name))
	{
		snprintf(naming.error, sizeof(naming.error), "A name needs a letter or a digit.");
		snprintf(naming.error_for, sizeof(naming.error_for), "%s", naming.text.text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	setups_read(&setups);
	setup_capture(name, &setup);
	ae_rnd_setups_keep(&setups, &setup);
	setups_write(&setups);
	platform_log("ae randomizer: saved setup %s", name);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_remove(&naming);
}

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	naming.typing = 0;
}

static void typing_begin(void)
{
	naming.edit.done = typing_done;
	naming.edit.context = NULL;
	naming.edit.holder = &naming;
	ae_field_edit_begin(&naming.edit, &naming.text);
	ae_text_select_all(&naming.text);
	naming.typing = 1;
	naming.caret_since = ae_motion_now();
}

static void keyboard_done(int keep, void *context)
{
	(void)context;
	naming.keyboard_open = 0;
	if (naming.edit.active && naming.edit.text == &naming.text)
	{
		naming.typing = 1;
		naming.caret_since = ae_motion_now();
		return;
	}
	if (keep)
		naming_save();
}

static void keyboard_open_now(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.text = &naming.text;
	spec.field = naming.well;
	spec.bounds = naming.flow.frame.rect;
	spec.density = naming.flow.density;
	spec.done = keyboard_done;
	naming.edit.done = typing_done;
	naming.edit.context = NULL;
	naming.edit.holder = &naming;
	spec.edit = &naming.edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		naming.keyboard_open = 1;
}

static void naming_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &naming.flow;
	struct ae_density const *d = &flow->density;
	struct ae_field field;
	float x = ae_flow_left_x(flow), width = ae_flow_right_x(flow) - x, y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	char expanded[160];
	int error;

	(void)instance;
	ae_flow_full_density(flow);
	ae_flow_title(flow, "NAME THIS SETUP", "RANDOMIZER");
	error = naming.error[0] && !strcmp(naming.error_for, naming.text.text);
	if (!error)
		naming.error[0] = 0;
	memset(&field, 0, sizeof(field));
	field.label = "NAME";
	field.placeholder = "up to 24 letters or digits";
	field.text = &naming.text;
	field.flags = AE_FIELD_FOCUSED | (naming.typing ? AE_FIELD_EDITING : 0) | (error ? AE_FIELD_ERROR : 0);
	field.error = error ? naming.error : NULL;
	field.caret_since = naming.caret_since;
	ae_field_well(d, x, y, width, &field, &naming.well);
	y += ae_widget_field(d, x, y, width, &field, NAME_FIELD) + ae_flow_u(flow, 18.0f);
	ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		ae_help_expand("A name already saved is replaced. Every page's settings are kept, GO CRAZY's not.", expanded, sizeof(expanded)));
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, naming.text.length ? "Save" : "Type it", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void naming_update(struct ae_screen *instance)
{
	(void)instance;
	if (naming.typing && naming.edit.active && !ae_field_edit_keys(&naming.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int naming_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (naming.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
		{
			naming.typing = 0;
			ae_field_edit_end(&naming.edit, 1);
			naming_save();
		}
		else if (event->action == AE_ACTION_BACK)
		{
			naming.typing = 0;
			ae_field_edit_end(&naming.edit, 0);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
	case AE_ACTION_START:
		if (naming.text.length)
			naming_save();
		else if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
			typing_begin();
		else
			keyboard_open_now();
		return 1;
	case AE_ACTION_X:
		if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
			typing_begin();
		else
			keyboard_open_now();
		return 1;
	default:
		return 1;
	}
}

static void naming_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(naming.flow.prompts, naming.flow.prompt_count, pointer, NAME_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !naming.typing)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
		{
			event.device = AE_DEVICE_KEYBOARD_MOUSE;
			naming_handle(instance, &event);
		}
		return;
	}
	if (pointer->left_clicks && !naming.typing && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == NAME_FIELD)
		typing_begin();
}

static void naming_leave(struct ae_screen *instance)
{
	(void)instance;
	if (naming.typing)
	{
		naming.typing = 0;
		ae_field_edit_end(&naming.edit, 0);
	}
}

static struct ae_screen_class const naming_class =
{
	.name = "randomizer name", .leave = naming_leave, .handle = naming_handle, .draw = naming_draw, .pointer = naming_pointer,
	.update = naming_update,
};

static int name_open_text(const char *initial)
{
	memset(&naming, 0, sizeof(naming));
	naming.flow.hit_rows = NAME_ROWS;
	naming.flow.hit_prompts = NAME_PROMPTS;
	ae_text_init(&naming.text, initial, AE_RND_SETUP_NAME, 1);
	if (!ae_ui_push(&naming_class, AE_OWNER_ANY, &naming))
		return 0;
	platform_log("ae randomizer name: open");
	return 1;
}

static int name_open(void)
{
	return name_open_text("");
}

/* ---------- staged scenes (DEV MODE) */

/* three setups when none is saved, so the list has something to show */
static void stage_setups(void)
{
	struct ae_rnd_setups setups;
	static const char *const names[3] = { "NEEDLER NIGHTMARE", "SNIPER SAFARI", "ALL ELITES" };
	int each;

	setups_read(&setups);
	if (setups.count > 0)
		return;
	for (each = 0; each < 3; each++)
	{
		struct ae_rnd_setup setup;
		int key;

		setup_capture(names[each], &setup);
		for (key = 0; key < AE_RND_SETUP_KEYS; key++)
		{
			const char *name = ae_rnd_setup_key(key);

			if (!strcmp(name, "game.randomizer_enemies") || (each == 1 && !strcmp(name, "game.randomizer_pickups")) ||
				(each == 0 && !strcmp(name, "game.randomizer_enemy_weapons")))
				snprintf(setup.value[key], sizeof(setup.value[key]), "true");
		}
		ae_rnd_setups_keep(&setups, &setup);
	}
	setups_write(&setups);
}

int ae_screen_randomizer_open_staged(const char *scene)
{
	if (!scene)
		return 0;
	if (strcmp(scene, "hub-off"))
		write_value("game.randomizer", "true");
	if (!strcmp(scene, "hub-off"))
		write_value("game.randomizer", "false");
	if (!strcmp(scene, "ai"))
	{
		write_value("game.randomizer_enemies", "true");
		write_value("game.randomizer_pool", "one");
		write_value("game.randomizer_type", "elite");
		write_value("game.randomizer_elite_rank", "zealot");
		write_value("game.randomizer_allies", "true");
		write_value("game.randomizer_ally_pool", "as_enemies");
		write_value("game.randomizer_extra_enemies", "2");
	}
	else if (!strcmp(scene, "weapons"))
	{
		write_value("game.randomizer_enemy_weapons", "true");
		write_value("game.randomizer_weapon_pool", "precision");
	}
	else if (!strcmp(scene, "pickups"))
	{
		write_value("game.randomizer_pickups", "true");
		write_value("game.randomizer_pickup_pool", "one");
		write_value("game.randomizer_pickup_type", "shotgun");
	}
	else if (!strcmp(scene, "loadout"))
	{
		write_value("game.randomizer_loadout", "set");
		write_value("game.randomizer_loadout_primary", "sniper_rifle");
		write_value("game.randomizer_loadout_secondary", "pistol");
	}
	else if (!strcmp(scene, "loadout-ae"))
	{
		write_value("game.randomizer_loadout", "set");
		write_value("game.randomizer_loadout_primary", "ae:12");
		write_value("game.randomizer_loadout_secondary", "ae:13");
	}
	else if (!strcmp(scene, "loadout-set"))
	{
		write_value("game.randomizer_loadout", "set");
		write_value("game.randomizer_loadout_primary", "tag:5eed0001");
		write_value("game.randomizer_loadout_secondary", "none");
	}
	staged_set_weapons = !strcmp(scene, "loadout-set");
	cache.checked = 0;
	if (!strcmp(scene, "hub") || !strcmp(scene, "hub-off"))
		return hub_open();
	if (!strcmp(scene, "ai"))
		return page_open(AE_RND_PAGE_AI);
	if (!strcmp(scene, "weapons"))
		return page_open(AE_RND_PAGE_WEAPONS);
	if (!strcmp(scene, "pickups"))
		return page_open(AE_RND_PAGE_PICKUPS);
	if (!strcmp(scene, "loadout") || !strcmp(scene, "loadout-ae") || !strcmp(scene, "loadout-set"))
		return page_open(AE_RND_PAGE_LOADOUT);
	if (!strcmp(scene, "setups") || !strcmp(scene, "delete"))
	{
		stage_setups();
		return setups_open_scene(!strcmp(scene, "delete"));
	}
	if (!strcmp(scene, "itemset"))
		return items_open();
	if (!strcmp(scene, "name"))
		return name_open_text("NEEDLER NIGHTMARE");
	return 0;
}
