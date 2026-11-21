#include <stdio.h>
#include <string.h>
#include "ae_biped_pack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_biped_facts stock_facts(void)
{
	struct ae_biped_facts f;

	memset(&f, 0, sizeof(f));
	f.radius = 0.1f;
	f.standing_height = 0.7f;
	f.standing_eye = 0.62f;
	f.crouching_eye = 0.35f;
	f.body_vitality = 75.0f;
	f.shield_vitality = 75.0f;
	f.has_animation_graph = 1;
	f.uses_player_physics = 1;
	f.labels_checked = 9;
	f.seats_stock = 6;
	return f;
}

int main(void)
{
	struct ae_biped_facts stock = stock_facts(), b = stock_facts();
	char reason[AE_BIPED_PACK_REASON_SIZE], text[256], buffer[64];

	/* slots */
	CHECK(!strcmp(ae_biped_pack_id(1), "ae_biped_1") && !strcmp(ae_biped_pack_id(4), "ae_biped_4"));
	CHECK(!ae_biped_pack_id(0) && !ae_biped_pack_id(5));
	CHECK(ae_biped_pack_slot_of_id("ae_biped_3") == 3 && ae_biped_pack_slot_of_id("ae_biped_5") == 0 &&
		ae_biped_pack_slot_of_id(NULL) == 0 && ae_biped_pack_slot_of_id("ae_smoke") == 0);
	CHECK(ae_biped_pack_slot_bit(1) == 1 && ae_biped_pack_slot_bit(4) == 8 && ae_biped_pack_slot_bit(5) == 0);
	CHECK(ae_biped_model_sanitize(0) == 0 && ae_biped_model_sanitize(4) == 4 && ae_biped_model_sanitize(5) == 0 &&
		ae_biped_model_sanitize(-1) == 0 && ae_biped_model_sanitize(255) == 0);
	/* the attach mask: the host adds every pack it has when the game names a model, never otherwise; a joined machine
	always (it cannot know before its map is loaded) */
	CHECK(ae_biped_pack_attach_mask(0, 0, 0xF) == 0);
	CHECK(ae_biped_pack_attach_mask(0, 2, 0x3) == 0x3);
	CHECK(ae_biped_pack_attach_mask(0, 9, 0x3) == 0);
	CHECK(ae_biped_pack_attach_mask(1, 0, 0x5) == 0x5);
	CHECK(ae_biped_pack_attach_mask(1, 0, 0xFF) == 0xF);
	/* offered only where the game's rules are the casual ones */
	CHECK(ae_biped_pack_offered(0, 0, 0, 0));
	CHECK(!ae_biped_pack_offered(1, 0, 0, 0) && !ae_biped_pack_offered(0, 1, 0, 0) && !ae_biped_pack_offered(0, 0, 1, 0) &&
		!ae_biped_pack_offered(0, 0, 0, 1));
	/* the join: a game without a model refuses nobody; with one, a machine whose slots differ, or a plain machine */
	CHECK(!ae_biped_pack_join_refused(0, 0x3, 0, 0));
	CHECK(!ae_biped_pack_join_refused(1, 0x3, 0x3, 1));
	CHECK(ae_biped_pack_join_refused(1, 0x3, 0x1, 1) && ae_biped_pack_join_refused(1, 0x3, 0x7, 1));
	CHECK(ae_biped_pack_join_refused(1, 0x3, 0, 1) && ae_biped_pack_join_refused(1, 0x3, 0x3, 0));
	CHECK(ae_biped_pack_join_bits(0x3) == 0x30 && ae_biped_pack_join_told(0xAF31) == 3 && ae_biped_pack_join_told(0xAF01) == 0);
	CHECK((ae_biped_pack_join_bits(0xFF) & 0x0F) == 0);
	/* fitness: the stock sized biped that holds everything is fit */
	CHECK(ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && !reason[0]);
	/* a graph without a weapon label: the weapon would not be drawn */
	b = stock_facts(); b.labels_missing = 3; strcpy(b.label_first, "ar");
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "3 of the map's 9") && strstr(reason, "'ar'"));
	/* vehicle seats */
	b = stock_facts(); b.seats_missing = 1; strcpy(b.seat_first, "W-driver");
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "could not ride") && strstr(reason, "W-driver"));
	/* physics, graph */
	b = stock_facts(); b.uses_player_physics = 0;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "physics"));
	b = stock_facts(); b.has_animation_graph = 0;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "no animation graph"));
	/* sizes: the bounds are inclusive, just past them is refused */
	b = stock_facts(); b.radius = stock.radius * 1.25f;
	CHECK(ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)));
	b.radius = stock.radius * 1.26f;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "collision radius"));
	b = stock_facts(); b.standing_height = stock.standing_height * 0.79f;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "standing height"));
	b = stock_facts(); b.standing_eye = stock.standing_eye * 1.3f;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "standing eye"));
	b = stock_facts(); b.crouching_eye = 0.0f;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "crouching eye"));
	/* every failure, told together in the order labels, seats, sizes */
	b = stock_facts(); b.labels_missing = 1; b.radius = 1.0f; b.uses_player_physics = 0;
	CHECK(!ae_biped_pack_fitness(&stock, &b, reason, sizeof(reason)) && strstr(reason, "physics") && strstr(reason, "; it cannot hold") &&
		strstr(reason, "; collision radius") && strstr(reason, "weapon labels") < strstr(reason, "collision radius"));
	CHECK(!ae_biped_pack_fitness(NULL, &b, reason, sizeof(reason)) && strstr(reason, "not known"));
	/* vitality: both told */
	b = stock_facts(); b.shield_vitality = 130.0f; b.body_vitality = 20.0f;
	ae_biped_pack_vitality_text(&stock, &b, text, sizeof(text));
	CHECK(strstr(text, "shield 130 body 20") && strstr(text, "75 / 75"));
	/* limits */
	CHECK(!ae_biped_pack_limits_reason(400, 8 * 1024 * 1024, buffer, sizeof(buffer)));
	CHECK(ae_biped_pack_limits_reason(789, 1000, buffer, sizeof(buffer)) && strstr(buffer, "too many own tags (789"));
	CHECK(ae_biped_pack_limits_reason(10, 9 * 1024 * 1024, buffer, sizeof(buffer)) && strstr(buffer, "own bytes"));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_biped_pack_rules_test: all passed\n");
	return failures != 0;
}
