/* ae_killfeed_rules_test.c: the kill feed's weapons, rows, overflow, colours, layout and split-screen line (ae_killfeed_rules.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_killfeed_rules.h"
#include "ae_marker_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.01f)

static void weapon_test(void)
{
	char path[64];

	CHECK(ae_kf_weapon_of_path("weapons\\pistol\\bullet") == AE_KF_WEAPON_PISTOL);
	CHECK(ae_kf_weapon_of_path("Weapons/Assault Rifle/Bullet") == AE_KF_WEAPON_ASSAULT_RIFLE);
	CHECK(ae_kf_weapon_of_path("weapons\\sniper rifle\\sniper bullet") == AE_KF_WEAPON_SNIPER);
	CHECK(ae_kf_weapon_of_path("weapons\\shotgun\\pellet") == AE_KF_WEAPON_SHOTGUN);
	CHECK(ae_kf_weapon_of_path("weapons\\rocket launcher\\explosion") == AE_KF_WEAPON_ROCKET);
	/* the plasma pistol is not the pistol, and the fuel rod goes by either name */
	CHECK(ae_kf_weapon_of_path("weapons\\plasma pistol\\bolt") == AE_KF_WEAPON_PLASMA_PISTOL);
	CHECK(ae_kf_weapon_of_path("weapons\\plasma rifle\\bolt") == AE_KF_WEAPON_PLASMA_RIFLE);
	CHECK(ae_kf_weapon_of_path("weapons\\plasma cannon\\explosion") == AE_KF_WEAPON_FUEL_ROD);
	CHECK(ae_kf_weapon_of_path("weapons\\needler\\explosion") == AE_KF_WEAPON_NEEDLER);
	CHECK(ae_kf_weapon_of_path("weapons\\flamethrower\\burning") == AE_KF_WEAPON_FLAME);
	/* (0.7.5: the packs of slots 9-14 by their slot, and the community packs' "evolved" folders not taken for the Evo
	rifle) */
	CHECK(ae_kf_weapon_of_slot(9) == AE_KF_WEAPON_SNIPER_BOLT);
	CHECK(ae_kf_weapon_of_slot(10) == AE_KF_WEAPON_MAGNUM_SCOPED);
	/* (the HJ magnum's word, nine characters: it fits the column as BRUTE SHOT's ten do) */
	CHECK(!strcmp(ae_kf_weapon_word(AE_KF_WEAPON_MAGNUM_SCOPED), "HJ MAGNUM"));
	CHECK(ae_kf_weapon_of_slot(11) == AE_KF_WEAPON_BRUTE_SHOT);
	CHECK(ae_kf_weapon_of_slot(12) == AE_KF_WEAPON_AR_MODERN);
	CHECK(ae_kf_weapon_of_slot(13) == AE_KF_WEAPON_SIDEARM);
	CHECK(ae_kf_weapon_of_slot(14) == AE_KF_WEAPON_SWORD);
	/* (the cache's registry: ids 15 and up have no enumerator, the registry's word and, with the picture column "text", no
	picture; id 24's picture column names the pack's own icon) */
	CHECK(ae_kf_weapon_of_slot(0) == AE_KF_WEAPON_UNKNOWN && ae_kf_weapon_of_id(9999) == AE_KF_WEAPON_UNKNOWN);
	CHECK(ae_kf_weapon_of_slot(15) == AE_KF_WEAPON_REGISTRY + 15 && ae_kf_weapon_of_id(16) == AE_KF_WEAPON_REGISTRY + 16);
	CHECK(!strcmp(ae_kf_weapon_word(ae_kf_weapon_of_id(16)), "PDW") && !strcmp(ae_kf_weapon_tag(ae_kf_weapon_of_id(18)), "FOCUS BEAM"));
	CHECK(ae_kf_weapon_has_no_picture(ae_kf_weapon_of_id(16)) && !ae_kf_weapon_has_no_picture(ae_kf_weapon_of_id(24)));
	CHECK(!ae_kf_weapon_has_no_picture(ae_kf_weapon_of_id(1)) && ae_kf_weapon_of_id(1) == AE_KF_WEAPON_BATTLE_RIFLE);
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_SIDEARM), "SIDEARM"));
	/* (0.7.6: a kill by the round modes' zone reads ZONE, a word alone: the kill is dealt as falling damage) */
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_ZONE), "ZONE") && !strcmp(ae_kf_weapon_word(AE_KF_WEAPON_ZONE), "ZONE") &&
		ae_kf_weapon_has_no_picture(AE_KF_WEAPON_ZONE) && AE_KF_WEAPON_ZONE < AE_KF_WEAPON_COUNT);
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_BRUTE_SHOT), "BRUTE SHOT"));
	CHECK(ae_kf_weapon_of_path("ae\\packs\\brute_shot\\weapons\\evolved\\brute_shot\\damage_effects\\blast") ==
		AE_KF_WEAPON_UNKNOWN);
	CHECK(ae_kf_weapon_of_path("ae\\packs\\evo_rifle\\weapons\\evo_rifle\\bullet") == AE_KF_WEAPON_EVO);
	CHECK(ae_kf_weapon_of_path("weapons\\evo\\bullet") == AE_KF_WEAPON_EVO);
	CHECK(ae_kf_weapon_of_path("weapons\\assault rifle\\melee") == AE_KF_WEAPON_MELEE);
	/* a melee blow is a melee whatever weapon dealt it */
	CHECK(ae_kf_weapon_of_path("weapons\\pistol\\melee") == AE_KF_WEAPON_MELEE);
	CHECK(ae_kf_weapon_of_path("weapons\\frag grenade\\explosion") == AE_KF_WEAPON_FRAG);
	CHECK(ae_kf_weapon_of_path("weapons\\plasma grenade\\attached") == AE_KF_WEAPON_PLASMA_GRENADE);
	CHECK(ae_kf_weapon_of_path("vehicles\\warthog\\bullet") == AE_KF_WEAPON_VEHICLE);
	CHECK(ae_kf_weapon_of_path("globals\\falling") == AE_KF_WEAPON_FALL);
	CHECK(ae_kf_weapon_of_path("globals\\distance") == AE_KF_WEAPON_FALL);
	CHECK(ae_kf_weapon_of_path("weapons\\energy sword\\melee") == AE_KF_WEAPON_MELEE);
	CHECK(ae_kf_weapon_of_path("weapons\\energy sword\\sword") == AE_KF_WEAPON_SWORD);
	CHECK(ae_kf_weapon_of_path("weapons\\gravity hammer\\hit") == AE_KF_WEAPON_HAMMER);
	CHECK(ae_kf_weapon_of_path("weapons\\bomb\\blast") == AE_KF_WEAPON_BOMB);
	CHECK(ae_kf_weapon_of_path("weapons\\something else\\bullet") == AE_KF_WEAPON_UNKNOWN);
	CHECK(ae_kf_weapon_of_path(NULL) == AE_KF_WEAPON_UNKNOWN && ae_kf_weapon_of_path("") == AE_KF_WEAPON_UNKNOWN);
	/* the packs' slots */
	CHECK(ae_kf_weapon_of_slot(1) == AE_KF_WEAPON_BATTLE_RIFLE && ae_kf_weapon_of_slot(2) == AE_KF_WEAPON_SMG);
	CHECK(ae_kf_weapon_of_slot(3) == AE_KF_WEAPON_SMG_SILENCED && ae_kf_weapon_of_slot(4) == AE_KF_WEAPON_CARBINE);
	CHECK(ae_kf_weapon_of_slot(5) == AE_KF_WEAPON_AR_SILENCED && ae_kf_weapon_of_slot(6) == AE_KF_WEAPON_DMR);
	CHECK(ae_kf_weapon_of_slot(7) == AE_KF_WEAPON_EVO && ae_kf_weapon_of_slot(8) == AE_KF_WEAPON_NEEDLER && ae_kf_weapon_of_slot(0) == AE_KF_WEAPON_UNKNOWN);
	/* words: the spec's list; tags are their words but for the plasma grenade and the unknown */
	CHECK(!strcmp(ae_kf_weapon_word(AE_KF_WEAPON_ROCKET), "ROCKETS") && !strcmp(ae_kf_weapon_word(AE_KF_WEAPON_BATTLE_RIFLE), "BR"));
	CHECK(!strcmp(ae_kf_weapon_word(AE_KF_WEAPON_FRAG), "GRENADE") && !strcmp(ae_kf_weapon_word(AE_KF_WEAPON_PLASMA_GRENADE), "GRENADE"));
	CHECK(!strcmp(ae_kf_weapon_word(AE_KF_WEAPON_UNKNOWN), "") && !strcmp(ae_kf_weapon_word(-3), "") && !strcmp(ae_kf_weapon_word(999), ""));
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_PLASMA_GRENADE), "PLASMA") && !strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_FALL), "FALL"));
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_MELEE), "MELEE") && !strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_UNKNOWN), "?"));
	CHECK(!strcmp(ae_kf_weapon_tag(AE_KF_WEAPON_PISTOL), "PISTOL"));
	/* the weapon tag by the damage's folder */
	CHECK(ae_kf_weapon_tag_path("weapons\\pistol\\bullet", path, sizeof(path)) && !strcmp(path, "weapons\\pistol\\pistol"));
	CHECK(ae_kf_weapon_tag_path("Weapons/Plasma Rifle/bolt", path, sizeof(path)) && !strcmp(path, "weapons\\plasma rifle\\plasma rifle"));
	CHECK(!ae_kf_weapon_tag_path("vehicles\\warthog\\bullet", path, sizeof(path)) && path[0] == 0);
	CHECK(!ae_kf_weapon_tag_path("weapons\\bullet", path, sizeof(path)) && !ae_kf_weapon_tag_path(NULL, path, sizeof(path)));
	CHECK(!ae_kf_weapon_tag_path("weapons\\pistol\\bullet", path, 10));
	CHECK(ae_kf_weapon_has_no_picture(AE_KF_WEAPON_MELEE) && ae_kf_weapon_has_no_picture(AE_KF_WEAPON_FALL));
	CHECK(!ae_kf_weapon_has_no_picture(AE_KF_WEAPON_PISTOL) && !ae_kf_weapon_has_no_picture(AE_KF_WEAPON_EVO));
}

static struct ae_kf_row row_of(boolean mine, boolean assist)
{
	struct ae_kf_row row;

	memset(&row, 0, sizeof(row));
	row.kind = AE_KF_KIND_KILL;
	row.mine = mine;
	row.assist = assist;
	row.killer = 1;
	row.victim = 2;
	row.weapon_tag = -1;
	return row;
}

static void life_test(void)
{
	struct ae_kf_list list;
	struct ae_kf_row plain = row_of(FALSE, FALSE), own = row_of(TRUE, FALSE), helped = row_of(FALSE, TRUE);
	real slot, alpha, slide;

	/* every row lives 5 s, its own and an assist's too */
	CHECK(AE_KF_LIFE == 150 && AE_KF_FADE_TICKS == 15 && AE_KF_MOST == 5 && AE_KF_FULL_LEAVE_TICKS == 90);
	ae_kf_list_reset(&list);
	CHECK(ae_kf_list_count(&list) == 0);
	CHECK(ae_kf_list_add(&list, &plain, 100) == 0);
	CHECK(ae_kf_list_add(&list, &own, 100) == 1);
	CHECK(ae_kf_list_add(&list, &helped, 100) == 2);
	CHECK(list.rows[0].life_end == 250 && list.rows[1].life_end == 250 && list.rows[2].life_end == 250);
	/* new: it slides in 16 and is clear, and is whole after 4 ticks */
	CHECK(ae_kf_row_pose(&list.rows[2], 100, &slot, &alpha, &slide) && NEAR(alpha, 0.0f) && NEAR(slide, 16.0f) && NEAR(slot, 0.0f));
	CHECK(ae_kf_row_pose(&list.rows[2], 102, &slot, &alpha, &slide) && NEAR(alpha, 0.5f) && NEAR(slide, 8.0f));
	CHECK(ae_kf_row_pose(&list.rows[2], 104, &slot, &alpha, &slide) && NEAR(alpha, 1.0f) && NEAR(slide, 0.0f));
	/* the older ones are one pitch down per row newer, moving in 4 ticks */
	ae_kf_list_step(&list, 100);
	CHECK(ae_kf_row_pose(&list.rows[0], 100, &slot, &alpha, &slide) && NEAR(slot, 0.0f));
	CHECK(ae_kf_row_pose(&list.rows[0], 102, &slot, &alpha, &slide) && NEAR(slot, 1.0f));
	CHECK(ae_kf_row_pose(&list.rows[0], 104, &slot, &alpha, &slide) && NEAR(slot, 2.0f));
	CHECK(ae_kf_row_pose(&list.rows[2], 110, &slot, &alpha, &slide) && NEAR(slot, 0.0f));
	/* a row holds to 5 s, then fades over 0.5 s */
	ae_kf_list_step(&list, 249);
	CHECK(ae_kf_row_pose(&list.rows[0], 249, &slot, &alpha, &slide) && NEAR(alpha, 1.0f));
	CHECK(ae_kf_row_pose(&list.rows[0], 250, &slot, &alpha, &slide) && NEAR(alpha, 1.0f));
	CHECK(ae_kf_row_pose(&list.rows[0], 257, &slot, &alpha, &slide) && alpha > 0.4f && alpha < 0.6f);
	CHECK(!ae_kf_row_pose(&list.rows[0], 265, &slot, &alpha, &slide));
	ae_kf_list_step(&list, 264);
	CHECK(ae_kf_list_count(&list) == 3);
	ae_kf_list_step(&list, 265);
	CHECK(ae_kf_list_count(&list) == 0);
	/* a row in the middle going: the older one under it closes up over 4 ticks */
	ae_kf_list_reset(&list);
	ae_kf_list_add(&list, &own, 0);
	ae_kf_list_add(&list, &plain, 0);
	ae_kf_list_add(&list, &own, 0);
	list.rows[1].life_end = 20;
	ae_kf_list_step(&list, 34);
	CHECK(ae_kf_row_pose(&list.rows[0], 34, &slot, &alpha, &slide) && NEAR(slot, 2.0f));
	ae_kf_list_step(&list, 35);
	CHECK(ae_kf_list_count(&list) == 2);
	CHECK(ae_kf_row_pose(&list.rows[0], 35, &slot, &alpha, &slide) && NEAR(slot, 2.0f));
	CHECK(ae_kf_row_pose(&list.rows[0], 37, &slot, &alpha, &slide) && NEAR(slot, 1.5f));
	CHECK(ae_kf_row_pose(&list.rows[0], 39, &slot, &alpha, &slide) && NEAR(slot, 1.0f));
	CHECK(ae_kf_row_pose(&list.rows[1], 39, &slot, &alpha, &slide) && NEAR(slot, 0.0f));
}

static void overflow_test(void)
{
	struct ae_kf_list list;
	struct ae_kf_row plain = row_of(FALSE, FALSE), own = row_of(TRUE, FALSE);
	int index;
	real slot, alpha, slide;

	/* four rows, none squeezed; the fifth fills the list: the oldest is to leave once it has had 3 s */
	ae_kf_list_reset(&list);
	for (index = 0; index < 4; index++)
	{
		plain.killer = 10 + index;
		CHECK(ae_kf_list_add(&list, &plain, 100 + index) == index);
	}
	CHECK(list.rows[0].life_end == 250);
	plain.killer = 14;
	CHECK(ae_kf_list_add(&list, &plain, 110) == 4);
	CHECK(list.rows[0].life_end == 190 && list.rows[1].life_end == 251 && list.rows[4].life_end == 260);
	/* it fades after those 3 s, and the next oldest is not squeezed while it is leaving */
	ae_kf_list_step(&list, 195);
	CHECK(ae_kf_row_pose(&list.rows[0], 195, &slot, &alpha, &slide) && alpha < 1.0f && alpha > 0.5f);
	CHECK(list.rows[1].life_end == 251);
	ae_kf_list_step(&list, 205);
	CHECK(ae_kf_list_count(&list) == 4 && list.rows[0].killer == 11);
	/* a full list whose oldest had its 3 s long ago: it starts to leave now */
	ae_kf_list_reset(&list);
	for (index = 0; index < 4; index++)
		ae_kf_list_add(&list, &plain, 0);
	ae_kf_list_step(&list, 140);
	plain.killer = 77;
	ae_kf_list_add(&list, &plain, 140);
	CHECK(list.rows[0].life_end == 140);
	/* a sixth arriving at once, the oldest not yet 3 s old: it goes in 100 ms; no queue */
	ae_kf_list_reset(&list);
	for (index = 0; index < 5; index++)
	{
		plain.killer = 20 + index;
		ae_kf_list_add(&list, &plain, 50 + index);
	}
	plain.killer = 99;
	CHECK(ae_kf_list_add(&list, &plain, 60) == 5);
	CHECK(list.rows[0].gone_at == 60 && list.rows[0].killer == 20);
	CHECK(ae_kf_list_live(&list, 60) == 5);
	CHECK(ae_kf_row_pose(&list.rows[0], 61, &slot, &alpha, &slide) && alpha > 0.5f && alpha < 0.8f);
	CHECK(!ae_kf_row_pose(&list.rows[0], 63, &slot, &alpha, &slide));
	ae_kf_list_step(&list, 63);
	CHECK(ae_kf_list_count(&list) == 5 && list.rows[0].killer == 21 && list.rows[4].killer == 99);
	/* your own rows are no different: the oldest goes whoever's it is */
	ae_kf_list_reset(&list);
	ae_kf_list_add(&list, &own, 10);
	for (index = 0; index < 4; index++)
	{
		plain.killer = 30 + index;
		ae_kf_list_add(&list, &plain, 11);
	}
	plain.killer = 88;
	ae_kf_list_add(&list, &plain, 12);
	CHECK(list.rows[0].gone_at == 12 && list.rows[1].gone_at < 0);
	/* a long burst never runs the array over */
	ae_kf_list_reset(&list);
	for (index = 0; index < 40; index++)
		ae_kf_list_add(&list, &plain, 100);
	CHECK(ae_kf_list_live(&list, 100) == 5 && ae_kf_list_count(&list) <= AE_KF_ROWS);
	/* the clock starting again (a new game) empties the list */
	ae_kf_list_step(&list, 5);
	CHECK(ae_kf_list_count(&list) == 0);
}

static void colour_test(void)
{
	struct ae_kf_rgb c;

	CHECK(ae_kf_is_ally(TRUE, FALSE, TRUE) && !ae_kf_is_ally(TRUE, FALSE, FALSE));
	CHECK(ae_kf_is_ally(FALSE, TRUE, FALSE) && ae_kf_is_ally(TRUE, TRUE, FALSE));
	/* a free for all: only you */
	CHECK(!ae_kf_is_ally(FALSE, FALSE, TRUE));
	/* CLASSIC with ENEMY NAME COLOR CLASSIC: an enemy is drawn white; everything else is the game's rule */
	CHECK(ae_kf_name_white(FALSE, FALSE, FALSE));
	CHECK(!ae_kf_name_white(FALSE, FALSE, TRUE) && !ae_kf_name_white(FALSE, TRUE, FALSE));
	CHECK(!ae_kf_name_white(TRUE, FALSE, FALSE) && !ae_kf_name_white(TRUE, TRUE, FALSE));
	c = ae_kf_navy();
	CHECK(c.r == 3 && c.g == 24 && c.b == 58);
	c = ae_kf_tick_mine();
	CHECK(c.r == 184 && c.g == 214 && c.b == 255);
	c = ae_kf_tick_assist();
	CHECK(c.r == 40 && c.g == 148 && c.b == 254);
	c = ae_kf_grey();
	CHECK(c.r == 188 && c.g == 198 && c.b == 212);
	c = ae_kf_white();
	CHECK(c.r == 229 && c.g == 240 && c.b == 255);
	/* no amber anywhere in the feed's colours: blue always beats red */
	c = ae_kf_ice();
	CHECK(c.b >= c.r && c.b >= c.g);
	c = ae_kf_tick_assist();
	CHECK(c.b >= c.r && c.b >= c.g);
}

static void layout_test(void)
{
	struct ae_kf_parts parts;
	struct ae_kf_layout layout;
	real width, height;

	CHECK(NEAR(ae_kf_text_px(1.0f), 18.0f) && NEAR(ae_kf_text_px(0.5f), 16.0f) && NEAR(ae_kf_text_px(2.0f), 36.0f));
	CHECK(ae_kf_icon_fit(144.0f, 48.0f, 1.0f, &width, &height) && NEAR(width, 72.0f) && NEAR(height, 24.0f));
	/* a tall picture is held by the height, a wide one by the width, the shape kept */
	CHECK(ae_kf_icon_fit(100.0f, 100.0f, 1.0f, &width, &height) && NEAR(width, 24.0f) && NEAR(height, 24.0f));
	CHECK(ae_kf_icon_fit(300.0f, 30.0f, 2.0f, &width, &height) && NEAR(width, 144.0f) && NEAR(height, 14.4f));
	CHECK(!ae_kf_icon_fit(0.0f, 10.0f, 1.0f, &width, &height) && !ae_kf_icon_fit(10.0f, 10.0f, 0.0f, &width, &height));
	memset(&parts, 0, sizeof(parts));
	parts.scale = 1.0f;
	parts.has_killer = TRUE;
	parts.killer_width = 100.0f;
	parts.icon_width = 72.0f;
	parts.victim_width = 90.0f;
	ae_kf_layout(&parts, &layout);
	/* 12 inset, killer, 8, icon, 8, victim, 12 inset */
	CHECK(NEAR(layout.killer_x, 12.0f) && NEAR(layout.icon_x, 120.0f) && NEAR(layout.victim_x, 200.0f) && NEAR(layout.width, 302.0f));
	parts.has_mark = TRUE;
	parts.mark_width = 18.0f;
	ae_kf_layout(&parts, &layout);
	CHECK(NEAR(layout.mark_x, 200.0f) && NEAR(layout.victim_x, 226.0f) && NEAR(layout.width, 328.0f));
	/* a suicide or a fall: the picture, then the name */
	parts.has_killer = FALSE;
	parts.has_mark = FALSE;
	ae_kf_layout(&parts, &layout);
	CHECK(NEAR(layout.icon_x, 12.0f) && NEAR(layout.victim_x, 92.0f) && NEAR(layout.width, 194.0f));
	/* everything scales with the view */
	parts.scale = 2.0f;
	ae_kf_layout(&parts, &layout);
	CHECK(NEAR(layout.icon_x, 24.0f) && NEAR(layout.victim_x, 24.0f + 72.0f + 16.0f));
	/* the stack: left aligned from the anchor, growing up from its bottom, a pitch of 28.5 */
	CHECK(AE_KF_ALIGN == AE_KF_ALIGN_LEFT && AE_KF_GROWS_UP == 1);
	CHECK(NEAR(ae_kf_row_left(143.0f, 300.0f), 143.0f));
	CHECK(NEAR(ae_kf_slot_top(800.0f, 0.0f, 1.0f), 773.5f) && NEAR(ae_kf_slot_top(800.0f, 2.0f, 1.0f), 716.5f));
	CHECK(NEAR(ae_kf_slot_top(800.0f, 1.5f, 2.0f), 800.0f - 53.0f - 85.5f));
	/* where it stands: the tick 13 px left of the sensor's left edge; the bottom 782 (517 with the MCC medals), at the view's scale */
	CHECK(NEAR(ae_kf_anchor_x(TRUE, 140.0f, 0.0f, 1.0f), 127.0f) && NEAR(ae_kf_anchor_x(TRUE, 280.0f, 0.0f, 2.0f), 254.0f));
	CHECK(NEAR(ae_kf_anchor_x(FALSE, 0.0f, 20.0f, 1.0f), 147.0f));
	/* (issue #9) a sensor's edge implausibly near the screen's left is no sensor's: the fallback; 2560x1440's real one stands */
	CHECK(NEAR(ae_kf_anchor_x(TRUE, 17.0f, 0.0f, 1440.0f / 1080.0f), 127.0f * 1440.0f / 1080.0f));
	CHECK(NEAR(ae_kf_anchor_x(TRUE, 219.0f, 0.0f, 1440.0f / 1080.0f), 219.0f - 13.0f * 1440.0f / 1080.0f));
	CHECK(NEAR(ae_kf_anchor_x(TRUE, 94.0f, 0.0f, 960.0f / 1080.0f), 94.0f - 13.0f * 960.0f / 1080.0f));
	/* (0.7.4) wider than 16:9: the middle column's left edge, 141 in: x 628 at 3440 x 1440, 1468 at 5120 x 1440; 16:9 and narrower: none */
	CHECK(NEAR(ae_kf_column_left(3440.0f, 1440.0f), 440.0f) && NEAR(ae_kf_column_left(5120.0f, 1440.0f), 1280.0f));
	CHECK(NEAR(ae_kf_column_left(1920.0f, 1080.0f), 0.0f) && NEAR(ae_kf_column_left(1440.0f, 1080.0f), 0.0f));
	CHECK(NEAR(ae_kf_anchor_x_column(440.0f, 1440.0f / 1080.0f), 628.0f) && NEAR(ae_kf_anchor_x_column(1280.0f, 4.0f / 3.0f), 1468.0f));
	CHECK(NEAR(ae_kf_anchor_y(FALSE, 1.0f), 782.0f) && NEAR(ae_kf_anchor_y(TRUE, 1.0f), 517.0f) && NEAR(ae_kf_anchor_y(FALSE, 0.5f), 391.0f));
	/* five rows fill from 782 up to 782 - 4 x 28.5 - 26.5 = 641.5 */
	CHECK(NEAR(ae_kf_slot_top(ae_kf_anchor_y(FALSE, 1.0f), 4.0f, 1.0f), 641.5f));
}

static int has_run(wchar_t const *text, int red, int green, int blue)
{
	for (; *text; text++)
		if (text[0] == AE_RUN_RGB && AE_RUN_IS_BYTE(text[1]) && AE_RUN_BYTE_VALUE(text[1]) == red &&
			AE_RUN_BYTE_VALUE(text[2]) == green && AE_RUN_BYTE_VALUE(text[3]) == blue)
			return 1;
	return 0;
}

static void split_test(void)
{
	wchar_t out[128], plain[128];
	struct ae_kf_split line;
	struct ae_kf_rgb white = { 229, 240, 255 }, red = { 255, 77, 77 };
	int length;

	memset(&line, 0, sizeof(line));
	line.killer = L"Kojo";
	line.victim = L"Mendoza";
	line.kind = AE_KF_KIND_KILL;
	line.weapon = AE_KF_WEAPON_SNIPER;
	line.killer_rgb = white;
	line.victim_rgb = red;
	length = ae_kf_split_line(out, 128, &line);
	CHECK(length > 0 && out[0] == AE_RUN_NOWRAP);
	CHECK(has_run(out, 229, 240, 255) && has_run(out, 255, 77, 77) && has_run(out, 188, 198, 212) && !has_run(out, 184, 214, 255));
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"Kojo SNIPER Mendoza"));
	/* a head shot, a perfect kill (in place of it) and your own line in ice */
	line.headshot = TRUE;
	ae_kf_split_line(out, 128, &line);
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"Kojo SNIPER HEADSHOT Mendoza"));
	line.perfect = TRUE;
	ae_kf_split_line(out, 128, &line);
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"Kojo SNIPER PERFECT Mendoza"));
	line.mine = TRUE;
	ae_kf_split_line(out, 128, &line);
	CHECK(has_run(out, 184, 214, 255) && !has_run(out, 188, 198, 212));
	/* a suicide and a fall */
	line.kind = AE_KF_KIND_SUICIDE;
	ae_kf_split_line(out, 128, &line);
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"Mendoza SUICIDE"));
	line.kind = AE_KF_KIND_ENVIRONMENT;
	line.weapon = AE_KF_WEAPON_FALL;
	ae_kf_split_line(out, 128, &line);
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"FALL Mendoza"));
	/* a weapon not known has a mark rather than no word */
	line.kind = AE_KF_KIND_KILL;
	line.weapon = AE_KF_WEAPON_UNKNOWN;
	line.headshot = line.perfect = FALSE;
	ae_kf_split_line(out, 128, &line);
	wcscpy(plain, out);
	ae_feed_strip_runs(plain);
	CHECK(!wcscmp(plain, L"Kojo ? Mendoza"));
	/* no room: nothing, not half a line */
	CHECK(ae_kf_split_line(out, 12, &line) == 0 && out[0] == 0);
	CHECK(ae_kf_split_line(NULL, 12, &line) == 0);
}

static void message_test(void)
{
	long message;

	/* the game's six kill lines, and "killed enemy" and "killed friendly"; multi kills and sprees stay */
	for (message = 1; message <= 6; message++)
		CHECK(ae_kf_message_is_kill_line(message));
	CHECK(ae_kf_message_is_kill_line(8) && ae_kf_message_is_kill_line(13));
	CHECK(!ae_kf_message_is_kill_line(0) && !ae_kf_message_is_kill_line(7) && !ae_kf_message_is_kill_line(9));
	CHECK(!ae_kf_message_is_kill_line(10) && !ae_kf_message_is_kill_line(11) && !ae_kf_message_is_kill_line(12));
	CHECK(!ae_kf_message_is_kill_line(14) && !ae_kf_message_is_kill_line(28));
	CHECK(!ae_kf_message_is_kill_line(-1));
	/* a kill line with a score is one too; the multikill and spree lines, plain and with a score, are the medals' */
	CHECK(ae_kf_message_is_kill_line(19) && !ae_kf_message_is_medal_line(19));
	CHECK(ae_kf_message_is_medal_line(7) && ae_kf_message_is_medal_line(9) && ae_kf_message_is_medal_line(10));
	CHECK(ae_kf_message_is_medal_line(11) && ae_kf_message_is_medal_line(12));
	CHECK(ae_kf_message_is_medal_line(14) && ae_kf_message_is_medal_line(18));
	/* (every multikill and spree line, with a score too: the announcer speaks the map's line only for a player whose
	MEDALS are off, as one with them on never has the line, ae_announcer_replaces) */
	CHECK(ae_kf_message_is_medal_line(15) && ae_kf_message_is_medal_line(16) && ae_kf_message_is_medal_line(17));
	CHECK(!ae_kf_message_is_medal_line(8) && !ae_kf_message_is_medal_line(13) && !ae_kf_message_is_medal_line(20));
}

static void pickup_text_test(void)
{
	/* the feed on for a player: the game's pickup lines (ammo, grenade, weapon, powerup) are not shown; off: all of them, whole */
	CHECK(!ae_kf_pickup_text_shown(TRUE));
	CHECK(ae_kf_pickup_text_shown(FALSE));
}

/* a stack of chat lines growing up from bottom (HUD units, the view 480 high), laid out at line_height and drawn at text_scale
about bottom, as chat.c and network_chat.c draw theirs; entries[] each a count of lines, the newest first. The feed's room is kept
as the chats keep it (ae_kf_room, ae_kf_layout_y, ae_kf_entry_bottom). Returns how many entries touch the feed's five rows (or
come nearer them than AE_KF_CLEAR at scale), and how many moved */
static int chat_stack(int view_height, boolean mcc_medals, real bottom, real line_height, real text_scale,
	int const *entries, int count, int *moved)
{
	real scale = (real)view_height / 1080.0f;
	real ppu = (real)view_height / 480.0f;
	real anchor = ae_kf_anchor_y(mcc_medals, scale);
	real feed_top = ae_kf_slot_top(anchor, (real)(AE_KF_MOST - 1), scale);
	real feed_bottom = anchor;
	real room_top, room_bottom, layout_top, layout_bottom, edge = bottom;
	int index, touching = 0;

	ae_kf_room(mcc_medals, scale, &room_top, &room_bottom);
	layout_top = ae_kf_layout_y(room_top, ppu, bottom, text_scale);
	layout_bottom = ae_kf_layout_y(room_bottom, ppu, bottom, text_scale);
	*moved = 0;
	for (index = 0; index < count; index++)
	{
		real top = edge - (real)entries[index] * line_height;
		real placed = ae_kf_entry_bottom(top, edge, layout_top, layout_bottom);
		/* (as drawn: the layout scaled about the anchor, then pixels) */
		real drawn_top, drawn_bottom;

		if (placed != edge)
			(*moved)++;
		top = placed - (real)entries[index] * line_height;
		drawn_top = (bottom + (top - bottom) * text_scale) * ppu;
		drawn_bottom = (bottom + (placed - bottom) * text_scale) * ppu;
		if (drawn_top < feed_bottom + AE_KF_CLEAR * scale - 0.5f && drawn_bottom > feed_top - AE_KF_CLEAR * scale + 0.5f)
			touching++;
		edge = top;
	}
	return touching;
}

static void chat_room_test(void)
{
	/* (chat.c: the HUD font's lines (21 units: ascending 17, descending 4) at 0.8, their bottom 92 units over the view's
	bottom; network_chat.c: the same font at 0.5 from the motion sensor's top, about 388 units) */
	static int const heights[] = { 768, 1080, 1440 };
	static int const welcome[] = { 3 };
	static int const singles[] = { 1, 1, 1, 1 };
	static int const join[] = { 1, 3 };
	static int const pairs[] = { 2, 2 };
	static int const notices[] = { 3, 3 };
	int index, moved;
	real top, bottom;

	/* the rule alone: an entry below the room, or above it, keeps its place; one reaching into it is put on its top; no room,
	no change */
	CHECK(NEAR(ae_kf_entry_bottom(300.0f, 340.0f, 200.0f, 260.0f), 340.0f));
	CHECK(NEAR(ae_kf_entry_bottom(150.0f, 190.0f, 200.0f, 260.0f), 190.0f));
	CHECK(NEAR(ae_kf_entry_bottom(250.0f, 290.0f, 200.0f, 260.0f), 200.0f));
	CHECK(NEAR(ae_kf_entry_bottom(210.0f, 250.0f, 200.0f, 260.0f), 200.0f));
	CHECK(NEAR(ae_kf_entry_bottom(250.0f, 290.0f, 260.0f, 260.0f), 290.0f));
	/* the room: the five rows and AE_KF_CLEAR at scale over and under them (1080 lines: 633.5 to 790) */
	ae_kf_room(FALSE, 1.0f, &top, &bottom);
	CHECK(NEAR(top, 782.0f - AE_KF_ROW_HEIGHT - 4.0f * AE_KF_ROW_PITCH - AE_KF_CLEAR) && NEAR(bottom, 782.0f + AE_KF_CLEAR));
	/* a pixel of the view in a layout drawn at 0.8 about 388 units (1080 lines: 2.25 pixels a unit) */
	CHECK(NEAR(ae_kf_layout_y(388.0f * 2.25f, 2.25f, 388.0f, 0.8f), 388.0f));
	CHECK(NEAR(ae_kf_layout_y(380.0f * 2.25f, 2.25f, 388.0f, 0.8f), 378.0f));

	for (index = 0; index < (int)(sizeof(heights) / sizeof(heights[0])); index++)
	{
		int height = heights[index];

		/* the owner's report: a server's welcome of three lines over the feed (its top at 0.70 of the view, the feed's
		lowest row's bottom at 0.724); and the lines that join with it */
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, welcome, 1, &moved) == 0 && moved == 1);
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, join, 2, &moved) == 0);
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, singles, 4, &moved) == 0);
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, pairs, 2, &moved) == 0);
		/* a line or two under the feed keeps its place: the chat moves only for what would reach the feed */
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, singles, 2, &moved) == 0 && moved == 0);
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.8f, pairs, 1, &moved) == 0 && moved == 0);
		/* OpenCE's chat's lines (the host's notices): six rows at 0.5 from the sensor's top */
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.5f, notices, 2, &moved) == 0);
		CHECK(chat_stack(height, FALSE, 388.0f, 21.0f, 0.5f, singles, 4, &moved) == 0);
		/* with the MCC medals the feed stands higher: the four lines never reach it, nothing moves */
		CHECK(chat_stack(height, TRUE, 388.0f, 21.0f, 0.8f, singles, 4, &moved) == 0 && moved == 0);
		CHECK(chat_stack(height, TRUE, 388.0f, 21.0f, 0.8f, welcome, 1, &moved) == 0 && moved == 0);
	}
}

int main(void)
{
	weapon_test();
	life_test();
	overflow_test();
	colour_test();
	layout_test();
	split_test();
	message_test();
	pickup_text_test();
	chat_room_test();
	if (failures)
	{
		printf("ae_killfeed_rules_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("ae_killfeed_rules_test: ok\n");
	return 0;
}
