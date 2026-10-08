/*
AE_WIDGETS.H

The AE menus' widget core (ae_widgets.c, spec 4.1-4.3): the pointer's hit
rectangles, text fitted and wrapped, the separator dot and dashed outlines,
the row, the group header, and the list with its scrollbar. Each widget is one
call taking a rectangle, its state and a density (ae_style.h), drawing through
ae_draw.h in the current view's units and recording its hit rectangles. Sizes
and colours come from ae_style.h and the spec; this file and ae_widgets.c are
the only place they are applied. Pure: no engine includes (unit tests:
port/linux/tests/ae_widgets_test.c with the recording ae_draw stub).
*/

#ifndef __AE_WIDGETS_H
#define __AE_WIDGETS_H

#include "ae_style.h"
#include "ae_list.h"
#include "ae_ui.h"
#include "ae_motion.h"

/* hits: what the pointer can act on, recorded as widgets draw, in layout units of the whole frame */
enum { AE_HITS_MAXIMUM = 512 };
enum { AE_PART_ROW, AE_PART_ARROW_LEFT, AE_PART_ARROW_RIGHT, AE_PART_VALUE, AE_PART_TRACK, AE_PART_THUMB,
	AE_PART_TAB, AE_PART_DOT, AE_PART_PROMPT, AE_PART_CHIP, AE_PART_ITEM, AE_PART_KEY, AE_PART_CHOICE,
	AE_PART_FIELD, AE_PART_CARD, AE_PART_OUTSIDE };
struct ae_hit { struct ae_rect rect; short id, part, index, layer; };
void ae_hits_clear(void);                      /* before ae_ui_draw each frame (ae_hooks) */
void ae_hits_layer(short layer);               /* the screen index drawing now (before_draw) */
/* nonzero when a hit found the AE_HITS_MAXIMUM full since the last clear (it was dropped: ae_hooks logs it) */
int ae_hits_overflowed(void);
/* x, y, width, height in the current ae_draw view's units (stored in layout units via ae_draw_current_view) */
void ae_hit_add(float x, float y, float width, float height, short id, short part, short index);
/* the last added hit under a layout point at layer >= minimum_layer; 0 when none */
int ae_hit_at(float x, float y, short minimum_layer, struct ae_hit *hit);
/* text fitted to width with "…" (U+2026); 1 when cut; never splits a UTF-8 sequence */
int ae_fit_text(int font, float size, const char *text, float width, char *out, int out_size);
/* text wrapped at spaces into at most maximum lines of width (a word wider than the width is cut with "…", and
so is the last line when more text is left); returns the line count */
int ae_wrap_text(int font, float size, const char *text, float width, char lines[][160], int maximum);
/* the separator: a drawn dot (never U+00B7) of diameter u, centred on x, y */
void ae_widget_separator(struct ae_density const *density, float x, float center_y, float diameter_u,
	unsigned int rgba);
/* a dashed outline (chips, preview area, open card): dash_u on, dash_u off */
void ae_widget_dashed(float x, float y, float width, float height, float thickness, float dash, unsigned int rgba);

enum
{
	AE_ROW_FOCUSED = 1 << 0, AE_ROW_HOVER = 1 << 1, AE_ROW_CHANGED = 1 << 2, AE_ROW_AE = 1 << 3,
	AE_ROW_DISABLED = 1 << 4, AE_ROW_ON_BAR = 1 << 5,   /* drawn on the selection bar (dark text) */
	AE_ROW_BAR_BY_LIST = 1 << 6,                       /* the list draws the moving bar, the row not */
	AE_ROW_HOVER_LEFT = 1 << 7, AE_ROW_HOVER_RIGHT = 1 << 8,
	/* (with AE_ROW_BAR_BY_LIST) the moving bar overlaps the row: the row draws no background either */
	AE_ROW_UNDER_BAR = 1 << 9
};
struct ae_row
{
	const char *label;          /* UTF-8 */
	const char *value;          /* NULL: an action row (opens something) */
	const char *base_value;     /* the struck base value, shown when AE_ROW_CHANGED */
	const char *reason;         /* shown instead of the value when AE_ROW_DISABLED */
	unsigned int flags;
	short tick_side;            /* value tick: -1, 1, 0 none */
	struct ae_motion const *tick;
	const char *old_value;      /* during a tick */
};
/* a row's parts (drawing units, x from the row's left) */
struct ae_row_layout
{
	float height, label_x, label_width, value_x, value_width, base_x, base_width, dot_x, badge_x, badge_width;
	float arrow_left_x, arrow_right_x, arrow_cell;
	int label_cut;
	char label[160];            /* the label as drawn (with "…" when cut) */
	/* (M2) the struck base value as drawn ("…" when short of room, empty when left out), and whether the changed dot
	and the AE badge are drawn (left out only when even an empty label leaves them no room) */
	char base[160];
	int dot, badge;
};
void ae_row_layout(struct ae_density const *density, float width, struct ae_row const *row,
	struct ae_row_layout *layout);
float ae_widget_row(struct ae_density const *density, float x, float y, float width, struct ae_row const *row,
	short hit_id, short index);                                     /* returns its height */
float ae_widget_group(struct ae_density const *density, float x, float y, float width, const char *label);

typedef void (*ae_list_item_draw)(void *context, short item, float x, float y, float width, float height,
	unsigned int flags /* AE_ROW_* */);
struct ae_list_view
{
	struct ae_list list;
	struct ae_motion scroll, bar;   /* first (rows) and the selection bar's row (items; drawn in drawing units), 100 ms */
	short previous_focus;
	short hover;                    /* -1 none */
	int mouse;                      /* the scrollbar's mouse look, after a mouse move, until pad / key input */
	int dragging;
	float drag_grab;                /* where in the thumb it was grabbed */
	/* (as last drawn, layout units: the scrollbar's track and thumb, for the pointer) */
	float track_top, track_height, thumb_height;
	/* (as last drawn, layout units: the list's whole rectangle, the wheel's target; and its hit layer: the wheel
	scrolls it only where nothing of a higher layer, a popover over it, lies under the pointer) */
	struct ae_rect area;
	short layer;
};
/* a list of count items in rows rows: the focus on the first, no motion running, nothing hovered */
void ae_list_view_init(struct ae_list_view *view, short count, short rows);
/* lays out rows of the density's row height and gap in height, draws the visible ones through draw_item (clipped),
the moving selection bar, overflow fades and "▲ / ▼ N more", and the scrollbar; records hits */
void ae_widget_list(struct ae_density const *density, struct ae_list_view *view, float x, float y, float width,
	float height, ae_list_item_draw draw_item, void *context, short hit_id);
short ae_list_view_rows(struct ae_density const *density, float height);
/* the thumb, in the list's own coordinates (the track from 0 to track_height): never from view or frame offsets
(the M1 right-hand-view bug) */
void ae_scrollbar_thumb(short count, short rows, float first, float track_height, float minimum,
	float *thumb_y, float *thumb_height);
/* the first row for a thumb dragged to thumb_y (same coordinates) */
short ae_scrollbar_first(short count, short rows, float track_height, float thumb_height, float thumb_y);
/* events: up/down move (cursor sound), triggers / PgUp / PgDn page by the visible rows minus one, wrap off;
returns 1 when handled */
int ae_list_view_event(struct ae_list_view *view, struct ae_event const *event);
/* pointer: hover focuses only when pointer->moved; the wheel scrolls 3 rows and never moves the focus unless it
leaves the window; track click pages; thumb drags; returns the item clicked or -1 */
short ae_list_view_pointer(struct ae_list_view *view, struct ae_pointer const *pointer, short hit_id);

/* ---------- navigation (ae_widgets_nav.c, spec 4.4, 4.10, 4.13, 6) */

struct ae_tab { const char *label; int changed; int disabled; const char *disabled_reason; };
struct ae_tabs { struct ae_tab const *tabs; short count, active, hover; struct ae_motion strip;
	int drawn;   /* (M2) set at the first draw: a strip starts where it belongs, later changes slide */ };
/* draws the strip (LB / RB glyphs or Q / E caps at its ends), records hits; returns its height */
float ae_widget_tabs(struct ae_density const *density, struct ae_tabs *tabs, float x, float y, float width,
	short hit_id);
/* the next enabled tab in direction (wraps; skips disabled), or the same when none */
short ae_tabs_step(struct ae_tabs const *tabs, short direction);
/* LB / RB / Q / E / clicks: 1 handled; a click on a disabled tab returns 2 (the screen shows why) */
int ae_tabs_event(struct ae_tabs *tabs, struct ae_event const *event);
int ae_tabs_pointer(struct ae_tabs *tabs, struct ae_pointer const *pointer, short hit_id);

void ae_widget_page_dots(struct ae_density const *density, float center_x, float y, const char *page_title,
	short count, short current, unsigned int current_color, short hit_id);
short ae_page_step(short count, short current, short direction);   /* wraps */
/* in place (left / right on a row): the new index, wrapping off; the caller starts the value tick */
short ae_value_step(short count, short current, short direction);

struct ae_prompt { int button; const char *label; const char *key; /* NULL: ae_prompt_key(button) */ int action; };
const char *ae_prompt_key(int button);      /* A Enter, B Esc, X Ctrl+F, Y R, LB Q, RB E, LT PgUp, RT PgDn,
                                               START Enter */
unsigned int ae_prompt_tint(int device, int button);   /* AE_TINT_*, or AE_COLOR_KEY_CAP */
/* the footer: a row whose centre is center_y; 32 u between prompts; status right-aligned at right_x (muted);
pressed: the prompt index the pointer holds down (-1) */
void ae_widget_prompts(struct ae_density const *density, float x, float center_y, struct ae_prompt const *prompts,
	short count, const char *status, float right_x, short pressed, short hit_id);
/* a click on a prompt: its action as an event of player 0 from the keyboard; 1 when sent. (A pointer move over the
prompts also sets which one shows the hover wash at the next draw) */
int ae_prompts_pointer(struct ae_prompt const *prompts, short count, struct ae_pointer const *pointer,
	short hit_id, struct ae_event *event);
/* a drawn key cap with words (keyboard prompts, PlayStation OPTIONS / CREATE); returns its width */
float ae_widget_key_cap(struct ae_density const *density, float x, float y, float height, const char *words);
float ae_key_cap_width(struct ae_density const *density, float height, const char *words);   /* (M2) */

/* the in-view panel (§6) in a view of view_width x view_height pixels, drawn in an ae_draw view of that view:
view-panel fill, a 3 u top stripe in the player colour, header (emblem with the slot number, "PLAYER n", page name),
page dots under it in the player colour; *content gets the rectangle left for the page (drawing units), above the
help strip and prompts */
void ae_widget_view_panel(struct ae_density const *density, float view_width, float view_height, short player,
	unsigned int player_color, const char *page_name, short page, short page_count, struct ae_rect *content);
/* (M2) where the panel's footer goes, drawing units: the help strip's top and the prompts row's centre (under the
content ae_widget_view_panel leaves, a rule between them and the prompts) */
void ae_view_panel_footer(struct ae_density const *density, float view_width, float view_height, float *help_y,
	float *prompts_center_y);
/* VIEW's help: two lines (Overpass 750 body, minor floor) above the prompts, the second cut with "…" */
void ae_widget_help_strip(struct ae_density const *density, float x, float y, float width, const char *text);

/* ---------- picking (ae_widgets_pick.c, spec 4.5-4.7) */

/* the open list's width: the longest value (Overpass 900 22 u) + 76 u, at least 240 u (drawing units) */
float ae_picker_width(struct ae_density const *density, const char *const *values, short count);
/* the open list's placement: width = the longest value + 76 u (at least 240 u), items item_height, at most 8
visible; the current item over the row; upward when it would leave the bounds below; never outside bounds (the
frame, or the VIEW panel) */
void ae_picker_place(struct ae_rect const *row, struct ae_rect const *bounds, short count, short current,
	float item_height, float width, struct ae_rect *popover, short *first_visible);
/* opens the picker as a popover screen owned by owner; picked(index, context) on A / Enter / click; B / Esc / a
click outside close it unchanged; focus returns to the opening row (focus memory). Every rect of a popover spec is in
layout units of the whole frame; view is the player's view it draws in (preflight P12: empty for FULL), so a VIEW
popover draws in that view, inside its panel */
struct ae_picker_spec
{
	const char *const *values; short count, current;
	struct ae_rect row;          /* layout units */
	struct ae_rect bounds;       /* layout units */
	struct ae_density density;
	void (*picked)(short index, void *context); void *context;
	struct ae_rect view;         /* (M2, P12) layout units; zero size: the whole frame */
};
int ae_picker_open(struct ae_picker_spec const *spec, short owner);

enum { AE_CHIP_ON = 1, AE_CHIP_FOCUSED = 2, AE_CHIP_HOVER = 4, AE_CHIP_UNSUPPORTED = 8, AE_CHIP_DISABLED = 16 };
struct ae_chip { const char *label; short count; /* -1 none */ unsigned int flags; };
/* lays chips out wrapping in width: rects (drawing units, relative to x, y; room for count of them); returns the
height (ae_widget_chips draws at most 32) */
float ae_chips_layout(struct ae_density const *density, float width, struct ae_chip const *chips, short count,
	struct ae_rect *rects);
float ae_widget_chips(struct ae_density const *density, float x, float y, float width, struct ae_chip const *chips,
	short count, short hit_id);

struct ae_help
{
	const char *title, *body;
	const char *const *values; short value_count, current;   /* value chips; value_count 0: none */
	const char *default_value;                               /* NULL: no "Default:" line */
	const char *changed_from;                                /* NULL: unchanged */
	int preview;                                             /* reserve the preview area */
};
/* the help / preview panel in rect; *preview gets the preview area (or zero size); never takes focus. Its hits: the
value chips (AE_PART_CHIP, the value's index) and the reset (AE_PART_PROMPT, index -1) */
void ae_widget_help(struct ae_density const *density, struct ae_rect const *rect, struct ae_help const *help,
	short hit_id, struct ae_rect *preview);

/* ---------- text (ae_widgets_text.c, spec 4.8-4.9; the glue: ae_glue_text.c) */

#include "ae_text_edit.h"

/* a key typed while a field is being edited (the game's input_get_key: the platform's typing mode) */
enum { AE_TEXT_KEY_CHAR, AE_TEXT_KEY_BACKSPACE, AE_TEXT_KEY_DELETE, AE_TEXT_KEY_HOME, AE_TEXT_KEY_END,
	AE_TEXT_KEY_SELECT_ALL, AE_TEXT_KEY_COPY, AE_TEXT_KEY_PASTE };
struct ae_text_key { short kind; short shift; char character; };
/* the engine contact the pure widgets use (preflight P13): ae_glue_text_install puts the game's in (the hooks call it
when the menus are on), a test its own; with none, each does nothing (and reads nothing) */
struct ae_host
{
	void (*write_log)(const char *text);
	void (*text_begin)(void);
	void (*text_end)(void);
	int (*text_keys)(struct ae_text_key *keys, int maximum);
	int (*clipboard_get)(char *text, int size);
	void (*clipboard_set)(const char *text);
};
void ae_host_set(struct ae_host const *host);
void ae_host_log(const char *text);
void ae_host_text_begin(void);
void ae_host_text_end(void);
int ae_host_text_keys(struct ae_text_key *keys, int maximum);
int ae_host_clipboard_get(char *text, int size);
void ae_host_clipboard_set(const char *text);
/* (ae_glue_text.c, game side) the platform's typing mode and keys, the clipboard */
void ae_glue_text_begin(void);   /* platform_text_field(TRUE); drains input_get_key (as text_field_begin does) */
void ae_glue_text_end(void);     /* platform_text_field(FALSE) */
int ae_glue_text_keys(struct ae_text_key *keys, int maximum);   /* this frame's (input_get_key) */
int ae_glue_clipboard_get(char *text, int size);
void ae_glue_clipboard_set(const char *text);
/* (M2) the glue as the host; whether a field is being typed into (AE's own keys then stand aside: ae_input.c) */
void ae_glue_text_install(void);
int ae_glue_text_typing(void);

enum { AE_FIELD_FOCUSED = 1, AE_FIELD_EDITING = 2, AE_FIELD_HOVER = 4, AE_FIELD_DISABLED = 8, AE_FIELD_ERROR = 16 };
struct ae_field { const char *label; const char *placeholder; struct ae_text *text; unsigned int flags;
	const char *error; unsigned long caret_since; };
float ae_widget_field(struct ae_density const *density, float x, float y, float width, struct ae_field const *field,
	short hit_id);   /* returns its height incl. label and error line */
/* (M2) typing into a field (keyboard and mouse: Enter or a click on it starts editing, preflight P17; a pad's A opens
AE's keyboard instead): begin keeps the text to restore on cancel and starts the platform's typing mode (the host);
keys applies this frame's typed keys; end keeps the text or restores it. ae_field_type applies keys to a text (pure):
typing, Backspace, Delete, Home, End, Ctrl+A / C / V; 0 when something was refused (the limit, a character fields
don't take): the caller plays failure once */
/* One physical keyboard: one typing OWNER at a time (an edit), never two fields receiving keys. A begin from another
edit commits the owner's (keeps its text, as Enter would; its done(1)) and takes the keys; only the owner's end turns
typing mode off. An edit whose screen is gone (holder no longer on the stack: ae_field_edit_guard, once a frame),
a reset stack (ae_ui_reset) or menus going away (ae_field_edit_abort) is cancelled (its text restored, done(0)) and
typing mode ends. done (optional) hears an end it didn't call itself; holder (optional) is its screen's data */
struct ae_field_edit { struct ae_text *text; struct ae_text before; int active;
	void (*done)(int keep, void *context); void *context; void const *holder; };
void ae_field_edit_begin(struct ae_field_edit *edit, struct ae_text *text);
int ae_field_edit_keys(struct ae_field_edit *edit);
void ae_field_edit_end(struct ae_field_edit *edit, int keep);
int ae_field_type(struct ae_text *text, struct ae_text_key const *keys, int count);
/* (M2) the owner's safety nets: guard ends an edit whose holder left the stack; abort ends any; live: an edit owns
the keys now */
void ae_field_edit_guard(void);
void ae_field_edit_abort(void);
/* (M2) AE's screens not shown this frame: abort only when the stack is empty (the server browser over them or no
renderer: an open edit is kept) */
void ae_field_edit_screens_hidden(void);
int ae_field_edit_live(void);
/* opens AE's keyboard (a popover) for a field: only after a pad's A on the field; a keyboard types into the field
whenever it is editing, keyboard shown or not. Its rects are layout units, view the player's view it draws in
(preflight P12: zero size for the whole frame); one per owner at a time (0 when the owner's is open) */
struct ae_keyboard_spec { struct ae_text *text; struct ae_rect field; struct ae_rect bounds; struct ae_density density;
	void (*done)(int keep, void *context); void *context;
	struct ae_rect view;         /* (M2, P12) */
	/* (M2) the field's edit: a printable key typed on a physical keyboard while this keyboard is open closes it (kept)
	and goes on typing into the field through it, the first key inserted; NULL: the key is inserted, typing ends */
	struct ae_field_edit *edit; };
int ae_keyboard_open(struct ae_keyboard_spec const *spec, short owner);
/* the keyboard's keys: rects (drawing units, relative to the popover) for a page (0 letters, 1 symbols) */
short ae_keyboard_layout(struct ae_density const *density, float content_width, int page, struct ae_rect *keys,
	const char **labels, short maximum);

/* ---------- (M2, ae_widgets_dialog.c) the dialog and the roster cards (spec 4.11, 4.12) */

/* A dialog: a popover over the scrim (its open motion: ae_motion_dialog; its close is instant, preflight P7), a 4 u top
stripe (accent; an error's warning, with a warning title), the title (Overpass 900 32 u), the body (22 u on 32 u
lines), the choices as ordinary rows and its prompts inside it. The first focus is the safe choice; A / Enter picks
the focused choice, B / Esc the cancel choice; a click outside does nothing; the opener's focus is untouched (focus
memory). A timed revert shows "Reverting in N s" over a 3 u accent bar draining over 10 s (REDUCE MOTION: in whole
seconds) and picks timeout_choice at 0 (ae_dialog_tick: covered by other screens too; a reset stack picks it at
once). An error plays failure as it opens and logs "ae menus: error: <title>: <body>" (the glue's reason text as it
is). Rects are layout units of the whole frame; view is the player's view it draws in (preflight P12: zero size for
FULL). The dialog copies its strings (the caller's need not outlive the call). One per owner at a time (0 when the
owner's is open); an error then is logged and fails at once and waits, shown when the slot frees (a newer error
replaces a waiting one; a reset drops it) */
enum { AE_DIALOG_CONFIRM, AE_DIALOG_TIMED_REVERT, AE_DIALOG_ERROR };
enum { AE_DIALOG_CHOICES = 4, AE_DIALOG_REVERT_MS = 10000 };
struct ae_dialog_spec
{
	short kind;
	const char *title, *body;
	const char *choices[AE_DIALOG_CHOICES]; short choice_count;
	short safe_choice;            /* the first focus (CANCEL on quit, KEEP after a display change, BACK on an error) */
	short cancel_choice;          /* B / Esc */
	short timeout_choice;         /* timed revert: picked at 0 (REVERT); 10 s */
	struct ae_density density;
	struct ae_rect bounds;        /* FULL: the frame; VIEW: the panel (the dialog draws inside it, at its width) */
	void (*picked)(short choice, void *context); void *context;
	struct ae_rect view;          /* (P12) */
};
int ae_dialog_open(struct ae_dialog_spec const *spec, short owner);
/* once a frame (ae_hooks, AE's screens shown or not): timed reverts at 0 pick their timeout choice wherever they are
on the stack; a waiting error opens once its slot is free */
void ae_dialog_tick(void);
/* the dialog's box (layout units): 720 u wide (the body wrapped in it), up to 840 u for a wide title or choice,
centred in the bounds; under 720 u only when the room forces it: VIEW (preflight P14) at most the bounds' width less
a pad each side */
void ae_dialog_place(struct ae_dialog_spec const *spec, struct ae_rect *box);

enum { AE_CARD_FOCUSED = 1, AE_CARD_EDITING = 2, AE_CARD_AWAY = 4, AE_CARD_GUEST = 8, AE_CARD_HOST = 16,
	AE_CARD_TEAM = 32, AE_CARD_HOVER = 64 };
struct ae_roster_card
{
	const char *name; short slot; unsigned int color;          /* emblem: the colour WITH the slot number */
	const char *sub_line;                                     /* input, where, state */
	const char *team_name; unsigned int team_color;           /* AE_CARD_TEAM */
	const char *reason;                                       /* away / disconnected */
	unsigned int flags;
};
/* a roster card (66 u, VIEW 52): the emblem (42 u, VIEW 32) with the slot number, the name, one sub-line (a guest's
says "guest (not saved)", an away one's its reason), the team stripe and name, the HOST badge; focus a white bar,
editing a 2 u outline in the player's colour, away 43 %. Hit AE_PART_CARD, index; returns its height */
float ae_widget_roster_card(struct ae_density const *density, float x, float y, float width,
	struct ae_roster_card const *card, short hit_id, short index);
/* the open card, always last: a dashed rule, "Press [START] to add a player (split screen)"; keyboard_only ->
"Connect a controller and press START" (hit AE_PART_CARD, index -1) */
float ae_widget_open_card(struct ae_density const *density, float x, float y, float width, int keyboard_only,
	short hit_id);
/* (ruling) the slot number's colour on an emblem: AE_COLOR_SELECTION_TEXT or AE_COLOR_TITLE, whichever contrasts
more with the emblem's colour (WCAG contrast ratio) */
unsigned int ae_emblem_number_color(unsigned int emblem_rgba);

#endif
