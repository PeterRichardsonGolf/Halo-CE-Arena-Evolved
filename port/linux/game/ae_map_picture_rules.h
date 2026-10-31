/*
AE_MAP_PICTURE_RULES.H

The rules of a map's own picture on the map select screen (ae_map_pictures.h):
where the picture of a map is (a "previews" folder beside the map's file, the
file named as the map: maps\previews\atlas.png for maps\atlas.map,
maps_ce\previews\xbox_zerohour.png for maps_ce\xbox_zerohour.map), and which
files are drawn (an 8-bit RGBA PNG, not interlaced, as the menus' PNG reader
takes them: port/linux/src/hud_hires.c). Pure (no game types): unit tested in
port/linux/tests/ae_map_picture_rules_test.c.
*/

#ifndef __AE_MAP_PICTURE_RULES_H
#define __AE_MAP_PICTURE_RULES_H

enum
{
	/* a picture's largest side, and its largest file (a picture of the screen's box, 140 by 114, is drawn at
	most a few times that size: 560 by 456 is what the map pack ships) */
	AE_MAP_PICTURE_MAXIMUM_SIDE = 2048,
	AE_MAP_PICTURE_MAXIMUM_FILE = 8 << 20,
	/* the bytes of a PNG's start that ae_map_picture_png_usable reads */
	AE_MAP_PICTURE_HEADER_SIZE = 33
};

/* the picture's path for a map whose file is at map_path (as the game opens it, d:\maps\atlas.map, or a host
path with /), its file's name without .map being file: the folder of map_path, then previews, then file.png, with
map_path's own separator; 0 (out empty) if file is not a plain name (empty, a separator, a drive, ..) or the path
does not fit in size bytes */
int ae_map_picture_path(char const *map_path, char const *file, char *out, unsigned int size);

/* whether a file's first bytes (at least AE_MAP_PICTURE_HEADER_SIZE of them) and its size are a PNG the menus
draw: its signature and IHDR, 1 to AE_MAP_PICTURE_MAXIMUM_SIDE wide and high, 8 bits a channel, RGBA, not
interlaced, no larger than AE_MAP_PICTURE_MAXIMUM_FILE */
int ae_map_picture_png_usable(unsigned char const *header, unsigned int header_size, unsigned int file_size);

#endif
