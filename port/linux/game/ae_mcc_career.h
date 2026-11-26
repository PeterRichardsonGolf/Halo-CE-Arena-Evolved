/*
AE_MCC_CAREER.H

The career insignia of the player's own MCC install (ae_mcc_career_rules.h): 30 rank pictures and 11 tour pictures read from its
Unreal pak (ae_pak_rules.h), found as game.mcc_path finds the install and read when first asked for. The pak is many GB: only its
footer, its index (about 5 MB, freed again once the 43 entries are placed) and, when a picture is asked for, that picture's bytes
are read. A menu image is made from a picture once and kept. Without the install, with game.mcc_use "no", or when the files are
not what is expected, there is no picture: the career rank shows as its name and tour in AE's own look. Nothing of MCC's files
ships with the game, is copied or leaves the machine. Menus' thread only. Nothing draws these yet: the screens ask for them.

Log: "mcc career: insignia of 30 ranks and 11 tours in <pak>; order read from the data asset" / "mcc career: no MCC folder ..." /
"mcc career: the insignia could not be read from <root> (<why>): ranks are shown by name".
*/

#ifndef __AE_MCC_CAREER_H
#define __AE_MCC_CAREER_H

#include <stddef.h>

/* whether the install's insignia can be read (the pak's footer, index and data asset looked at once) */
int ae_mcc_career_available(void);
/* a picture as r, g, b, a rows from the top at side by side pixels (side 512, 256, 128, 64 or 32) into rgba (side * side * 4 bytes
   at least); kind 0: rank 1 .. 30 (Rookie first), kind 1: tour 1 .. 11 (the first tour first). 1 when decoded */
int ae_mcc_career_rgba(int kind, int number, unsigned int side, unsigned char *rgba, size_t size);
/* the picture as AE's menus draw it (ae_draw_image: the image's id, 0 for none), made once and kept at side 128 */
int ae_mcc_career_menu_image(int kind, int number);

#endif
