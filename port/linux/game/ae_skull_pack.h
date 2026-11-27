/*
AE_SKULL_PACK.H

The skulls' pictures from the player's own MCC install (ae_skull_pack.c): its skull picture pack, Data/UI/texturepacks/
skullstexturepack, found as game.mcc_path finds the install, read the first time a picture is asked for. A set is drawn from MCC only
when MCC has every skull in it; otherwise it is drawn from Content Evolved (ae_pve_menus_ui.c). A PC with no MCC, or with MCC's files
turned off, uses Content Evolved. Nothing of MCC's files ships with the game, is copied or leaves the machine.
*/

#ifndef __AE_SKULL_PACK_H
#define __AE_SKULL_PACK_H

/* the skull's (halo_skulls.h's number) picture from MCC's files as AE's menus draw it (ae_draw_image: the image's id), trimmed to its
visible pixels and made once and kept, and again when the drawing freed it for a newer one; 0 for none: no MCC, none for that skull,
or files that do not read. Width and height receive the trimmed size */
int ae_skull_pack_menu_image(int skull, int *width, int *height);
/* whether MCC's files have a picture for the skull (the files read, the picture's place checked; no texture made) */
int ae_skull_pack_has_picture(int skull);

#endif /* __AE_SKULL_PACK_H */
