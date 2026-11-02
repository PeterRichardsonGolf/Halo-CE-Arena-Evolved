/*
AE_DS5_RULES.H

Arena Evolved: the DualSense's output bytes, pure (no SDL, no game), so a unit test checks
them (port/linux/tests/ae_ds5_rules_test.c).

- The output report's shared 47-byte body ("effects state"): the same layout as SDL3's DS5EffectsState_t
  (src/joystick/hidapi/SDL_hidapi_ps5.c, zlib licence): enable bits, rumble, audio, the right then the left
  trigger's 11-byte effect, LED fields.
- A trigger effect's 11 bytes: the zone-based modes (feedback 0x21, weapon 0x25, vibration 0x26) packed as in
  John "Nielk1" Klein's TriggerEffectGenerator (MIT licence), and 0x05, the "off" SDL's testcontroller.c sends.
- The whole report for a hidraw write: USB report 0x02 (1 + 47 bytes, padded to 48) or Bluetooth report 0x31
  (78 bytes: id, sequence in the high nibble, tag 0x10, the body, padding, then a CRC-32 over the byte 0xA2 and the
  first 74 bytes), as SDL's driver frames them.
*/

#ifndef __AE_DS5_RULES_H
#define __AE_DS5_RULES_H

#include <stddef.h>

#define AE_DS5_EFFECT_SIZE 11
#define AE_DS5_BODY_SIZE 47
#define AE_DS5_USB_REPORT_SIZE 48
#define AE_DS5_BLUETOOTH_REPORT_SIZE 78

/* the body's fields used here (byte offsets) */
#define AE_DS5_BODY_ENABLE1 0 /* 0x04 right trigger effect, 0x08 left trigger effect */
#define AE_DS5_BODY_ENABLE2 1 /* 0x04 lightbar colour, 0x10 player lights */
#define AE_DS5_BODY_RIGHT_TRIGGER 10
#define AE_DS5_BODY_LEFT_TRIGGER 21
#define AE_DS5_BODY_PLAYER_LIGHTS 43
#define AE_DS5_BODY_RED 44
#define AE_DS5_BODY_GREEN 45
#define AE_DS5_BODY_BLUE 46

#define AE_DS5_ENABLE1_RIGHT_TRIGGER 0x04
#define AE_DS5_ENABLE1_LEFT_TRIGGER 0x08
#define AE_DS5_ENABLE2_LIGHTBAR 0x04
#define AE_DS5_ENABLE2_PLAYER_LIGHTS 0x10

/* the weapon kinds, each with its own R2 profile */
enum ae_ds5_weapon_kind
{
	AE_DS5_WEAPON_NONE = 0,
	AE_DS5_WEAPON_SEMI,      /* the pistol, the plasma pistol's tap: a short click */
	AE_DS5_WEAPON_HEAVY,     /* slow single shots (shotgun, rocket launcher): a heavy click */
	AE_DS5_WEAPON_AUTOMATIC, /* assault rifle, plasma rifle, needler, flamethrower: buzz at the rate of fire */
	AE_DS5_WEAPON_SNIPER,    /* two zoom levels and a slow trigger: firm resistance */
	AE_DS5_WEAPON_CHARGE,    /* a trigger that charges (plasma pistol, fuel rod): resistance from mid pull */
	AE_DS5_WEAPON_BURST,     /* AE's battle rifle (one burst a pull): a firm click */
	AE_DS5_WEAPON_NEEDLER,   /* the tag's weapon type needler: a fast light buzz */
	AE_DS5_WEAPON_PLASMA_HEAT, /* rounds make heat (plasma rifle): rises with heat, locked when overheated */
	AE_DS5_WEAPON_CARRY,     /* no trigger with a projectile (ball, flag): free */
	AE_DS5_WEAPON_FLAME,     /* fast, with heat, not the plasma rifle (flamethrower): a roar while it burns */
	AE_DS5_WEAPON_SMG,       /* AE's SMG pack (15 rounds/s): a light hold, a fast buzz */
	AE_DS5_WEAPON_SMG_SILENCED, /* AE's silenced SMG pack: the SMG's hold, a softer, lower buzz */
	AE_DS5_WEAPON_CARBINE,   /* AE's carbine pack (5 rounds/s, semi-automatic): a quick crisp click, lighter than the
	                            pistol's */
	AE_DS5_WEAPON_AR_SILENCED, /* AE's silenced assault rifle pack: the assault rifle's hold, a softer pulse a round */
	AE_DS5_WEAPON_KINDS
};

/* the weapon pack a weapon comes from (the game side reads it from the pack's slot, ae_weapon_slots.h; the same
numbers): its own feel, whatever its tag's numbers say */
enum ae_ds5_pack
{
	AE_DS5_PACK_NONE = 0,
	AE_DS5_PACK_BATTLE_RIFLE,
	AE_DS5_PACK_SMG,
	AE_DS5_PACK_SMG_SILENCED,
	AE_DS5_PACK_CARBINE,
	AE_DS5_PACK_AR_SILENCED
};

/* the kind from the weapon's tag numbers: whether it is AE's battle rifle, the weapon pack it comes from (an
ae_ds5_pack: a pack weapon's kind is the pack's), whether its weapon type is the needler, its first trigger's rate of
fire (rounds a second), the longest charging time of its triggers (seconds, 0 none), its first trigger's heat a round,
its zoom level count, whether no trigger has a projectile (a carried ball or flag), whether its weapon type is the
plasma rifle (a fast weapon with heat is the plasma rifle's kind if so, else the flamethrower's) */
enum ae_ds5_weapon_kind ae_ds5_weapon_kind(int burst_rifle, int pack, int needler, float rate_of_fire,
	float charging_time, float heat_per_round, int zoom_levels, int no_projectile, int plasma_rifle_type);

/* the weapon in hand now (the game side reads it each frame) */
struct ae_ds5_weapon_state
{
	enum ae_ds5_weapon_kind kind;
	float rate_of_fire;
	float heat;   /* 0-1 */
	int overheated;
	float charge; /* 0-1 while charging or charged, else 0 */
	int firing;   /* a round in the last few ticks */
	int reloading;
	int empty;    /* the magazine has no round loaded, or a battery weapon's battery is spent */
	int readying; /* switching weapons: readying or putting away */
	long ticks;   /* the game time, for the overheat's throb */
};

/* the events a trigger feels once (a kick, then the held shape again) with a rumble pulse */
enum ae_ds5_event
{
	AE_DS5_EVENT_SHOT,
	AE_DS5_EVENT_SHOT_CHARGED, /* a fully charged shot (the plasma pistol's blast) */
	AE_DS5_EVENT_MAG_IN,       /* the reload's magazine (or shell) going in */
	AE_DS5_EVENT_SETTLE,       /* a weapon readied */
	AE_DS5_EVENT_CHAMBER,      /* the weapon's own chambering after a shot (the shotgun's pump) */
	AE_DS5_EVENT_GRENADE_FRAG, /* (L2) a grenade leaving the hand */
	AE_DS5_EVENT_GRENADE_PLASMA
};
struct ae_ds5_rumble
{
	int left, right; /* 0-255: the heavy (left) and light (right) motors */
	int ms;
};
char const *ae_ds5_weapon_kind_name(enum ae_ds5_weapon_kind kind);

/* the 11 bytes of a trigger effect; out-of-range numbers are clamped, a strength or amplitude of 0 is "off" */
void ae_ds5_effect_off(unsigned char effect[AE_DS5_EFFECT_SIZE]);
/* resistance from zone `position` (0-9) to the end, strength 1-8 */
void ae_ds5_effect_feedback(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int strength);
/* resistance between zone `start` (2-7) and `end` (start+1 to 8) that gives way: a click; strength 1-8 */
void ae_ds5_effect_weapon(unsigned char effect[AE_DS5_EFFECT_SIZE], int start, int end, int strength);
/* resistance over zones `first` to `last` (0-9) only, free after it (a tick early in the pull), strength 1-8 */
void ae_ds5_effect_feedback_span(unsigned char effect[AE_DS5_EFFECT_SIZE], int first, int last, int strength);
/* vibration from zone `position` (0-9) on, amplitude 1-8, frequency 1-255 (Hz) */
void ae_ds5_effect_vibration(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int amplitude, int frequency);

/* the weapon profiles (ae_ds5_rules.c's table, one a kind). R2 as the trigger rests / is pulled now, by the weapon's
state, first rule that applies (the owner's, 0.3.0): no weapon or a carried ball/flag: off; readying or putting away:
free (QUICK SETTLE); overheated: LOCKED + VENTING THROB; reloading: SLACK (SEAT BUMP); empty: the DRY CLICK; firing:
the profile's firing shape; else its held shape; heat or charge ramps the strength where the profile says. Its words
in text */
void ae_ds5_right_held(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_weapon_state const *state, int strength,
	char *text, size_t text_size);
/* while a charge weapon charges (the plasma pistol): 1 and a rumble pulse every few ticks (right motor rising with the
charge, the left joining), else 0 (owner 10-15: the motors carry the charge's feel, the trigger only hums) */
int ae_ds5_charge_rumble(enum ae_ds5_weapon_kind kind, float charge, long ticks, struct ae_ds5_rumble *rumble);

/* an event's kick on its trigger (R2, or L2 for a grenade) and how long it lasts (ms; 0: no kick, e.g. an automatic's
shot, which its firing shape does), and its rumble pulse: a human weapon's on the left motor, a Covenant one's on the
right */
int ae_ds5_event(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_rumble *rumble, enum ae_ds5_weapon_kind kind,
	enum ae_ds5_event event, int strength);
/* whether the kind is a Covenant (energy) weapon's */
int ae_ds5_kind_covenant(enum ae_ds5_weapon_kind kind);
/* the plasma pistol's overcharged release on the lightbar: green, fading over 0.3 s; 0 when ticks (since the
release, -1 none) are past it */
int ae_ds5_overcharge_flash(unsigned char rgb[3], long ticks);
/* the profile's feel in a few words */
char const *ae_ds5_profile_feel(enum ae_ds5_weapon_kind kind);
/* L2: light while zoomed (free on LIGHT); grenades in hand a hair trigger on every preset (the lightest tick still
felt, zones 1-2, nothing after it); a grenade wound up free (the throw has only its motor pulse); none, the dry click.
Returns a short text for the log */
char const *ae_ds5_left_effect(unsigned char effect[AE_DS5_EFFECT_SIZE], int zoomed, int grenades, int winding,
	int strength);

/* the powerups on the player now (the game side reads them each frame) */
struct ae_ds5_powerups
{
	float shield;          /* shield vitality: above 1 an overshield (up to 3) */
	int shield_rising;     /* it rose in the last few ticks: an overshield charging */
	float camo;            /* active camouflage, 0-1 */
	long juggernaut_ticks; /* ticks since this player became AE's juggernaut (-1: not now) */
	long ticks;            /* the game time, for the pulses */
};

/* a colour too dark for the lightbar (black armour): the game shows the shield's colours instead (owner 10-15: the
lightbar "stays white" with black armour's dim white; ae_ds5_lightbar keeps a dim white for it) */
int ae_ds5_lightbar_black(unsigned char const base[3]);
/* the lightbar from the player's colour and the powerups (black armour shows a dim white, as black is no light):
becoming the juggernaut flashes white and fades back (0.67 s); an overshield charging pulses at 2 Hz (35-100 %);
an overshield held breathes slowly (0.5 Hz, 70-100 %); active camouflage dims it (to 15 % fully cloaked). Steps are
coarse so the pad gets few reports. Its words in text */
void ae_ds5_lightbar(unsigned char rgb[3], unsigned char const base[3], struct ae_ds5_powerups const *powerups,
	char *text, size_t text_size);
/* L2 while a powerup speaks: becoming the juggernaut a 0.4 s strong buzz, an overshield charging a buzz rising with
it. Returns 0 (nothing: L2 is the zoom's) or 1, with its words in text */
int ae_ds5_left_powerup(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_powerups const *powerups, int strength,
	char *text, size_t text_size);

/* LIGHTBAR's SHIELD: the local player's shield meter as the HUD draws it */
struct ae_ds5_shield
{
	float vitality;          /* the shield: 0 empty, 1 full, above 1 an overshield's layers */
	int dead;
	long popped_ticks;       /* ticks since the shield broke (-1: not lately) */
	int pop_flash;           /* COMBAT FEEDBACK ARENA EVOLVED: a short white flash on the pop */
	long ticks;              /* the game time, for the low shield's blink */
	/* the unit HUD interface's shield meter colours (0x..RRGGBB; 0: the tag has none) */
	unsigned long min_color, max_color, flash_color, empty_color;
};
/* the lightbar for the shield: the meter's colour from its min to its max colour with the fill, brighter as it fills
(never under 25 %); under a quarter (where the HUD's meter flashes) it blinks with the meter's flash colour at 4 Hz;
empty, the meter's empty colour (the HUD's red if none); just broken, with COMBAT FEEDBACK ARENA EVOLVED, white for
0.2 s; an overshield, the HUD's colour of its top layer (red, green, yellow, violet, as hud_unit.c draws them),
brighter as that layer fills; dead, dark. No meter colours in the tag: the HUD's blue. Steps are coarse (16) so the
pad gets few reports. Its words in text */
void ae_ds5_lightbar_shield(unsigned char rgb[3], struct ae_ds5_shield const *shield, char *text, size_t text_size);

/* TRIGGER STRENGTH (input.trigger_strength: light, normal, strong) as a percent (AE_DS5_STRENGTH_LIGHT, 100,
AE_DS5_STRENGTH_STRONG; anything else 100). The table is NORMAL's (owner 10-21, 0.3.7):
- STRONG: a hold (resistance to the end of the pull) two steps up, at most 6, so a full pull never tires; a click two
  steps up, at most 7; kicks and buzzes two steps up (at most 8); the motors at AE_DS5_RUMBLE_STRONG percent;
- LIGHT is a HAIR TRIGGER on both triggers: every hold or click, every weapon, a tiny tick early in the pull (zones
  1-2, strength 2) and nothing after it, no zoom hold; kicks and buzzes at most 2; the motors at AE_DS5_RUMBLE_LIGHT
  percent;
- L2's grenade a hair trigger on every preset (ae_ds5_left_effect). */
#define AE_DS5_STRENGTH_LIGHT 75
#define AE_DS5_STRENGTH_STRONG 175
#define AE_DS5_RUMBLE_LIGHT 75
#define AE_DS5_RUMBLE_STRONG 140
int ae_ds5_strength_percent(char const *name);
/* a rumble's motors scaled by TRIGGER STRENGTH's percent (ae_ds5_event does it itself; for ae_ds5_charge_rumble's) */
void ae_ds5_rumble_scaled(struct ae_ds5_rumble *rumble, int percent);
/* the player lights' byte for split-screen player 1-4 (0 for another number), and back: the player index (0-3) a
byte shows, -1 for none */
unsigned char ae_ds5_player_lights(int player_number);
int ae_ds5_player_index_of_lights(unsigned char lights);

/* CRC-32 (the zlib / IEEE one, reflected, polynomial 0xEDB88320), continued from `crc` (0 to start) */
unsigned long ae_ds5_crc32(unsigned long crc, unsigned char const *data, size_t size);

/* the whole output report for a hidraw write: the body framed for USB or Bluetooth (sequence 0-15); returns its
size, or 0 when `size` is too small */
size_t ae_ds5_output_report(unsigned char *report, size_t size, int bluetooth, int sequence,
	unsigned char const body[AE_DS5_BODY_SIZE]);

/* ONE WRITER A PAD (0.3.0, the Bluetooth dropouts): a pad's link keeps the state it has from us and the state wanted,
by field (the triggers, the rumble motors, the lightbar, the player lights). Each frame the wanted fields are set, then
ae_ds5_link_due says what to write now: only the fields whose bytes differ from the pad's (or not sent yet, or asked
again), all in one report. On Bluetooth at most one report every AE_DS5_BLUETOOTH_INTERVAL_MS: a change inside the
window waits for it, later changes fold into the waiting one (the latest wins) and it goes out as the window opens.
USB: one report a frame at most, at once. A report the pad's queue had no room for (busy) waits a window and is
tried again; nothing is retried inside a frame */
#define AE_DS5_BLUETOOTH_INTERVAL_MS 50
/* (busy on USB: the next try this long after) */
#define AE_DS5_BUSY_RETRY_MS 16

#define AE_DS5_BODY_RUMBLE_RIGHT 2 /* the light (right) motor, 0-255 */
#define AE_DS5_BODY_RUMBLE_LEFT 3  /* the heavy (left) motor */
#define AE_DS5_BODY_ENABLE3 38
/* the rumble's enable bits as the kernel's hid-playstation sends them: "haptics select" (classic rumble, the audio
haptics off) and the compatible vibration, the first one (enable1 0x01) or, on firmware 2.21 and newer and the
DualSense Edge, the second (enable3 0x04) */
#define AE_DS5_ENABLE1_RUMBLE 0x01
#define AE_DS5_ENABLE1_HAPTICS_SELECT 0x02
#define AE_DS5_ENABLE3_RUMBLE_V2 0x04

enum ae_ds5_field
{
	AE_DS5_FIELD_TRIGGERS = 1,
	AE_DS5_FIELD_RUMBLE = 2,
	AE_DS5_FIELD_LIGHTBAR = 4,
	AE_DS5_FIELD_PLAYER_LIGHTS = 8,
	AE_DS5_FIELDS = 15
};

/* a write's outcome: sent; busy (the queue full, try the next window); gone (the pad left: its node is closed, opened
again when it is back, and everything sent again); refused (the pad is left alone) */
enum ae_ds5_write_result
{
	AE_DS5_WRITE_SENT,
	AE_DS5_WRITE_BUSY,
	AE_DS5_WRITE_GONE,
	AE_DS5_WRITE_REFUSED
};
/* a failed write's errno as an outcome */
enum ae_ds5_write_result ae_ds5_write_result_of_errno(int error);

struct ae_ds5_link
{
	int bluetooth;
	int vibration_v2;
	/* the wanted state (every field's bytes in a body), the fields wanted, the ones to send though unchanged */
	unsigned char want[AE_DS5_BODY_SIZE];
	int wanted;
	int forced;
	/* what the pad has from us, and which fields it has */
	unsigned char sent[AE_DS5_BODY_SIZE];
	int known;
	/* when the window next opens (ms), whether a change waits for it, the last frame's wanted bytes (to count what
	folds into a waiting report) */
	unsigned long long open_ms;
	int pending;
	unsigned char last_want[AE_DS5_BODY_SIZE];
	int last_dirty;
	/* (debug.ds5_log's minute) reports written; wanted changes folded into a later report; changes that waited for
	the window; failed writes (busy, gone, refused) */
	unsigned long writes, merged, deferred, errors;
};

void ae_ds5_link_init(struct ae_ds5_link *link, int bluetooth, int vibration_v2);
/* the pad has nothing from us (opened again after it left): everything wanted is sent again */
void ae_ds5_link_forget(struct ae_ds5_link *link);
/* this frame's wanted fields */
void ae_ds5_link_want_triggers(struct ae_ds5_link *link, unsigned char const right[AE_DS5_EFFECT_SIZE],
	unsigned char const left[AE_DS5_EFFECT_SIZE]);
/* the motors 0-255: the heavy (left) and the light (right) */
void ae_ds5_link_want_rumble(struct ae_ds5_link *link, int left, int right);
void ae_ds5_link_want_lightbar(struct ae_ds5_link *link, int red, int green, int blue);
void ae_ds5_link_want_player_lights(struct ae_ds5_link *link, unsigned char lights);
/* the fields sent next time though unchanged (a refresh) */
void ae_ds5_link_force(struct ae_ds5_link *link, int fields);
/* the fields no longer driven (the pad keeps what it has) */
void ae_ds5_link_drop(struct ae_ds5_link *link, int fields);
/* the fields that differ from the pad's */
int ae_ds5_link_dirty(struct ae_ds5_link const *link);
/* once a frame: the fields to write now (0: none) with the body to send (their enable bits set, the other fields'
bytes as wanted but not enabled); `now` in ms. Counts the waiting and the folded changes */
int ae_ds5_link_due(struct ae_ds5_link *link, unsigned long long now, unsigned char body[AE_DS5_BODY_SIZE]);
/* as ae_ds5_link_due, the window ignored (at exit, a reset) */
int ae_ds5_link_due_now(struct ae_ds5_link *link, unsigned long long now, unsigned char body[AE_DS5_BODY_SIZE]);
/* the outcome of writing what ae_ds5_link_due gave */
void ae_ds5_link_result(struct ae_ds5_link *link, int fields, enum ae_ds5_write_result result, unsigned long long now);

/* ONE PAD, ONE PORT: `port` is now pad `id`'s (0: no pad), and no other port is. A pad moves port mid-game when the
one before it is unplugged; the old port's handle went with that pad, so nothing sets the old port again, and a
binding left there gave the pad both ports' reports in turn (the lightbar blinking, the triggers clicking at rest) */
void ae_ds5_bind(unsigned int *bound, int ports, int port, unsigned int id);

#endif
