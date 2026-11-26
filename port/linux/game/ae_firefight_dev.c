/*
AE_FIREFIGHT_DEV.C

See ae_firefight_dev.h. Logs "dev: menu firefight <scene> opened" lines for the tests.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "ae_firefight_dev.h"
#include "ae_firefight_doors.h"
#include "ae_firefight_doors_rules.h"
#include "ae_firefight_menus_rules.h"
#include "ae_glue_firefight.h"
#include "ae_glue_setup_options.h"
#include "ae_screen_create_game.h"
#include "ae_screen_firefight_content.h"
#include "ae_screen_game_options.h"
#include "ae_screen_skulls.h"

void platform_log(char const *format, ...);

static int word_is(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if ((*a | 32) != (*b | 32))
			return 0;
	return *a == *b;
}

int ae_firefight_dev_menu(struct ae_dev_cmd const *cmd, char *why, int size)
{
	const char *scene = cmd->words >= 2 ? cmd->word[1] : "";
	const char *map = NULL;
	char const *changes[3];
	int at, entry = -1, lan = 0, content_scene = 0, custom = 0, mode = -1, changes_count = 0, ok = 0;
	static const char *const data_words[] = { "ready", "preparing", "needsmaps", "failed" };

	why[0] = 0;
	if (word_is(scene, "data"))
	{
		int state;

		if (cmd->words >= 3 && word_is(cmd->word[2], "auto"))
			ae_firefight_doors_data_force(-1);
		else
		{
			for (state = 0; state < 4 && !(cmd->words >= 3 && word_is(cmd->word[2], data_words[state])); state++)
				;
			if (state == 4)
			{
				snprintf(why, (size_t)size, "data ready|preparing|needsmaps|failed|auto");
				return 0;
			}
			ae_firefight_doors_data_force(state);
		}
		platform_log("dev: menu firefight data %s", cmd->words >= 3 ? cmd->word[2] : "");
		return 1;
	}
	for (at = 2; at < cmd->words; at++)
	{
		if (word_is(cmd->word[at], "variant") && at + 1 < cmd->words)
			entry = atoi(cmd->word[++at]) - 1;
		else if (word_is(cmd->word[at], "map") && at + 1 < cmd->words)
			map = cmd->word[++at];
		else if (word_is(cmd->word[at], "mode") && at + 1 < cmd->words)
		{
			at++;
			mode = word_is(cmd->word[at], "include") ? AE_FFM_MODE_INCLUDE : word_is(cmd->word[at], "only") ? AE_FFM_MODE_ONLY :
				AE_FFM_MODE_VANILLA;
		}
		else if (word_is(cmd->word[at], "change") && at + 1 < cmd->words && changes_count < 3)
			changes[changes_count++] = cmd->word[++at];
		else if (word_is(cmd->word[at], "lan"))
			lan = 1;
		else if (word_is(cmd->word[at], "content"))
			content_scene = 1;
		else if (word_is(cmd->word[at], "custom"))
			custom = 1;
		else
		{
			snprintf(why, (size_t)size, "what is %s?", cmd->word[at]);
			return 0;
		}
	}
	if (word_is(scene, "lobby"))
		ok = ae_screen_create_game_firefight_open_staged((short)entry, map, lan, content_scene);
	else if (word_is(scene, "multiplayer"))
	{
		/* (the multiplayer CREATE GAME as the menu opens it, right after a Firefight lobby: the Firefight copy is put away first) */
		ok = ae_screen_create_game_from_menu();
		if (ok)
		{
			char gametype[16];

			ae_setup_edit_name(gametype, sizeof(gametype));
			platform_log("dev: menu firefight multiplayer: CREATE GAME opened, gametype '%s', Firefight copy %s", gametype,
				ae_setup_edit_is_firefight() ? "STILL OPEN" : "gone");
		}
	}
	else
	{
		/* (the rules, skulls and content screens ask for a Firefight copy: begun here when no lobby is open) */
		if (!ae_setup_edit_is_firefight() && !ae_setup_edit_begin_firefight(entry >= 0 ? entry : 0))
		{
			snprintf(why, (size_t)size, "no Firefight variant to edit");
			return 0;
		}
		if (word_is(scene, "rules"))
		{
			ok = ae_screen_game_options_dev("all", changes, changes_count, why, size);
			if (ok)
				why[0] = 0;
		}
		else if (word_is(scene, "skulls"))
			ok = ae_screen_skulls_open_firefight();
		else if (word_is(scene, "objectives"))
			ok = ae_screen_firefight_objectives_open();
		else if (word_is(scene, "content") || word_is(scene, "tags") || word_is(scene, "loadout"))
		{
			unsigned char bits = 0;

			/* (the made-up map's content: its three kinds, or the map named) */
			if (mode >= 0)
				bits = ae_ffm_content_with_mode(bits, mode);
			else
				bits = ae_ffm_content_with_mode(bits, AE_FFM_MODE_INCLUDE);
			if (custom)
				bits = ae_ffm_content_with_choice(bits, AE_FFM_KIND_WEAPONS, AE_FFM_CHOICE_CUSTOM);
			ae_setup_edit_firefight_content_set(bits);
			ok = ae_screen_firefight_open_staged(scene, map ? map : AE_FFG_DEV_MAP, map ? map : "BLOOD GULCH");
		}
		else
		{
			snprintf(why, (size_t)size, "no scene %s (lobby, multiplayer, rules, skulls, content, tags, loadout, objectives, data)", scene);
			return 0;
		}
	}
	if (ok)
		platform_log("dev: menu firefight %s opened", scene);
	else if (!why[0])
		snprintf(why, (size_t)size, "%s did not open", scene);
	return ok;
}
