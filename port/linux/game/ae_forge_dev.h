/*
AE_FORGE_DEV.H

DEV MODE's `forge` command (ae_dev_cmd.c): a Forge session started on a map from the menus or the console, the Monitor
entered or left, and what the session holds. Forge is in the 64-bit Linux build alone (tools/forge_build.py): any other
build answers that it has none.
*/

#ifndef __AE_FORGE_DEV_H
#define __AE_FORGE_DEV_H

/* forge start <map>: a Forge session on a level alone (a level's leaf name, `bloodgulch`, `b30`, or its path or a
Custom Edition map's name); the text says what happened. 1 when it started, 0 when it did not, -1 when Forge's tags
are not built yet: they are asked for, and the command is asked again once the pack is ready */
int ae_forge_dev_start(char const *map, char *text, int size);

/* forge host <map> [variant] / forge join <a.b.c.d:port>: a Forge game hosted, or joined, as the network test does (debug.network_test "host:<map>:<variant>"
or "join", HALO_NETWORK_TEST_JOIN_ADDRESS), from the menus: the session's flag on, so the game's version is Forge's. 0 with why in the text */
int ae_forge_dev_host(char const *map, char const *variant, char *text, int size);
int ae_forge_dev_join(char const *address, char *text, int size);

/* forge ready: 1 when a staged scene is up (the session on, Forge up on the level, no edit queued, no mode asked for left), -1 while it is being made
(the text says what for), 0 with no session or no Forge */
int ae_forge_dev_ready(char *text, int size);

/* forge mode monitor|play: the session's mode, taken at the next Forge update as the toggle key does */
int ae_forge_dev_mode(int monitor, char *text, int size);

/* forge status: one line, the session's mode, what is placed, the base map's tag count and the network version */
int ae_forge_dev_status(char *text, int size);

/* Forge's update asks (Forge's toggle of the Monitor) whether the console asked for the other mode than `mode`: 1 once
per request */
int ae_forge_dev_mode_toggle(int mode);

/* forge place <tag path> [x y z [yaw]] / copy <n> / delete <n> / grab <n> / snap <0|15|45|90> / list: the editing done as the tools
do it, asked here and carried out at the next Forge update, once the Monitor flies (the result is a "forge dev:" line in the
log); `place` stands at x y z (yaw in degrees), else 5 world units ahead of the Monitor. 0 when the line cannot be queued */
enum { AE_FORGE_DEV_PLACE, AE_FORGE_DEV_COPY, AE_FORGE_DEV_DELETE, AE_FORGE_DEV_GRAB, AE_FORGE_DEV_SNAP, AE_FORGE_DEV_LIST,
	AE_FORGE_DEV_SAVE, AE_FORGE_DEV_GAMETYPE, AE_FORGE_DEV_VOLUME, AE_FORGE_DEV_BIPED, AE_FORGE_DEV_HANDS, AE_FORGE_DEV_FOG,
	AE_FORGE_DEV_TESTSPAWN, AE_FORGE_DEV_AI, AE_FORGE_DEV_ROUTE, AE_FORGE_DEV_KILL, AE_FORGE_DEV_FF, AE_FORGE_DEV_AMMO, AE_FORGE_DEV_MOVE, AE_FORGE_DEV_SEND,
	AE_FORGE_DEV_FACE, AE_FORGE_DEV_WHERE };
int ae_forge_dev_edit(int op, char const *tag, int number, int has_position, float const *position, float yaw_degrees,
	char *text, int size);

/* forge volume <kind> [value [x y z]]: a volume of a kind (`tag`: gravity, kill, damage, hurt or oneway; `number` its strength in hundredths
of the world's gravity or its damage in percent, -1 for the kind's own) made where the Monitor's feet would stand, or at x y z. As the
other edits, done once the Monitor flies, and a mode asked for after it (forge mode play) waits for it. forge settings biped|hands <team>
<name> sets a team's player biped or hands to the level's tag with `tag` in its last name (`number` the team: 0 red, 1 blue, 2 free for
all); forge settings fog <atmosphere> sets the map's atmosphere (`number`: 0 the map's, 1 clear, 2 light, 3 heavy, 4 flood); forge testspawn
<team> begins MAP SETTINGS' TEST SPAWN for the team, which spawns the player as its biped once the Monitor's mode is left */

/* forge ai <actor> <team> [goal [seconds]] (`tag` the actor, `number` ae_forge_rules_ai_pack's team, goal and respawn) places an AI spawn ahead of
the Monitor; forge route <points> (`number` the points) an AI route. Done once the Monitor flies */

/* forge save <name> (`tag` the name, saved as Forge's SAVE MAP does once the session is up) and forge gametype <n> <kind> (`tag`
all, ctf, slayer, oddball, koth or race: the placed object #n is for that gametype only) are edits asked the same way */

/* forge play <name> <gametype>: a game of the saved map, started from the main menu as a TEST RUN's setup starts one; 0 with the
reason in text when there is no such map */
int ae_forge_dev_play(char const *name, char const *gametype, char *text, int size);

/* Forge's twelve binds (0..11, forge_bind_action) as Forge holds them: the Windows virtual key and Forge's pad button;
the two pad speeds (0 look, 1 fly; 1.0 = 100 %). The file is Faber_data/forge_controls.ini, as Forge's own screen writes it.
A build with no Forge holds none: get answers 0 */
int ae_forge_binds_get(int action, int *vk, int *gp);
void ae_forge_binds_set(int action, int vk, int gp);
float ae_forge_pad_get(int which);
void ae_forge_pad_set(int which, float value);
void ae_forge_binds_load(void);
void ae_forge_binds_save(void);
void ae_forge_binds_defaults(void);
/* a bind's key and a pad button as Forge's menus name them ("F1", "D-PAD RIGHT"; "-" for none) */
void ae_forge_bind_names(int vk, int gp, char *key, int key_size, char *pad, int pad_size);

/* whether a Forge session is on */
int ae_forge_dev_session(void);

/* the pause's SAVE MAP: Forge's own save action (its box for a first save's name; later the map is updated at once); 0 when no
session */
int ae_forge_dev_save_map(void);

/* the session just started is the saved map `file`'s (its name kept): the map counts as saved, so the pause's SAVE MAP and QUICK SAVE update
it at once. Taken once Forge is up */
void ae_forge_dev_resave(char const *file);

/* whether this session's map has been saved, its name (as Forge keeps it) into name (size bytes) */
int ae_forge_dev_saved(char *name, int size);

/* whether an object marked for gametypes is in the game now (ae_forge_rules.h's ae_forge_rules_gametype_plays: the running game's engine; a Forge
session shows everything) */
int ae_forge_dev_gametype_plays(unsigned flags);

/* whether the key (a Windows virtual key) is one of Forge's binds in a Forge session on: then TEST RUN and the menus do not read
it (ae_forge_rules.h's ae_forge_rules_key_taken); 0 in a build with no Forge and in any other game */
int ae_forge_dev_key_taken(int vk);

#endif /* __AE_FORGE_DEV_H */
