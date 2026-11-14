#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_weapon_tune_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* every key text the rules know, with the number 0 and 1 where the key has one */
static void round_trip_tests(void)
{
	static char const *const texts[] =
	{
		"trigger%d.rate", "trigger%d.spread", "trigger%d.projectiles", "trigger%d.rounds", "magazine%d.size",
		"magazine%d.total", "magazine%d.reload", "projectile%d.speed", "projectile%d.impact.damage",
	};
	static char const *const plain[] = { "melee.damage", "weapon.zoom", "weapon.autoaim" };
	char text[64], name[64];
	struct ae_tune_key key;
	int i, n;

	for (i = 0; i < 9; i++)
		for (n = 0; n < 2; n++)
		{
			snprintf(text, sizeof text, texts[i], n);
			CHECK(ae_tune_key_parse(text, &key) && key.index == n);
			ae_tune_key_name(key, name, sizeof name);
			CHECK(!strcmp(name, text));
		}
	for (i = 0; i < 3; i++)
	{
		CHECK(ae_tune_key_parse(plain[i], &key) && key.index == 0);
		ae_tune_key_name(key, name, sizeof name);
		CHECK(!strcmp(name, plain[i]));
	}
}

static void refusal_tests(void)
{
	struct ae_tune_key key;

	CHECK(!ae_tune_key_parse("trigger.rate", &key));
	CHECK(!ae_tune_key_parse("trigger-1.rate", &key));
	CHECK(!ae_tune_key_parse("magazine9x.size", &key));
	CHECK(!ae_tune_key_parse("weapon.bogus", &key));
	CHECK(!ae_tune_key_parse("", &key));
	CHECK(!ae_tune_key_parse(NULL, &key));
	CHECK(!ae_tune_key_parse("trigger0.rate ", &key));
	CHECK(!ae_tune_key_parse("melee0.damage", &key));
	CHECK(!ae_tune_key_parse("trigger10.rate", &key));
}

static void path_tests(void)
{
	struct ae_tune_key key;
	char path[96];

	CHECK(ae_tune_key_parse("trigger1.rate", &key) && ae_tune_invader_path(key, path, sizeof path) &&
		!strcmp(path, "triggers[1].maximum_rate_of_fire"));
	CHECK(ae_tune_key_parse("magazine0.size", &key) && ae_tune_invader_path(key, path, sizeof path) &&
		!strcmp(path, "magazines[0].rounds_loaded_maximum"));
	CHECK(ae_tune_key_parse("trigger0.spread", &key) && ae_tune_invader_path(key, path, sizeof path) &&
		!strcmp(path, "triggers[0].error_angle"));
	CHECK(ae_tune_key_parse("projectile0.impact.damage", &key) && ae_tune_invader_path(key, path, sizeof path) &&
		!strcmp(path, "damage_upper_bound") && ae_tune_key_tag_kind(key) == AE_TUNE_IN_IMPACT_EFFECT);
	CHECK(ae_tune_key_parse("melee.damage", &key) && ae_tune_key_tag_kind(key) == AE_TUNE_IN_MELEE_EFFECT);
	CHECK(ae_tune_key_parse("projectile0.speed", &key) && ae_tune_key_tag_kind(key) == AE_TUNE_IN_PROJECTILE &&
		ae_tune_invader_path(key, path, sizeof path) && !strcmp(path, "initial_velocity") &&
		ae_tune_invader_path_second(key, path, sizeof path) && !strcmp(path, "final_velocity"));
	CHECK(ae_tune_key_parse("weapon.autoaim", &key) && !ae_tune_invader_path_second(key, path, sizeof path) &&
		path[0] == 0);
	/* (a path that does not fit is cut, never overrun) */
	CHECK(ae_tune_key_parse("magazine1.size", &key) && ae_tune_invader_path(key, path, 8) && strlen(path) == 7);
}

static void value_tests(void)
{
	struct ae_tune_key key;
	struct ae_tune_value value;
	char text[64];

	CHECK(ae_tune_key_parse("magazine0.size", &key) && !ae_tune_value_allowed(key, -1.0));
	CHECK(ae_tune_value_allowed(key, 0.0) && ae_tune_value_allowed(key, 60.0) && !ae_tune_value_allowed(key, 40000.0));
	CHECK(!ae_tune_value_allowed(key, 2.5));
	CHECK(!ae_tune_value_allowed(key, NAN) && !ae_tune_value_allowed(key, INFINITY) &&
		!ae_tune_value_allowed(key, -INFINITY));
	CHECK(ae_tune_key_parse("trigger0.rate", &key) && ae_tune_value_allowed(key, 0.0) &&
		!ae_tune_value_allowed(key, -0.5) && !ae_tune_value_allowed(key, NAN));
	CHECK(ae_tune_key_parse("projectile0.speed", &key) && ae_tune_value_allowed(key, 1e6) &&
		!ae_tune_value_allowed(key, -1.0) && !ae_tune_value_allowed(key, 1e39));
	/* (a damage may be anything a float holds: one-shot weapons are allowed) */
	CHECK(ae_tune_key_parse("projectile0.impact.damage", &key) && ae_tune_value_allowed(key, 5000.0) &&
		ae_tune_value_allowed(key, -10.0));

	CHECK(ae_tune_key_parse("trigger0.rate", &key) && ae_tune_value_parse(key, "9", &value) && value.low == 9.0 &&
		value.high == 9.0);
	CHECK(ae_tune_value_parse(key, "6..9", &value) && value.low == 6.0 && value.high == 9.0);
	CHECK(ae_tune_value_parse(key, "-1.5..2", &value) && value.low == -1.5);
	CHECK(!ae_tune_value_parse(key, "6..", &value) && !ae_tune_value_parse(key, "..9", &value) &&
		!ae_tune_value_parse(key, "", &value) && !ae_tune_value_parse(key, "abc", &value) &&
		!ae_tune_value_parse(key, "9 ", &value) && !ae_tune_value_parse(key, "0x10", &value));
	ae_tune_value_text(key, value = (struct ae_tune_value){ 6.0, 9.5 }, text, sizeof text);
	CHECK(!strcmp(text, "6 9.5"));
	/* (a key with one number takes no pair) */
	CHECK(ae_tune_key_parse("magazine0.size", &key) && !ae_tune_value_parse(key, "1..2", &value) &&
		ae_tune_value_parse(key, "30", &value));
	ae_tune_value_text(key, value, text, sizeof text);
	CHECK(!strcmp(text, "30"));
}


/* a weapon's key comes from a map: whatever it holds, the file name is a name inside the tune folder */
static void name_tests(void)
{
	char out[AE_TUNE_NAME_MOST + 8];
	char small[6];
	char const *hostile[] = { "../../etc/passwd", "..", ".", "a/b\\c", "x\"y", "Assault Rifle", "", "\x01\x02", "caf\xc3\xa9" };
	int i;

	ae_tune_safe_name("ar_silenced", out, sizeof out);
	CHECK(!strcmp(out, "ar_silenced"));
	ae_tune_safe_name("Assault Rifle", out, sizeof out);
	CHECK(!strcmp(out, "assault_rifle"));
	ae_tune_safe_name("../x", out, sizeof out);
	CHECK(!strcmp(out, "___x"));
	ae_tune_safe_name("", out, sizeof out);
	CHECK(!strcmp(out, "weapon"));
	for (i = 0; i < (int)(sizeof hostile / sizeof hostile[0]); i++)
	{
		char const *at;

		ae_tune_safe_name(hostile[i], out, sizeof out);
		CHECK(out[0] != 0);
		for (at = out; *at; at++)
			CHECK((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_' || *at == '-');
	}
	{
		char long_text[300];

		memset(long_text, 'a', sizeof long_text - 1);
		long_text[sizeof long_text - 1] = 0;
		ae_tune_safe_name(long_text, out, sizeof out);
		CHECK(strlen(out) == AE_TUNE_NAME_MOST);
	}
	ae_tune_safe_name("abcdefghij", small, sizeof small);
	CHECK(strlen(small) == 5);
}

static void json_tests(void)
{
	char out[64], tiny[8];

	CHECK(ae_tune_json_escape("plain", out, sizeof out) && !strcmp(out, "plain"));
	CHECK(ae_tune_json_escape("a\"b\\c", out, sizeof out) && !strcmp(out, "a\\\"b\\\\c"));
	CHECK(ae_tune_json_escape("\n\t\x01", out, sizeof out) && !strcmp(out, "\\u000a\\u0009\\u0001"));
	CHECK(ae_tune_json_escape("\xc3\xa9", out, sizeof out) && !strcmp(out, "\\u00c3\\u00a9"));
	/* (too long: refused, whole escapes only, ended) */
	CHECK(!ae_tune_json_escape("\"\"\"\"\"\"", tiny, sizeof tiny) && strlen(tiny) == 6 && tiny[0] == '\\');
	CHECK(!ae_tune_json_escape("x", tiny, 1) && tiny[0] == 0);
}

/* two weapons share one projectile: the one original is kept, whoever changed it first */
static void origin_tests(void)
{
	static struct ae_tune_origins table;
	double a, b;

	memset(&table, 0, sizeof table);
	CHECK(ae_tune_origin_keep(&table, 1000, 324.0, 324.0, &a, &b) && a == 324.0 && b == 324.0);
	/* (the second change reads the memory's changed number 600: the original stays 324) */
	CHECK(ae_tune_origin_keep(&table, 1000, 600.0, 600.0, &a, &b) && a == 324.0 && b == 324.0 && table.count == 1);
	CHECK(ae_tune_origin_keep(&table, 2000, 10.0, 12.0, &a, &b) && a == 10.0 && b == 12.0 && table.count == 2);
	ae_tune_origin_drop(&table, 1000);
	CHECK(table.count == 1);
	/* (dropped: the next change keeps its own) */
	CHECK(ae_tune_origin_keep(&table, 1000, 5.0, 5.0, &a, &b) && a == 5.0);
	ae_tune_origin_drop(&table, 77);
	CHECK(table.count == 2);
	{
		int i, ok = 1;

		memset(&table, 0, sizeof table);
		for (i = 0; i < AE_TUNE_ORIGINS_MOST; i++)
			ok &= ae_tune_origin_keep(&table, 100 + i, 1, 1, &a, &b);
		CHECK(ok && !ae_tune_origin_keep(&table, 99999, 1, 1, &a, &b));
	}
}

int main(void)
{
	round_trip_tests();
	refusal_tests();
	path_tests();
	value_tests();
	name_tests();
	json_tests();
	origin_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_weapon_tune_rules_test: ok\n");
	return failures != 0;
}
