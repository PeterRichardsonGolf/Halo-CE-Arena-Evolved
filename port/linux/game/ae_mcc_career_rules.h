/*
AE_MCC_CAREER_RULES.H

The career insignia of the player's own MCC install, as pure rules (port/linux/tests/ae_career_pak_test.c runs them). MCC keeps
them in its Unreal pak (ae_pak_rules.h): one 512 by 512 picture for each of the 30 ranks and one for each of the 11 tours, and a data
asset that lists them in the order of the career. The data asset is read as an Unreal package: its name table names the pictures
(as "/Game/...", the pak's "MCC/Content/..."), and its pictures' references sit in its export data one after another, a rank's
record a fixed distance after the one before it. The order is the one the data asset lists, or none: this never guesses an order.

Every length and offset is checked against the buffer it sits in. Nothing of MCC's files ships with the game.
*/

#ifndef __AE_MCC_CAREER_RULES_H
#define __AE_MCC_CAREER_RULES_H

#include <stddef.h>

enum
{
	AE_MCC_CAREER_RANKS = 30,
	AE_MCC_CAREER_TOURS = 11,
	AE_MCC_CAREER_PATH = 192,
	/* the largest data asset package and export file read */
	AE_MCC_CAREER_MOST_ASSET = 64 * 1024,
	/* a picture's side, at most */
	AE_MCC_CAREER_SIDE_MOST = 1024
};

/* the order of the career's pictures from the data asset's package (uasset) and export data (uexp): the pak paths of the pictures'
   export files ("MCC/Content/.../T_Im_X.uexp") of the 30 ranks, lowest first, and of the 11 tours, the first tour first. Returns 1
   when both lists were found whole, else 0 (the paths are then not to be used) */
int ae_mcc_career_order(unsigned char const *uasset, size_t uasset_size, unsigned char const *uexp, size_t uexp_size,
	char ranks[AE_MCC_CAREER_RANKS][AE_MCC_CAREER_PATH], char tours[AE_MCC_CAREER_TOURS][AE_MCC_CAREER_PATH]);
/* a picture's export file (uexp) holds a 32 bit BGRA picture: 1 with its side and where its pixels start (side * side * 4 bytes); 0
   for another format or a picture that does not fit */
int ae_mcc_career_texture(unsigned char const *uexp, size_t uexp_size, unsigned int *side, size_t *pixels);
/* the picture's BGRA pixels at side_in as RGBA at side_out (side_in a whole multiple of side_out: each output pixel the average of a
   block; the colour weighed by its alpha); 0 when the sides do not fit */
int ae_mcc_career_scale(unsigned char const *bgra, unsigned int side_in, unsigned char *rgba, unsigned int side_out);

#endif
