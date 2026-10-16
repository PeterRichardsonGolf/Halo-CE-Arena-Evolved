/*
AE_MAP_PICTURES.H

A map's own picture on the map select screen and lobby, in the PC menus
(display.menus "pc": port/linux/game/menu_functions.c) and the Xbox menus
(their map list's frame of a row: ui_map_list_picture_index, ui_map_list.c;
owner 2026-10-16): a PNG in a "previews" folder
beside the map's file (ae_map_picture_rules.h), shipped with the map, as AE's
map pack ships its maps' pictures. A map past the Xbox's thirteen (an Xbox v5
community map, a Custom Edition or HaloMD map) that has one shows it in place
of the picture the list gives it (the loaded ui.map's unknown level, or Halo
PC's); a map with none shows what it did. The stock maps keep the loaded
ui.map's own pictures.

Each picture is a bitmap of its own past ui.map's frames (AE_MAP_PICTURE_BASE
and up, as ui_map_list.c's Halo PC pictures are past theirs), a small D3D
texture the PNG is drawn for, through the menus' art (menu_tags.c's way:
halo_menus_art_register), its file read when the art asks for it
(ae_map_picture_png, from menu_files.c's file_data). Files are looked for once
a run, as the maps are.
*/

#ifndef __AE_MAP_PICTURES_H
#define __AE_MAP_PICTURES_H

struct bitmap_data;

/* the bitmap frame of a map's own picture (both menus' map select and lobby), or NONE: none, or a stock map */
short ae_map_picture_frame(char const *map_name);

/* the bitmap of a frame of ae_map_picture_frame's (ui_map_list_picture), or NULL for any other frame */
struct bitmap_data *ae_map_picture_bitmap(short frame_index);

/* the PNG of a picture's art name (the menus' art reading it: port/linux/src/menu_files.c), its size in size, or
NULL if the name is not a picture's; the data are kept until the next call */
unsigned char const *ae_map_picture_png(char const *art_name, unsigned int *size);

#endif
