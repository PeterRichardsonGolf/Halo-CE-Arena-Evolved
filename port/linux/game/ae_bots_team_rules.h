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
- WINGMAN (EASY and up): one bot of a team for each human player of it stays near that player: past 8 units it goes to
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
/* whether a level plays wingman to a human teammate */
int ae_bots_team_wingman_level(int level);
/* the wingman's move: 1 go to the player (past AE_BOTS_WINGMAN_FAR), 2 move about them (within it) */
int ae_bots_team_wingman_move(float distance);

#endif
