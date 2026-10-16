#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "ae_ds5_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int same(unsigned char const *a, unsigned char const *b, size_t size)
{
	return memcmp(a, b, size) == 0;
}

/* ONE WRITER: the link's coalescing, the Bluetooth window, a busy queue, a pad that went */
static void link_tests(void)
{
	struct ae_ds5_link link;
	unsigned char body[AE_DS5_BODY_SIZE];
	unsigned char held[AE_DS5_EFFECT_SIZE], kick[AE_DS5_EFFECT_SIZE], off[AE_DS5_EFFECT_SIZE];
	int fields;

	ae_ds5_effect_weapon(held, 4, 7, 6);
	ae_ds5_effect_feedback(kick, 2, 8);
	ae_ds5_effect_off(off);

	/* errno as an outcome */
	CHECK(ae_ds5_write_result_of_errno(EAGAIN) == AE_DS5_WRITE_BUSY);
	CHECK(ae_ds5_write_result_of_errno(EINTR) == AE_DS5_WRITE_BUSY);
	CHECK(ae_ds5_write_result_of_errno(ENODEV) == AE_DS5_WRITE_GONE);
	CHECK(ae_ds5_write_result_of_errno(EIO) == AE_DS5_WRITE_GONE);
	CHECK(ae_ds5_write_result_of_errno(EINVAL) == AE_DS5_WRITE_REFUSED);
	CHECK(ae_ds5_write_result_of_errno(EACCES) == AE_DS5_WRITE_REFUSED);

	/* USB: the same body twice is one write; nothing wanted, nothing written */
	ae_ds5_link_init(&link, 0, 0);
	CHECK(ae_ds5_link_due(&link, 0, body) == 0);
	ae_ds5_link_want_triggers(&link, held, off);
	fields = ae_ds5_link_due(&link, 0, body);
	CHECK(fields == AE_DS5_FIELD_TRIGGERS);
	CHECK(body[AE_DS5_BODY_ENABLE1] == (AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER) &&
		body[AE_DS5_BODY_ENABLE2] == 0 && body[AE_DS5_BODY_ENABLE3] == 0);
	CHECK(same(body + AE_DS5_BODY_RIGHT_TRIGGER, held, sizeof(held)) && same(body + AE_DS5_BODY_LEFT_TRIGGER, off,
		sizeof(off)));
	ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, 0);
	ae_ds5_link_want_triggers(&link, held, off);
	CHECK(ae_ds5_link_due(&link, 1, body) == 0);
	CHECK(ae_ds5_link_due(&link, 2, body) == 0);
	CHECK(link.writes == 1 && link.deferred == 0 && link.merged == 0);
	/* USB has no window: a change the next frame goes out at once */
	ae_ds5_link_want_triggers(&link, kick, off);
	CHECK(ae_ds5_link_due(&link, 3, body) == AE_DS5_FIELD_TRIGGERS);
	ae_ds5_link_result(&link, AE_DS5_FIELD_TRIGGERS, AE_DS5_WRITE_SENT, 3);
	/* all fields merged in one report; only the changed ones enabled */
	ae_ds5_link_want_rumble(&link, 200, 40);
	ae_ds5_link_want_lightbar(&link, 255, 0, 0);
	ae_ds5_link_want_player_lights(&link, ae_ds5_player_lights(2));
	fields = ae_ds5_link_due(&link, 4, body);
	CHECK(fields == (AE_DS5_FIELD_RUMBLE | AE_DS5_FIELD_LIGHTBAR | AE_DS5_FIELD_PLAYER_LIGHTS));
	CHECK(body[AE_DS5_BODY_ENABLE1] == (AE_DS5_ENABLE1_RUMBLE | AE_DS5_ENABLE1_HAPTICS_SELECT));
	CHECK(body[AE_DS5_BODY_ENABLE2] == (AE_DS5_ENABLE2_LIGHTBAR | AE_DS5_ENABLE2_PLAYER_LIGHTS));
	CHECK(body[AE_DS5_BODY_RUMBLE_LEFT] == 200 && body[AE_DS5_BODY_RUMBLE_RIGHT] == 40 && body[AE_DS5_BODY_RED] == 255 &&
		body[AE_DS5_BODY_PLAYER_LIGHTS] == 0x2a);
	ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, 4);
	/* a refresh: unchanged, sent again once */
	ae_ds5_link_force(&link, AE_DS5_FIELD_TRIGGERS);
	CHECK(ae_ds5_link_due(&link, 5, body) == AE_DS5_FIELD_TRIGGERS);
	ae_ds5_link_result(&link, AE_DS5_FIELD_TRIGGERS, AE_DS5_WRITE_SENT, 5);
	CHECK(ae_ds5_link_due(&link, 6, body) == 0);
	/* a field no longer driven is not sent */
	ae_ds5_link_drop(&link, AE_DS5_FIELD_LIGHTBAR);
	ae_ds5_link_want_rumble(&link, 0, 0);
	CHECK(ae_ds5_link_due(&link, 7, body) == AE_DS5_FIELD_RUMBLE);
	CHECK(body[AE_DS5_BODY_ENABLE2] == 0);
	ae_ds5_link_result(&link, AE_DS5_FIELD_RUMBLE, AE_DS5_WRITE_SENT, 7);

	/* the second compatible vibration: haptics select, enable3 0x04, not enable1 0x01 */
	ae_ds5_link_init(&link, 0, 1);
	ae_ds5_link_want_rumble(&link, 300, -5);
	CHECK(ae_ds5_link_due(&link, 0, body) == AE_DS5_FIELD_RUMBLE);
	CHECK(body[AE_DS5_BODY_ENABLE1] == AE_DS5_ENABLE1_HAPTICS_SELECT &&
		body[AE_DS5_BODY_ENABLE3] == AE_DS5_ENABLE3_RUMBLE_V2);
	CHECK(body[AE_DS5_BODY_RUMBLE_LEFT] == 255 && body[AE_DS5_BODY_RUMBLE_RIGHT] == 0);

	/* Bluetooth: the first change goes at once; three changes inside the next 50 ms are one write, with the last,
	as the window opens (nothing lost at the end of a burst: no new wish needed) */
	ae_ds5_link_init(&link, 1, 1);
	ae_ds5_link_want_lightbar(&link, 10, 10, 10);
	CHECK(ae_ds5_link_due(&link, 1000, body) == AE_DS5_FIELD_LIGHTBAR);
	ae_ds5_link_result(&link, AE_DS5_FIELD_LIGHTBAR, AE_DS5_WRITE_SENT, 1000);
	ae_ds5_link_want_lightbar(&link, 20, 0, 0);
	CHECK(ae_ds5_link_due(&link, 1010, body) == 0);
	ae_ds5_link_want_lightbar(&link, 0, 30, 0);
	CHECK(ae_ds5_link_due(&link, 1020, body) == 0);
	ae_ds5_link_want_lightbar(&link, 0, 0, 40);
	ae_ds5_link_want_triggers(&link, kick, off);
	CHECK(ae_ds5_link_due(&link, 1030, body) == 0);
	CHECK(ae_ds5_link_due(&link, 1049, body) == 0);
	CHECK(link.writes == 1 && link.deferred == 1 && link.merged == 2);
	fields = ae_ds5_link_due(&link, 1050, body);
	CHECK(fields == (AE_DS5_FIELD_LIGHTBAR | AE_DS5_FIELD_TRIGGERS));
	CHECK(body[AE_DS5_BODY_RED] == 0 && body[AE_DS5_BODY_GREEN] == 0 && body[AE_DS5_BODY_BLUE] == 40);
	CHECK(same(body + AE_DS5_BODY_RIGHT_TRIGGER, kick, sizeof(kick)));
	ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, 1050);
	CHECK(link.writes == 2);
	CHECK(ae_ds5_link_due(&link, 1200, body) == 0);
	/* a change that goes back to what the pad has while it waits: no write, counted as folded */
	ae_ds5_link_want_lightbar(&link, 1, 2, 3);
	CHECK(ae_ds5_link_due(&link, 1210, body) == AE_DS5_FIELD_LIGHTBAR);
	ae_ds5_link_result(&link, AE_DS5_FIELD_LIGHTBAR, AE_DS5_WRITE_SENT, 1210);
	ae_ds5_link_want_lightbar(&link, 9, 9, 9);
	CHECK(ae_ds5_link_due(&link, 1220, body) == 0);
	ae_ds5_link_want_lightbar(&link, 1, 2, 3);
	CHECK(ae_ds5_link_due(&link, 1230, body) == 0);
	CHECK(ae_ds5_link_due(&link, 1300, body) == 0);
	CHECK(link.writes == 3 && link.merged == 3);
	/* the window ignored (a reset at exit) */
	ae_ds5_link_want_lightbar(&link, 0, 0, 128);
	CHECK(ae_ds5_link_due_now(&link, 1301, body) == AE_DS5_FIELD_LIGHTBAR);
	ae_ds5_link_result(&link, AE_DS5_FIELD_LIGHTBAR, AE_DS5_WRITE_SENT, 1301);

	/* busy (EAGAIN): kept, never tried again inside the window, then sent */
	ae_ds5_link_want_lightbar(&link, 50, 60, 70);
	fields = ae_ds5_link_due(&link, 2000, body);
	CHECK(fields == AE_DS5_FIELD_LIGHTBAR);
	ae_ds5_link_result(&link, fields, ae_ds5_write_result_of_errno(EAGAIN), 2000);
	CHECK(link.errors == 1 && link.pending);
	CHECK(ae_ds5_link_due(&link, 2001, body) == 0);
	CHECK(ae_ds5_link_due(&link, 2049, body) == 0);
	CHECK(ae_ds5_link_due(&link, 2050, body) == AE_DS5_FIELD_LIGHTBAR && body[AE_DS5_BODY_RED] == 50);
	ae_ds5_link_result(&link, AE_DS5_FIELD_LIGHTBAR, AE_DS5_WRITE_SENT, 2050);
	CHECK(!link.pending && ae_ds5_link_due(&link, 2200, body) == 0);
	/* (USB busy: the next try AE_DS5_BUSY_RETRY_MS on) */
	{
		struct ae_ds5_link usb;

		ae_ds5_link_init(&usb, 0, 0);
		ae_ds5_link_want_lightbar(&usb, 1, 1, 1);
		CHECK(ae_ds5_link_due(&usb, 100, body) == AE_DS5_FIELD_LIGHTBAR);
		ae_ds5_link_result(&usb, AE_DS5_FIELD_LIGHTBAR, AE_DS5_WRITE_BUSY, 100);
		CHECK(ae_ds5_link_due(&usb, 101, body) == 0);
		CHECK(ae_ds5_link_due(&usb, 100 + AE_DS5_BUSY_RETRY_MS, body) == AE_DS5_FIELD_LIGHTBAR);
	}

	/* gone (ENODEV): the pad has nothing from us any more: opened again, everything wanted is sent at once */
	ae_ds5_link_want_triggers(&link, held, off);
	ae_ds5_link_want_rumble(&link, 0, 0);
	fields = ae_ds5_link_due(&link, 3000, body);
	CHECK(fields == (AE_DS5_FIELD_TRIGGERS | AE_DS5_FIELD_RUMBLE));
	ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, 3000);
	ae_ds5_link_want_lightbar(&link, 7, 7, 7);
	fields = ae_ds5_link_due(&link, 3100, body);
	ae_ds5_link_result(&link, fields, ae_ds5_write_result_of_errno(ENODEV), 3100);
	CHECK(link.known == 0);
	fields = ae_ds5_link_due(&link, 3101, body);
	CHECK(fields == (AE_DS5_FIELD_TRIGGERS | AE_DS5_FIELD_RUMBLE | AE_DS5_FIELD_LIGHTBAR));
	ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, 3101);
	CHECK(ae_ds5_link_due(&link, 3300, body) == 0);
}

/* (printed for the hand-back) reports a second in a modelled fight: 4 s at 60 frames a second, the game's rumble set
each 30 Hz tick (a fading pulse after each shotgun shot, 1.4 shots a second, and an assault rifle's buzz for 1.5 s),
the shotgun's kick (130 ms) and the shield's lightbar stepping each 100 ms while it recharges (2 s). Before: our
reports (a change of the triggers or the lightbar, a frame) plus the kernel's (each change of the game's motors,
SDL_RumbleGamepad -> force feedback). After: one link, the rumble in our reports */
static void write_rate_model(void)
{
	int bluetooth;

	for (bluetooth = 0; bluetooth < 2; bluetooth++)
	{
		struct ae_ds5_link link;
		unsigned char body[AE_DS5_BODY_SIZE];
		unsigned char held[AE_DS5_EFFECT_SIZE], kick[AE_DS5_EFFECT_SIZE];
		unsigned char last_right[AE_DS5_EFFECT_SIZE];
		int last_rgb = -1, last_motor = -1, before = 0, frame, frames = 240;

		ae_ds5_link_init(&link, bluetooth, 1);
		ae_ds5_effect_weapon(held, 4, 7, 6);
		ae_ds5_effect_feedback(kick, 2, 8);
		memset(last_right, 0xff, sizeof(last_right));
		for (frame = 0; frame < frames; frame++)
		{
			unsigned long long now = (unsigned long long)frame * 1000ull / 60ull;
			long tick = (long)(now * 30ull / 1000ull);
			long shot_ms = (long)(now % 700ull);
			int motor = 0, rgb, fields;
			unsigned char const *right = shot_ms < 130 ? kick : held;

			/* the shotgun's pulse fades over 10 ticks; the rifle buzzes between 2 s and 3.5 s */
			if (shot_ms < 333)
				motor = 255 - (int)(shot_ms * 30 / 1000) * 25;
			if (now >= 2000 && now < 3500)
				motor = motor > (tick & 1 ? 180 : 120) ? motor : (tick & 1 ? 180 : 120);
			rgb = now >= 1000 && now < 3000 ? (int)((now - 1000) / 100) * 12 : 240;
			/* before */
			if (memcmp(right, last_right, sizeof(last_right)) || rgb != last_rgb)
				before++;
			if (motor != last_motor)
				before++;
			memcpy(last_right, right, sizeof(last_right));
			last_rgb = rgb;
			last_motor = motor;
			/* after */
			ae_ds5_link_want_triggers(&link, right, held);
			ae_ds5_link_want_rumble(&link, motor, 0);
			ae_ds5_link_want_lightbar(&link, 0, rgb, 255);
			fields = ae_ds5_link_due(&link, now, body);
			if (fields)
				ae_ds5_link_result(&link, fields, AE_DS5_WRITE_SENT, now);
		}
		printf("write rate model (%s): before %.1f reports/s (ours + the kernel's), after %.1f reports/s "
			"(merged %lu, deferred %lu)\n", bluetooth ? "Bluetooth" : "USB", before / 4.0, link.writes / 4.0,
			link.merged, link.deferred);
		if (bluetooth)
			CHECK(link.writes <= 4 * (1000 / AE_DS5_BLUETOOTH_INTERVAL_MS) + 1);
		CHECK(link.writes <= (unsigned long)before);
	}
}

int main(void)
{
	unsigned char effect[AE_DS5_EFFECT_SIZE];
	unsigned char body[AE_DS5_BODY_SIZE];
	unsigned char report[AE_DS5_BLUETOOTH_REPORT_SIZE];
	static unsigned char const off[AE_DS5_EFFECT_SIZE] = { 0x05 };
	/* zones 2-9, each at level 7 (strength 8): bits 6-29 */
	static unsigned char const feedback_2_8[AE_DS5_EFFECT_SIZE] = { 0x21, 0xfc, 0x03, 0xc0, 0xff, 0xff, 0x3f };
	static unsigned char const weapon_4_7_6[AE_DS5_EFFECT_SIZE] = { 0x25, 0x90, 0x00, 0x05 };
	/* zones 3-9 at level 4 (amplitude 5) = 0x24924800, 15 Hz */
	static unsigned char const vibration_3_5_15[AE_DS5_EFFECT_SIZE] =
		{ 0x26, 0xf8, 0x03, 0x00, 0x48, 0x92, 0x24, 0, 0, 15, 0 };
	static unsigned char const nine[] = "123456789";

	ae_ds5_effect_off(effect);
	CHECK(same(effect, off, sizeof(off)));
	ae_ds5_effect_feedback(effect, 2, 8);
	CHECK(same(effect, feedback_2_8, sizeof(feedback_2_8)));
	ae_ds5_effect_feedback(effect, 2, 0);
	CHECK(same(effect, off, sizeof(off)));
	ae_ds5_effect_weapon(effect, 4, 7, 6);
	CHECK(same(effect, weapon_4_7_6, sizeof(weapon_4_7_6)));
	/* clamped: start 2..7, end after start up to 8, strength up to 8 */
	ae_ds5_effect_weapon(effect, 0, 0, 99);
	CHECK(effect[0] == 0x25 && effect[1] == ((1 << 2) | (1 << 3)) && effect[2] == 0 && effect[3] == 7);
	ae_ds5_effect_weapon(effect, 9, 9, 1);
	CHECK(effect[1] == 0x80 && effect[2] == 0x01 && effect[3] == 0);
	ae_ds5_effect_vibration(effect, 3, 5, 15);
	CHECK(same(effect, vibration_3_5_15, sizeof(vibration_3_5_15)));
	ae_ds5_effect_vibration(effect, 3, 5, 0);
	CHECK(same(effect, off, sizeof(off)));

	/* kinds from the tag numbers */
	CHECK(ae_ds5_weapon_kind(1, 0, 15.0f, 0.0f, 0.0f, 1, 0, 0) == AE_DS5_WEAPON_BURST);
	CHECK(ae_ds5_weapon_kind(0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0) == AE_DS5_WEAPON_SEMI);
	CHECK(!strcmp(ae_ds5_weapon_kind_name(AE_DS5_WEAPON_SNIPER), "sniper"));
	CHECK(!strcmp(ae_ds5_weapon_kind_name(AE_DS5_WEAPON_PLASMA_HEAT), "plasma heat"));
	CHECK(!strcmp(ae_ds5_weapon_kind_name((enum ae_ds5_weapon_kind)99), "?"));

	/* every CE multiplayer weapon, with its tag's own numbers (the game's catalogue log, debug.ds5_log, of the stock Blood Gulch and
	Sidewinder maps: triggers, rate, charging, heat a round, zoom levels, weapon type), has a profile that is not off;
	the ball and the flag (no projectile) are ruled off: R2 swings them */
	{
		static struct
		{
			char const *name;
			int burst, needler;
			float rate, charging, heat;
			int zoom, no_projectile, plasma_rifle;
			enum ae_ds5_weapon_kind kind;
		} const weapons[] = {
			{ "pistol", 0, 0, 3.5f, 0.0f, 0.0f, 1, 0, 0, AE_DS5_WEAPON_SEMI },
			{ "assault rifle", 0, 0, 15.0f, 0.0f, 0.0f, 0, 0, 0, AE_DS5_WEAPON_AUTOMATIC },
			{ "sniper rifle", 0, 0, 2.0f, 0.0f, 0.0f, 2, 0, 0, AE_DS5_WEAPON_SNIPER },
			{ "shotgun", 0, 0, 1.0f, 0.0f, 0.0f, 0, 0, 0, AE_DS5_WEAPON_HEAVY },
			{ "rocket launcher", 0, 0, 0.5f, 0.0f, 0.0f, 1, 0, 0, AE_DS5_WEAPON_HEAVY },
			{ "plasma pistol", 0, 0, 0.0f, 0.6f, 0.16f, 0, 0, 0, AE_DS5_WEAPON_CHARGE },
			{ "plasma rifle", 0, 0, 10.0f, 0.0f, 0.08f, 0, 0, 1, AE_DS5_WEAPON_PLASMA_HEAT },
			{ "needler", 0, 1, 10.0f, 0.0f, 0.0f, 0, 0, 0, AE_DS5_WEAPON_NEEDLER },
			{ "flamethrower", 0, 0, 15.0f, 0.0f, 0.041f, 0, 0, 0, AE_DS5_WEAPON_FLAME },
			{ "battle rifle (AE pack)", 1, 0, 15.0f, 0.0f, 0.0f, 1, 0, 0, AE_DS5_WEAPON_BURST },
			{ "gravity rifle", 0, 0, 30.0f, 0.0f, 0.0f, 0, 0, 0, AE_DS5_WEAPON_AUTOMATIC },
			{ "ball", 0, 0, 0.0f, 0.0f, 0.0f, 0, 1, 0, AE_DS5_WEAPON_CARRY },
			{ "flag", 0, 0, 0.0f, 0.0f, 0.0f, 0, 1, 0, AE_DS5_WEAPON_CARRY },
		};
		size_t i;

		for (i = 0; i < sizeof(weapons) / sizeof(weapons[0]); i++)
		{
			struct ae_ds5_weapon_state state;
			char text[96];
			enum ae_ds5_weapon_kind kind = ae_ds5_weapon_kind(weapons[i].burst, weapons[i].needler, weapons[i].rate,
				weapons[i].charging, weapons[i].heat, weapons[i].zoom, weapons[i].no_projectile, weapons[i].plasma_rifle);

			if (kind != weapons[i].kind)
				printf("FAIL kind of %s: %s\n", weapons[i].name, ae_ds5_weapon_kind_name(kind));
			CHECK(kind == weapons[i].kind);
			memset(&state, 0, sizeof(state));
			state.kind = kind;
			state.rate_of_fire = weapons[i].rate;
			ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
			if (kind == AE_DS5_WEAPON_CARRY)
				CHECK(same(effect, off, sizeof(off)) && !strcmp(text, "free (carrying)"));
			else
			{
				if (same(effect, off, sizeof(off)))
					printf("FAIL %s is off\n", weapons[i].name);
				CHECK(!same(effect, off, sizeof(off)));
			}
		}
	}

	/* the weapon's state comes first, for every kind: weapon state in, effect out */
	{
		int kind;

		for (kind = AE_DS5_WEAPON_SEMI; kind < AE_DS5_WEAPON_KINDS; kind++)
		{
			struct ae_ds5_weapon_state state;
			char text[96];

			memset(&state, 0, sizeof(state));
			state.kind = (enum ae_ds5_weapon_kind)kind;
			state.rate_of_fire = 15.0f;
			state.firing = 1;
			state.readying = 1;
			ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
			CHECK(same(effect, off, sizeof(off)));
			state.readying = 0;
			state.reloading = 1;
			ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
			CHECK(same(effect, off, sizeof(off)));
			CHECK(kind == AE_DS5_WEAPON_CARRY || !strcmp(text, "free (reloading)"));
			state.reloading = 0;
			/* DRY CLICK: a weak click at the bottom of the pull (zones 7-8, strength 2) */
			state.empty = 1;
			ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
			if (kind == AE_DS5_WEAPON_CARRY)
				CHECK(same(effect, off, sizeof(off)));
			else
				CHECK(effect[0] == 0x25 && effect[1] == 0x80 && effect[2] == 0x01 && effect[3] == 1 &&
					!strncmp(text, "dry click (empty)", 17));
			state.empty = 0;
			/* LOCKED + VENTING THROB: locked solid, a throb two ticks of eight */
			state.overheated = 1;
			state.ticks = 2;
			ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
			if (kind == AE_DS5_WEAPON_CARRY)
				CHECK(same(effect, off, sizeof(off)));
			else
			{
				CHECK(effect[0] == 0x21 && effect[6] == 0x3f && !strcmp(text, "locked (overheated, venting throb)"));
				state.ticks = 9;
				ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
				CHECK(effect[0] == 0x26 && effect[9] == 8 && !strcmp(text, "locked (overheated, venting throb)"));
				/* (overheated and reloading or empty at once: the heat speaks) */
				state.reloading = state.empty = 1;
				ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
				CHECK(!strcmp(text, "locked (overheated, venting throb)"));
			}
		}
	}

	/* the profiles */
	{
		static unsigned char const full_kick[AE_DS5_EFFECT_SIZE] = { 0x21, 0xff, 0x03, 0xff, 0xff, 0xff, 0x3f };
		static unsigned char const weapon_4_7_4[AE_DS5_EFFECT_SIZE] = { 0x25, 0x90, 0x00, 0x03 };
		static unsigned char const weapon_4_7_6[AE_DS5_EFFECT_SIZE] = { 0x25, 0x90, 0x00, 0x05 };
		struct ae_ds5_weapon_state state;
		struct ae_ds5_rumble rumble;
		char text[96];

		memset(&state, 0, sizeof(state));
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, off, sizeof(off)) && !strcmp(text, "off"));
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_NONE, AE_DS5_EVENT_SHOT, 100) == 0 &&
			same(effect, off, sizeof(off)) && rumble.left == 0 && rumble.right == 0);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_CARRY, AE_DS5_EVENT_SHOT, 100) == 0);

		/* pistol: round 1's click, a full snap at each shot */
		state.kind = AE_DS5_WEAPON_SEMI;
		state.rate_of_fire = 3.5f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, weapon_4_7_4, sizeof(weapon_4_7_4)));
		CHECK(!strcmp(text, "held: click zones 4-7 strength 4"));
		state.firing = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, weapon_4_7_4, sizeof(weapon_4_7_4)));
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_SEMI, AE_DS5_EVENT_SHOT, 100) == 35 &&
			same(effect, full_kick, sizeof(full_kick)));
		/* (human: the left motor) */
		CHECK(rumble.left == 150 && rumble.right == 0 && rumble.ms == 60);

		/* battle rifle: the pistol's zones at the shotgun's strength */
		state.kind = AE_DS5_WEAPON_BURST;
		state.firing = 0;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, weapon_4_7_6, sizeof(weapon_4_7_6)));
		/* (firing: a buzz at the burst's 15 rounds a second, amplitude 7, owner 10-15 "more punch like the assault
		rifle") */
		state.firing = 1;
		state.rate_of_fire = 15.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "firing: vibration from zone 3 amplitude 7 at 15 Hz"));
		state.firing = 0;

		/* assault rifle: light hold; firing, round 1's buzz at the rate of fire; no kick */
		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_AUTOMATIC;
		state.rate_of_fire = 15.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(effect[0] == 0x21 && effect[1] == 0xf8 && effect[3] == 0x00 && effect[4] == 0x92 && effect[6] == 0x09);
		state.firing = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, vibration_3_5_15, sizeof(vibration_3_5_15)));
		CHECK(!strcmp(text, "firing: vibration from zone 3 amplitude 5 at 15 Hz"));
		/* (the owner's bug: the buzz went on while reloading) */
		state.reloading = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, off, sizeof(off)));
		state.reloading = 0;
		state.rate_of_fire = 100.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(effect[9] == 60);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_AUTOMATIC, AE_DS5_EVENT_SHOT, 100) == 0 && rumble.left == 90);

		/* needler: twice the rate of fire, amplitude 3 */
		state.kind = AE_DS5_WEAPON_NEEDLER;
		state.rate_of_fire = 10.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(effect[0] == 0x26 && effect[9] == 20 && !strcmp(text, "firing: vibration from zone 3 amplitude 3 at 20 Hz"));

		/* flamethrower: a fixed low roar */
		state.kind = AE_DS5_WEAPON_FLAME;
		state.rate_of_fire = 15.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(effect[0] == 0x26 && effect[9] == 18);

		/* plasma rifle: with heat, then locked */
		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_PLASMA_HEAT;
		state.rate_of_fire = 10.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 1"));
		state.heat = 1.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 7"));
		state.heat = 0.5f;
		state.firing = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "firing: vibration from zone 2 amplitude 5 at 10 Hz"));
		state.overheated = 1;
		state.ticks = 4;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(same(effect, full_kick, sizeof(full_kick)) && !strcmp(text, "locked (overheated, venting throb)"));

		/* plasma pistol: round 1's strength from zone 2, building to full with the charge */
		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_CHARGE;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 4"));
		/* (charging: the trigger is held down, so a hum rising with the charge, faster once charged) */
		state.charge = 0.5f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "charging: vibration from zone 2 amplitude 2 at 25 Hz"));
		state.charge = 1.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "charged: vibration from zone 2 amplitude 3 at 40 Hz"));
		/* (the motors while it charges: a pulse every 3 ticks, rising with the charge; none for other kinds or at rest) */
		{
			struct ae_ds5_rumble charge_rumble;

			CHECK(ae_ds5_charge_rumble(AE_DS5_WEAPON_CHARGE, 0.5f, 3, &charge_rumble) && charge_rumble.right == 140 &&
				charge_rumble.left == 55 && charge_rumble.ms == 140);
			CHECK(ae_ds5_charge_rumble(AE_DS5_WEAPON_CHARGE, 1.0f, 6, &charge_rumble) && charge_rumble.right == 210 &&
				charge_rumble.left == 110);
			CHECK(!ae_ds5_charge_rumble(AE_DS5_WEAPON_CHARGE, 0.5f, 4, &charge_rumble));
			CHECK(!ae_ds5_charge_rumble(AE_DS5_WEAPON_CHARGE, 0.0f, 3, &charge_rumble));
			CHECK(!ae_ds5_charge_rumble(AE_DS5_WEAPON_SEMI, 0.5f, 3, &charge_rumble));
		}
		/* (its battery spent: the dry click) */
		state.empty = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(effect[0] == 0x25 && !strncmp(text, "dry click (empty)", 17));

		/* the other kicks */
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_HEAVY, AE_DS5_EVENT_SHOT, 100) == 130 && effect[0] == 0x26 &&
			effect[9] == 30 && rumble.left == 255);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_SNIPER, AE_DS5_EVENT_SHOT, 100) == 90 &&
			same(effect, full_kick, sizeof(full_kick)));
		/* (the battle rifle: no kick, its firing buzz carries the burst; both motors a round, 50 ms) */
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_BURST, AE_DS5_EVENT_SHOT, 100) == 0 && rumble.left == 255 &&
			rumble.right == 140 && rumble.ms == 50);
		/* the plasma pistol: a powerful pulse a shot (Covenant: the right motor), the overcharged blast both */
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_CHARGE, AE_DS5_EVENT_SHOT, 100) == 70 && effect[0] == 0x26 &&
			effect[9] == 40 && rumble.right == 160 && rumble.left == 0);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_CHARGE, AE_DS5_EVENT_SHOT_CHARGED, 100) == 240 &&
			effect[0] == 0x26 && effect[9] == 15 && rumble.left == 170 && rumble.right == 255 && rumble.ms == 240);
		/* (charged means nothing to a weapon with no charged kick) */
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_HEAVY, AE_DS5_EVENT_SHOT_CHARGED, 100) == 130);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_PLASMA_HEAT, AE_DS5_EVENT_SHOT, 100) == 0 &&
			rumble.right == 80 && rumble.left == 0);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_NEEDLER, AE_DS5_EVENT_SHOT, 100) == 0 && rumble.right == 70);
		/* the states' events: the seat bump, the settle, the pump; grenades */
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_AUTOMATIC, AE_DS5_EVENT_MAG_IN, 100) == 40 &&
			effect[0] == 0x21 && rumble.left == 60);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_SNIPER, AE_DS5_EVENT_SETTLE, 100) == 30 && effect[0] == 0x21 &&
			rumble.left == 0 && rumble.right == 0);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_HEAVY, AE_DS5_EVENT_CHAMBER, 100) == 45 && rumble.left == 120);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_NONE, AE_DS5_EVENT_GRENADE_FRAG, 100) == 80 &&
			effect[0] == 0x26 && rumble.left == 140 && rumble.right == 0);
		CHECK(ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_NONE, AE_DS5_EVENT_GRENADE_PLASMA, 100) == 80 &&
			effect[9] == 45 && rumble.right == 140 && rumble.left == 0);
		/* the families */
		CHECK(ae_ds5_kind_covenant(AE_DS5_WEAPON_CHARGE) && ae_ds5_kind_covenant(AE_DS5_WEAPON_NEEDLER) &&
			ae_ds5_kind_covenant(AE_DS5_WEAPON_PLASMA_HEAT));
		CHECK(!ae_ds5_kind_covenant(AE_DS5_WEAPON_SEMI) && !ae_ds5_kind_covenant(AE_DS5_WEAPON_FLAME) &&
			!ae_ds5_kind_covenant(AE_DS5_WEAPON_BURST));
		/* the overcharge's green flash */
		{
			unsigned char rgb[3];

			CHECK(ae_ds5_overcharge_flash(rgb, 0) && rgb[1] == 255 && rgb[0] < rgb[1] && rgb[2] < rgb[1]);
			CHECK(ae_ds5_overcharge_flash(rgb, 8) && rgb[1] == 85);
			CHECK(!ae_ds5_overcharge_flash(rgb, 9) && !ae_ds5_overcharge_flash(rgb, -1));
		}
		CHECK(!strcmp(ae_ds5_profile_feel(AE_DS5_WEAPON_BURST), "click, buzzes through the burst"));
	}

	/* the powerups: lightbar and L2 */
	{
		static unsigned char const red[3] = { 200, 0, 0 };
		static unsigned char const black[3] = { 0, 0, 0 };
		struct ae_ds5_powerups powerups;
		unsigned char rgb[3];
		char text[96];

		memset(&powerups, 0, sizeof(powerups));
		powerups.shield = 1.0f;
		powerups.juggernaut_ticks = -1;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 200 && rgb[1] == 0 && rgb[2] == 0 && !strcmp(text, "colour"));
		/* (black armour: the game shows the shield's colours instead; a dark colour only) */
		CHECK(ae_ds5_lightbar_black(black) && !ae_ds5_lightbar_black(red));
		ae_ds5_lightbar(rgb, black, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 48 && rgb[1] == 48 && rgb[2] == 48);
		CHECK(!ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text)));
		/* camouflage: dimmed to 15 % */
		powerups.camo = 1.0f;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 30 && !strcmp(text, "active camouflage (dimmed)"));
		powerups.camo = 0.0f;
		/* an overshield charging: a pulse between 35 % and 100 %, and L2 buzzing, rising with it */
		powerups.shield = 2.0f;
		powerups.shield_rising = 1;
		powerups.ticks = 0;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 70);
		powerups.ticks = 7;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 200);
		CHECK(ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text)) && effect[0] == 0x26 && effect[9] == 30);
		CHECK(!strcmp(text, "overshield charging (vibration amplitude 4 at 30 Hz)"));
		powerups.shield = 3.0f;
		ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text));
		CHECK(!strcmp(text, "overshield charging (vibration amplitude 6 at 30 Hz)"));
		/* held: a slow breath, 70-100 %, L2 left to the zoom */
		powerups.shield_rising = 0;
		powerups.ticks = 0;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 140 && !strcmp(text, "overshield (slow breath)"));
		CHECK(!ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text)));
		/* becoming the juggernaut: white, fading back; L2 a short strong buzz */
		powerups.juggernaut_ticks = 0;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255 && !strcmp(text, "juggernaut flash"));
		CHECK(ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text)) && effect[0] == 0x26);
		powerups.juggernaut_ticks = 15;
		CHECK(!ae_ds5_left_powerup(effect, &powerups, 100, text, sizeof(text)));
		powerups.juggernaut_ticks = 25;
		ae_ds5_lightbar(rgb, red, &powerups, text, sizeof(text));
		CHECK(!strcmp(text, "overshield (slow breath)"));
	}

	/* L2: zoomed, a grenade wound up, grenades in hand, none (the dry click) */
	CHECK(!strcmp(ae_ds5_left_effect(effect, 1, 0, 0, 100), "light resistance (zoomed)") && effect[0] == 0x21);
	CHECK(!strcmp(ae_ds5_left_effect(effect, 0, 2, 1, 100), "grenade wound up (feedback from zone 2)") &&
		effect[3] == 0x00 && effect[4] == 0x49);
	CHECK(!strcmp(ae_ds5_left_effect(effect, 0, 2, 0, 100), "grenade ready (light hold)") && effect[1] == 0xf8);
	CHECK(!strcmp(ae_ds5_left_effect(effect, 0, 0, 0, 100), "no grenades (dry click)") && effect[0] == 0x25 &&
		effect[1] == 0x80 && effect[2] == 0x01);

	/* the energy weapons at rest, firing, charging, overheated, empty (the owner's round 2 log: the plasma pistol
	read "overheated" and the plasma rifle "empty" at rest) */
	{
		struct ae_ds5_weapon_state state;
		char text[96];

		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_CHARGE;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 4"));
		state.firing = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 4"));
		state.firing = 0;
		state.charge = 0.75f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "charging: vibration from zone 2 amplitude 3 at 25 Hz"));
		state.charge = 0.0f;
		state.overheated = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "locked (overheated, venting throb)"));
		state.overheated = 0;
		state.empty = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strncmp(text, "dry click (empty)", 17));

		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_PLASMA_HEAT;
		state.rate_of_fire = 10.0f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 1"));
		state.firing = 1;
		state.heat = 0.25f;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "firing: vibration from zone 2 amplitude 4 at 10 Hz"));
		state.overheated = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strcmp(text, "locked (overheated, venting throb)"));
		state.overheated = 0;
		state.firing = 0;
		state.empty = 1;
		ae_ds5_right_held(effect, &state, 100, text, sizeof(text));
		CHECK(!strncmp(text, "dry click (empty)", 17));
	}

	/* LIGHTBAR SHIELD: the shield meter as the HUD draws it */
	{
		struct ae_ds5_shield shield;
		unsigned char rgb[3];
		char text[64];

		memset(&shield, 0, sizeof(shield));
		shield.popped_ticks = -1;
		shield.min_color = 0xff0000UL;
		shield.max_color = 0x0000ffUL;
		shield.flash_color = 0x00ff00UL;
		shield.empty_color = 0x400000UL;
		/* full: the owner's deep navy (0, 14, 117); just under full: the meter's max colour */
		shield.vitality = 1.0f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 0 && rgb[1] == 14 && rgb[2] == 117 && !strcmp(text, "shield: full"));
		shield.vitality = 0.99f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 255 && !strcmp(text, "shield: meter"));
		/* half: halfway from min to max, at 62.5 % */
		shield.vitality = 0.5f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 80 && rgb[1] == 0 && rgb[2] == 80);
		/* (coarse steps: a little more shield, the same colour) */
		shield.vitality = 0.51f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 80 && rgb[2] == 80);
		/* low: blinks with the flash colour */
		shield.vitality = 0.125f;
		shield.ticks = 0;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[1] == 0 && !strcmp(text, "shield: low (blink)"));
		shield.ticks = 4;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[1] > 0 && rgb[0] == 0 && !strcmp(text, "shield: low (blink)"));
		/* empty: the empty colour */
		shield.vitality = 0.0f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 38 && !strcmp(text, "shield: empty"));
		/* popped: white only with the hit feedback */
		shield.popped_ticks = 2;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(!strcmp(text, "shield: empty"));
		shield.pop_flash = 1;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
		shield.popped_ticks = 6;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(!strcmp(text, "shield: empty"));
		/* overshield: the HUD's layer colours */
		shield.vitality = 2.0f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 51 && !strcmp(text, "shield: overshield layer 1"));
		shield.vitality = 2.5f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[1] > rgb[0] && rgb[1] > 60 && rgb[1] < 209 && !strcmp(text, "shield: overshield layer 2"));
		shield.vitality = 3.0f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 14 && rgb[1] == 209 && rgb[2] == 0 && !strcmp(text, "shield: overshield layer 2"));
		/* dead: dark */
		shield.dead = 1;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[2] == 15 && !strcmp(text, "shield: dark (dead)"));
		/* no colours in the tag: the HUD's blue */
		memset(&shield, 0, sizeof(shield));
		shield.popped_ticks = -1;
		shield.vitality = 0.99f;
		ae_ds5_lightbar_shield(rgb, &shield, text, sizeof(text));
		CHECK(rgb[0] == 0x28 && rgb[1] == 0x96 && rgb[2] == 0xff);
	}

	/* TRIGGER STRENGTH: every strength-like number scaled, kept 1-8; the player lights */
	{
		struct ae_ds5_weapon_state state;
		struct ae_ds5_rumble rumble;
		char text[96];

		CHECK(ae_ds5_strength_percent("light") == 60 && ae_ds5_strength_percent("normal") == 100 &&
			ae_ds5_strength_percent("strong") == 140 && ae_ds5_strength_percent("") == 100);
		memset(&state, 0, sizeof(state));
		state.kind = AE_DS5_WEAPON_SNIPER;
		ae_ds5_right_held(effect, &state, 60, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 4"));
		ae_ds5_right_held(effect, &state, 140, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 2 strength 8"));
		state.kind = AE_DS5_WEAPON_NEEDLER;
		ae_ds5_right_held(effect, &state, 60, text, sizeof(text));
		CHECK(!strcmp(text, "held: feedback from zone 3 strength 1"));
		ae_ds5_event(effect, &rumble, AE_DS5_WEAPON_SEMI, AE_DS5_EVENT_SHOT, 60);
		CHECK(effect[0] == 0x21 && effect[3] == 0x24);
		ae_ds5_left_effect(effect, 1, 0, 0, 140);
		CHECK(effect[0] == 0x21 && effect[3] == 0x80);
		CHECK(ae_ds5_player_lights(1) == 0x24 && ae_ds5_player_lights(2) == 0x2a && ae_ds5_player_lights(4) == 0x3b);
		CHECK(ae_ds5_player_lights(0) == 0 && ae_ds5_player_lights(5) == 0);
		CHECK(ae_ds5_player_index_of_lights(0x24) == 0 && ae_ds5_player_index_of_lights(0x3b) == 3);
		CHECK(ae_ds5_player_index_of_lights(0) == -1);
	}

	/* CRC-32 as zlib's, and continued */
	CHECK(ae_ds5_crc32(0, nine, 9) == 0xCBF43926UL);
	CHECK(ae_ds5_crc32(ae_ds5_crc32(0, nine, 4), nine + 4, 5) == 0xCBF43926UL);

	/* the reports */
	memset(body, 0, sizeof(body));
	body[AE_DS5_BODY_ENABLE1] = AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER;
	body[AE_DS5_BODY_ENABLE2] = AE_DS5_ENABLE2_LIGHTBAR;
	body[AE_DS5_BODY_RED] = 255;
	CHECK(ae_ds5_output_report(report, 47, 0, 0, body) == 0);
	CHECK(ae_ds5_output_report(report, sizeof(report), 0, 0, body) == AE_DS5_USB_REPORT_SIZE);
	CHECK(report[0] == 0x02 && report[1] == 0x0c && report[2] == 0x04 && report[1 + AE_DS5_BODY_RED] == 255 &&
		report[47] == 0);
	CHECK(ae_ds5_output_report(report, 77, 1, 3, body) == 0);
	CHECK(ae_ds5_output_report(report, sizeof(report), 1, 3, body) == AE_DS5_BLUETOOTH_REPORT_SIZE);
	CHECK(report[0] == 0x31 && report[1] == 0x30 && report[2] == 0x10 && report[3] == 0x0c &&
		report[3 + AE_DS5_BODY_RED] == 255);
	/* (zlib.crc32(b"\xa2" + report[:74]) of this report) */
	CHECK(report[74] == 0x09 && report[75] == 0xf2 && report[76] == 0x98 && report[77] == 0x6e);

	link_tests();
	write_rate_model();

	if (failures)
		return 1;
	printf("ae_ds5_rules_test: all passed\n");
	return 0;
}
