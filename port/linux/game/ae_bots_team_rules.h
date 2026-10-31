/*
AE_BOTS_TEAM_RULES.H

Arena Evolved: the bots play as a team (owner 2026-10-20 after play-tests: "my bot is running away from me when i try
to group with him"; his teammate bot "runs in like the leroy jenkins meme and doesn't wait for his team mate, goes on
lots of 1v2's"; the pro-skills list: the numbers game, playing your life, a teammate's crossfire). Pure
(port/linux/tests/ae_bots_team_rules_test.c). Team games only; levels as ae_bots_combat_rules.h's.

- THE NUMBERS GAME (NORMAL and up): living teammates (the bot counted) against living enemies, as a player reads the
  kill feed. Two or more behind: it regroups on its nearest teammate instead of roaming. Two or more ahead: it hunts as
  ever.
- OUTNUMBERED (NORMAL and up): every enemy near (within 12 units) past the teammates near (the bot counted) takes from
  its confidence in the fight (ae_bots_combat_confidence): NORMAL 0.2, HEROIC 0.25, LEGENDARY and DEMON 0.3 each, 0.6
  at most. A bot at full strength still fights a 2v1; hurt, it backs off (the retreat's own rule).
- WINGMAN (EASY and up): bots of a team stay near each human player of it (ae_bots_team_human_squad, owner 2026-10-30: a lone
  human gets a squad, not one bot): past 8 units it goes to
  them, within 8 it moves about them (a spot 3 to 6 units from them), never on their heels; in a fight, past 10 units
  it moves back towards them while it shoots (the leash), so it fights beside its player, not off on its own.
*/

#ifndef __AE_BOTS_TEAM_RULES_H
#define __AE_BOTS_TEAM_RULES_H

#define AE_BOTS_TEAM_NEAR 12.0f
#define AE_BOTS_WINGMAN_FAR 8.0f
#define AE_BOTS_WINGMAN_NEAR 3.0f
#define AE_BOTS_WINGMAN_SPOT 6.0f
/* in a fight, a wingman farther than this from its player moves back towards them, still shooting */
#define AE_BOTS_WINGMAN_LEASH 10.0f

/* -1: hold back and regroup; 0: as ever; 1: push (allies counts the bot) */
int ae_bots_team_stance(int level, int allies_alive, int enemies_alive);
/* how much confidence the enemies near past the allies near take (allies counts the bot) */
float ae_bots_team_outnumbered_penalty(int level, int enemies_near, int allies_near);
/* ANGLE ISOLATION (HEROIC and up; owner's pro-skills list: "positioning ... so that only one enemy can see them at any
given micro-second, turning a 1v3 into three 1v1s"): every enemy within AE_BOTS_TEAM_EXPOSED_RANGE with a clear line to
the bot past the first takes HEROIC 0.1, LEGENDARY 0.15, DEMON 0.2 from its confidence (0.6 at most), so a bot in a
crossfire backs out to one angle. Taken with OUTNUMBERED as the larger of the two, not both. */
#define AE_BOTS_TEAM_EXPOSED_RANGE 30.0f
float ae_bots_team_exposed_penalty(int level, int exposed);
/* HEARING (NORMAL and up; the pro-skills list's audio awareness): how far a bot with no enemy in mind hears an enemy's
fresh shot (world units): NORMAL 15, HEROIC 20, LEGENDARY 25, DEMON 30; RECRUIT and EASY 0. It then goes to look
where the shot came from (as where an enemy was last seen), never seeing or shooting through a wall */
float ae_bots_team_hearing_range(int level);
/* the bots that stay with each human of a team of humans and bots. A lone human: 1 with up to 3 bots, 2 with 4 or 5, 3 from 6.
Two humans or more: 1 each, 2 each when the team has 4 bots a human. */
int ae_bots_team_human_squad(int humans, int bots);
/* FLAG CREWS (owner 2026-10-30: at least two crews run the flag): a CTF team's squad with a human, cut so that 4 of the bots
able to go (its site defenders out) are left for two crews; one bot stays with the human at the least */
int ae_bots_team_human_squad_leaving_crews(int squad, int able_bots);
/* whether a level plays wingman to a human teammate */
int ae_bots_team_wingman_level(int level);
/* the wingman's move: 1 go to the player (past AE_BOTS_WINGMAN_FAR), 2 move about them (within it) */
int ae_bots_team_wingman_move(float distance);

/* MINI SQUADS (owner 2026-10-30, big team games: bots should break out into small groups and take the ledges and tunnels): a team with
AE_BOTS_TEAM_MINI_SQUADS_FROM bots or more has its bots not with a human (free) cut into squads, of 3 or 4 from 6 free, of 2 or 3
below. How many squads there are, and which a bot is (ordinal: its place among the free bots). Each squad has a route for a while (epoch), all different while there
are routes to go round: 0 ledge, 1 tunnel, 2 left side, 3 right side, 4 straight on. Teams start apart so they meet. */
#define AE_BOTS_TEAM_MINI_SQUADS_FROM 6
#define AE_BOTS_TEAM_FLAG_CREWS_BOTS 4
enum { AE_BOTS_ROUTE_LEDGE, AE_BOTS_ROUTE_TUNNEL, AE_BOTS_ROUTE_LEFT, AE_BOTS_ROUTE_RIGHT, AE_BOTS_ROUTE_STRAIGHT, AE_BOTS_ROUTES };
int ae_bots_team_mini_squads(int free_bots, int team_bots);
int ae_bots_team_mini_squad_of(int ordinal, int free_bots, int squads);
int ae_bots_team_mini_squad_route(int squad, long epoch, int team);

#endif
