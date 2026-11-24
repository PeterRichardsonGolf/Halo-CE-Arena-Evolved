/*
TAG_PACK.H

The tag packs (tag_pack.c): tag_pack/<name>.pack, each a few maps' tags and
the pixels and samples a manifest keeps of them (port/assets/tag_packs:
port/linux/src/port_tag_packs.c), written once by the main menu from the
maps in maps/, on a thread of their own behind a checklist, and read by the tag importer
(tag_import.c) as a level loads. port/linux/TAG_INJECTION.md tells how to
make one.
*/

#ifndef __TAG_PACK_H
#define __TAG_PACK_H

/* the packs' file format: 'rpak', this, the pack's content hash
(port_tag_packs_content), how many maps; then a 56-byte record each: the
map's name (32 bytes), its file length, tag offset and tag size, where its
tags are in the pack, how many pixel and sample ranges and where their
table is (twelve bytes each: the offset in the map, the size, where the
bytes are in the pack). tools/tag_pack.py writes the same */
#define TAG_PACK_FORMAT_VERSION 2
#define TAG_PACK_HEADER_SIZE 16
#define TAG_PACK_RECORD_SIZE 56
#define TAG_PACK_MAP_NAME_SIZE 32
#define TAG_PACK_MAPS 32

/* a pack's file (tag_pack/<name>.pack, or tag_pack/<language>/ for another
language's maps), or NULL when no manifest has that name */
char const *tag_pack_file_name(char const *name);
/* what it holds: its manifest's hash, which its header must carry */
unsigned long tag_pack_content(char const *name);

/* the pack is there, written by this build's manifest, and each map of it
in maps/ is the one it was written from */
int tag_pack_ready(char const *name);
/* the pack is wanted: at the main menu, it is written with the others asked
for. Nothing when it is ready, or none of its maps is there */
void tag_pack_request(char const *name);
/* the batch of the packs asked for, started now rather than at the main menu's next frame (the game's
level playing, on the batch's own worker) */
void tag_pack_start_now(void);
/* a pack's writing for a screen to show: it is queued or being written, the maps it has so far and the maps of it
this PC has, done (there, current), failed (not written), and a line why a map was left out or it was not written */
struct tag_pack_state
{
	int writing;
	int maps_done;
	int maps_wanted;
	int done;
	int failed;
	char reason[160];
};
void tag_pack_state(char const *name, struct tag_pack_state *state);
/* packs being written, or waiting to be */
int tag_pack_busy(void);
/* the main menu, each frame (main.c): at the first, the packs an older build
left among the maps moved into tag_pack/, and those their manifests want at
start-up asked for; then those asked for written together, on a thread */
void tag_pack_update(boolean main_menu);
/* the checklist of the packs being written up (and a moment after): the
menus take no input meanwhile (menu_functions.c's menu_port_event_taken) */
int tag_pack_checklist_shown(void);
/* (interface.c, over everything) the checklist drawn */
void tag_pack_draw(void);

/* a map's header, as a pack records it. 0 when the map is not there or is
not an Xbox map */
int tag_pack_map_header(char const *leaf, long *file_length, long *tag_offset, long *tag_size, int *compressed);

/* a manifest's "condition:<name>" (tag_pack_conditions.c): whether the pack
is wanted at start-up */
struct tag_pack_condition
{
	char const *name;
	int (*wanted)(void);
};

extern struct tag_pack_condition const tag_pack_conditions[];

#endif /* __TAG_PACK_H */
