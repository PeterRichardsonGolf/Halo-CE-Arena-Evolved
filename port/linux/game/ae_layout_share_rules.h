/*
AE_LAYOUT_SHARE_RULES.H

Layouts travel with maps (0.3.5): a host's map layout file (ae_layout_rules.h) sent to its guests that run this game,
beside the map that map sharing sends, so that every machine plays the same level (a guest without the file kept the
scenery the host's layout removes, its collision with it, and had none of its bot lines). Pure: no engine includes
(port/linux/tests/ae_layout_share_rules_test.c runs them); ae_map_share_all.c sends and takes the frames, ae_layout.c
reads and keeps the files.

ON THE WIRE. A layout goes in frames of map sharing's frame message (ae_map_share_rules.h: the offer message with a
level name no map has, sent only to a machine that runs this game, ae_peer_is_ae), of a type of its own
(AE_MAP_SHARE_FRAME_LAYOUT). Network version 29 is unchanged:
- OpenCE's machines (build-232) and ChupathingyCE's (0.7.5d) are never sent one, as they are never sent a frame.
- Arena Evolved 0.3.0 to 0.3.4 take a frame of a type they do not know for no frame (ae_map_share_frame_decode
  refuses it) and drop it: such a guest plays with its own file, as it always did.
- A host of those versions sends none: a guest then plays with its own file, as it always did.

A frame (after the offer message's own 68 bytes): type (3), flags (1: the host has no layout for the level), serial
(little-endian word: a new one for each new text of the host's), total bytes (little-endian long, at most
AE_LAYOUT_MAXIMUM_FILE_SIZE), the offset of this frame's bytes (long), their length (word, at most
AE_LAYOUT_SHARE_CHUNK), 2 unused, the level's name as the game names it (64 bytes, ended), then the bytes.

WHICH LAYOUT A GUEST PLAYS (ae_layout_share_decide): its host's word for the match, so both play one level.
- No word from the host (an older host, or its frames had not all come when the map loaded): its own file, as before.
- The host has no layout for the map: none, though the guest has a file (the host plays the map as it is).
- The host's layout, when it was made for the very map file the guest has (its header's name and hash prefix): that
  one; else (another version of the map) its own, and the host's is not kept.

WHAT IS WRITTEN (only after the player's yes to downloading from that host, as for any file of map sharing; without
it the host's layout is used for the match and nothing is written): a guest without a file gets the host's in the
map's layouts folder (<the map's folder>\layouts\<map>.txt); a guest whose own file is the same keeps it; a guest whose
own file differs keeps it untouched, and the host's goes beside it as <map>.host.txt.
*/

#ifndef __AE_LAYOUT_SHARE_RULES_H
#define __AE_LAYOUT_SHARE_RULES_H

#include "ae_layout_rules.h"

enum
{
	/* the level name's bytes in a frame (with its end) */
	AE_LAYOUT_SHARE_LEVEL_BYTES = 64,
	/* a frame's own bytes before its layout's */
	AE_LAYOUT_SHARE_FRAME_HEADER_BYTES = 16 + AE_LAYOUT_SHARE_LEVEL_BYTES,
	/* a frame's most layout bytes (a frame stays under a list frame's size, ae_map_share_rules.h) */
	AE_LAYOUT_SHARE_CHUNK = 840,
	AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES = AE_LAYOUT_SHARE_FRAME_HEADER_BYTES + AE_LAYOUT_SHARE_CHUNK,
	/* the levels' layouts a guest keeps from its host at once (the game's map, those it fetches) */
	AE_LAYOUT_SHARE_SLOTS = 8,
};

/* the frame's type (ae_map_share_rules.h's frames' types go on from it) and its flag */
#define AE_LAYOUT_SHARE_FRAME_TYPE 3
#define AE_LAYOUT_SHARE_FLAG_NONE 0x01

struct ae_layout_share_frame
{
	int flags;
	unsigned int serial;
	long total;
	long offset;
	int length;
	char level[AE_LAYOUT_SHARE_LEVEL_BYTES];
	unsigned char data[AE_LAYOUT_SHARE_CHUNK];
};

/* a frame into a message's bytes (the bytes written; 0 if it is not one to send or does not fit) and out of them (0
when it is not one: another type, another size than its length says, a level name not ended or empty, more bytes
than a frame's or than its total, a total over the file's limit, a "none" with bytes) */
long ae_layout_share_frame_encode(struct ae_layout_share_frame const *frame, unsigned char *bytes, long size);
int ae_layout_share_frame_decode(unsigned char const *bytes, long size, struct ae_layout_share_frame *frame);

/* the frame of a layout's text (length bytes) that starts at offset, for level: its bytes there; a "none" frame for
level when text is NULL. 0 when offset is past the text (or the text is over the limit) */
int ae_layout_share_frame_make(char const *level, unsigned int serial, char const *text, long length, long offset,
	struct ae_layout_share_frame *frame);

/* a slot's state */
enum
{
	AE_LAYOUT_SHARE_EMPTY,
	/* frames came, not all of them */
	AE_LAYOUT_SHARE_PARTIAL,
	/* the host has no layout for the level */
	AE_LAYOUT_SHARE_NONE,
	/* the host's layout, whole */
	AE_LAYOUT_SHARE_TEXT,
};

struct ae_layout_share_slot
{
	char level[AE_LAYOUT_SHARE_LEVEL_BYTES];
	unsigned int serial;
	int state;
	long total;
	long received;
	unsigned long used;
	char text[AE_LAYOUT_MAXIMUM_FILE_SIZE];
};

struct ae_layout_share_inbox
{
	unsigned long clock;
	struct ae_layout_share_slot slots[AE_LAYOUT_SHARE_SLOTS];
};

/* the inbox emptied (another host) */
void ae_layout_share_clear(struct ae_layout_share_inbox *inbox);
/* a frame taken: its level's layout begun anew at offset 0 (the least lately used slot given up for a new level), else
added on if it is the next of the same serial. -1 when it was dropped (not the next, or no level's begun), 0 when taken
and more is to come, 1 when the level's layout (or its "none") is now whole. *slot gets the slot's index */
int ae_layout_share_take(struct ae_layout_share_inbox *inbox, struct ae_layout_share_frame const *frame, int *slot);
/* the slot of a level (its name, any case), or -1 */
int ae_layout_share_find(struct ae_layout_share_inbox const *inbox, char const *level);

/* what a guest knows of its host's layout for a map */
enum
{
	AE_LAYOUT_SHARE_HOST_UNKNOWN,
	AE_LAYOUT_SHARE_HOST_NONE,
	AE_LAYOUT_SHARE_HOST_TEXT,
};
/* which layout is applied */
enum
{
	AE_LAYOUT_SHARE_APPLY_OWN,
	AE_LAYOUT_SHARE_APPLY_HOST,
	AE_LAYOUT_SHARE_APPLY_NOTHING,
};
/* what is written */
enum
{
	AE_LAYOUT_SHARE_DISK_NOTHING,
	AE_LAYOUT_SHARE_DISK_WRITE,
	AE_LAYOUT_SHARE_DISK_BESIDE,
};

/* the layout a guest applies (AE_LAYOUT_SHARE_APPLY_*) and what it writes (*disk): host its host's word, host_fits
whether the host's layout was made for this machine's map file, own_exists / own_same whether this machine has a file
of its own and whether it is the host's to the byte, may_write whether the player said yes to downloading from it */
int ae_layout_share_decide(int host, int host_fits, int own_exists, int own_same, int may_write, int *disk);

/* the file the host's layout is kept in beside a guest's own (<map>.txt -> <map>.host.txt), into out: 0 if it does not
fit or own_path does not end in .txt */
int ae_layout_share_beside_path(char const *own_path, char *out, long size);

/* whether a text is a layout this map file takes: it reads (ae_layout_parse, into *scratch) and was made for map_file
(no .map) of that hash prefix */
int ae_layout_share_fits(char const *text, long length, char const *map_file, unsigned long map_hash,
	struct ae_layout *scratch);

#endif /* __AE_LAYOUT_SHARE_RULES_H */
