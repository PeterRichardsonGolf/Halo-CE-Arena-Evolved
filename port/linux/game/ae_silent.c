/*
AE_SILENT.C

SILENT MOVEMENT and SILENT WEAPONS: the engine side (ae_silent.h; the rules
are ae_silent_rules.c's). The hook lines in upstream files call these.

Every impulse sound is recorded with the innermost scope open when it is made
(sound_new_impulse), and every effect too (effect_allocate); an effect's
parts, made by effects_update after its creation, are made inside its own
recorded scope, so a delayed part keeps who made it. The tables are by datum
slot, the whole handle kept: a slot taken by another datum reads as untagged.
Muting is the gain at the listener (update_channel_for_impulse_sound): the
sound is made, ages and stops exactly as before, so the simulation and every
random number stay the same.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"
#include "networking/network_server_manager.h"
#include "objects/objects.h"
#include "units/units.h"
#include "halo_port_limits.h"
#include "../src/port_config.h"

#include "ae_silent.h"
#include "ae_silent_rules.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"

void platform_log(char const *format, ...);

/* the pools' sizes (sound_manager.c's "sounds", effects.c's "effect") */
#define AE_SILENT_SOUNDS 0x200
#define AE_SILENT_EFFECTS 2048
#define AE_SILENT_MACHINES HALO_PORT_MAXIMUM_NETWORK_MACHINES
/* the test runs' counts in the log, at most this often (debug.network_test_silent set) */
#define AE_SILENT_LOG_MS 10000

static struct
{
	struct ae_silent_scopes scopes;
	struct ae_silent_tag sounds[AE_SILENT_SOUNDS];
	struct ae_silent_tag effects[AE_SILENT_EFFECTS];
	/* per role: sounds made with it, started, started muted */
	unsigned int made[NUMBER_OF_AE_SILENT_ROLES];
	unsigned int started[NUMBER_OF_AE_SILENT_ROLES];
	unsigned int muted[NUMBER_OF_AE_SILENT_ROLES];
	/* (and of them, the listener's own: its player's) */
	unsigned int own_started[NUMBER_OF_AE_SILENT_ROLES];
	unsigned int own_muted[NUMBER_OF_AE_SILENT_ROLES];
	unsigned int logged_at;
	unsigned int logged_total;
	/* the host's: each client machine's capability as its join request told it, by the server's record of it (a
	slot of its own), then by the machine slot it was accepted into, and whether it was dropped for having none */
	void const *heard_machine[AE_SILENT_MACHINES];
	signed char heard_capability[AE_SILENT_MACHINES];
	unsigned long heard_next;
	signed char capability[AE_SILENT_MACHINES];
	boolean dropped[AE_SILENT_MACHINES];
} silent;

/* ---- the gametype */

static unsigned char variant_byte(struct game_variant const *variant)
{
	return variant ? (unsigned char)(AE_SILENT_VARIANT_BYTE(variant) & AE_SILENT_BYTE_MASK) : 0;
}

/* the game's (a multiplayer game running; none in the campaign) */
static unsigned char game_byte(void)
{
	return game_engine_running() ? variant_byte(game_engine_get_variant()) : 0;
}

/* ---- tagging */

void ae_silent_scope_begin(int role, long player_index)
{
	ae_silent_scope_push(&silent.scopes, role, player_index);
}

void ae_silent_scope_end(void)
{
	ae_silent_scope_pop(&silent.scopes);
}

static long unit_player(long unit_index)
{
	struct unit_datum *unit = unit_index != NONE ? unit_try_and_get(unit_index) : NULL;

	return unit ? unit->unit.player_index : NONE;
}

void ae_silent_weapon_scope_begin(long weapon_index, boolean ready_or_put_away)
{
	struct object_datum *weapon = weapon_index != NONE ? object_try_and_get(weapon_index) : NULL;

	ae_silent_scope_begin(ae_silent_weapon_role(ready_or_put_away),
		weapon ? unit_player(weapon->object.parent_object_index) : NONE);
}

void ae_silent_unit_action_scope_begin(long unit_index, long action)
{
	ae_silent_scope_begin(ae_silent_unit_action_role((int)action), unit_player(unit_index));
}

void ae_silent_effect_created(long effect_index)
{
	long player;
	int role = ae_silent_scope_role(&silent.scopes, &player);

	ae_silent_table_set(silent.effects, AE_SILENT_EFFECTS, effect_index, role, player);
}

void ae_silent_effect_scope_begin(long effect_index)
{
	long player;
	int role = ae_silent_table_get(silent.effects, AE_SILENT_EFFECTS, effect_index, &player);

	/* (an untagged effect opens a scope of no role: its parts are its own, not a scope around the update's) */
	ae_silent_scope_begin(role, player);
}

void ae_silent_sound_created(long sound_index)
{
	long player;
	int role;

	if (sound_index == NONE)
		return;
	role = ae_silent_scope_role(&silent.scopes, &player);
	ae_silent_table_set(silent.sounds, AE_SILENT_SOUNDS, sound_index, role, player);
	silent.made[role]++;
}

static void log_counts(char const *when)
{
	platform_log("silent sounds (%s): movement made %u, started %u (own %u), muted %u (own %u); weapons made %u, "
		"started %u (own %u), muted %u (own %u); other started %u, muted %u",
		when,
		silent.made[AE_SILENT_ROLE_MOVEMENT], silent.started[AE_SILENT_ROLE_MOVEMENT],
		silent.own_started[AE_SILENT_ROLE_MOVEMENT], silent.muted[AE_SILENT_ROLE_MOVEMENT],
		silent.own_muted[AE_SILENT_ROLE_MOVEMENT],
		silent.made[AE_SILENT_ROLE_WEAPON], silent.started[AE_SILENT_ROLE_WEAPON],
		silent.own_started[AE_SILENT_ROLE_WEAPON], silent.muted[AE_SILENT_ROLE_WEAPON],
		silent.own_muted[AE_SILENT_ROLE_WEAPON],
		silent.started[AE_SILENT_ROLE_NONE], silent.muted[AE_SILENT_ROLE_NONE]);
}

real ae_silent_gain(long sound_index, short listener_index, boolean starting)
{
	long acting_player;
	int role = ae_silent_table_get(silent.sounds, AE_SILENT_SOUNDS, sound_index, &acting_player);
	long listener_player = NONE;
	boolean muted = FALSE;

	if (role != AE_SILENT_ROLE_NONE)
	{
		unsigned char byte = game_byte();

		if (byte || starting)
		{
			listener_player = listener_index >= 0 && listener_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ?
				local_player_get_player_index(listener_index) : NONE;
		}
		if (byte)
			muted = ae_silent_muted(byte, role, acting_player, listener_player) != 0;
	}
	if (starting)
	{
		boolean own = acting_player != NONE && acting_player == listener_player;

		silent.started[role]++;
		if (muted)
			silent.muted[role]++;
		if (own)
		{
			silent.own_started[role]++;
			if (muted)
				silent.own_muted[role]++;
		}
		/* (the test runs' counts as they grow) */
		if (*config_string("debug.network_test_silent"))
		{
			unsigned int now = (unsigned int)system_milliseconds();
			unsigned int total = silent.started[0] + silent.started[1] + silent.started[2];

			if (total != silent.logged_total && now - silent.logged_at >= AE_SILENT_LOG_MS)
			{
				silent.logged_at = now;
				silent.logged_total = total;
				log_counts("so far");
			}
		}
	}
	return muted ? 0.0f : 1.0f;
}

void ae_silent_log_rules(void)
{
	struct game_variant const *variant = game_engine_get_variant();
	unsigned char byte = variant && variant->game_engine_index ? variant_byte(variant) : 0;

	memset(silent.made, 0, sizeof(silent.made));
	memset(silent.started, 0, sizeof(silent.started));
	memset(silent.muted, 0, sizeof(silent.muted));
	memset(silent.own_started, 0, sizeof(silent.own_started));
	memset(silent.own_muted, 0, sizeof(silent.own_muted));
	silent.logged_total = 0;
	platform_log("silent sounds: movement %s, weapons %s%s",
		ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT)),
		ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON)),
		ae_silent_any(byte) ? " (Arena Evolved joiners only)" : "");
}

void ae_silent_log_counts(void)
{
	if (silent.made[AE_SILENT_ROLE_MOVEMENT] || silent.made[AE_SILENT_ROLE_WEAPON] || silent.started[AE_SILENT_ROLE_NONE])
		log_counts("end of map");
}

/* ---- joining */

void ae_silent_join_request_mark(wchar_t *machine_name, short length)
{
	unsigned int loaded = 0;
	int level;
	short slot;

	ae_silent_join_mark((unsigned short *)machine_name, length);
	/* (and which weapon packs this machine has: AE_RIFLE_CAPABILITY, the battle rifle's, or a higher level of
	ae_weapon_slots.h's, in the same place; a host of before reads any capability over 0 as the SILENT options',
	and 2 or more as the battle rifle's) */
	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
		if (ae_weapon_pack_slot_loaded(slot))
			loaded |= AE_WEAPON_SLOT_BIT(slot);
	level = ae_weapon_slots_capability(loaded);
	if (machine_name && length >= 3 && level >= AE_RIFLE_CAPABILITY)
		((unsigned short *)machine_name)[length - 1] = (unsigned short)(AE_SILENT_JOIN_MARK | (unsigned int)level);
}

/* the capability level the game's gametype's weapon packs need (ae_weapon_slots.h; 0 none, AE_RIFLE_CAPABILITY the
battle rifle's alone) */
static int packs_required(struct game_variant const *variant)
{
	return variant ? ae_weapon_slots_required((unsigned int)game_variant_weapon_pack(variant->universal_variant.weapon_pack)) : 0;
}

/* the game's gametype uses weapon packs and the machine has not said it has them all: on a machine without a
pack's tags its weapon is an object of no definition */
static boolean rifle_join_refused(struct game_variant const *variant, int capability)
{
	return capability < packs_required(variant);
}

/* the refusal's code: the battle rifle's alone keeps its own (older builds know it) */
static short packs_rejection_code(struct game_variant const *variant)
{
	return packs_required(variant) > AE_RIFLE_CAPABILITY ? AE_PACKS_REJECTION_CODE : AE_RIFLE_REJECTION_CODE;
}

/* the record's place in the heard table: its own, else a free one, else NONE */
static long heard_slot(void const *client_machine, boolean take)
{
	long index, free_index = NONE;

	for (index = 0; index < AE_SILENT_MACHINES; index++)
	{
		if (silent.heard_machine[index] == client_machine)
			return index;
		if (!silent.heard_machine[index] && free_index == NONE)
			free_index = index;
	}
	if (!take)
		return NONE;
	/* (full, of a server gone's records: the oldest place taken in turn) */
	if (free_index == NONE)
		free_index = (long)(silent.heard_next++ % AE_SILENT_MACHINES);
	silent.heard_machine[free_index] = client_machine;
	return free_index;
}

void ae_silent_join_request_heard(void const *client_machine, wchar_t const *machine_name, short length)
{
	long slot = client_machine ? heard_slot(client_machine, TRUE) : NONE;

	if (slot != NONE)
		silent.heard_capability[slot] = (signed char)ae_silent_join_capability((unsigned short const *)machine_name,
			length);
}

static int machine_capability(long machine_index)
{
	return machine_index >= 0 && machine_index < AE_SILENT_MACHINES ? silent.capability[machine_index] : 0;
}

/* whether a machine of this host's game runs this game: its join request was marked (map sharing's list goes to
those alone: ae_map_share_all.c) */
boolean ae_peer_is_ae(long machine_index)
{
	return machine_capability(machine_index) > 0;
}

boolean ae_silent_join_refused_now(void const *client_machine, long machine_index, struct game_variant const *variant,
	short *refusal_code)
{
	unsigned char byte = variant_byte(variant);
	long slot = client_machine ? heard_slot(client_machine, FALSE) : NONE;

	/* (what its join request told, now the slot's; a machine never heard tells nothing) */
	if (machine_index >= 0 && machine_index < AE_SILENT_MACHINES)
	{
		silent.capability[machine_index] = slot != NONE ? silent.heard_capability[slot] : 0;
		silent.dropped[machine_index] = FALSE;
	}
	if (rifle_join_refused(variant, machine_capability(machine_index)))
	{
		platform_log("weapon pack: refusing machine #%d: it has not the weapon packs the game's gametype uses (it "
			"tells level %d, the game needs %d)", (int)machine_index, machine_capability(machine_index),
			packs_required(variant));
		if (refusal_code)
			*refusal_code = packs_rejection_code(variant);
		return TRUE;
	}
	if (!ae_silent_join_refused(byte, machine_capability(machine_index), 0))
		return FALSE;
	platform_log("silent sounds: refusing machine #%d: it has not Arena Evolved's SILENT options (the game's "
		"movement %s, weapons %s)", (int)machine_index,
		ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT)),
		ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON)));
	if (refusal_code)
		*refusal_code = AE_SILENT_REJECTION_CODE;
	return TRUE;
}

void ae_silent_host_machine(long machine_index, boolean remote_joined, struct game_variant const *variant)
{
	unsigned char byte = variant_byte(variant);

	if (!remote_joined || machine_index < 0 || machine_index >= AE_SILENT_MACHINES || silent.dropped[machine_index])
		return;
	/* (the gametype changed in the lobby to one with the battle rifle) */
	if (rifle_join_refused(variant, machine_capability(machine_index)))
	{
		if (network_game_server_drop_machine(machine_index, packs_rejection_code(variant)))
		{
			silent.dropped[machine_index] = TRUE;
			platform_log("weapon pack: dropping machine #%d: it has not the weapon packs the game's gametype now "
				"uses (it tells level %d, the game needs %d)", (int)machine_index, machine_capability(machine_index),
				packs_required(variant));
		}
		return;
	}
	if (!ae_silent_join_refused(byte, machine_capability(machine_index), 0))
		return;
	if (network_game_server_drop_machine(machine_index, AE_SILENT_REJECTION_CODE))
	{
		silent.dropped[machine_index] = TRUE;
		platform_log("silent sounds: dropping machine #%d: it has not Arena Evolved's SILENT options, which the "
			"game's gametype now has on", (int)machine_index);
	}
}

void ae_silent_join_rejected(word rejection_code)
{
	static wchar_t const text[] = L"This game uses Arena Evolved's SILENT options: join with Arena Evolved "
		L"(version 0.2.3 or later).";

	if (rejection_code == AE_PACKS_REJECTION_CODE)
	{
		static wchar_t const packs_text[] = L"This game uses Arena Evolved's weapons: join with Arena Evolved "
			L"0.3.6 or later, with its packs folder beside the game.";

		platform_log("weapon pack: the host refused this machine: its game uses weapon packs this machine has not");
		display_error_text_when_main_menu_loaded(packs_text);
		return;
	}
	if (rejection_code == AE_RIFLE_REJECTION_CODE)
	{
		static wchar_t const rifle_text[] = L"This game uses Arena Evolved's battle rifle: join with Arena Evolved "
			L"0.2.5 or later, with its packs folder beside the game.";

		platform_log("weapon pack: the host refused this machine: its game uses the battle rifle's pack");
		display_error_text_when_main_menu_loaded(rifle_text);
		return;
	}
	if (rejection_code != AE_SILENT_REJECTION_CODE)
		return;
	platform_log("silent sounds: the host refused this machine: its game has Arena Evolved's SILENT options on");
	display_error_text_when_main_menu_loaded(text);
}

/* ---- the gametype editor's ARENA OPTIONS rows */

#define SPINNER_MOVEMENT "silent_movement_spinner"
#define SPINNER_WEAPONS "silent_weapons_spinner"
#define SPINNER_PRESET "sound_preset_spinner"
/* the preset's values: CUSTOM (the two rows as set), SPLIT SCREEN (both SILENT) */
#define PRESET_CUSTOM 0
#define PRESET_SPLIT_SCREEN 1

static struct
{
	struct widget_instance const *list;
	int creation_time;
	short movement, weapons, preset;
	/* the two rows' values before SPLIT SCREEN was picked (NONE: not known) */
	short before_movement, before_weapons;
} menu = { NULL, 0, 0, 0, 0, NONE, NONE };

static struct widget_instance *spinner_named(struct widget_instance *list, char const *name)
{
	struct widget_instance *row;

	for (row = list ? list->child : NULL; row; row = row->next)
	{
		struct widget_instance *child;

		for (child = row->child; child; child = child->next)
		{
			if (child->type == 2 /* spinner */ && child->name && !strncmp(child->name, name, 31))
				return child;
		}
	}
	return NULL;
}

static short spinner_value(struct widget_instance *spinner, short count)
{
	short value = spinner ? spinner->parameters.list.selected_index : 0;

	return value < 0 ? 0 : value >= count ? (short)(count - 1) : value;
}

static void spinner_set(struct widget_instance *spinner, short value)
{
	if (spinner)
		spinner->parameters.list.selected_index = value;
}

/* the rows as last seen, for the list now shown */
static void menu_remember(struct widget_instance *list, struct widget_instance *movement,
	struct widget_instance *weapons, struct widget_instance *preset)
{
	menu.list = list;
	menu.creation_time = list->creation_time;
	menu.movement = spinner_value(movement, NUMBER_OF_AE_SILENT_MODES);
	menu.weapons = spinner_value(weapons, NUMBER_OF_AE_SILENT_MODES);
	menu.preset = spinner_value(preset, 2);
}

void ae_silent_menu_rows(struct widget_instance *list, struct game_variant *variant, boolean save)
{
	struct widget_instance *movement = spinner_named(list, SPINNER_MOVEMENT);
	struct widget_instance *weapons = spinner_named(list, SPINNER_WEAPONS);
	struct widget_instance *preset = spinner_named(list, SPINNER_PRESET);

	if (!variant || (!movement && !weapons))
		return;
	if (save)
	{
		unsigned char byte = (unsigned char)AE_SILENT_VARIANT_BYTE(variant);

		if (movement)
			byte = ae_silent_byte_set(byte, AE_SILENT_ROLE_MOVEMENT, spinner_value(movement, NUMBER_OF_AE_SILENT_MODES));
		if (weapons)
			byte = ae_silent_byte_set(byte, AE_SILENT_ROLE_WEAPON, spinner_value(weapons, NUMBER_OF_AE_SILENT_MODES));
		AE_SILENT_VARIANT_BYTE(variant) = byte;
		platform_log("menus: the gametype's silent movement %s, silent weapons %s",
			ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT)),
			ae_silent_mode_name(ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON)));
		return;
	}
	spinner_set(movement, (short)ae_silent_mode(variant_byte(variant), AE_SILENT_ROLE_MOVEMENT));
	spinner_set(weapons, (short)ae_silent_mode(variant_byte(variant), AE_SILENT_ROLE_WEAPON));
	spinner_set(preset, ae_silent_split_screen(variant_byte(variant)) ? PRESET_SPLIT_SCREEN : PRESET_CUSTOM);
	menu.before_movement = menu.before_weapons = NONE;
	menu_remember(list, movement, weapons, preset);
}

void ae_silent_menu_update(struct widget_instance *list)
{
	struct widget_instance *movement = spinner_named(list, SPINNER_MOVEMENT);
	struct widget_instance *weapons = spinner_named(list, SPINNER_WEAPONS);
	struct widget_instance *preset = spinner_named(list, SPINNER_PRESET);
	short movement_value, weapons_value, preset_value;

	if (!movement || !weapons || !preset)
		return;
	if (menu.list != list || menu.creation_time != list->creation_time)
	{
		menu.before_movement = menu.before_weapons = NONE;
		menu_remember(list, movement, weapons, preset);
		return;
	}
	movement_value = spinner_value(movement, NUMBER_OF_AE_SILENT_MODES);
	weapons_value = spinner_value(weapons, NUMBER_OF_AE_SILENT_MODES);
	preset_value = spinner_value(preset, 2);
	if (preset_value != menu.preset)
	{
		/* (SPLIT SCREEN picked: both SILENT, the rows' values kept for CUSTOM; CUSTOM picked again: them back, else
		both NORMAL) */
		if (preset_value == PRESET_SPLIT_SCREEN)
		{
			menu.before_movement = movement_value;
			menu.before_weapons = weapons_value;
			spinner_set(movement, AE_SILENT_SILENT);
			spinner_set(weapons, AE_SILENT_SILENT);
		}
		else
		{
			boolean known = menu.before_movement != NONE &&
				!(menu.before_movement == AE_SILENT_SILENT && menu.before_weapons == AE_SILENT_SILENT);

			spinner_set(movement, known ? menu.before_movement : AE_SILENT_NORMAL);
			spinner_set(weapons, known ? menu.before_weapons : AE_SILENT_NORMAL);
		}
		platform_log("menus: SOUND PRESET %s: silent movement %s, silent weapons %s",
			preset_value == PRESET_SPLIT_SCREEN ? "split screen" : "custom",
			ae_silent_mode_name(spinner_value(movement, NUMBER_OF_AE_SILENT_MODES)),
			ae_silent_mode_name(spinner_value(weapons, NUMBER_OF_AE_SILENT_MODES)));
	}
	else if (movement_value != menu.movement || weapons_value != menu.weapons)
	{
		/* (a row moved: the preset shows what the two rows make) */
		spinner_set(preset, movement_value == AE_SILENT_SILENT && weapons_value == AE_SILENT_SILENT ?
			PRESET_SPLIT_SCREEN : PRESET_CUSTOM);
		platform_log("menus: silent movement %s, silent weapons %s: SOUND PRESET %s",
			ae_silent_mode_name(movement_value), ae_silent_mode_name(weapons_value),
			spinner_value(preset, 2) == PRESET_SPLIT_SCREEN ? "split screen" : "custom");
	}
	menu_remember(list, movement, weapons, preset);
}

/* ---- tests */

static int mode_named(char const *text, int length)
{
	if (length == 7 && !strncmp(text, "just_me", 7))
		return AE_SILENT_JUST_ME;
	if (length == 6 && !strncmp(text, "silent", 6))
		return AE_SILENT_SILENT;
	return AE_SILENT_NORMAL;
}

void ae_silent_network_test(struct game_variant *variant)
{
	/* (debug.network_test_silent: "MOVEMENT,WEAPONS" or one mode for both, each normal, just_me or silent) */
	char const *text = config_string("debug.network_test_silent");
	char const *comma = strchr(text, ',');
	int movement, weapons;
	unsigned char byte;

	if (!variant || !*text)
		return;
	movement = mode_named(text, comma ? (int)(comma - text) : (int)strlen(text));
	weapons = comma ? mode_named(comma + 1, (int)strlen(comma + 1)) : movement;
	byte = ae_silent_byte_set((unsigned char)AE_SILENT_VARIANT_BYTE(variant), AE_SILENT_ROLE_MOVEMENT, movement);
	AE_SILENT_VARIANT_BYTE(variant) = ae_silent_byte_set(byte, AE_SILENT_ROLE_WEAPON, weapons);
	platform_log("network test: silent movement %s, silent weapons %s", ae_silent_mode_name(movement),
		ae_silent_mode_name(weapons));
}
