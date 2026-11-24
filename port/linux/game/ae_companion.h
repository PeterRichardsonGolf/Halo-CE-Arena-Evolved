/*
AE CAMPAIGN COMPANIONS: allied co-op players of a campaign game that the engine's own AI plays.

With game.companions_brain bot (the default) the MP bots' brain drives the body instead (bots.h bots_squad_drive; port/linux/game/
ae_bots_squad.inc): the level's AI enemies are its targets, the nearest human player its leader, and the squad is asked for as the
level starts, so that it is in the game from the intro as co-op players are. The actor drive below is game.companions_brain actor.

A companion is a player of a bot machine (bots.h), added to the game in progress as a late co-op player is: it has a name, a
slot, the team and the co-op spawn and respawn beside the players, and the Spartan body. Its body is given an actor (an
allied marine's variant of the level, for the combat and pathing of the level's own data), and the actor's controls drive the
body in place of the player's. ae_companion_orders runs after an actor's action has set its movement and keeps the companion
near the nearest human player:
  - past FAR the companion moves to the player, fighting or not;
  - past NEAR with no enemy in sight it moves to the player until it is within SETTLE;
  - otherwise it stands (no enemy in sight) or fights where it is (an enemy in sight).
*/
#ifndef AE_COMPANION_H
#define AE_COMPANION_H

#define AE_COMPANION_MOST 4

/* asks for count companions (as bots' players of a campaign game): they join on the server's next turn; the number asked, or 0 with a
reason in text */
int ae_companion_spawn(int count, char *text, unsigned int text_size);

/* the squad the next level started from the menus has from its start (a TEST RUN's campaign test, set before its load; -1:
the setting's); the level's own count once it starts */
void ae_companion_next_level(int count);
/* sends the companions off: they leave the game */
void ae_companion_stop(void);
/* DEV MODE (a TEST RUN's reset between the tests of a kept level): the companion players stay in the game; for the next 2 s a living
body far from the human players (or in a vehicle) is put behind the nearest, the dead respawn as co-op players do, and the brain
forgets its fight and path. The players kept, -1 when none are asked for */
int ae_companion_reset(void);

/* once a game tick: the companion players' bodies are given actors, and the log's samples */
void ae_companion_tick(void);

/* DEV MODE: the companions' bodies die; they come back as a co-op player's does. The number killed */
int ae_companion_kill(void);
/* DEV MODE companion dry: every weapon a companion carries is emptied (loaded and reserve); the count emptied */
int ae_companion_dry(void);
/* DEV MODE companion down: every living human player of this machine killed (PLAYER DOWN's test); the count */
int ae_companion_down_humans(void);
/* the companions with a living body now; -1: none are asked for */
int ae_companion_bodies(void);

/* the server's turn: the players that are to join or leave do */
struct network_game_server;
struct network_game;
void ae_companion_server_tick(struct network_game_server *server, struct network_game *game);

/* an actor's turn, after its action set its orders: the leash, for a companion's actor */
void ae_companion_orders(long actor_index);

/* DEV MODE: the level is won this many seconds after it starts */
void ae_companion_won_after(int seconds);

/* whether a level started alone is to have companions: it is then hosted as the co-op of one machine */
int ae_companion_asked_for_start(void);

/* the level's living AI enemies of the players' team (their units, at most max; the number): the bots' brain's targets */
long ae_companion_foes(long *units, long max);

/* the nearest living human player's unit to a point (its place in at), NONE when none: the bots' brain's leader */
long ae_companion_leader(real_point3d const *from, real_point3d *at);

/* the host's update: the controls of a companion player's body by the bots' brain (game.companions_brain bot); 0 when the player is
no companion's or the actor AI drives it (the action is then left as it is) */
struct player_action;
int ae_companion_action(long absolute_player_index, struct player_action *action);

/* ae_bots_squad.inc: whether a unit is on a deck: an object's collision (a lift, a machine) or narrow ground (a bridge deck, a
ledge: ae_companion_rules.h); and the spot of the first human player's walked track a member at `from` walks to next there:
1 with it, 0 when the member is not on the track */
int ae_companion_on_deck(long unit_index);
int ae_companion_track_goal(real_point3d const *from, real_point3d *goal);

/* the co-op scoreboard (game_engine.c): whether a player's row is the AI squad's (a bot machine's player in a campaign level),
drawn AI SQUAD with no ping and never picked; whether every player not on this machine is the squad's (no one to pick) */
int ae_companion_squad_row(long player_index);
int ae_companion_squad_only(void);

/* ENTRY (ae_companion_rules.h): a squad player who has not spawned in this level waits while the intro holds the players, or
the first human player has no unit, or sits in a vehicle (5 s at most: it then spawns and boards): TRUE while it waits. Spawned, it is put behind the player on walkable
ground. Not on a level whose intro the squad rides (ae_companion_intro_seat) while its cutscene plays */
int ae_companion_spawn_waits(long player_index);

/* the level's intro, on a level of the intro ride's list only: a squad player (bot brain) that has not spawned, while a cutscene
plays and the first human player sits in a vehicle with a free passenger seat, spawns into it: TRUE with the vehicle and the
seat. Never on foot in a cutscene */
int ae_companion_intro_seat(long player_index, long *vehicle, short *seat);
/* the body just spawned takes that seat */
void ae_companion_intro_board(long unit_index, long vehicle, short seat);

/* player_add_equipment, for a body that is a companion's: with game.companions_loadout ae the body takes the AE weapons of its slot
(ae_campaign_weapons.h) in place of the starting profile's. 1 when it did (the profile's weapons are then not given), else 0 */
int ae_companion_loadout(long unit_index, int reset_equipment);

/* the level's intro holds the players: a cinematic plays, its letterbox is up or the players' controls are off. The squad's brain
waits and the squad keeps its seats */
int ae_companion_intro_holds(void);
/* a squad player of a campaign game, which the level's scripts do not count among the players (players.c's script lists): their
player 0 and its followers are the human players */
int ae_companion_script_skips(long player_index);
/* a unit is a companion player's with an actor driving it: players.c and the actor leave its controls to the actor */
int ae_companion_is_driven(long unit_index);
/* PLAYER DOWN (ae_companion_player_down_counts): whether this living player keeps a campaign game going and is one a downed player
comes back beside; every player outside a campaign game */
int ae_companion_player_down_keeps(long player_index);

#endif
