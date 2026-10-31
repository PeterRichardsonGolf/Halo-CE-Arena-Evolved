/* ae_key_bindings.c: Arena Evolved menus, the KEY BINDINGS model (see ae_key_bindings.h). No engine includes. */

#include <stdio.h>
#include <string.h>

#include "ae_key_bindings.h"

/* (CHAT: voice and text; COMMUNICATION, the audit's name, met its rail count at 130 %) */
static const char *const section_names[AE_KEYS_SECTIONS] = { "MOVEMENT", "WEAPONS", "ACTIONS", "CHAT" };

/* (menu_functions.c's controls[], their order and labels; ChupathingyCE's CHAT LOG while its chat is on. Its CHAT ALL
and CHAT TEAM are left out: config.toml leaves them empty so that CHAT and TEAM CHAT open the one chat) */
static struct ae_key_action const actions[] =
{
	{ "controls.move_forward", "MOVE FORWARD", 0, "Move forward.", 0 },
	{ "controls.move_backward", "MOVE BACKWARD", 0, "Move backward.", 0 },
	{ "controls.strafe_left", "STRAFE LEFT", 0, "Step left without turning.", 0 },
	{ "controls.strafe_right", "STRAFE RIGHT", 0, "Step right without turning.", 0 },
	{ "controls.jump", "JUMP", 0, "Jump, and skip cutscenes.", 0 },
	{ "controls.crouch", "CROUCH", 0, "Crouch while it is held.", 0 },
	{ "controls.fire", "FIRE", 1, "Fire your weapon.", 0 },
	{ "controls.throw_grenade", "THROW GRENADE", 1, "Throw a grenade.", 0 },
	{ "controls.melee", "MELEE", 1, "Melee attack.", 0 },
	{ "controls.reload", "RELOAD", 1, "Reload your weapon.", 0 },
	{ "controls.zoom", "ZOOM", 1, "Zoom in, and step through the zoom levels.", 0 },
	{ "controls.switch_weapon", "SWITCH WEAPON", 1, "Switch weapons. WHEEL is either way the wheel turns.", 0 },
	{ "controls.switch_grenade", "SWITCH GRENADE", 1, "Switch between frag and plasma grenades.", 0 },
	{ "controls.action", "ACTION", 2, "Pick up and swap weapons, get in and out of vehicles, press switches.", 0 },
	{ "controls.flashlight", "FLASHLIGHT", 2, "Your flashlight.", 0 },
	{ "controls.scoreboard", "SHOW SCORES", 2, "The scores, while it is held.", 0 },
	{ "controls.pause", "PAUSE MENU", 2, "The pause menu: a controller's START.", 0 },
	{ "controls.screenshot", "SCREENSHOT", 2,
		"A screenshot in screenshots/ beside config.toml. F9 records video and can't be used here.", 0 },
	{ "controls.push_to_talk", "PUSH TO TALK", 3, "Voice chat: you talk while it is held (AUDIO's VOICE CHAT).", 0 },
	{ "controls.chat", "CHAT", 3, "Type a line to everyone in a network game. Enter sends it.", 0 },
	{ "controls.team_chat", "TEAM CHAT", 3, "Type a line to your team. Enter sends it.", 0 },
	{ "controls.chat_log", "CHAT LOG", 3, "ChupathingyCE's chat: its recent lines and its players.", AE_KEYS_WITH_CHUPA_CHAT },
	/* (DEV MODE's cameras, while it is on: ae_dev_camera.c) */
	{ "controls.dev_camera", "DEV CAMERA", 2, "DEV MODE: the third-person camera over the shoulder, on or off.",
		AE_KEYS_WITH_DEV_MODE },
	{ "controls.dev_shoulder", "DEV SHOULDER", 2, "DEV MODE: the third-person camera's other shoulder.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.dev_free_camera", "DEV FREE CAMERA", 2,
		"DEV MODE: the free camera, on or off. It moves and looks as you do; jump and crouch raise and lower it.",
		AE_KEYS_WITH_DEV_MODE },
	/* (TEST RUN's verdict keys, with DEV MODE: ae_test_run_input.c) */
	{ "controls.test_pass", "TEST PASS", 2, "TEST RUN: the verdict PASS for the test on the card.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_fail", "TEST FAIL", 2, "TEST RUN: the verdict FAIL.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_note", "TEST NOTE", 2, "TEST RUN: a note on the test.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_skip", "TEST SKIP", 2, "TEST RUN: skip the test.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_reload", "TEST RELOAD", 2, "TEST RUN: reload the test's setup.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_back", "TEST BACK", 2, "TEST RUN: go back one test.", AE_KEYS_WITH_DEV_MODE },
	{ "controls.test_hide_card", "TEST HIDE CARD", 2, "TEST RUN: hide the card while it is held.", AE_KEYS_WITH_DEV_MODE },
};

short ae_keys_action_count(void)
{
	return (short)(sizeof(actions) / sizeof(actions[0]));
}

struct ae_key_action const *ae_keys_action(short action)
{
	return action >= 0 && action < ae_keys_action_count() ? &actions[action] : NULL;
}

short ae_keys_action_of(const char *key)
{
	short action;

	for (action = 0; key && action < ae_keys_action_count(); action++)
	{
		if (!strcmp(actions[action].key, key))
			return action;
	}
	return -1;
}

const char *ae_keys_section_name(short section)
{
	return section >= 0 && section < AE_KEYS_SECTIONS ? section_names[section] : "";
}

short ae_keys_entries(int with, short *entries, short maximum)
{
	short count = 0, section, action;

	for (section = 0; section < AE_KEYS_SECTIONS; section++)
	{
		int header = 0;

		for (action = 0; action < ae_keys_action_count(); action++)
		{
			if (actions[action].section != section || (actions[action].only_with & ~with))
				continue;
			if (!header && count < maximum)
			{
				entries[count++] = (short)(AE_KEYS_HEADER | section);
				header = 1;
			}
			if (count < maximum)
				entries[count++] = action;
		}
	}
	return count;
}

int ae_keys_entry_is_header(short entry)
{
	return (entry & AE_KEYS_HEADER) != 0;
}

short ae_keys_section_at(short const *entries, short count, short place)
{
	if (place < 0 || place >= count)
		return -1;
	if (ae_keys_entry_is_header(entries[place]))
		return (short)(entries[place] & ~AE_KEYS_HEADER);
	return actions[entries[place]].section;
}

short ae_keys_section_place(short const *entries, short count, short section)
{
	short place;

	for (place = 0; place < count; place++)
	{
		if (entries[place] == (short)(AE_KEYS_HEADER | section))
			return place;
	}
	return -1;
}

short ae_keys_section_rows(short const *entries, short count, short section)
{
	short place, rows = 0;

	for (place = 0; place < count; place++)
	{
		if (!ae_keys_entry_is_header(entries[place]) && actions[entries[place]].section == section)
			rows++;
	}
	return rows;
}

short ae_keys_row_step(short const *entries, short count, short place, short direction)
{
	short at;

	for (at = (short)(place + direction); at >= 0 && at < count; at = (short)(at + direction))
	{
		if (!ae_keys_entry_is_header(entries[at]))
			return at;
	}
	return place;
}

void ae_keys_parse(const char *text, char names[AE_KEYS_SLOTS][AE_KEYS_NAME])
{
	short slot;

	for (slot = 0; slot < AE_KEYS_SLOTS; slot++)
	{
		size_t length;

		names[slot][0] = 0;
		if (!text)
			continue;
		while (*text == ' ')
			text++;
		length = strcspn(text, ",");
		while (length && text[length - 1] == ' ')
			length--;
		if (length >= AE_KEYS_NAME)
			length = AE_KEYS_NAME - 1;
		memcpy(names[slot], text, length);
		names[slot][length] = 0;
		text += strcspn(text, ",");
		if (*text == ',')
			text++;
	}
}

void ae_keys_join(char const names[AE_KEYS_SLOTS][AE_KEYS_NAME], char *text, size_t size)
{
	const char *first = names[0], *second = names[1];

	if (!size)
		return;
	snprintf(text, size, "%s%s%s", first[0] ? first : second, first[0] && second[0] ? ", " : "",
		first[0] ? second : "");
}

static int same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

int ae_keys_overlap(const char *a, const char *b)
{
	if (!a || !b || !a[0] || !b[0])
		return 0;
	if (same_name(a, b))
		return 1;
	/* ("Wheel" is either way the wheel turns) */
	if (same_name(a, "Wheel"))
		return same_name(b, "Wheel Up") || same_name(b, "Wheel Down");
	if (same_name(b, "Wheel"))
		return same_name(a, "Wheel Up") || same_name(a, "Wheel Down");
	return 0;
}

short ae_keys_bind(struct ae_keys *keys, short action, short slot, const char *name, short *from)
{
	short other, other_slot, lost = 0;

	if (from)
		*from = -1;
	if (action < 0 || action >= AE_KEYS_ACTIONS || slot < 0 || slot >= AE_KEYS_SLOTS)
		return 0;
	if (name && name[0])
	{
		for (other = 0; other < AE_KEYS_ACTIONS; other++)
		{
			for (other_slot = 0; other_slot < AE_KEYS_SLOTS; other_slot++)
			{
				if ((other == action && other_slot == slot) || !ae_keys_overlap(keys->names[other][other_slot], name))
					continue;
				keys->names[other][other_slot][0] = 0;
				lost++;
				if (from && other != action && *from < 0)
					*from = other;
			}
		}
	}
	snprintf(keys->names[action][slot], AE_KEYS_NAME, "%s", name ? name : "");
	return lost;
}

int ae_keys_has_any(struct ae_keys const *keys, short action)
{
	short slot;

	for (slot = 0; action >= 0 && action < AE_KEYS_ACTIONS && slot < AE_KEYS_SLOTS; slot++)
	{
		if (keys->names[action][slot][0])
			return 1;
	}
	return 0;
}

void ae_keys_moved_text(const char *shown, const char *from_label, int from_has_none, char *text, size_t size)
{
	if (from_has_none)
		snprintf(text, size, "%s moved here from %s, which has no key now.", shown, from_label);
	else
		snprintf(text, size, "%s moved here from %s.", shown, from_label);
}

void ae_keys_keep_body(const char *const *labels, short count, char *text, size_t size)
{
	char list[200];
	short index, named = count > 3 ? 3 : count;
	size_t length = 0;

	list[0] = 0;
	for (index = 0; index < named; index++)
	{
		const char *joint = index == 0 ? "" : count <= 3 && index == named - 1 ? " and " : ", ";

		length += (size_t)snprintf(list + length, length < sizeof(list) ? sizeof(list) - length : 0, "%s%s", joint,
			labels[index]);
		if (length >= sizeof(list))
			break;
	}
	if (count > 3)
		snprintf(list + strlen(list), sizeof(list) - strlen(list), " and %d more", count - 3);
	snprintf(text, size, "%d action%s new keys on this PC: %s. They are in use already.", count,
		count == 1 ? " has" : "s have", list);
}

void ae_keys_restore_body(const char *section, short section_changes, short all_changes, char *text, size_t size)
{
	if (all_changes <= 0)
		snprintf(text, size, "Every key is already the game's.");
	else if (section_changes <= 0)
		snprintf(text, size, "%s has the game's keys already. Every section: %d action%s back to the game's keys. "
			"You can still undo it as you leave.", section, all_changes, all_changes == 1 ? " goes" : "s go");
	else
		snprintf(text, size, "%s: %d action%s back to the game's keys. Every section: %d action%s. You can still "
			"undo it as you leave.", section, section_changes, section_changes == 1 ? " goes" : "s go", all_changes,
			all_changes == 1 ? "" : "s");
}
