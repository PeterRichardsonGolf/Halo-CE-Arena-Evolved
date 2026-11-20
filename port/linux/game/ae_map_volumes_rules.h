#ifndef AE_MAP_VOLUMES_RULES_H
#define AE_MAP_VOLUMES_RULES_H

/*
AE_MAP_VOLUMES_RULES.H

MAP VOLUMES (0.9.4; owner 11-16 on Dredwerkz: "the lifts don't work i think i should be dead where i am at it is like a
lava room"). Some community maps name trigger volumes for what they do, a pit's floor "Hell", a lift "TPA" to "TPB", and
leave the deed to something outside the map file (their own scripts never use them, so the stock game neither kills nor
lifts). In multiplayer AE does it: a volume named as a kill zone kills a living player in it (the host's), and a lift
volume TP<letter> (A, C, E ...) puts a player in it at the next letter's (TPA to TPB, one way). The bots take the kill
zones for deadly volumes from the map's load. Pure (port/linux/tests/ae_map_volumes_rules_test.c).
*/

enum
{
	AE_MAP_VOLUME_NONE = 0,
	AE_MAP_VOLUME_KILL,
	AE_MAP_VOLUME_LIFT
};

/* a lift is not taken again within this of the last one (a player put at its exit stands clear of it first) */
#define AE_MAP_VOLUME_LIFT_GAP_TICKS 30

/* what a volume of this name does (any case): KILL for hell, lava, kill, killzone, kill_zone, death, deathzone,
death_zone, deadzone; LIFT for TP and a letter of A, C, E ... Y; else NONE */
int ae_map_volume_kind(char const *name);
/* a LIFT's exit's name (TP and the next letter) into out: 1, else 0 */
int ae_map_volume_lift_exit(char const *name, char *out, int out_size);
/* two volume names the same, any case: 1 */
int ae_map_volume_same_name(char const *a, char const *b);

#endif
