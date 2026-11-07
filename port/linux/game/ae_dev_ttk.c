/*
AE_DEV_TTK.C

DEV MODE's time-to-kill readout (ae_dev_ttk.h; the table: ae_dev_ttk_rules.c). One line for every kill while the ttk command
is on, in the console, in halo.log ("dev ttk: ...") and in the event stream ("ttk": killer, victim, weapon, seconds, hits,
head).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "main/console.h"
#include "tag_files/tag_files.h"
#include "units/units.h"

#include "ae_dev_events.h"
#include "ae_dev_mode.h"
#include "ae_dev_ttk.h"
#include "ae_dev_ttk_rules.h"
#include "ae_view_model.h"

void platform_log(char const *format, ...);

static struct ae_ttk_table table;
static boolean on;
static boolean table_ready;

static void table_make(void)
{
	if (!table_ready)
	{
		ae_ttk_clear(&table);
		table_ready = TRUE;
	}
}

void ae_dev_ttk_set(boolean now_on)
{
	table_make();
	/* (turned on: nobody's earlier hits count) */
	if (now_on && !on)
		ae_ttk_clear(&table);
	on = now_on;
}

boolean ae_dev_ttk_is_on(void)
{
	return on;
}

void ae_dev_ttk_new_game(void)
{
	ae_ttk_clear(&table);
	table_ready = TRUE;
}

/* a player's name as text */
static void player_text(long player_index, char *text, int size)
{
	struct player_datum *player = player_get(player_index);
	int index;

	for (index = 0; index < size - 1 && index < 11 && player->name[index]; index++)
		text[index] = player->name[index] < 128 && player->name[index] >= 32 ? (char)player->name[index] : '?';
	text[index] = 0;
}

void ae_dev_ttk_damage(long owner_player_index, long victim_player_index, long definition_index, boolean headshot,
	float amount, boolean killed, boolean weapon_damage)
{
	struct player_datum *victim;
	int victim_slot, killer_slot;
	long life, tick = game_time_get();
	struct ae_ttk_result result;

	if (!on || !ae_dev_mode_on() || victim_player_index == NONE)
		return;
	table_make();
	victim = player_get(victim_player_index);
	victim_slot = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(victim_player_index);
	killer_slot = owner_player_index == NONE ? -1 : (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(owner_player_index);
	life = victim->unit_index;
	if (amount > 0.0f && owner_player_index != NONE)
		ae_ttk_hit(&table, killer_slot, victim_slot, life, tick, headshot);
	if (!killed)
		return;
	if (ae_ttk_kill(&table, killer_slot, victim_slot, life, tick, &result))
	{
		char killer_name[16], victim_name[16], weapon[AE_VIEW_MODEL_KEY], line[200];
		struct ae_view_model unused;

		player_text(owner_player_index, killer_name, sizeof killer_name);
		player_text(victim_player_index, victim_name, sizeof victim_name);
		/* (a bullet or projectile: the weapon the killer holds, by its pack or tag name; a melee blow or an explosion:
		the damage effect's own tag name) */
		if (!weapon_damage || !ae_view_model_held(owner_player_index, weapon, sizeof weapon, &unused))
		{
			char const *name = definition_index != NONE ? tag_name_strip_path(tag_get_name(definition_index)) : NULL;

			snprintf(weapon, sizeof weapon, "%s", name && *name ? name : "?");
		}
		ae_ttk_format(line, sizeof line, killer_name, victim_name, weapon, &result);
		console_printf(FALSE, "%s", line);
		platform_log("dev ttk: %s", line);
		/* (the killer's own screen: the HUD message list the pickups use, a short line; console_log is none in play) */
		{
			struct player_datum *killer = owner_player_index != NONE ? player_get(owner_player_index) : NULL;

			if (killer && killer->local_player_index != NONE)
			{
				char short_line[96];
				wchar_t wide[96];
				int at;

				ae_ttk_short_format(short_line, sizeof short_line, weapon, &result);
				for (at = 0; short_line[at] && at < 95; at++)
					wide[at] = (wchar_t)(unsigned char)short_line[at];
				wide[at] = 0;
				hud_print_message(killer->local_player_index, wide);
			}
		}
		if (ae_dev_events_on())
			ae_dev_event("ttk", "\"killer\":%d,\"victim\":%d,\"weapon\":\"%s\",\"seconds\":%.2f,\"hits\":%d,\"head\":%d",
				killer_slot, victim_slot, weapon, (double)result.ticks / (double)AE_TTK_TICKS_PER_SECOND, result.hits,
				result.head);
	}
}
