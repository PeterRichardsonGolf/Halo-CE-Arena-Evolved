/*
AE_ITEM_SETS_RULES.H

What the ITEM SETS page of MAPS & MODS decides without drawing (ae_item_sets_rules.c; ae_screen_item_sets.c draws it): the nine
kinds of a map's items and their words, an item's name and the folders above it made readable, the words of the engine's
reasons, what a set does in which kind of game, the footer's prompts of each step, and the scenes DEV MODE's `menu maps sets`
can stage. Plain C, no engine headers, so the unit tests build it alone.
*/

#ifndef AE_ITEM_SETS_RULES_H
#define AE_ITEM_SETS_RULES_H

/* the kinds, in the engine's order (tag_injector.h's categories 0..8) */
enum { AE_IS_KINDS = 9 };

/* the steps of the page, each B going up one */
enum { AE_IS_STEP_SETS, AE_IS_STEP_CONTENTS, AE_IS_STEP_MAPS, AE_IS_STEP_KINDS, AE_IS_STEP_ITEMS, AE_IS_STEPS };

/* the scenes DEV MODE's `menu maps sets <scene>` opens (the words are ae_maps_hub_rules.c's stage table, in this order) */
enum
{
	AE_IS_SCENE_LIST, AE_IS_SCENE_EMPTY, AE_IS_SCENE_OPTIONS, AE_IS_SCENE_DELETE, AE_IS_SCENE_PICK, AE_IS_SCENE_CANT_OPEN,
	AE_IS_SCENE_KINDS, AE_IS_SCENE_ITEMS, AE_IS_SCENE_ADDING, AE_IS_SCENE_ADDED, AE_IS_SCENE_REFUSED, AE_IS_SCENE_CONTENTS,
	AE_IS_SCENE_REMOVE, AE_IS_SCENE_FAILED, AE_IS_SCENE_BIPED, AE_IS_SCENE_SWITCHED, AE_IS_SCENE_NEW,
	/* (the real engine, a map this PC has: its kinds, a kind's items, and one item added to a new set of a fresh name) */
	AE_IS_SCENE_REAL_KINDS, AE_IS_SCENE_REAL_ITEMS, AE_IS_SCENE_REAL_ADD, AE_IS_SCENES
};

/* the kinds of game a set can be asked about */
enum { AE_IS_GAME_CAMPAIGN_ALONE, AE_IS_GAME_CAMPAIGN_COOP, AE_IS_GAME_FIREFIGHT, AE_IS_GAME_CHALLENGE, AE_IS_GAME_MULTIPLAYER,
	AE_IS_GAME_ONLINE, AE_IS_GAMES };

enum { AE_IS_HINT_OPTIONS, AE_IS_HINT_PLAY, AE_IS_HINT_STOP, AE_IS_HINT_NEW, AE_IS_HINT_OPEN, AE_IS_HINT_ADD, AE_IS_HINT_ON_OFF,
	AE_IS_HINT_REMOVE, AE_IS_HINT_ADD_ITEMS, AE_IS_HINT_SEARCH, AE_IS_HINT_PAGE, AE_IS_HINT_CLOSE_MAP, AE_IS_HINT_BACK,
	AE_IS_HINT_OPEN_MAP };
struct ae_is_hint { int hint; char const *label; };
enum { AE_IS_HINTS_MOST = 6 };

char const *ae_is_kind_name(int kind);
/* a kind's one-line help ("What players and AI carry"), and the word for one item of it ("WEAPON") */
char const *ae_is_kind_blurb(int kind);
char const *ae_is_kind_word(int kind);

/* an item's own name: the last part of its path (either slash) */
void ae_is_item_name(char const *path, char *name, int size);
/* the folders above an item joined with " > " (the real separator is U+203A); keep < 0: all of them; else the first, an
ellipsis, then the last `keep` (all when that is every one) */
void ae_is_item_folders(char const *path, int keep, char *folders, int size);
/* how many folders a path has above its name */
int ae_is_folder_count(char const *path);
/* a name cut in the middle to at most `chars` characters ("pistol_spir...riant_mp"); whole when it fits */
void ae_is_middle_cut(char const *name, int chars, char *cut, int size);
/* a path as lines of at most `chars` characters, folders joined with the separator, broken only between folders, each broken
line ending with the separator; how many lines (at most `most`) */
int ae_is_path_lines(char const *path, int chars, char lines[][96], int most);

/* a set's name as a person typed it: 1 when it has a letter or a digit and only letters, digits, spaces, - and _ */
int ae_is_set_name_ok(char const *name);
/* "23 items * 61 MB * from 3 maps", "No items yet" */
void ae_is_set_line(long items, unsigned long long bytes, long maps, char *text, int size);
/* "PLAYING * 23 ITEMS * 61 MB" */
void ae_is_set_state(int playing, long items, unsigned long long bytes, char *text, int size);

/* the engine's reason a tag could not be added, in a player's words ("" for none) */
void ae_is_reason_words(char const *engine, char *text, int size);
/* a profile tag's `failed` value as a sentence for the card (missing, unreadable, too large, ...) */
void ae_is_failed_words(char const *failed, char *text, int size);
/* the sentence of "WHAT TO DO" for a failed value */
void ae_is_failed_todo(char const *failed, char *text, int size);

/* whether a playing set's items are added to a game of that kind: only a campaign level played alone, and then only through
the randomizer (randomizer_on). Never a multiplayer, online, ranked, tournament or matchmaking game, a co-op game, Firefight
or a challenge: those read no set of this PC's */
int ae_is_plays_in(int game, int randomizer_on);
/* the card's sentence about where a set plays */
char const *ae_is_plays_words(int randomizer_on);
/* the card's sentence about other players */
char const *ae_is_others_words(void);

/* the footer of a step; `playing` is whether the focused set plays; `has_items`: on the sets step -1 when there is no set, on the
items step 1 when the focused item is in the set; how many */
int ae_is_hints(int step, int playing, int has_items, struct ae_is_hint hints[AE_IS_HINTS_MOST]);

#endif
