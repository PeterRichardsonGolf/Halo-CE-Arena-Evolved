/*
AE_ITEM_SWAPS_RULES.H

What ITEM SETS' campaign swaps and the campaign lobby's ITEM SETS picker decide without drawing (ae_item_swaps_rules.c;
ae_screen_item_sets.c and ae_screen_item_picker.c draw it): a swap's state and its words, the rows of a swap's page and which of them
hide, the values each row steps through (Content Evolved's own keys), a slot's words, what a pick writes, which choices wait and why, the
lobby row and its value, the firing-range row's wait, and the limits. Plain C, no engine headers, so the unit tests build it alone.
*/

#ifndef AE_ITEM_SWAPS_RULES_H
#define AE_ITEM_SWAPS_RULES_H

/* Content Evolved's limits: sets, items in a set, swaps */
enum { AE_IS_LIMIT_SETS = 64, AE_IS_LIMIT_ITEMS = 256, AE_IS_LIMIT_SWAPS = 64 };
/* the campaign's weapons and enemies a swap names a slot for (tag_campaign.h's TAG_CAMPAIGN_STOCK_*) */
enum { AE_IS_SWAP_WEAPON_SLOTS = 11, AE_IS_SWAP_ENEMY_SLOTS = 10 };

/* "64 SETS: THE MOST" and its two kinds ("" under the limit): what 0 sets, 1 items in a set, 2 swaps */
char const *ae_is_limit_word(int what, long count);

/* ---------- a swap in the list */

enum { AE_IS_SWAP_OK, AE_IS_SWAP_IN_USE, AE_IS_SWAP_SET_MISSING, AE_IS_SWAP_NOTHING_ON };
/* a swap's state: its set gone, or with no item on, wins over being the one in use (it plays nothing then) */
int ae_is_swap_state(int in_use, int set_found, long items_on);
/* its word on the row ("" for none) and the tone of it: 0 none, 2 accent, 3 warning */
char const *ae_is_swap_state_word(int state);
int ae_is_swap_state_tone(int state);
/* the row's sub-line: "Spirit Kit * weapons by weapon * enemies at random", or "Its set is gone"; replace values are the file's
("random", "manual") */
void ae_is_swap_sub(int set_found, char const *set_name, char const *weapon_replace, char const *enemy_replace, char *out, int size);
/* the count line's right side and the empty state's sentence */
char const *ae_is_swaps_count_right(void);
char const *ae_is_swaps_empty_words(void);

/* ---------- a swap's page */

enum
{
	AE_IS_SW_NAME, AE_IS_SW_SET, AE_IS_SW_WEAPON_ROT, AE_IS_SW_WEAPON_PICK, AE_IS_SW_WEAPON_SLOT, AE_IS_SW_ENEMY_ROT, AE_IS_SW_ENEMY_PICK,
	AE_IS_SW_ENEMY_SLOT, AE_IS_SW_BIPED, AE_IS_SW_HANDS, AE_IS_SW_MOVEMENT, AE_IS_SW_ROWS
};
enum { AE_IS_SWS_NONE, AE_IS_SWS_WEAPONS, AE_IS_SWS_ENEMIES, AE_IS_SWS_BODY };
enum { AE_IS_SWAP_ROWS_MOST = 2 + 2 + AE_IS_SWAP_WEAPON_SLOTS + 2 + AE_IS_SWAP_ENEMY_SLOTS + 3 };
struct ae_is_swap_row { int row, slot; };
/* the rows of a swap's page in order: the weapon rows only while the weapons are picked BY WEAPON, the enemy rows only BY ENEMY, HANDS and
MOVEMENT only with a biped picked. Returns how many (at most AE_IS_SWAP_ROWS_MOST) */
int ae_is_swap_rows(int by_weapon, int by_enemy, int has_biped, struct ae_is_swap_row rows[AE_IS_SWAP_ROWS_MOST]);
int ae_is_swap_row_section(int row);
char const *ae_is_swap_section_name(int section);
char const *ae_is_swap_row_label(int row);
/* a value row (the two rotations, the two pickings, MOVEMENT), first value the default: how many, a word, the key the file keeps ("" for
the default of MOVEMENT), and the index of a kept key (0 for one it does not know) */
int ae_is_swap_value_count(int row);
char const *ae_is_swap_value_word(int row, int index);
char const *ae_is_swap_value_key(int row, int index);
int ae_is_swap_value_index(int row, char const *key);
/* the keys of the file each row keeps its value under ("weapon_rotation", ...); "" for a row that is not a plain value */
char const *ae_is_swap_row_file_key(int row);
/* the section of the file's lists a slot row keeps its picks in ("weapons", "enemies", "loadout") */
char const *ae_is_swap_slot_section(int row, int loadout);
/* what a row does with A, 0 nothing, 1 left / right change it, 2 it is typed, 3 it opens a screen of its own (a slot, the item set's picker) */
int ae_is_swap_row_change(int row);
char const *ae_is_swap_row_card_text(int row);
char const *ae_is_swap_row_help(int row);

/* ---------- a slot of a swap (the weapon or the enemy it takes the place of) */

/* the row's value: "AS IT IS" with no pick, else "3 PICKED" */
void ae_is_slot_value(int picked, char *out, int size);
/* the screen's count line: "AS IT IS", "2 PICKED" or "3 PICKED * ONE AT RANDOM EACH TIME" */
void ae_is_slot_count_line(int picked, char *out, int size);
char const *ae_is_slot_replaced_head(int weapon);
char const *ae_is_slot_armed_head(void);
char const *ae_is_slot_own_word(int section);
char const *ae_is_slot_card_text(int weapon, char const *name, int mixed_in);
char const *ae_is_slot_help(int weapon);
char const *ae_is_slot_unarmed_words(void);

/* ---------- the lobby's picker and row */

enum { AE_IS_PICK_NONE, AE_IS_PICK_SWAP, AE_IS_PICK_SET };
/* what a pick writes: whether the swap setting is cleared, whether the randomizer's set is cleared, and what the injection switch becomes */
struct ae_is_pick_writes { int clear_swap, clear_set, swap_file, set_folder, injection; };
void ae_is_pick_writes(int pick, struct ae_is_pick_writes *writes);
/* the wait a swap's row shows in the picker, from its state ("" none) */
char const *ae_is_pick_swap_wait(int state);
/* the wait a set's row shows: no items on, the randomizer off, or an MCC mission (a set plays only through the randomizer, which has no MCC) */
char const *ae_is_pick_set_wait(long items_on, int randomizer_on, int mcc_mission);
/* the value of a row of the picker ("IN USE", its set's name for a swap, "3 ITEMS ON" for a set) */
void ae_is_pick_swap_value(int state, char const *set_name, char *out, int size);
void ae_is_pick_set_value(int in_use, long items_on, char *out, int size);
char const *ae_is_pick_none_value(int in_use);
char const *ae_is_pick_none_help(void);
void ae_is_pick_swap_help(char const *swap_name, char const *set_name, char *out, int size);
void ae_is_pick_set_help(char const *set_name, char *out, int size);
/* the lobby row: shown only with a set that has an item on, never in a challenge or an online co-op lobby */
int ae_is_lobby_row_shown(int has_set_on, int daily, int online_coop);
/* the row's value: NONE, the swap's name, or "<SET> * RANDOMIZER" (kind is AE_IS_PICK_*); the name in capitals */
void ae_is_lobby_value(int pick, char const *name, char *out, int size);
/* the RANDOMIZER row's wait while a swap plays ("SWAP PLAYS INSTEAD", else "") and its help */
char const *ae_is_randomizer_wait(int swap_picked);
char const *ae_is_randomizer_wait_help(void);
char const *ae_is_lobby_row_help(void);

/* ---------- a set's TRY IT ON THE FIRING RANGE row */

/* the wait: no item on, or no Xbox-format bloodgulch.map (the range is built on it); "" when it can start */
char const *ae_is_range_wait(long items_on, int range_map_ok);

#endif
