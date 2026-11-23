/*
AE_MAP_AUTO_PICTURE_RULES.H

(0.7.9) The rules of a map's automatic picture (ae_map_auto_picture.h): which maps are given one, when the shot is taken,
and the picture itself (16:9, 640 by 360, an 8-bit RGBA PNG as the menus draw, ae_map_picture_rules.h). Pure (no game
types): unit tested in port/linux/tests/ae_map_auto_picture_rules_test.c.
*/

#ifndef __AE_MAP_AUTO_PICTURE_RULES_H
#define __AE_MAP_AUTO_PICTURE_RULES_H

#include <stddef.h>

enum
{
	AE_MAP_AUTO_PICTURE_WIDTH = 640,
	AE_MAP_AUTO_PICTURE_HEIGHT = 360,
	/* the shot is taken this many ticks (30 a second) after the player's first life began, and not after the later one:
	the view before the player has moved, a map that is slow to start is not shot a second life in */
	AE_MAP_AUTO_PICTURE_SHOT_TICKS = 12,
	AE_MAP_AUTO_PICTURE_LAST_TICKS = 90,
	/* the stock pictures' frames of the map list (Halo PC's, UI_MAP_LIST_PICTURE_BASE and up) and the unknown level's,
	which a map with no art of its own is given */
	AE_MAP_AUTO_PICTURE_STOCK_BASE = 0x4000,
	AE_MAP_AUTO_PICTURE_STOCK_END = 0x5000,
	AE_MAP_AUTO_PICTURE_UNKNOWN_LEVEL = 19
};

/* whether a map is given a picture: map_name is the list's name ("file", "file@pack"); candidate: it is a custom map (or a
pack's) with no picture file and none refused; stock_art: the map list shows art of its own for it; local: a game this
machine runs the rules of (not a client's, never a dedicated server's). Never a Workshop map's ("@ws": its picture is the
catalogue's). */
int ae_map_auto_picture_wanted(char const *map_name, int candidate, int stock_art, int local);

/* whether a map list frame is Halo PC's art of a stock map (not the unknown level's stand-in, not a frame of another kind) */
int ae_map_auto_picture_stock_frame(int frame_index);

/* the file name a stock map's picture is looked up by: a map's list name with an "@mcc" tag (the MCC stock maps' rows,
"bloodgulch@mcc") taken off; any other name (plain, "x@ce", "x@pack", "x@ws<id>") is copied as it is. 1 and the name in out,
or 0 if out is too small */
int ae_map_stock_picture_file(char const *list_name, char *out, int size);

/* whether the first life's view is shot now: ticks since the first life began (the player alive) */
int ae_map_auto_picture_shot_due(long ticks_alive);

/* the 16:9 part of a width by height picture the shot is made of (the middle, whole rows or columns cut off); 0 if the
size is not usable */
int ae_map_auto_picture_crop(int width, int height, int *x, int *y, int *crop_width, int *crop_height);

/* the picture of a width by height frame (4 bytes a pixel: blue, green, red, any; row 0 the top), the 16:9 middle of it
scaled to AE_MAP_AUTO_PICTURE_WIDTH by AE_MAP_AUTO_PICTURE_HEIGHT (by box averages) as RGBA, opaque, into out (that size
times 4 bytes); 0 if the frame is not usable */
int ae_map_auto_picture_scale(unsigned char const *bgra, int width, int height, unsigned char *rgba);

/* the PNG of an RGBA picture into out: filtered rows (the best of the five filters for each row) in one dynamic Huffman
deflate block (stored blocks if that is not smaller), the same bytes for the same pixels on every platform. out must
hold ae_map_auto_picture_png_size bytes, an upper bound (the stored form's size); 1 and the bytes in written (at most
that), or 0 if out is too small */
unsigned int ae_map_auto_picture_png_size(int width, int height);
int ae_map_auto_picture_png(unsigned char const *rgba, int width, int height, unsigned char *out, unsigned int size,
	unsigned int *written);

/* ---------- sharper pictures */

/* the whole-number scale a picture of this width is enlarged by before it is uploaded, so the screen's filter has less to
stretch: 1 for a picture at least AE_MAP_PICTURE_SHARP_WIDTH wide, else the smallest scale (at most 4) that reaches it */
enum { AE_MAP_PICTURE_SHARP_WIDTH = 640, AE_MAP_PICTURE_SHARP_MOST = 4 };
int ae_map_picture_sharp_scale(int width, int height);

/* an RGBA picture enlarged by a whole scale (1 to AE_MAP_PICTURE_SHARP_MOST) with a Catmull-Rom cubic, edges clamped, each
channel on its own: out holds width * scale by height * scale pixels. 0 for a size or scale not usable; nothing is written then */
int ae_map_picture_enlarge(unsigned char const *rgba, int width, int height, int scale, unsigned char *out);

/* ---------- the MCC level pictures (the player's own MCC install, read at run time) */

/* the picture file's record of a stock map: its name in levelstexturepack.perm.bin, its size, and where its blocks (BC1) sit in
levelstexturepack.temp.bin */
struct ae_map_level_record
{
	unsigned int width, height, size;
	unsigned long offset;
};

/* the pack's name of a stock Halo CE multiplayer map's wide picture from its file's name (bloodgulch, any case; beavercreek is
BATTLECREEK there, carousel DERELICT, putput CHIRONTL34): H1MP_<NAME>_LARGE into out. 0 for a file that is none of the 19, or an
out too small */
int ae_map_level_picture_name(char const *file, char *out, int size);

/* the index's (levelstexturepack.perm.bin, index_size bytes) record of a name: 1 when found and a picture this reader decodes
(BC1, a side at most 1024, its blocks wholly inside a pixels' file of pixels_size bytes); 0 else, record untouched */
int ae_map_level_picture_record(unsigned char const *index, size_t index_size, char const *name, size_t pixels_size,
	struct ae_map_level_record *record);
/* whether the pixels' file starts as one does (levelstexturepack.temp.bin) */
int ae_map_level_pixels_valid(unsigned char const *head, size_t head_size);

#endif
