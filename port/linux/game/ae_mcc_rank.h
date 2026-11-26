/*
AE_MCC_RANK.H

The rank pictures of the player's own MCC install (ae_mcc_rank_rules.h): found as game.mcc_path finds the install, read when first
asked for. A picture is decoded from the pack when asked for, a menu image made from it once and kept. Without the install, with
game.mcc_use "no", or when the files are not what is expected, there is no picture: the rank shows as its plain number
(ae_mcc_rank_text). Nothing of MCC's files ships with the game, is copied or leaves the machine. Menus' thread only. Nothing draws
these yet: the screens ask for them.

Log: "mcc rank: pictures of rank 1 .. 50 (small and large) in MCC's folder <root>" / "mcc rank: no MCC folder to take the rank
pictures from: ranks are shown as numbers" / "mcc rank: the rank pictures could not be read from <root>: ranks are shown as
numbers".
*/

#ifndef __AE_MCC_RANK_H
#define __AE_MCC_RANK_H

#include <stddef.h>

/* whether the install's rank pictures can be read (the files looked at once: the index and the pixels' header) */
int ae_mcc_rank_available(void);
/* a rank's picture (1 .. 50; large 0: 52 by 52, 1: 152 by 152) as r, g, b, a rows from the top into rgba (size bytes at least:
   width * height * 4); the sides are returned. 1 when decoded, 0 for none */
int ae_mcc_rank_rgba(int rank, int large, unsigned char *rgba, size_t size, int *width, int *height);
/* the picture as AE's menus draw it (ae_draw_image: the image's id, 0 for none), made once and kept; sides returned */
int ae_mcc_rank_menu_image(int rank, int large, int *width, int *height);
/* the rank as plain text: its number, "7" (an out of range rank is clamped to 1 .. 50) */
void ae_mcc_rank_text(int rank, char *out, size_t size);

#endif
