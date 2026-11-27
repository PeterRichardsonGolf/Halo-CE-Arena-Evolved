/*
FORGE_MAP_SETTINGS.H

A Forge map's general settings (port/forge/src/forge_map_settings.c): its
display name, its sky, the players' bipeds and first-person hands (red team,
blue team, free for all) and its atmosphere (fog). Held down, the palette's
button opens them (forge_3d_mode.c); a saved map keeps them on lines of its
trailer (MAP_SETTINGS|...), each tag by its path, read as the level loads, on
every machine. In a Forge session the host alone changes them, and tells its
clients (!FG... packets, a tag by its index: their tag tables are the host's).
*/

#ifndef __FORGE_MAP_SETTINGS_H
#define __FORGE_MAP_SETTINGS_H

#include <stdint.h>

/* ---------- constants */

#define FORGE_MAP_DISPLAY_NAME_MAX 40
#define FORGE_MAP_SETTINGS_PATH_MAX 256

/* a saved map's header holds its display name (the map lists read it
there, custom_edition_cache.c): 'FGDN', then the name, before the trailer's
offsets at 0x1E4 */
#define FORGE_DISPLAY_NAME_HEADER_OFFSET 0x1A0u
#define FORGE_DISPLAY_NAME_HEADER_SIZE 0x44u
#define FORGE_DISPLAY_NAME_MAGIC 0x4E444746u /* "FGDN" */

/* the players the bipeds and hands are chosen for */
enum forge_map_settings_slot
{
	_forge_map_slot_red = 0,
	_forge_map_slot_blue,
	_forge_map_slot_ffa,
	NUMBER_OF_FORGE_MAP_SLOTS
};

enum forge_map_atmosphere
{
	_forge_atmosphere_map_default = 0,
	_forge_atmosphere_clear,
	_forge_atmosphere_light_fog,
	_forge_atmosphere_heavy_fog,
	_forge_atmosphere_flood_fog,
	_forge_atmosphere_custom,
	NUMBER_OF_FORGE_ATMOSPHERES
};

/* behind the fog, the sky: hidden when the fog is thick enough to hide
everything (AUTO), always shown, or never */
enum forge_map_sky_visibility
{
	_forge_sky_auto = 0,
	_forge_sky_shown,
	_forge_sky_hidden,
	NUMBER_OF_FORGE_SKY_VISIBILITIES
};

/* how a slot's biped moves: as the map's own player biped (a Spartan)
does, the default: its player physics (strafing, the player's speeds),
jump, slopes, turning and landings, copied onto the biped's tag in memory;
or as its tag has it. (Saved maps of before wrote 1 for a Spartan's, and
nothing for the biped's own, which reads as the default now) */
enum forge_map_movement
{
	_forge_movement_default = 0,
	_forge_movement_spartan,
	_forge_movement_own,
	NUMBER_OF_FORGE_MOVEMENTS
};

/* a slot's CONFIG: its own biped, hands and movement (CUSTOM), or another
slot's (1 + that slot: red, blue, free for all) */
enum
{
	_forge_link_custom = 0
};

/* what can be chosen for a tag setting: skies, bipeds or hands */
enum forge_map_settings_kind
{
	_forge_map_kind_sky = 0,
	_forge_map_kind_biped,
	_forge_map_kind_hands,
	NUMBER_OF_FORGE_MAP_KINDS
};

/* ---------- structures */

struct forge_map_settings
{
	char display_name[FORGE_MAP_DISPLAY_NAME_MAX + 1];
	/* (each "": the map's own) */
	char sky[FORGE_MAP_SETTINGS_PATH_MAX];
	char biped[NUMBER_OF_FORGE_MAP_SLOTS][FORGE_MAP_SETTINGS_PATH_MAX];
	char hands[NUMBER_OF_FORGE_MAP_SLOTS][FORGE_MAP_SETTINGS_PATH_MAX];
	int movement[NUMBER_OF_FORGE_MAP_SLOTS];
	/* (each _forge_link_custom, or 1 + the slot it plays) */
	int link[NUMBER_OF_FORGE_MAP_SLOTS];
	int atmosphere;
	float fog_color[3];
	float fog_density; /* 0 to 1 */
	float fog_start; /* world units */
	float fog_opaque;
	int fog_indoors;
	int sky_visibility;
	/* how far the map is drawn (world units; 0: the game's, 1024), for a big
	map; and how fast the Monitor flies in Forge (a multiple; 0: 1) */
	float render_distance;
	float monitor_speed;
};

/* ---------- prototypes/FORGE_MAP_SETTINGS.C */

/* the settings (read only: the setters below tell the clients and apply
them) */
struct forge_map_settings const *forge_map_settings_get(void);
/* any of them changed from the map's own */
int forge_map_settings_any(void);

/* a level loading (randomizer.c, as its tags are chosen): a saved Forge
map's trailer text, its settings read from it; NULL for none (any other
level, and a level going away) */
void forge_map_settings_level_loading(char const *saved_state_text);

/* a slot's player biped, hands (paths; "" the map's own) and movement a
level's profile plays, under any a Forge map has of its own: a campaign
profile's player (each slot), a multiplayer profile's teams (tag_campaign.c,
tag_multiplayer.c, after the level's tags load); forgotten as the next level
loads */
void forge_map_settings_players_set(int slot, int link, char const *biped, char const *hands, int movement);
/* one body for every slot (a set's YOUR BODY: ae_set_body.c): the biped's path, its hands' ("path" or "path#map"; NULL: the hands of
the biped's map in `profile` that fit it best, "" the campaign's own) and whether it moves as its own tag has it, else as a Spartan.
An empty biped clears the layer */
void forge_map_settings_players_body(long profile, char const *biped, char const *hands, int movement_own);
/* a profile's player settings, from its options (`option` gives the value
of a key: tag_campaign.c, tag_multiplayer.c): a campaign profile's
player_biped, player_hands and player_movement for each slot, or a
multiplayer profile's biped_red ... movement_ffa and link_red ... link_ffa
(another team's name: "red", "blue", "ffa"; "" its own). A movement is
"own" for the biped's own, anything else a Spartan's */
void forge_map_settings_players_from_options(char const *(*option)(void *context, char const *key), void *context,
	int campaign);

/* the trailer's lines (forge_cache_builder.c): how many bytes */
int forge_map_settings_save_lines(char *text, unsigned long size);
/* the tags the settings name, for the map's bake: how many */
int forge_map_settings_tags(uint32_t *groups, char const **paths, int capacity);

/* a profile's tag of a setting's path (a map's hands named "path#map" are a
profile's tag of that path and map, as profiles of before have them) */
long forge_map_settings_profile_tag(long profile, uint32_t group, char const *name);

/* the menu's (the host's: forge_map_settings_can_edit) */
int forge_map_settings_can_edit(void);
void forge_map_settings_set_display_name(char const *name);
void forge_map_settings_set_tag(int kind, int slot, long tag_index);
void forge_map_settings_set_atmosphere(int atmosphere);
void forge_map_settings_set_movement(int slot, int movement);
/* a slot's CONFIG (_forge_link_custom, or 1 + another slot) */
void forge_map_settings_set_link(int slot, int link);
/* the slot whose settings a slot plays, by the Forge map's links */
int forge_map_settings_link_slot(int slot);
void forge_map_settings_set_fog(float const color[3], float density, float start, float opaque, int indoors,
	int sky_visibility);
void forge_map_settings_reset(void);
/* RENDER DISTANCE (0: the game's) and MONITOR SPEED (0 or 1: as it is) */
void forge_map_settings_set_render_distance(float distance);
void forge_map_settings_set_monitor_speed(float speed);
/* the Monitor's speed's multiple (1 when the map sets none) */
float forge_map_settings_monitor_speed(void);

/* the tags that can be chosen for a kind, the level's: how many, at most
`capacity` into `tags` */
int forge_map_settings_candidates(int kind, long *tags, int capacity);
/* a tag's name in the menu: its path's last part */
void forge_map_settings_tag_label(long tag_index, char *text, unsigned long size);
/* the tag a setting names in the level (NONE: the map's own, or not there) */
long forge_map_settings_tag(int kind, int slot);
char const *forge_map_settings_atmosphere_name(int atmosphere);

/* the network's (forge_network.c): a client asking for them all, and a
!FG packet; TRUE when the packet was the settings' */
void forge_map_settings_net_send_all(long machine_index);
int forge_map_settings_net_receive(char const *text, int from_client);

/* the test spawn (a slot's page's TEST SPAWN): the local player, out of
Forge mode (forge_3d_mode.c leaves it), killed, spawns again as the slot's
biped, with a copy of it standing before it that moves as in a mirror, to
see it from outside; over once Forge mode is back. FALSE when it cannot
start (a client's) */
int forge_map_settings_test_start(int slot);
int forge_map_settings_test_active(void);
/* each frame of a Forge session (forge_3d_update), in Forge mode or not */
void forge_map_settings_test_tick(int forge_mode);

/* ---------- the engine's (players.c, first_person_weapons.c, render.c,
damage.c) */

/* the local player's controls each tick, which the test's copy mirrors */
struct unit_control_data;
void forge_map_settings_player_control(long unit_index, struct unit_control_data const *control);
/* damage the test's copy deals, which hurts no one */
int forge_map_settings_damage_ignored(long owner_object_index, long object_index);

/* the biped a player spawns as */
long forge_map_settings_player_biped(long player_index, long unit_definition_index);
/* the first-person hands of a unit */
long forge_map_settings_hands(long unit_index, long hands_index);
/* the atmosphere's fog over the map's; TRUE when the sky is hidden by it */
struct render_fog;
int forge_map_settings_fog(short local_player_index, int indoors, struct render_fog *fog);

#endif
