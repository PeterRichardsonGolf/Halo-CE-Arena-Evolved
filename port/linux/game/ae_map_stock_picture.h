/*
AE_MAP_STOCK_PICTURE.H

A stock map's picture for AE's own screens (CHANGE MAP, the AE TOURNAMENT lobby, the server lobby): the picture the PC menus
show for it, the loaded ui.map's (the Xbox maps' mp_map_grafix, Halo PC's pictures of its own maps), decoded to an RGBA image.
These pictures are not files (no previews folder has them): every screen that draws a map's picture asks
ae_setup_map_picture / ae_setup_map_picture_named (ae_glue_setup_maps.c), which falls to this after the map's own file.
*/

#ifndef __AE_MAP_STOCK_PICTURE_H
#define __AE_MAP_STOCK_PICTURE_H

/* 1 with the picture's pixels malloc'd (the game's allocator: free it with free) in rgba, width * height * 4 bytes in RGBA order;
0 (nothing set) for a map with no picture of its own in the loaded ui.map (its unknown level's "?" is no picture), a ui.map not
loaded, or a pixel format not read. retry (or NULL): set to 1 when asking again later may succeed (ui.map not loaded yet) */
int ae_map_stock_picture_rgba(char const *map_name, unsigned char **rgba, int *width, int *height, int *retry);

#endif
