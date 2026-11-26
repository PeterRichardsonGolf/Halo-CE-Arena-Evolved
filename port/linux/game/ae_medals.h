/*
AE_MEDALS.H

MEDALS: earned on the host (ae_medal_rules.c's rules, fed by ae_medals.c), sent to the player's machine, shown in the player's view
(ae_medal_draw.c), with the pictures from the player's own install (ae_medal_pack.c). Nothing of that install ships with the game.
*/

#ifndef __AE_MEDALS_H
#define __AE_MEDALS_H

#include <wchar.h>

#include "ae_pure_types.h"

enum
{
#define AE_MEDAL(name, key, points) AE_MEDAL_##name,
#include "ae_medal_ids.h"
#undef AE_MEDAL
	AE_MEDAL_COUNT
};

/* a medal's key in the career list; NULL for a number from nowhere */
char const *ae_medal_key(int medal);

/* (ae_medal_draw.c) this local player earned it: it joins their view now */
void ae_medal_display_add(short local_player_index, int medal);
/* (ae_medal_draw.c) whether this local player's MEDAL STYLE is MCC (display.medal_style); MODERN otherwise */
boolean ae_medal_style_is_mcc(short local_player_index);
/* (ae_medal_draw.c) this local player killed a player: a "Killed" row joins their MODERN stack (nothing in the other styles or
where the stack does not show). A team mate's kill is none */
void ae_medal_display_killed(short local_player_index, long victim_player_index, short victim_team, wchar_t const *victim_name,
	boolean betrayal);
/* (ae_medal_draw.c) hud.c's turn, after the markers */
void ae_medal_display_draw(short local_player_index);
/* (ae_medal_draw.c) a new game: nothing showing */
void ae_medal_display_reset(void);
/* (ae_medal_draw.c) one local player's view's medals gone (a split-screen player who quit: ae_split_quit.c) */
void ae_medal_display_reset_local(short local_player_index);

#endif
