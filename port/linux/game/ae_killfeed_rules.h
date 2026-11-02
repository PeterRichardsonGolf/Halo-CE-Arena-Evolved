/*
AE_KILLFEED_RULES.H

The kill feed as pure rules (port/linux/tests/ae_killfeed_rules_test.c runs them): which weapon a kill's damage names, the words and
tags of the weapons, how long a row lives and how a list of rows overflows, where a row's parts sit, the colour a name takes, and the
line a split-screen player's message list gets. Nothing here knows the engine.

The feed shows everyone's kills as rows: killer, the weapon's picture, an optional mark (a head shot's ring or a perfect kill's tag),
victim. Rows that involve the player (their kill, their death, or a kill they helped with) are marked with a light tick and a faint backing.
Every row lives five seconds. Time is the game's ticks (30 a second), so a paused game holds its feed. A list holds at most AE_KF_MOST
rows, and there is no queue: with a full list the oldest row leaves once it has had three seconds, and when a sixth arrives before
that the oldest goes at once.

Sizes are a stage of 1080 lines, scaled by the view. Where the feed stands, how it is aligned and how strong the backing of the
player's own rows is are the constants below, so that a change of them is one small edit.
*/

#ifndef __AE_KILLFEED_RULES_H
#define __AE_KILLFEED_RULES_H

#include <wchar.h>

#include "ae_pure_types.h"

/* ---------- where it stands, how it is aligned, how strong its own rows' backing is: the constants that may change */

#define AE_KF_ALIGN_LEFT 0
#define AE_KF_ALIGN_RIGHT 1
/* the rows' edge against the anchor: left aligned, rows start at the anchor */
#define AE_KF_ALIGN AE_KF_ALIGN_LEFT
/* the stack grows up from its anchor (the newest row nearest the anchor); 0: down from it */
#define AE_KF_GROWS_UP 1
/* the anchor: the stack's bottom edge on the stage (with the MODERN medals, and with the MCC medals above their holder), and the
aligned side's tick this far left of the motion sensor's left edge; where the sensor is not drawn, this place of the stage */
#define AE_KF_BOTTOM 782.0f
#define AE_KF_BOTTOM_MCC 517.0f
#define AE_KF_SENSOR_GAP 13.0f
#define AE_KF_FALLBACK_X 127.0f
/* a row of the player's own: a faint backing and a light tick on the aligned side (an assist's is fainter still) */
#define AE_KF_BACKING_ALPHA 0.30f
#define AE_KF_BACKING_ALPHA_ASSIST 0.18f
#define AE_KF_TICK_ALPHA 0.95f
#define AE_KF_TICK_ALPHA_ASSIST 0.70f

/* ---------- sizes (the stage's) */

#define AE_KF_TEXT_PX 18.0f
#define AE_KF_TEXT_FLOOR_PX 16.0f
#define AE_KF_ROW_HEIGHT 26.5f
#define AE_KF_ROW_PITCH 28.5f
#define AE_KF_ICON_WIDTH 72.0f
#define AE_KF_ICON_HEIGHT 24.0f
#define AE_KF_MARK_SIZE 18.0f
#define AE_KF_GAP 8.0f
#define AE_KF_INSET 12.0f
#define AE_KF_PLATE_RADIUS 5.0f
#define AE_KF_TICK_WIDTH 3.0f
/* a text tag (a placeholder where there is no picture) is drawn at this share of the text */
#define AE_KF_TAG_SHARE 0.80f

/* ---------- time (game ticks) */

#define AE_KF_TICKS_PER_SECOND 30
#define AE_KF_MOST 5
/* the rows a list keeps while some are leaving */
#define AE_KF_ROWS 10
#define AE_KF_LIFE (5 * AE_KF_TICKS_PER_SECOND)
/* with a full list the oldest row's life ends once it has had this long */
#define AE_KF_FULL_LEAVE_TICKS (3 * AE_KF_TICKS_PER_SECOND)
#define AE_KF_FADE_TICKS 15
/* a new row slides in and fades in over this, the older ones dropping a pitch in it; rows close up over CLOSE; a row pushed
out leaves in GONE */
#define AE_KF_ENTER_TICKS 4
#define AE_KF_CLOSE_TICKS 4
#define AE_KF_GONE_TICKS 3
#define AE_KF_SLIDE 16.0f
/* a hit on the victim this long before the death makes an assist */
#define AE_KF_ASSIST_TICKS 180

/* ---------- weapons */

enum ae_kf_weapon
{
	AE_KF_WEAPON_UNKNOWN,
	AE_KF_WEAPON_PISTOL,
	AE_KF_WEAPON_ASSAULT_RIFLE,
	AE_KF_WEAPON_SNIPER,
	AE_KF_WEAPON_SHOTGUN,
	AE_KF_WEAPON_ROCKET,
	AE_KF_WEAPON_PLASMA_PISTOL,
	AE_KF_WEAPON_PLASMA_RIFLE,
	AE_KF_WEAPON_NEEDLER,
	AE_KF_WEAPON_FUEL_ROD,
	AE_KF_WEAPON_FLAME,
	AE_KF_WEAPON_BATTLE_RIFLE,
	AE_KF_WEAPON_DMR,
	AE_KF_WEAPON_EVO,
	AE_KF_WEAPON_CARBINE,
	AE_KF_WEAPON_SMG,
	AE_KF_WEAPON_SMG_SILENCED,
	AE_KF_WEAPON_AR_SILENCED,
	AE_KF_WEAPON_SWORD,
	AE_KF_WEAPON_HAMMER,
	AE_KF_WEAPON_BOMB,
	AE_KF_WEAPON_FRAG,
	AE_KF_WEAPON_PLASMA_GRENADE,
	AE_KF_WEAPON_MELEE,
	AE_KF_WEAPON_VEHICLE,
	AE_KF_WEAPON_FALL,
	AE_KF_WEAPON_COUNT
};

/* what a damage effect's (or a weapon's) tag path names: by the words in it, case and slash direction aside; UNKNOWN for none */
int ae_kf_weapon_of_path(char const *path);
/* a weapon pack's slot (ae_weapon_slots.h's numbers: 1 battle rifle, 2 SMG, 3 silenced SMG, 4 carbine, 5 silenced assault
rifle, 6 DMR, 7 EVO rifle) as a weapon; UNKNOWN for another */
int ae_kf_weapon_of_slot(int slot);
/* the word a split-screen line says (PISTOL, AR, ROCKETS, GRENADE...): "" for UNKNOWN */
char const *ae_kf_weapon_word(int weapon);
/* the plain text tag drawn where a weapon has no picture (the placeholder): its word, but PLASMA for the plasma grenade and
"?" for UNKNOWN */
char const *ae_kf_weapon_tag(int weapon);
/* a damage tag's weapon tag by its folder: "weapons\pistol\bullet" is "weapons\pistol\pistol"; FALSE for a path not of a
weapon's folder */
boolean ae_kf_weapon_tag_path(char const *damage_path, char *out, int size);
/* whether a weapon is a melee-range or environment kill (no picture is looked for) */
boolean ae_kf_weapon_has_no_picture(int weapon);

/* ---------- a row */

enum ae_kf_kind
{
	AE_KF_KIND_KILL,		/* killer, weapon, victim */
	AE_KF_KIND_SUICIDE,		/* the skull and the victim */
	AE_KF_KIND_ENVIRONMENT	/* the weapon's own picture or tag (a fall...) and the victim */
};

#define AE_KF_NAME 13

struct ae_kf_row
{
	boolean used;
	long born;
	/* the tick its life ends (the fade follows), and when it was pushed out (-1 while it was not) */
	long life_end;
	long gone_at;
	/* the slot (0 the newest) it is moving from and to, and when the move began */
	real from;
	real to;
	long moved_at;
	/* what the viewer is to it */
	boolean mine;		/* the viewer killed, or died */
	boolean assist;		/* the viewer helped, and did neither */
	/* the kill's facts */
	int kind;
	int weapon;
	long weapon_tag;	/* the engine's weapon tag index to take a picture from, -1 for none */
	boolean headshot;
	boolean perfect;
	boolean betrayal;
	/* who: the player's index (-1 for none) and team, for the viewer's ally or enemy colour, and the names */
	long killer;
	long victim;
	short killer_team;
	short victim_team;
	wchar_t killer_name[AE_KF_NAME];
	wchar_t victim_name[AE_KF_NAME];
};

struct ae_kf_list
{
	struct ae_kf_row rows[AE_KF_ROWS];
	long last;			/* the tick of the last call */
};

void ae_kf_list_reset(struct ae_kf_list *list);
/* the row joins as the newest. now before the last call is a new game: the list empties first. The row's born, life, slots and
leaving are set here. With AE_KF_MOST rows already live the oldest goes (leaves in AE_KF_GONE_TICKS); with the list full after it
joins, the oldest's life ends once it has had AE_KF_FULL_LEAVE_TICKS. Returns the rows that were live before it joined */
int ae_kf_list_add(struct ae_kf_list *list, struct ae_kf_row const *row, long now);
/* the time is now: rows past their life are dropped, the rest take the slots the live ones above them leave, and a full list's
oldest row is given its leaving time. A tick before the last one empties the list */
void ae_kf_list_step(struct ae_kf_list *list, long now);
/* how many rows are in the list at all (leaving ones too) and how many hold a slot */
int ae_kf_list_count(struct ae_kf_list const *list);
int ae_kf_list_live(struct ae_kf_list const *list, long now);
/* a row's slot (0 the newest, a fraction while it moves), alpha and the pixels it still has to slide (the stage's, 16 at
birth to 0) at now; FALSE for a row past its time or an unused one */
boolean ae_kf_row_pose(struct ae_kf_row const *row, long now, real *slot, real *alpha, real *slide);

/* ---------- colours */

struct ae_kf_rgb
{
	unsigned char r, g, b;
};

/* the plate's navy, the assist's tick, the player's tick, the weapon word in a split line (grey) and on the player's own (ice) */
struct ae_kf_rgb ae_kf_navy(void);
struct ae_kf_rgb ae_kf_tick_mine(void);
struct ae_kf_rgb ae_kf_tick_assist(void);
struct ae_kf_rgb ae_kf_ice(void);
struct ae_kf_rgb ae_kf_grey(void);
struct ae_kf_rgb ae_kf_white(void);
/* the icons' ink, AE: ice. Classic: the HUD's blue (the caller reads it) */
/* whether someone is an ally of the viewer: the viewer themselves; in a team game a player of their team; never in a free for
all */
boolean ae_kf_is_ally(boolean teams, boolean same_player, boolean same_team);
/* the feed's own colour for an enemy's name where the game's rule would give the HUD's blue (CLASSIC HUD with ENEMY NAME COLOR
CLASSIC makes both sides one colour): TRUE when the name is drawn white instead. Everywhere else the game's rule gives the name's
colour, so that the feed agrees with the names over heads */
boolean ae_kf_name_white(boolean ae_style, boolean enemy_name_red, boolean ally);

/* ---------- where the parts of a row go */

/* the text's em in view pixels at a view's scale: AE_KF_TEXT_PX x scale, never under AE_KF_TEXT_FLOOR_PX */
real ae_kf_text_px(real scale);
/* a picture's size fitted into the icon box of AE_KF_ICON_WIDTH x AE_KF_ICON_HEIGHT at scale, its shape kept; FALSE when its
size is not a size */
boolean ae_kf_icon_fit(real picture_width, real picture_height, real scale, real *width, real *height);

struct ae_kf_parts
{
	/* in: widths in view pixels (a part not on the row is 0) */
	real killer_width;
	real icon_width;
	real mark_width;
	real victim_width;
	/* in: whether there is a killer, and a mark */
	boolean has_killer;
	boolean has_mark;
	real scale;
};

struct ae_kf_layout
{
	real width;			/* the whole row, the insets included */
	real killer_x;		/* each part's left, from the row's left */
	real icon_x;
	real mark_x;
	real victim_x;
};

void ae_kf_layout(struct ae_kf_parts const *parts, struct ae_kf_layout *layout);
/* the row's left in the view from the anchor: the anchor itself left aligned, the anchor less the width right aligned */
real ae_kf_row_left(real anchor_x, real width);
/* a slot's top in the view from the anchor's bottom (growing up) or top (growing down), the row's height at scale */
real ae_kf_slot_top(real anchor_y, real slot, real scale);
/* the anchor's x in the view: the sensor's left edge less AE_KF_SENSOR_GAP at scale, else AE_KF_FALLBACK_X at scale from the
HUD area's left (area_left, view pixels); the anchor's y: the stage's bottom for the player's medal style at scale */
real ae_kf_anchor_x(boolean sensor_known, real sensor_left, real area_left, real scale);
real ae_kf_anchor_y(boolean mcc_medals, real scale);

/* ---------- a split-screen player's message list line */

struct ae_kf_split
{
	wchar_t const *killer;
	wchar_t const *victim;
	int kind;
	int weapon;
	boolean headshot;
	boolean perfect;
	boolean mine;
	struct ae_kf_rgb killer_rgb;
	struct ae_kf_rgb victim_rgb;
};

/* the line as the feed's text with colour runs (ae_marker_rules.h): it does not wrap; the names in their colours, the weapon's word
and HEADSHOT or PERFECT in grey, or in ice on a line of the player's own. A kill is "Killer WEAPON Victim" (the mark's word after
the weapon), a suicide "Victim SUICIDE", an environment death "WORD Victim". Returns the length written, 0 when size has no room */
int ae_kf_split_line(wchar_t *out, int size, struct ae_kf_split const *line);

/* ---------- the game's kill lines */

/* whether a game message (game_engine.c's enum game_engine_message) is one of the kill lines the feed replaces: killed by
unknown, biped, vehicle, player, friendly fire, self (1 to 6), killed enemy (8) and killed friendly (13). Multi kills and
sprees (7, 9 to 12, 14 to 19) stay */
boolean ae_kf_message_is_kill_line(long message);
/* the game's multikill and spree lines, plain and with a score */
boolean ae_kf_message_is_medal_line(long message);

/* ---------- the game's pickup lines */

/* whether the game's pickup text (ammo, grenade, weapon and powerup lines of the HUD's item messages) is shown to a player: not while
that player's KILL FEED is on (the feed is the view's list of events), whole while it is off. The hold-to-pick-up prompt is another
thing and stays */
boolean ae_kf_pickup_text_shown(boolean feed_on);

#endif /* __AE_KILLFEED_RULES_H */
