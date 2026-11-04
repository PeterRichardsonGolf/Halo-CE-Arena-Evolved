#include <stdio.h>
#include <string.h>
#include "ae_h2_presets_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_h2_preset const *named(char const *stored)
{
	return ae_h2_preset_get(ae_h2_preset_find(stored));
}

int main(void)
{
	int a, b;
	struct ae_variant_ext ext;

	CHECK(ae_h2_preset_count() == AE_H2_PRESET_COUNT);
	for (a = 0; a < ae_h2_preset_count(); a++)
	{
		struct ae_h2_preset const *p = ae_h2_preset_get(a);

		CHECK(strlen(p->stored) <= 11 && strlen(p->display) <= 31);
		CHECK(p->merge || (p->base[0] && p->style[0] && p->group[0]));
		CHECK(p->merge || !strncmp(p->stored, "AE ", 3));
		CHECK(p->merge || (!strstr(p->display, "H2") && !strstr(p->display, "HALO") && !strncmp(p->display, "AE ", 3)));
		CHECK(ae_h2_preset_find(p->stored) == a);
		CHECK(p->engine <= AE_H2_ENGINE_TERRITORIES);
		memcpy(&ext, p->ext, sizeof(ext));
		CHECK(ae_variant_ext_sanitize(&ext));
		for (b = a + 1; b < ae_h2_preset_count(); b++)
			CHECK(strcmp(p->stored, ae_h2_preset_get(b)->stored) && (p->merge || ae_h2_preset_get(b)->merge || strcmp(p->display, ae_h2_preset_get(b)->display)));
	}
	CHECK(ae_h2_preset_find("ae crazykX") >= 0 && ae_h2_preset_find("TEAM SLAYER") == -1 && ae_h2_preset_find(0) == -1);

	/* the presets' own rules */
	ae_h2_preset_ext(ae_h2_preset_find("AE TERRIT"), &ext);
	CHECK(ext.mode == AE_EXT_MODE_TERRITORIES && ext.territories == 3);
	ae_h2_preset_ext(ae_h2_preset_find("AE CRAZYKX"), &ext);
	CHECK(ext.king_move_10s == 6 && (named("AE CRAZYKX")->twist & AE_H2_TWIST_CRAZY));
	ae_h2_preset_ext(ae_h2_preset_find("AE C ASSLT"), &ext);
	CHECK(ext.bomb_type == AE_EXT_OBJECTIVE_NEUTRAL && ext.mode == AE_EXT_MODE_ASSAULT && named("AE C ASSLT")->arm_seconds == 5 && named("AE C ASSLT")->score == 3 && !strcmp(named("AE C ASSLT")->base, "AE BOMB"));
	ae_h2_preset_ext(ae_h2_preset_find("AE ELIMX"), &ext);
	CHECK(ext.rounds == AE_EXT_ROUNDS_BEST_OF_5 && ext.round_time_10s == 12 && named("AE ELIMX")->lives == 1);
	ae_h2_preset_ext(ae_h2_preset_find("AE CTF CLAS"), &ext);
	CHECK((ext.ctf_flags & AE_EXT_CTF_TOUCH_RETURN) && named("AE CTF CLAS")->flag_at_home);
	ae_h2_preset_ext(ae_h2_preset_find("AE 1 FLAG"), &ext);
	CHECK(ext.ctf_flag_type == AE_EXT_OBJECTIVE_SINGLE && named("AE 1 FLAG")->single_flag_seconds == 180);
	/* the merges: an existing seed's block only; the comp king moves its hill every minute */
	ae_h2_preset_ext(ae_h2_preset_find("AE COMP KOH"), &ext);
	CHECK(named("AE COMP KOH")->merge && ext.king_move_10s == 6);
	CHECK(named("AE KING")->merge && (named("AE KING")->ext[9] & AE_EXT_KING_UNCONTESTED));
	CHECK(ae_h2_preset_find("EV ROCKBALL") == ae_h2_preset_find("AE ROCKBALL") && ae_h2_preset_find("BR ROCKBALL") >= 0);
	/* a preset keeps its base seed's rules: only what its own settings change is set */
	CHECK(!(named("AE TERRIT")->twist & AE_H2_TWIST_RESPAWN) && !strcmp(named("AE TERRIT")->base, "AE KING"));
	CHECK(!strcmp(named("AE C TERRIT")->base, "AE COMP KOH") && !strcmp(named("AE C TERRIT")->style, "AE COMP"));
	CHECK(!named("AE ROCKETSX")->teams && named("AE TM PHANT")->teams && !strcmp(named("AE TM PHANT")->base, "AE TEAM SLY"));
	CHECK(named("AE ROCKETSX")->primary == 6 && named("AE SNIPERSX")->primary == 5 && named("AE SWORDS")->no_vehicles);
	CHECK(named("AE NINJANT")->engine == AE_H2_ENGINE_JUGGERNAUT && named("AE 2 ON 1")->ext[5] == 3);
	printf(failures ? "FAILED\n" : "PASS\n");
	return failures != 0;
}
