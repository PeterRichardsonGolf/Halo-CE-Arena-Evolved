/*
AE_ITEM_SETS_RULES.H

What the ITEM SETS page of MAPS & MODS decides without drawing (ae_item_sets_rules.c; ae_screen_item_sets.c draws it): the nine
kinds of a map's items and their words, an item's name and the folders above it made readable, the words of the engine's
reasons, what a set does in which kind of game, the footer's prompts of each step, and the scenes DEV MODE's `menu maps sets`
can stage. Plain C, no engine headers, so the unit tests build it alone.
*/

#ifndef AE_ITEM_SETS_RULES_H
#define AE_ITEM_SETS_RULES_H

#include "ae_biped_pack_rules.h"

/* the kinds, in the engine's order (tag_injector.h's categories 0..8) */
enum { AE_IS_KINDS = 9 };

/* the steps of the page, each B going up one */
/* (SETTINGS is an item's own page, opened from a set's contents: its B goes back to them) */
enum { AE_IS_STEP_SETS, AE_IS_STEP_CONTENTS, AE_IS_STEP_MAPS, AE_IS_STEP_KINDS, AE_IS_STEP_ITEMS, AE_IS_STEP_SETTINGS,
	/* (the campaign swaps: their list, a swap's page, an enemy's ranks, and a slot's picks) */
	AE_IS_STEP_SWAPS, AE_IS_STEP_SWAP, AE_IS_STEP_RANKS, AE_IS_STEP_SLOT, AE_IS_STEPS };

/* the scenes DEV MODE's `menu maps sets <scene>` opens (the words are ae_maps_hub_rules.c's stage table, in this order) */
enum
{
	AE_IS_SCENE_LIST, AE_IS_SCENE_EMPTY, AE_IS_SCENE_OPTIONS, AE_IS_SCENE_DELETE, AE_IS_SCENE_PICK, AE_IS_SCENE_CANT_OPEN,
	AE_IS_SCENE_KINDS, AE_IS_SCENE_ITEMS, AE_IS_SCENE_ADDING, AE_IS_SCENE_ADDED, AE_IS_SCENE_REFUSED, AE_IS_SCENE_CONTENTS,
	AE_IS_SCENE_REMOVE, AE_IS_SCENE_FAILED, AE_IS_SCENE_BIPED, AE_IS_SCENE_SWITCHED, AE_IS_SCENE_NEW,
	/* (the real engine, a map this PC has: its kinds, a kind's items, and one item added to a new set of a fresh name) */
	AE_IS_SCENE_REAL_KINDS, AE_IS_SCENE_REAL_ITEMS, AE_IS_SCENE_REAL_ADD,
	/* (an item's settings page: a weapon's, a biped that fits a player, and one that does not; and the set's items with its body on) */
	AE_IS_SCENE_SETTINGS, AE_IS_SCENE_BODY, AE_IS_SCENE_BODY_NOFIT, AE_IS_SCENE_BODY_CARD,
	/* (campaign swaps: the list, the empty list, a swap's weapons, its enemies and body, an enemy's ranks, a slot, the delete question,
	the set picker of a new swap; a set's firing range row and its waiting state; the lobby's picker and its waiting rows) */
	AE_IS_SCENE_SWAPS, AE_IS_SCENE_SWAPS_EMPTY, AE_IS_SCENE_SWAP_WEAPONS, AE_IS_SCENE_SWAP_ENEMIES, AE_IS_SCENE_SWAP_RANKS, AE_IS_SCENE_SWAP_SLOT,
	AE_IS_SCENE_SWAP_DELETE, AE_IS_SCENE_SWAP_NEW, AE_IS_SCENE_RANGE, AE_IS_SCENE_RANGE_WAIT, AE_IS_SCENE_PICKER, AE_IS_SCENE_PICKER_WAITS,
	/* (the first screen with HOW SETS PLAY's CAMPAIGN SWAPS door focused) */
	AE_IS_SCENE_HOME,
	AE_IS_SCENES
};

/* the kinds of game a set can be asked about */
enum { AE_IS_GAME_CAMPAIGN_ALONE, AE_IS_GAME_CAMPAIGN_COOP, AE_IS_GAME_FIREFIGHT, AE_IS_GAME_CHALLENGE, AE_IS_GAME_MULTIPLAYER,
	AE_IS_GAME_ONLINE, AE_IS_GAMES };

enum { AE_IS_HINT_OPTIONS, AE_IS_HINT_PLAY, AE_IS_HINT_STOP, AE_IS_HINT_NEW, AE_IS_HINT_OPEN, AE_IS_HINT_ADD, AE_IS_HINT_ON_OFF,
	AE_IS_HINT_REMOVE, AE_IS_HINT_ADD_ITEMS, AE_IS_HINT_SEARCH, AE_IS_HINT_PAGE, AE_IS_HINT_CLOSE_MAP, AE_IS_HINT_BACK,
	AE_IS_HINT_OPEN_MAP, AE_IS_HINT_SETTINGS, AE_IS_HINT_CHANGE };
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

/* ---------- an item's settings page (Content Evolved's tag settings, by kind) */

/* the rows an item's settings page can have, in the order they are listed */
enum
{
	AE_IS_ROW_NAME, AE_IS_ROW_ON, AE_IS_ROW_FACTION, AE_IS_ROW_CLASS, AE_IS_ROW_COUNTS_AS, AE_IS_ROW_GROUP, AE_IS_ROW_HUD,
	AE_IS_ROW_REPLACE, AE_IS_ROW_PLAY_AS, AE_IS_ROW_HANDS, AE_IS_ROW_MOVEMENT, AE_IS_ROW_REMOVE, AE_IS_ROWS
};
/* the sections the rows sit under (0: none, the page's first rows) */
enum { AE_IS_SECTION_NONE, AE_IS_SECTION_RANDOMIZER, AE_IS_SECTION_LEVEL, AE_IS_SECTION_BODY, AE_IS_SECTION_SET };

/* the rows of an item of that kind, in order (at most AE_IS_ROWS): a weapon's faction, class, group and HUD; an AI's faction, what
it counts as and group; a biped's body rows (HANDS and MOVEMENT only while PLAY AS IT is on, and PLAY AS IT waiting when the biped
does not fit); every kind's name, on, and the removal. replace_known: whether the campaign's tags are known, which the row of
TAKES THE CAMPAIGN'S PLACE needs. Returns how many */
int ae_is_setting_rows(int kind, int replace_known, int play_as_on, int rows[AE_IS_ROWS]);
int ae_is_row_section(int row);
char const *ae_is_row_label(int row);
/* a row's values (a value row: faction, class, counts as, HUD, replace, play as, movement), AUTO first: how many, a value's word,
and the text the engine's setting keeps ("" for AUTO and for OFF); the index of a kept text (0 for one it does not know) */
int ae_is_value_count(int row, int kind);
char const *ae_is_value_word(int row, int kind, int index);
char const *ae_is_value_key(int row, int kind, int index);
int ae_is_value_index(int row, int kind, char const *key);
/* the index after a step (direction +1 or -1) round a list of count values */
int ae_is_value_step(int count, int index, int direction);
/* whether a typed setting (an item's name, a group name) is one the set's file keeps: empty, or letters, digits, spaces, - and _ in at
most 31 characters */
int ae_is_typed_ok(char const *text);
/* whether a row is typed (a name, a group name) */
int ae_is_row_typed(int row);
/* a row's explanation on the card (the focused row's, in plain words) */
char const *ae_is_row_card_text(int row);
/* the hands list's first two values: ITS OWN (Content Evolved's rule: the hands whose path shares most folders) and the campaign's */
char const *ae_is_hands_word(int index);
/* a hands item's kept text ("path#map") */
void ae_is_hands_key(char const *path, char const *map, char *key, int size);

/* ---------- the body (YOUR BODY: the biped a set or a campaign swap makes the player) */

/* where the body played comes from: nowhere (the campaign's own), the set playing, or a campaign swap playing */
enum { AE_IS_BODY_NONE, AE_IS_BODY_SET, AE_IS_BODY_SWAP };
/* the body a game plays. Only a campaign level played alone (and in split screen) has one: no multiplayer, online or co-op game,
Firefight or challenge. A campaign swap that plays decides: its biped when it names one, else the campaign's own, whatever its
set's PLAY AS IT says. With no swap, a set that plays through the randomizer gives its biped when it has one */
int ae_is_body_source(int game, int randomizer_on, int set_playing, int set_has_body, int swap_playing, int swap_has_body);
/* whether two paths name the same biped (the set's kept path and an item's, any case, either slash) */
int ae_is_body_is(char const *kept, char const *path);

/* the hands a set's kept HANDS names: "" ITS OWN (the hands of the biped's map that fit it best), "campaign" the campaign's own, else a
chosen hands item's kept text ("path#map") */
enum { AE_IS_HANDS_OWN, AE_IS_HANDS_CAMPAIGN, AE_IS_HANDS_CHOSEN };
int ae_is_hands_choice(char const *kept);
/* whether a kept MOVEMENT is the biped's own ("own"), else it moves as the campaign's player does */
int ae_is_movement_own(char const *kept);
/* what the players' layer (the engine's bipeds, hands and movement a level plays) holds once the body is chosen: AE_IS_LAYER_LEAVE no
body plays (the layer is not touched), AE_IS_LAYER_BODY the body (it fits a player and its biped is in the level), AE_IS_LAYER_CLEAR a body
that is not used (it does not fit, or the level has no such biped: the campaign's own player, hands and movement) */
enum { AE_IS_LAYER_LEAVE, AE_IS_LAYER_BODY, AE_IS_LAYER_CLEAR };
int ae_is_body_layer(int source, int biped_in_level, int fits);

/* what the fitness check says of a biped, as the card shows it */
struct ae_is_fit
{
	/* 0: not checked yet (the biped has not been read by a level) */
	int known;
	int fits;
	/* the rule's reasons together, each after a semicolon; "" when it fits */
	char reason[AE_BIPED_PACK_REASON_SIZE];
	/* standing height against the stock player's, in percent; the player-physics flag; the weapon labels held of those checked; the vehicle seats it has of the stock player's */
	int size_percent, physics, weapons_held, weapons_total, seats_have, seats_total;
	/* the biped has no animation graph; and whether the numbers above were measured (a level that only said yes or no leaves them 0) */
	int no_graph, measured;
};
/* a fit from the facts of a biped and of the stock player (ae_biped_pack_rules.h's rule); returns whether it fits */
int ae_is_fit_from_facts(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, struct ae_is_fit *fit);
/* a reason list as sentences, one a line (a capital, a full stop): how many */
int ae_is_fit_reason_lines(char const *reason, char lines[][160], int most);
/* why a biped does not fit, in plain words built from the facts of the fit ("It has no player physics, and it has no seat for 3 of the
stock player's 14 vehicle seats."): the sizes, the physics, the weapons held and the seats; a biped no level has measured gets a general
sentence. "" when it fits or nothing is known */
void ae_is_fit_words(struct ae_is_fit const *fit, char *text, int size);
/* a short form of it for a notice: "it has no player physics and no seat for 3 of 14 vehicle seats" style clauses joined, no capital or stop */
void ae_is_fit_short(struct ae_is_fit const *fit, char *text, int size);

/* AUTO's answers: what an item's AUTO setting finds from its path and its name, in the words the randomizer reads (the same lists as
randomizer.c's custom_weapon_covenant, custom_weapon_classes and classify, and tag_injector.c's stock HUD words; a unit test holds them
the same). row is FACTION, WEAPON CLASS, COUNTS AS or HUD DRAWN AS; the answer in capitals ("HUMAN", "PRECISION", "GRUNT", "THE PISTOL'S",
"ITS PARTS'" when no word of its name chooses a HUD). 1 when the row has an answer for the kind */
int ae_is_auto_answer(int row, int kind, char const *path, char const *name, char *out, int size);
/* the pools an item joins in the randomizer, from the settings it has (keys as the set's file keeps them, "" for AUTO) and what AUTO finds:
short_form 0: "ANY \xE2\x80\xA2 HUMAN \xE2\x80\xA2 PRECISION"; short_form 1: "Human \xE2\x80\xA2 Precision (auto)" ((auto) when any part is AUTO's) */
void ae_is_pools_words(int kind, char const *path, char const *name, char const *faction, char const *klass, char const *type, int short_form,
	char *out, int size);
/* a row's one-line rule for the help line ("AUTO finds it from the item's name, as the randomizer finds the campaign's.") */
char const *ae_is_row_help(int row);
/* whether the spawn applies HANDS and MOVEMENT: 0 until the engine plays them (the rows then wait) */
int ae_is_body_rows_live(void);
/* the kind of press a settings row takes: 0 none (a section's head, the removal and rows that wait), 1 left / right change it, 2 it is typed */
int ae_is_row_change(int row, int play_as_blocked);

/* the footer of a step; `playing` is whether the focused set plays; `has_items`: on the sets step -1 when there is no set, on the
items step 1 when the focused item is in the set, on a set's items 1 when an item is focused, on an item's settings page ae_is_row_change's
answer for the focused row; how many */
int ae_is_hints(int step, int playing, int has_items, struct ae_is_hint hints[AE_IS_HINTS_MOST]);

#endif
