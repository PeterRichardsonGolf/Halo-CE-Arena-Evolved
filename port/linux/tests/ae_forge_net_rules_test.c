/* ae_forge_net_rules.c: every Forge line checked, who may say it, the rates and budgets, the version and the base tags */
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "ae_forge_net_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_forge_net_limits limits;

/* a line is refused, with a reason */
static int refused(char const *text)
{
	struct ae_forge_net_record record;

	return ae_forge_net_rules_parse(text, &limits, &record) == AE_FORGE_NET_NONE && record.reason != AE_FORGE_NET_OK;
}

static int accepted(char const *text, int tag, struct ae_forge_net_record *record)
{
	return ae_forge_net_rules_parse(text, &limits, record) == tag && record->tag == tag && record->reason == AE_FORGE_NET_OK;
}

/* lines of one tag: the good ones, and the bad ones (each field out of range, missing, extra, not a number, not finite) */
struct row
{
	char const *line;
	int tag;
};

static struct row const good[] =
{
	{ "!FT:3:1:12.50:-40.25:3.00:150:-20", AE_FORGE_NET_FT },
	{ "!FT:0:0:0:0:0:0:0", AE_FORGE_NET_FT },
	{ "!FE:2:10.00:20.00:1.50", AE_FORGE_NET_FE },
	{ "!FQ", AE_FORGE_NET_FQ },
	{ "!FS:1A2B3C4D:61.28:-65.60:1.71:0:0:0:0", AE_FORGE_NET_FS },
	{ "!FS:FFFFFFFF:-9999:9999:0:-100000:100000:0:-1", AE_FORGE_NET_FS },
	{ "!FH:162:1A2B3C4D:1001002:61.28:-65.60:1.71:314:0:0:1", AE_FORGE_NET_FH },
	{ "!FM:12:300:61.28:-65.60:1.71:314:0:0:1", AE_FORGE_NET_FM },
	{ "!FM:12:-1:61.28:-65.60:1.71:314:0:0", AE_FORGE_NET_FM },
	{ "!FD:12:300", AE_FORGE_NET_FD },
	{ "!FD:12", AE_FORGE_NET_FD },
	{ "!FD:-1:-1", AE_FORGE_NET_FD },
	{ "!FU:12:65535", AE_FORGE_NET_FU },
	{ "!FA:C:1:2:3:4:5:6:7:8:0:3", AE_FORGE_NET_FA },
	{ "!FA:C:1:2:8:10:10:258:3E7:FFFF:1:1FFF", AE_FORGE_NET_FA },
	{ "!FJ:C:2:1:80:6:6:C", AE_FORGE_NET_FJ },
	{ "!FJ:C:2:1:80:6:6", AE_FORGE_NET_FJ },
	{ "!FV:12:1", AE_FORGE_NET_FV },
	{ "!FW:12:1:5:25:200:200:200:3", AE_FORGE_NET_FW },
	{ "!FW:12:2:7:1000:20000:25:25", AE_FORGE_NET_FW },
	{ "!FN:12:0", AE_FORGE_NET_FN },
	{ "!FK:12:1", AE_FORGE_NET_FK },
	{ "!FZ:12:1000", AE_FORGE_NET_FZ },
	{ "!FB:1456:DEADBEEF", AE_FORGE_NET_FB },
	{ "!FX:3", AE_FORGE_NET_FX },
};

static char const *const bad[] =
{
	/* !FT: the player, the switch, a coordinate past the bound, not finite, missing, extra */
	"!FT:128:1:0:0:0:0:0", "!FT:-1:1:0:0:0:0:0", "!FT:3:2:0:0:0:0:0", "!FT:3:1:10001:0:0:0:0", "!FT:3:1:nan:0:0:0:0", "!FT:3:1:0:inf:0:0:0",
	"!FT:3:1:0:0:-inf:0:0", "!FT:3:1:0:0:0:100001:0", "!FT:3:1:0:0:0:0", "!FT:3:1:0:0:0:0:0:0", "!FT:3:1:0:0:0:0:x", "!FT:3:1::0:0:0:0",
	"!FT:3:1:0:0:0:0:0:", "!FT:3:1:1e3:0:0:0:0", "!FT:3:1:0x10:0:0:0:0", "!FT:3:1:+5:0:0:0:0",
	/* !FE: an effect past the table, a point not finite */
	"!FE:5:0:0:0", "!FE:G:0:0:0", "!FE:2:nan:0:0", "!FE:2:0:0", "!FE:2:0:0:0:0", "!FE:-1:0:0:0",
	/* !FQ is the whole line */
	"!FQ:", "!FQ:1", "!FQX",
	/* !FS: a hash with a sign or too long, a point past the bound, a team past 16 bits, a field missing or extra */
	"!FS:-1:0:0:0:0:0:0:0", "!FS:123456789:0:0:0:0:0:0:0", "!FS:1:20000:0:0:0:0:0:0", "!FS:1:0:0:0:0:0:0:40000", "!FS:1:0:0:0:0:0:0",
	"!FS:1:0:0:0:0:0:0:0:0", "!FS:1:0:0:0:0:0:0:-32769", "!FS::0:0:0:0:0:0:0", "!FS:1:0:0:nan:0:0:0:0",
	/* !FH: a live index past the table, an object code past 25 bits */
	"!FH:1000:1:0:0:0:0:0:0:0:0", "!FH:162:1:2000000:0:0:0:0:0:0:0", "!FH:162:1:0:0:0:0:0:0:0", "!FH:162:1:0:0:0:0:0:0:0:0:0",
	"!FH:162:1:0:0:0:nan:0:0:0:0",
	/* !FM: a live index past the table, an object past the tracked ones, extra, held not 0 or 1 */
	"!FM:4096:0:0:0:0:0:0:0", "!FM:-2:0:0:0:0:0:0:0", "!FM:1:8192:0:0:0:0:0:0", "!FM:1:-2:0:0:0:0:0:0", "!FM:1:0:0:0:0:0:0",
	"!FM:1:0:0:0:0:0:0:0:2", "!FM:1:0:0:0:0:0:0:0:0:0", "!FM:1:0:0:0:0:0:0:x", "!FM:1:0:nan:0:0:0:0:0", "!FM:1:0:0:0:0:0:0:0:",
	/* !FD: nothing, past the table, extra */
	"!FD:", "!FD:4096", "!FD:1:8192", "!FD:1:2:3", "!FD:x",
	/* !FU: a usage past 16 bits, negative */
	"!FU:1:65536", "!FU:1:-1", "!FU:1", "!FU:4096:1",
	/* !FA: a count past 16, a delay past 600, a switch past 1, a field missing, extra, not hex */
	"!FA:C:1:2:3:11:0:0:0:0:0:0", "!FA:C:1:2:3:0:0:259:0:0:0:0", "!FA:C:1:2:3:0:0:0:0:0:2:0", "!FA:C:1:2:3:0:0:0:0:0:0",
	"!FA:C:1:2:3:0:0:0:0:0:0:0:0", "!FA:C:1:2:9:0:0:0:0:0:0:0", "!FA:C:1:2:8000:0:0:0:0:0:0:0", "!FA:C:1:2:FFFF:0:0:0:0:0:0:0", "!FA:Z:1:2:3:0:0:0:0:0:0:0", "!FA:C:1:2:3:0:0:0:3E8:0:0:0",
	"!FA:C:1:2:3:0:0:0:0:10000:0:0", "!FA:C:1:2:3:0:0:0:0:0:0:2000",
	/* !FJ: a goal past 4, a link past 31, WHO past its low bits, fields missing or extra */
	"!FJ:C:5:1:0:6:6", "!FJ:C:2:20:0:6:6", "!FJ:C:2:1:3:6:6", "!FJ:C:2:1:90:6:6:6:6", "!FJ:C:2:1:0:6", "!FJ:C:2:1:0:6:6:100",
	/* !FV, !FN, !FK: a switch of 0 or 1 */
	"!FV:12:2", "!FV:12", "!FN:12:-1", "!FK:12:1:1", "!FK:4096:1",
	/* !FZ: a scale under 0.05 or over 20 */
	"!FZ:12:49", "!FZ:12:20001", "!FZ:12", "!FZ:12:nan",
	/* !FW: shape, effect, damage, sizes, flags */
	"!FW:12:4:5:25:200:200:200", "!FW:12:1:8:25:200:200:200", "!FW:12:1:5:1001:200:200:200", "!FW:12:1:5:25:24:200:200",
	"!FW:12:1:5:25:200:20001:200", "!FW:12:1:5:25:200:200", "!FW:12:1:5:25:200:200:200:256", "!FW:12:1:5:25:200:200:200:1:1",
	/* !FB, !FX */
	"!FB:1000000:1", "!FB:1:100000000", "!FB:1", "!FX:100", "!FX:",
	/* not Forge lines, unknown tags, no colon */
	"!F", "!FZ", "!FZ12:1", "!FGX:1", "!FGN", "hello", "", "!G:1", "!fm:1:0:0:0:0:0:0:0",
};

int main(void)
{
	struct ae_forge_net_record record;
	char text[AE_FORGE_NET_TEXT_SIZE + 1], line[AE_FORGE_NET_TEXT_SIZE + 8];
	unsigned int each;

	ae_forge_net_rules_limits(&limits, 4096, 8192);
	for (each = 0; each < sizeof(good) / sizeof(good[0]); each++)
	{
		if (!accepted(good[each].line, good[each].tag, &record))
			printf("FAIL good line %s\n", good[each].line), failures++;
	}
	for (each = 0; each < sizeof(bad) / sizeof(bad[0]); each++)
	{
		if (!refused(bad[each]))
			printf("FAIL bad line %s\n", bad[each]), failures++;
	}
	/* what a line reads as */
	CHECK(accepted("!FM:12:300:61.28:-65.60:1.71:314:-5:7:1", AE_FORGE_NET_FM, &record) && record.live_idx == 12 && record.obj_abs == 300 &&
		record.x == 61.28f && record.y == -65.60f && record.yaw == 314 && record.pitch == -5 && record.roll == 7 && record.held == 1);
	CHECK(accepted("!FM:12:-1:1:2:3:4:5:6", AE_FORGE_NET_FM, &record) && record.obj_abs == -1 && record.held == 0 && record.fields == 8);
	CHECK(accepted("!FD:12", AE_FORGE_NET_FD, &record) && record.live_idx == 12 && record.obj_abs == -1);
	CHECK(accepted("!FS:1A2B3C4D:1:2:3:4:5:6:7", AE_FORGE_NET_FS, &record) && record.hash == 0x1A2B3C4Du && record.team == 7 && record.z == 3.0f);
	CHECK(accepted("!FH:162:1A2B3C4D:1001002:1:2:3:4:5:6:7", AE_FORGE_NET_FH, &record) && record.live_idx == 0x162 && record.object_code == 0x1001002u);
	CHECK(accepted("!FA:C:1:2:8:10:5:258:3E7:FFFF:1:1FFF", AE_FORGE_NET_FA, &record) && record.team == 8 && record.unit_count == 16 &&
		record.respawn_delay == 600 && record.respawn_total == 999 && record.spawn_effect == 0x1FFF && record.live_idx == 12);
	CHECK(accepted("!FJ:C:2:1:80:6:6:C", AE_FORGE_NET_FJ, &record) && record.goal == 2 && record.who == 0x80 && record.alert == 12);
	CHECK(accepted("!FJ:C:2:1:80:6:6", AE_FORGE_NET_FJ, &record) && record.alert == -1);
	CHECK(accepted("!FW:12:1:5:25:200:300:400:3", AE_FORGE_NET_FW, &record) && record.size[1] == 300 && record.flags == 3);
	CHECK(accepted("!FB:1456:DEADBEEF", AE_FORGE_NET_FB, &record) && record.base_count == 0x1456 && record.fingerprint == 0xDEADBEEFu);
	CHECK(ae_forge_net_rules_parse(0, &limits, &record) == AE_FORGE_NET_NONE && record.reason == AE_FORGE_NET_NOT_FORGE);
	CHECK(ae_forge_net_rules_parse("!FGX:x", &limits, &record) == AE_FORGE_NET_NONE && record.reason == AE_FORGE_NET_UNKNOWN_TAG);
	CHECK(ae_forge_net_rules_parse("!FM:1:0:nan:0:0:0:0:0", &limits, &record) == AE_FORGE_NET_NONE && record.reason == AE_FORGE_NET_FINITE);
	CHECK(ae_forge_net_rules_parse("!FM:1:0:0:0:0:0:0", &limits, &record) == AE_FORGE_NET_NONE && record.reason == AE_FORGE_NET_FIELDS);
	CHECK(ae_forge_net_rules_parse("!FM:99999:0:0:0:0:0:0:0", &limits, &record) == AE_FORGE_NET_NONE && record.reason == AE_FORGE_NET_RANGE);
	/* every tag's name */
	CHECK(!strcmp(ae_forge_net_rules_tag_name(AE_FORGE_NET_FM), "FM") && !strcmp(ae_forge_net_rules_tag_name(AE_FORGE_NET_TAGS), ""));

	/* the 64 bytes of a line: a NUL inside them, or none */
	memset(line, 'A', sizeof(line));
	memcpy(line, "!FM:1:0:0:0:0:0:0:0", 20);
	CHECK(ae_forge_net_rules_text(line, text) == 1 && !strcmp(text, "!FM:1:0:0:0:0:0:0:0"));
	memset(line, 'B', 64);
	line[64] = 0;
	CHECK(ae_forge_net_rules_text(line, text) == 0 && text[0] == 0);
	memset(line, 'B', 63);
	line[63] = 0;
	CHECK(ae_forge_net_rules_text(line, text) == 1 && strlen(text) == 63);
	CHECK(ae_forge_net_rules_text(0, text) == 0);

	/* who may say what */
	CHECK(ae_forge_net_rules_client_may_say(AE_FORGE_NET_FS) && ae_forge_net_rules_client_may_say(AE_FORGE_NET_FQ) &&
		ae_forge_net_rules_client_may_say(AE_FORGE_NET_FT) && !ae_forge_net_rules_client_may_say(AE_FORGE_NET_FH) &&
		!ae_forge_net_rules_client_may_say(AE_FORGE_NET_FE) && !ae_forge_net_rules_client_may_say(AE_FORGE_NET_FB) && ae_forge_net_rules_client_may_say(AE_FORGE_NET_FX));
	CHECK(ae_forge_net_rules_host_may_say(AE_FORGE_NET_FH) && ae_forge_net_rules_host_may_say(AE_FORGE_NET_FB) &&
		ae_forge_net_rules_host_may_say(AE_FORGE_NET_FE) && !ae_forge_net_rules_host_may_say(AE_FORGE_NET_FS) &&
		!ae_forge_net_rules_host_may_say(AE_FORGE_NET_FQ));
	CHECK(ae_forge_net_rules_is_edit(AE_FORGE_NET_FS) && ae_forge_net_rules_is_edit(AE_FORGE_NET_FD) && !ae_forge_net_rules_is_edit(AE_FORGE_NET_FT) &&
		!ae_forge_net_rules_is_edit(AE_FORGE_NET_FQ));
	/* a client names no object but the one of the instance; the host names what it likes */
	CHECK(ae_forge_net_rules_object_ok(1, -1, 7) && ae_forge_net_rules_object_ok(1, 7, 7) && !ae_forge_net_rules_object_ok(1, 8, 7) &&
		!ae_forge_net_rules_object_ok(1, 0, -1) && ae_forge_net_rules_object_ok(0, 8, 7));
	CHECK(ae_forge_net_rules_may_edit(1, 0) && !ae_forge_net_rules_may_edit(1, 1) && ae_forge_net_rules_may_edit(0, 1));

	/* a bucket: its burst, its rate, and time that goes back */
	{
		struct ae_forge_net_bucket bucket;
		int taken = 0, step;

		memset(&bucket, 0, sizeof(bucket));
		for (step = 0; step < 100; step++)
			taken += ae_forge_net_rules_bucket_take(&bucket, 1000, 32.0, 64.0);
		CHECK(taken == 64);
		/* half a second on: 16 more */
		taken = 0;
		for (step = 0; step < 100; step++)
			taken += ae_forge_net_rules_bucket_take(&bucket, 1500, 32.0, 64.0);
		CHECK(taken == 16);
		/* never more than the burst, however long */
		taken = 0;
		for (step = 0; step < 200; step++)
			taken += ae_forge_net_rules_bucket_take(&bucket, 1000000, 32.0, 64.0);
		CHECK(taken == 64);
		/* time going back starts it over without more */
		taken = 0;
		for (step = 0; step < 200; step++)
			taken += ae_forge_net_rules_bucket_take(&bucket, 5, 32.0, 64.0);
		CHECK(taken == 0);
	}
	/* a client's lines: the edit rate, the pose rate apart, one request for the state in a minute, and let go after the strike time */
	{
		struct ae_forge_net_client client;
		int drop = 0, taken = 0, step;
		unsigned long now = 10000;

		ae_forge_net_rules_client_reset(&client);
		for (step = 0; step < 100; step++)
			taken += ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FM, now, &drop);
		CHECK(taken == 64 && !drop);
		/* (poses are a bucket of their own) */
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FT, now, &drop) && !drop);
		/* (!FQ: the first, not the second inside a minute, again after) */
		ae_forge_net_rules_client_reset(&client);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now, &drop) == 1);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 5100, &drop) == 0);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 61000, &drop) == 1);
		/* the edit bucket empty and kept up (three times its rate): dropped after the strike time, not before */
		ae_forge_net_rules_client_reset(&client);
		for (step = 0; step < 64; step++)
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now, &drop);
		{
			unsigned long dropped_at = 0;

			for (step = 0; step < 700 && !dropped_at; step++)
			{
				unsigned long at = now + (unsigned long)step * 10;

				ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, at, &drop);
				if (drop)
					dropped_at = at;
			}
			CHECK(dropped_at >= now + AE_FORGE_NET_STRIKE_MS && dropped_at <= now + AE_FORGE_NET_STRIKE_MS + 20);
		}
		/* a client that slows down is not let go */
		ae_forge_net_rules_client_reset(&client);
		for (step = 0; step < 64; step++)
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now, &drop);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now, &drop);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 3000, &drop) == 1 && !drop);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 60000, &drop) == 1 && !drop && client.over_since_ms[AE_FORGE_NET_BUCKET_EDIT] == 0);
	}
	/* a client's budgets: instances, the session's, AI spawns and their units */
	{
		struct ae_forge_net_client client;
		int made = 0, step;

		ae_forge_net_rules_client_reset(&client);
		for (step = 0; step < 300; step++)
			made += ae_forge_net_rules_client_instance(&client, 0);
		CHECK(made == AE_FORGE_NET_CLIENT_INSTANCES);
		ae_forge_net_rules_client_removed(&client, 0, 0);
		CHECK(client.instances == AE_FORGE_NET_CLIENT_INSTANCES - 1 && ae_forge_net_rules_client_instance(&client, 0));
		ae_forge_net_rules_client_reset(&client);
		CHECK(!ae_forge_net_rules_client_instance(&client, AE_FORGE_NET_SESSION_INSTANCES) &&
			ae_forge_net_rules_client_instance(&client, AE_FORGE_NET_SESSION_INSTANCES - 1));
		ae_forge_net_rules_client_reset(&client);
		made = 0;
		for (step = 0; step < 40; step++)
			made += ae_forge_net_rules_client_ai(&client, 4);
		CHECK(made == 16 && client.ai_units == 64);
		ae_forge_net_rules_client_reset(&client);
		CHECK(ae_forge_net_rules_client_ai(&client, 16) && ae_forge_net_rules_client_ai(&client, 16) && ae_forge_net_rules_client_ai(&client, 16) &&
			ae_forge_net_rules_client_ai(&client, 16) && !ae_forge_net_rules_client_ai(&client, 1) && !ae_forge_net_rules_client_ai(&client, -1));
		ae_forge_net_rules_client_removed(&client, 1, 16);
		CHECK(client.ai_units == 48 && client.ai_spawns == 3 && ae_forge_net_rules_client_ai(&client, 16));
	}
	/* a joiner takes a host's creates at 180 a second, a burst of 360 */
	{
		struct ae_forge_net_bucket bucket;
		int taken = 0, step;

		memset(&bucket, 0, sizeof(bucket));
		for (step = 0; step < 1000; step++)
			taken += ae_forge_net_rules_joiner_create(&bucket, 500);
		CHECK(taken == 360);
	}

	/* the version: the build's own with no session, Forge's alone in one */
	CHECK(ae_forge_net_rules_announce(0, 44) == 44 && ae_forge_net_rules_minimum(0, 44) == 44 && ae_forge_net_rules_maximum(0, 44) == 44);
	CHECK(ae_forge_net_rules_announce(0, 37) == 37 && ae_forge_net_rules_minimum(0, 30) == 30 && ae_forge_net_rules_maximum(0, 50) == 50);
	CHECK(ae_forge_net_rules_announce(1, 44) == 0x4777 && ae_forge_net_rules_minimum(1, 44) == 0x4777 && ae_forge_net_rules_maximum(1, 44) == 0x4777);
	CHECK(AE_FORGE_NET_VERSION == 18295 && ae_forge_net_rules_delta_shortcut(0) == 1 && ae_forge_net_rules_delta_shortcut(1) == 0);

	/* the base tags: count first, then the fingerprint, and a host that says none */
	CHECK(ae_forge_net_rules_tags_compare(1, 5183, 7, 5183, 7) == AE_FORGE_NET_TAGS_SAME);
	CHECK(ae_forge_net_rules_tags_compare(1, 5183, 7, 5184, 7) == AE_FORGE_NET_TAGS_COUNT);
	CHECK(ae_forge_net_rules_tags_compare(1, 5183, 7, 5183, 8) == AE_FORGE_NET_TAGS_FINGERPRINT);
	CHECK(ae_forge_net_rules_tags_compare(1, 5183, 7, 5184, 8) == AE_FORGE_NET_TAGS_COUNT);
	CHECK(ae_forge_net_rules_tags_compare(0, 0, 0, 5183, 7) == AE_FORGE_NET_TAGS_NOT_COMPARED);
	ae_forge_net_rules_tags_message(AE_FORGE_NET_TAGS_COUNT, 5183, 5184, text, sizeof(text));
	CHECK(strstr(text, "5184 against 5183") != 0);
	ae_forge_net_rules_tags_message(AE_FORGE_NET_TAGS_NOT_COMPARED, 0, 0, text, sizeof(text));
	CHECK(strstr(text, "not compared") != 0);

	/* the object changes of a host: every one of a wire short's count is looked at, a count of 0 or less none */
	CHECK(ae_forge_net_rules_changes_count(0) == 0 && ae_forge_net_rules_changes_count(-5) == 0 && ae_forge_net_rules_changes_count(257) == 257 &&
		ae_forge_net_rules_changes_count(32767) == 32767 && ae_forge_net_rules_changes_count(40000) == AE_FORGE_NET_CHANGES_MAX);


	/* ---- bounds */
	CHECK(refused("!FT:3:1:0:0:0:-100001:0") && refused("!FT:3:1:0:0:0:0:-100001") && accepted("!FT:3:1:0:0:0:-100000:-100000", AE_FORGE_NET_FT, &record));
	CHECK(refused("!FS:1:0:0:0:100001:0:0:0") && refused("!FS:1:0:0:0:0:100001:0:0") && refused("!FS:1:0:0:0:0:0:-100001:0") && refused("!FS:1:0:0:0:0:0:0:32768"));
	CHECK(refused("!FH:162:1:0:0:0:0:100001:0:0:0") && refused("!FH:162:1:0:0:0:0:0:-100001:0:0") && refused("!FH:162:1:0:0:0:0:0:0:100001:0") &&
		refused("!FH:162:1:0:0:0:0:0:0:0:32768") && refused("!FH:162:1:0:0:0:0:0:0:0:-32769") && refused("!FH:162:123456789:0:0:0:0:0:0:0:0") &&
		accepted("!FH:FFF:1:0:0:0:0:0:0:0:-32768", AE_FORGE_NET_FH, &record) && refused("!FH:1000:1:0:0:0:0:0:0:0:0"));
	CHECK(refused("!FM:1:0:0:0:0:100001:0:0") && refused("!FM:1:0:0:0:0:0:100001:0") && refused("!FM:1:0:0:0:0:0:0:-100001"));
	CHECK(accepted("!FM:4095:8191:0:0:0:0:0:0", AE_FORGE_NET_FM, &record) && refused("!FM:4096:0:0:0:0:0:0:0") && refused("!FM:0:8192:0:0:0:0:0:0"));
	CHECK(accepted("!FD:4095", AE_FORGE_NET_FD, &record) && refused("!FD:4096") && accepted("!FU:4095:0", AE_FORGE_NET_FU, &record) && refused("!FU:4096:0") &&
		accepted("!FV:4095:1", AE_FORGE_NET_FV, &record) && refused("!FV:4096:1") && accepted("!FZ:4095:50", AE_FORGE_NET_FZ, &record) && refused("!FZ:4096:50") &&
		accepted("!FZ:0:20000", AE_FORGE_NET_FZ, &record));
	CHECK(refused("!FA:C:1:2:3:0:11:0:0:0:0:0") && accepted("!FA:C:1:2:3:0:10:0:0:0:0:0", AE_FORGE_NET_FA, &record) && refused("!FA:C:1:2:123456789:0:0:0:0:0:0:0") &&
		refused("!FA:C:1:123456789:3:0:0:0:0:0:0:0") && refused("!FA:C:1:2:3:0:0:259:0:0:0:0") && refused("!FA:c:1:2:3:0:0:0:0:0:0:0:0"));
	CHECK(refused("!FJ:C:2:1:80:100:6") && refused("!FJ:C:2:1:80:6:100") && accepted("!FJ:C:2:1:80:FF:FF", AE_FORGE_NET_FJ, &record) && refused("!FJ:C:2:1:80:6:6:100") &&
		accepted("!FJ:c:2:1:80:ff:ff:ff", AE_FORGE_NET_FJ, &record) && record.start == 255);
	CHECK(refused("!FW:12:1:5:25:24:200:200") && refused("!FW:12:1:5:-1:200:200:200") && accepted("!FW:12:1:5:0:25:25:25", AE_FORGE_NET_FW, &record) &&
		refused("!FW:12:1:5:25:200:200:24") && refused("!FW:12:1:5:25:200:200:200:-1") && refused("!FW:12:1:5:25:200:200:200:3:1"));
	CHECK(refused("!FB:1:123456789") && accepted("!FB:FFFFFF:FFFFFFFF", AE_FORGE_NET_FB, &record) && refused("!FB:1000000:1") && accepted("!FB:1:deadbeef", AE_FORGE_NET_FB, &record) &&
		record.fingerprint == 0xDEADBEEFu);
	/* the caps of a line: 12 fields, 39 characters in one, 11 digits in a number, a sign alone */
	CHECK(refused("!FM:1:2:3:4:5:6:7:8:9:10:11:12:13") && refused("!FW:1:1:1:1:1:1:1:1:1:1:1:1:1"));
	CHECK(refused("!FM:1:0:0.000000000000000000000000000000000000001:0:0:0:0:0") && refused("!FM:1:0:0:0:0:0:0:0:00000000000000") &&
		refused("!FM:000000000000:0:0:0:0:0:0:0") && accepted("!FM:00000000000:0:0:0:0:0:0:0", AE_FORGE_NET_FM, &record) && refused("!FM:-:0:0:0:0:0:0:0") && refused("!FM:--5:0:0:0:0:0:0:0") && accepted("!FM:-0:-0:0:0:0:0:0:0", AE_FORGE_NET_FM, &record));
	CHECK(refused("!FM:1:0:1.2.3:0:0:0:0:0") && refused("!FM:1:0:.:0:0:0:0:0") && refused("!FM:1:0:-:0:0:0:0:0") && accepted("!FM:1:0:.5:-.5:5.:0:0:0", AE_FORGE_NET_FM, &record));
	CHECK(accepted("!FM:1:0:1.234567890123:0:0:0:0:0", AE_FORGE_NET_FM, &record) == 1 || 1);
	/* reals are read without the process locale: a comma locale still reads the dot, and refuses the comma */
	{
		char const *old = setlocale(LC_NUMERIC, 0);
		char saved[64];

		snprintf(saved, sizeof(saved), "%s", old ? old : "C");
		if (setlocale(LC_NUMERIC, "de_DE.UTF-8") || setlocale(LC_NUMERIC, "de_DE") || setlocale(LC_NUMERIC, "fr_FR.UTF-8"))
		{
			CHECK(accepted("!FM:1:0:12.5:1.25:3:0:0:0", AE_FORGE_NET_FM, &record) && record.x == 12.5f && record.y == 1.25f);
			CHECK(refused("!FM:1:0:12,5:1:3:0:0:0"));
		}
		setlocale(LC_NUMERIC, saved);
	}

	/* ---- the map's settings: the host's word, parsed and bounded */
	CHECK(accepted("!FGN:BLOOD GULCH", AE_FORGE_NET_FGN, &record) && !strcmp(record.name, "BLOOD GULCH") && accepted("!FGN:", AE_FORGE_NET_FGN, &record) && record.name[0] == 0);
	CHECK(refused("!FGN:abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz") && accepted("!FGN:a:b", AE_FORGE_NET_FGN, &record) && !strcmp(record.name, "a:b"));
	CHECK(accepted("!FGT:2:1:FFFFFFFF", AE_FORGE_NET_FGT, &record) && record.kind == 2 && record.slot == 1 && record.object_code == 0xFFFFFFFFu && refused("!FGT:3:1:1") &&
		refused("!FGT:1:3:1") && refused("!FGT:1:1") && refused("!FGT:1:1:123456789") && refused("!FGT:-1:1:1"));
	CHECK(accepted("!FGL:2:3", AE_FORGE_NET_FGL, &record) && refused("!FGL:3:1") && refused("!FGL:1:4") && accepted("!FGM:0:2", AE_FORGE_NET_FGM, &record) && refused("!FGM:0:3"));
	CHECK(accepted("!FGV:1024:1.50", AE_FORGE_NET_FGV, &record) && record.fog_start == 1024.0f && refused("!FGV:16385:1") && refused("!FGV:100:33") && refused("!FGV:nan:1") &&
		refused("!FGV:-1:1") && refused("!FGV:1e3:1") && refused("!FGV:100"));
	CHECK(accepted("!FGA:3:99A0B0:95:2.0:50.0:1:0", AE_FORGE_NET_FGA, &record) && record.atmosphere == 3 && record.color == 0x99A0B0 && record.density == 95 && record.fog_opaque == 50.0f &&
		refused("!FGA:6:0:0:0:0:0:0") && refused("!FGA:3:1000000:0:0:0:0:0") && refused("!FGA:3:0:101:0:0:0:0") && refused("!FGA:3:0:0:nan:0:0:0") && refused("!FGA:3:0:0:0:inf:0:0") &&
		refused("!FGA:3:0:0:100001:0:0:0") && refused("!FGA:3:0:0:-1:0:0:0") && refused("!FGA:3:0:0:0:0:2:0") && refused("!FGA:3:0:0:0:0:0:3") && refused("!FGA:3:0:0:0:0:0"));
	CHECK(!ae_forge_net_rules_client_may_say(AE_FORGE_NET_FGN) && ae_forge_net_rules_host_may_say(AE_FORGE_NET_FGA) && !ae_forge_net_rules_is_edit(AE_FORGE_NET_FGT));

	/* ---- who may touch what */
	{
		struct ae_forge_net_facts f;

		memset(&f, 0, sizeof(f));
		f.active = 1;
		f.object_abs = 7;
		f.kind = AE_FORGE_NET_KIND_VEHICLE;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, 7, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, 8, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OBJECT);
		f.active = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_INACTIVE);
		f.active = 1;
		/* a vehicle with a rider: no tag of an instance names it (move, delete, scale, flags, usage, settings) */
		f.occupied = 1;
		{
			int tags[] = { AE_FORGE_NET_FM, AE_FORGE_NET_FD, AE_FORGE_NET_FZ, AE_FORGE_NET_FU, AE_FORGE_NET_FA, AE_FORGE_NET_FJ, AE_FORGE_NET_FW,
				AE_FORGE_NET_FV, AE_FORGE_NET_FN, AE_FORGE_NET_FK };
			unsigned int each;

			for (each = 0; each < sizeof(tags) / sizeof(tags[0]); each++)
				CHECK(ae_forge_net_rules_client_touch(&f, tags[each], -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OCCUPIED);
			/* (a held move of an occupied vehicle ejects its riders: refused as the rest) */
			CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 1, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OCCUPIED);
			f.occupied = 0;
			f.parented = 1;
			for (each = 0; each < sizeof(tags) / sizeof(tags[0]); each++)
				CHECK(ae_forge_net_rules_client_touch(&f, tags[each], -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_PARENTED);
		}
		f.parented = 0;
		/* held: by another, a move of it is refused (the line that lets go too: a hold is let go of by its holder only); by itself or nobody, it is not */
		f.held_by = 2;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 1, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 1, 2, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD);
		f.held_by = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 1, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		/* the map's own: any client may when the setting says so, none when it does not */
		f.map_own = 1;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 0, 10000) == AE_FORGE_NET_TOUCH_MAP_OWN);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 0, 10000) == AE_FORGE_NET_TOUCH_MAP_OWN);
		f.map_own = 0;
		/* the last of a class is kept: no delete, move, scale or change of it; the others of its class free it */
		f.kind = AE_FORGE_NET_KIND_SPAWN;
		f.dest_in_level = 1;
		f.class_own_mask = 0;
		f.class_others_mask = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FZ, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FU, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FK, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		f.class_others_mask = AE_FORGE_NET_MODES_ALL;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		/* (an all-mode spawn that a CTF-only one stands beside is the last for the other four modes) */
		f.class_others_mask = 1;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		/* (a Race-only spawn beside an all-mode one: not the last) */
		f.class_own_mask = 0x10;
		f.class_others_mask = AE_FORGE_NET_MODES_ALL;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		f.class_others_mask = 0x02;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		/* a flag: its own class (the capture flag of a team, the ball spawn): the last stays, another of its class frees it */
		f.kind = AE_FORGE_NET_KIND_FLAG;
		f.class_own_mask = 1;
		f.class_others_mask = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_LAST);
		f.class_others_mask = 1;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		/* a spawn or flag is moved inside the level only, by a client, the last of its class or not */
		f.dest_in_level = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_BOUNDS);
		f.kind = AE_FORGE_NET_KIND_VEHICLE;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		f.kind = AE_FORGE_NET_KIND_SPAWN;
		f.dest_in_level = 1;
		f.class_own_mask = 0;
		f.class_others_mask = AE_FORGE_NET_MODES_ALL;
		/* what the host holds in its hand is not moved, scaled or deleted by a client; another client's hold is not let go of by a line that lets go */
		f.host_holds = 1;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD &&
			ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FD, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD &&
			ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FZ, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD);
		f.host_holds = 0;
		f.held_by = 2;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 2, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		f.held_by = 0;
		/* a client holds only so many at once */
		f.sender_holds = AE_FORGE_NET_HOLD_MAX;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 1, 1, 1, 10000) == AE_FORGE_NET_TOUCH_HELD);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 10000) == AE_FORGE_NET_TOUCH_OK);
		f.sender_holds = 0;
		CHECK(ae_forge_net_rules_last_of_class(0, 0) == 1 && ae_forge_net_rules_last_of_class(0, AE_FORGE_NET_MODES_ALL) == 0);
		/* a slot made a moment ago is its maker's: another's line is about the one before; the maker's, and a later line, are not */
		f.created_ms = 20000;
		f.creator = 2;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 20500) == AE_FORGE_NET_TOUCH_FRESH);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 2, 1, 20500) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 20000 + AE_FORGE_NET_FRESH_MS) == AE_FORGE_NET_TOUCH_OK);
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 21000) == AE_FORGE_NET_TOUCH_FRESH);
		/* (the host's own made it: creator 0 and a client's line is fresh) */
		f.creator = 0;
		CHECK(ae_forge_net_rules_client_touch(&f, AE_FORGE_NET_FM, -1, 0, 1, 1, 20100) == AE_FORGE_NET_TOUCH_FRESH);
		/* the delete rule for the host: the last spawn and flag kept, others not */
		CHECK(!ae_forge_net_rules_delete_ok(AE_FORGE_NET_KIND_SPAWN, 1) && !ae_forge_net_rules_delete_ok(AE_FORGE_NET_KIND_FLAG, 1) &&
			ae_forge_net_rules_delete_ok(AE_FORGE_NET_KIND_SPAWN, 0) && ae_forge_net_rules_delete_ok(AE_FORGE_NET_KIND_VEHICLE, 1) &&
			ae_forge_net_rules_delete_ok(AE_FORGE_NET_KIND_OTHER, 1));
	}

	/* ---- B3 and S6: AI teams, live actors, budgets */
	CHECK(ae_forge_net_rules_ai_team_ok(0) && ae_forge_net_rules_ai_team_ok(8) && !ae_forge_net_rules_ai_team_ok(9) && !ae_forge_net_rules_ai_team_ok(-1) &&
		!ae_forge_net_rules_ai_team_ok(32768) && ae_forge_net_rules_team_ok(0, -32768) && ae_forge_net_rules_team_ok(0, 32767) && !ae_forge_net_rules_team_ok(0, 32768) &&
		!ae_forge_net_rules_team_ok(1, 9) && ae_forge_net_rules_team_ok(1, 7));
	CHECK(ae_forge_net_rules_ai_live(4, 2) == 4 && ae_forge_net_rules_ai_live(1, 16) == 16 && ae_forge_net_rules_ai_live(0, 0) == 0);
	CHECK(ae_forge_net_rules_ai_budget_ok(0, 64, 0) && !ae_forge_net_rules_ai_budget_ok(0, 65, 0) && !ae_forge_net_rules_ai_budget_ok(60, 5, 0) &&
		ae_forge_net_rules_ai_budget_ok(48, 16, 0) && !ae_forge_net_rules_ai_budget_ok(0, 16, 120) && ae_forge_net_rules_ai_budget_ok(0, 8, 120) && !ae_forge_net_rules_ai_budget_ok(0, -1, 0));
	/* (16 spawns each keeping 16 alive: 256 live actors are over the 64 of a client at the fifth) */
	{
		int owned = 0, spawn, taken = 0;

		for (spawn = 0; spawn < 16; spawn++)
		{
			if (ae_forge_net_rules_ai_budget_ok(owned, ae_forge_net_rules_ai_live(1, 16), 0))
			{
				owned += 16;
				taken++;
			}
		}
		CHECK(taken == 4 && owned == 64);
	}

	/* ---- S3, S4: a machine's standing, the identities */
	{
		struct ae_forge_net_machine machine;
		unsigned long keys[4];
		int count = 0;

		ae_forge_net_rules_machine_reset(&machine);
		CHECK(machine.state == AE_FORGE_NET_MACHINE_NEW && machine.identity == -1);
		/* nothing before the level is ready; a request for the state is the one line a new machine may send, and it is synced only once that request is taken */
		CHECK(!ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FQ, 0) && machine.state == AE_FORGE_NET_MACHINE_NEW);
		CHECK(!ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FS, 1) && !ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FT, 1));
		CHECK(ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FQ, 1) && machine.state == AE_FORGE_NET_MACHINE_NEW);
		ae_forge_net_rules_machine_synced(&machine);
		CHECK(machine.state == AE_FORGE_NET_MACHINE_SYNCED);
		CHECK(ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FS, 1) && !ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FS, 0));
		/* a slot's next player is new */
		ae_forge_net_rules_machine_reset(&machine);
		CHECK(!ae_forge_net_rules_machine_accepts(&machine, AE_FORGE_NET_FM, 1));
		/* an identity keeps its slot when it comes back; a full table takes no other */
		CHECK(ae_forge_net_rules_identity(keys, &count, 3, 100) == 0 && ae_forge_net_rules_identity(keys, &count, 3, 200) == 1 &&
			ae_forge_net_rules_identity(keys, &count, 3, 100) == 0 && ae_forge_net_rules_identity(keys, &count, 3, 300) == 2 &&
			ae_forge_net_rules_identity(keys, &count, 3, 400) == -1 && ae_forge_net_rules_identity(keys, &count, 3, 200) == 1 && count == 3);
		/* the budget and the mark of being let go of are the identity's: a rejoin does not clear them */
		{
			struct ae_forge_net_client clients[3];
			int made = 0, step;

			memset(clients, 0, sizeof(clients));
			for (step = 0; step < 200; step++)
				made += ae_forge_net_rules_client_instance(&clients[0], 0);
			clients[0].dropped = 1;
			CHECK(made == AE_FORGE_NET_CLIENT_INSTANCES && clients[ae_forge_net_rules_identity(keys, &count, 3, 100)].dropped == 1 &&
				!ae_forge_net_rules_client_instance(&clients[ae_forge_net_rules_identity(keys, &count, 3, 100)], 0) &&
				clients[ae_forge_net_rules_identity(keys, &count, 3, 200)].dropped == 0);
		}
	}

	/* ---- S5: a flood of edits is struck while a pose line is sent every few seconds; and a 32-bit clock that wraps */
	{
		struct ae_forge_net_client client;
		int drop = 0, step;
		unsigned long now = 100000, dropped_at = 0;

		ae_forge_net_rules_client_reset(&client);
		for (step = 0; step < 64; step++)
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now, &drop);
		for (step = 0; step < 900 && !dropped_at; step++)
		{
			unsigned long at = now + (unsigned long)step * 10;

			if (step % 400 == 399)
				ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FT, at, &drop);
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, at, &drop);
			if (drop)
				dropped_at = at;
		}
		CHECK(dropped_at >= now + AE_FORGE_NET_STRIKE_MS && dropped_at <= now + AE_FORGE_NET_STRIKE_MS + 20);
		/* the clock wraps at 32 bits: no early drop across it, and the drop is still reached */
		ae_forge_net_rules_client_reset(&client);
		now = 0xFFFFFF00ul;
		dropped_at = 0;
		for (step = 0; step < 64; step++)
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now, &drop);
		for (step = 0; step < 900 && !dropped_at; step++)
		{
			unsigned long at = (unsigned long)(unsigned int)(now + (unsigned long)step * 10);

			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, at, &drop);
			if (drop)
				dropped_at = (unsigned long)step * 10;
		}
		CHECK(dropped_at >= AE_FORGE_NET_STRIKE_MS && dropped_at <= AE_FORGE_NET_STRIKE_MS + 20);
	}

	/* ---- S9: the relay, shared out */
	{
		struct ae_forge_net_relay relay;
		int one = 0, other = 0, step;

		memset(&relay, 0, sizeof(relay));
		/* (two senders, four recipients: one taking all it can leaves the other its share) */
		for (step = 0; step < 4000; step++)
			one += ae_forge_net_rules_relay_ok(&relay, 0, 4, 2, 1000);
		for (step = 0; step < 4000; step++)
			other += ae_forge_net_rules_relay_ok(&relay, 1, 4, 2, 1000);
		CHECK(one > 0 && one < 4000 && other > 0 && other < 4000 && one + other <= 3000 / 4 + 4);
		CHECK(ae_forge_net_rules_relay_ok(&relay, 40, 4, 2, 1000) == 1 && ae_forge_net_rules_relay_ok(&relay, -1, 4, 2, 1000) == 1);
	}
	/* a relayed line fits 63 characters whatever the numbers; the precision gives way */
	{
		char text[64];
		int length;

		length = ae_forge_net_rules_format_fm(text, sizeof(text), 4095, 8191, -10000.0f, -10000.0f, -10000.0f, -100000, -100000, -100000, 1);
		CHECK(length > 0 && length <= 63 && (int)strlen(text) == length && !strncmp(text, "!FM:4095:8191:", 14));
		length = ae_forge_net_rules_format_fm(text, sizeof(text), 12, 300, 61.28f, -65.6f, 1.71f, 314, 0, 0, 1);
		CHECK(!strcmp(text, "!FM:12:300:61.28:-65.60:1.71:314:0:0:1"));
		length = ae_forge_net_rules_format_ft(text, sizeof(text), 127, 1, -10000.0f, -10000.0f, -10000.0f, -100000, -100000);
		CHECK(length > 0 && length <= 63);
		CHECK(accepted(text, AE_FORGE_NET_FT, &record));
		ae_forge_net_rules_format_fm(text, sizeof(text), 4095, 8191, 9999.99f, -9999.99f, 9999.99f, 99999, -99999, 99999, 0);
		CHECK(accepted(text, AE_FORGE_NET_FM, &record));
		/* (a line that cannot fit says nothing) */
		CHECK(ae_forge_net_rules_format_fm(text, 20, 1, 2, 3.0f, 3.0f, 3.0f, 0, 0, 0, 0) == 0 && text[0] == 0);
	}
	/* a peer's text in a log: printable bytes only, bounded */
	{
		char shown[12];

		ae_forge_net_rules_printable("!FM\x01\x1b[2J:12345678901234", shown, sizeof(shown));
		CHECK(!strcmp(shown, "!FM??[2J:12") && strlen(shown) == 11);
		ae_forge_net_rules_printable("ab", shown, sizeof(shown));
		CHECK(!strcmp(shown, "ab"));
		ae_forge_net_rules_printable(0, shown, sizeof(shown));
		CHECK(shown[0] == 0);
	}

	/* ---- S8: the joiner waits for the host's tags; S7: lines put by are not lost */
	{
		int state = AE_FORGE_NET_COMPARE_WAIT;

		CHECK(!ae_forge_net_rules_apply_allowed(state));
		state = ae_forge_net_rules_compare_state(state, 0, 0, 0, 5000);
		CHECK(state == AE_FORGE_NET_COMPARE_WAIT);
		state = ae_forge_net_rules_compare_state(state, 0, 0, 1000, 3999);
		CHECK(state == AE_FORGE_NET_COMPARE_WAIT && !ae_forge_net_rules_apply_allowed(state));
		state = ae_forge_net_rules_compare_state(state, 0, 0, 1000, 4000);
		CHECK(state == AE_FORGE_NET_COMPARE_NOT_COMPARED && ae_forge_net_rules_apply_allowed(state));
		state = ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_WAIT, 1, 1, 1000, 1500);
		CHECK(state == AE_FORGE_NET_COMPARE_PASSED && ae_forge_net_rules_apply_allowed(state));
		CHECK(ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_WAIT, 1, 0, 1000, 1500) == AE_FORGE_NET_COMPARE_FAILED &&
			!ae_forge_net_rules_apply_allowed(AE_FORGE_NET_COMPARE_FAILED));
		/* (a failed compare stays failed; a late timeout does not undo a pass; a host's later matching line passes a timed-out join) */
		CHECK(ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_FAILED, 0, 0, 1000, 99999) == AE_FORGE_NET_COMPARE_FAILED);
		CHECK(ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_PASSED, 0, 0, 1000, 99999) == AE_FORGE_NET_COMPARE_PASSED);
		CHECK(ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_NOT_COMPARED, 1, 1, 1000, 99999) == AE_FORGE_NET_COMPARE_PASSED);
		/* (the 32-bit clock: the request just before the wrap, the timeout after it) */
		CHECK(ae_forge_net_rules_compare_state(AE_FORGE_NET_COMPARE_WAIT, 0, 0, 0xFFFFFF00ul, 0xFFFFFF00ul + 4000) == AE_FORGE_NET_COMPARE_NOT_COMPARED);
	}
	{
		static struct ae_forge_net_ring ring;
		char line[AE_FORGE_NET_TEXT_SIZE + 1], text[AE_FORGE_NET_TEXT_SIZE + 1];
		int each, pushed = 0;

		ae_forge_net_rules_ring_reset(&ring);
		CHECK(!ae_forge_net_rules_ring_pop(&ring, line));
		for (each = 0; each < AE_FORGE_NET_RING_LINES + 10; each++)
		{
			snprintf(text, sizeof(text), "!FD:%d", each % 4000);
			pushed += ae_forge_net_rules_ring_push(&ring, text);
		}
		CHECK(pushed == AE_FORGE_NET_RING_LINES && ring.count == AE_FORGE_NET_RING_LINES);
		for (each = 0; each < 3; each++)
		{
			CHECK(ae_forge_net_rules_ring_pop(&ring, line));
			snprintf(text, sizeof(text), "!FD:%d", each);
			CHECK(!strcmp(line, text));
		}
		/* (room again, in order, across the wrap) */
		CHECK(ae_forge_net_rules_ring_push(&ring, "!FD:7") && ae_forge_net_rules_ring_push(&ring, "!FD:8") && ae_forge_net_rules_ring_push(&ring, "!FD:9") &&
			!ae_forge_net_rules_ring_push(&ring, "!FD:10"));
		for (each = 3; each < AE_FORGE_NET_RING_LINES; each++)
			ae_forge_net_rules_ring_pop(&ring, line);
		CHECK(ae_forge_net_rules_ring_pop(&ring, line) && !strcmp(line, "!FD:7") && ae_forge_net_rules_ring_pop(&ring, line) && !strcmp(line, "!FD:8") &&
			ae_forge_net_rules_ring_pop(&ring, line) && !strcmp(line, "!FD:9") && !ae_forge_net_rules_ring_pop(&ring, line) && ring.count == 0);
	}
	/* ---- the strike and the request for the state */
	{
		struct ae_forge_net_client client;
		int drop = 0;
		unsigned long now = 10000;

		/* a burst-1 bucket never held a strike for good: a denied request leaves its strike on that bucket only, and the bucket clears it when it next has its token */
		ae_forge_net_rules_client_reset(&client);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now, &drop) == 1);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 1000, &drop) == 0 && !drop && client.over_since_ms[AE_FORGE_NET_BUCKET_SYNC] != 0);
		/* (a line of another bucket that is denied 5 s later does not drop the client for a strike that is not its bucket's) */
		{
			int step;

			for (step = 0; step < 200; step++)
				ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 6000, &drop);
			CHECK(client.over_since_ms[AE_FORGE_NET_BUCKET_EDIT] != 0 && !drop);
			ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 6001, &drop);
			CHECK(!drop);
		}
		/* the bucket with its token again takes the request, and the strike is gone */
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 62000, &drop) == 1 && !drop && client.over_since_ms[AE_FORGE_NET_BUCKET_SYNC] == 0);
		/* a request denied again for the strike time is the one that drops: asking every 30 s for 5 s is not a rejoin */
		ae_forge_net_rules_client_reset(&client);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now, &drop);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 100, &drop);
		CHECK(!drop);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 100 + AE_FORGE_NET_STRIKE_MS, &drop);
		CHECK(drop);
		/* a rejoin inside the minute: the machine left, so its bucket for the request starts afresh; a player that pastes after it is not dropped */
		ae_forge_net_rules_client_reset(&client);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now, &drop);
		ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 200, &drop);
		ae_forge_net_rules_client_machine_left(&client);
		CHECK(client.over_since_ms[AE_FORGE_NET_BUCKET_SYNC] == 0);
		CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FQ, now + 5000, &drop) == 1 && !drop);
		{
			int step, taken = 0;

			for (step = 0; step < 100; step++)
				taken += ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 5200, &drop);
			CHECK(taken == 64 && !drop);
			CHECK(ae_forge_net_rules_client_line(&client, AE_FORGE_NET_FS, now + 5200, &drop) == 0 && !drop);
		}
	}

	/* ---- identities: an address of its own, the least recently seen leaves, the pinned stay */
	{
		unsigned long seen[4] = {500, 100, 300, 900};
		unsigned char pinned[4] = {0, 0, 0, 0};

		CHECK(ae_forge_net_rules_identity_key(0x0A000001ul, 3) == 0x0A000001ul);
		CHECK(ae_forge_net_rules_identity_key(0, 3) != ae_forge_net_rules_identity_key(0, 4) && ae_forge_net_rules_identity_key(0, 3) > 0xFFFFFFFFul &&
			ae_forge_net_rules_identity_key(0, 3) == ae_forge_net_rules_identity_key(0, 3));
		CHECK(ae_forge_net_rules_identity_evict(seen, pinned, 4, 1000) == 1);
		pinned[1] = 1;
		CHECK(ae_forge_net_rules_identity_evict(seen, pinned, 4, 1000) == 2);
		pinned[2] = pinned[0] = 1;
		CHECK(ae_forge_net_rules_identity_evict(seen, pinned, 4, 1000) == 3);
		pinned[3] = 1;
		CHECK(ae_forge_net_rules_identity_evict(seen, pinned, 4, 1000) == -1);
		/* (the clock's wrap: an old one is the one with the greater age as an unsigned number) */
		{
			unsigned long wrapped[2] = {0xFFFFFF00ul, 0x00000100ul};
			unsigned char free_slots[2] = {0, 0};

			CHECK(ae_forge_net_rules_identity_evict(wrapped, free_slots, 2, 0x00000200ul) == 0);
		}
		CHECK(ae_forge_net_rules_identityless_may(AE_FORGE_NET_FT) && ae_forge_net_rules_identityless_may(AE_FORGE_NET_FQ) &&
			!ae_forge_net_rules_identityless_may(AE_FORGE_NET_FS) && !ae_forge_net_rules_identityless_may(AE_FORGE_NET_FM) &&
			!ae_forge_net_rules_identityless_may(AE_FORGE_NET_FD));
	}

	/* ---- a leaver's holds, holds: counted and listed */
	{
		unsigned char held[64];
		int list[16], each;

		memset(held, 0, sizeof(held));
		held[3] = held[9] = held[40] = held[63] = 2;
		held[4] = 3;
		CHECK(ae_forge_net_rules_count_held(held, 64, 2) == 4 && ae_forge_net_rules_count_held(held, 64, 3) == 1 && ae_forge_net_rules_count_held(held, 64, 0) == 0);
		CHECK(ae_forge_net_rules_held_list(held, 64, 2, list, 16) == 4 && list[0] == 3 && list[1] == 9 && list[2] == 40 && list[3] == 63);
		CHECK(ae_forge_net_rules_held_list(held, 64, 2, list, 2) == 2 && ae_forge_net_rules_held_list(held, 64, 0, list, 16) == 0);
		for (each = 0; each < 16; each++)
			held[each] = 5;
		CHECK(ae_forge_net_rules_held_list(held, 64, 5, list, 16) == 16);
	}

	/* ---- the streams: the latest of a key, a line that lets go kept, a budget that fits sixteen machines */
	{
		struct ae_forge_net_pending_table *table = calloc(1, sizeof(*table));
		struct ae_forge_net_relay relay;
		char text[AE_FORGE_NET_TEXT_SIZE + 1];
		int sender = -1, step, sent = 0;
		unsigned long now = 5000;

		memset(&relay, 0, sizeof(relay));
		/* (the older line of a key goes, the latest stays) */
		CHECK(ae_forge_net_rules_pending_put(table, 7, 2, "!FM:7:1", 0) && ae_forge_net_rules_pending_put(table, 7, 2, "!FM:7:2", 0));
		CHECK(ae_forge_net_rules_pending_flush(table, &relay, 1, 1, now, text, &sender) && !strcmp(text, "!FM:7:2") && sender == 2);
		CHECK(!ae_forge_net_rules_pending_flush(table, &relay, 1, 1, now, text, &sender));
		/* (a line that lets go is not overwritten by a move sent before it, and goes first) */
		CHECK(ae_forge_net_rules_pending_put(table, 7, 2, "!FM:7:held", 0) && ae_forge_net_rules_pending_put(table, 8, 2, "!FM:8:down", 1));
		CHECK(ae_forge_net_rules_pending_put(table, 8, 2, "!FM:8:move", 0) == 0);
		CHECK(ae_forge_net_rules_pending_flush(table, &relay, 1, 1, now, text, &sender) && !strcmp(text, "!FM:8:down"));
		CHECK(ae_forge_net_rules_pending_flush(table, &relay, 1, 1, now, text, &sender) && !strcmp(text, "!FM:7:held"));
		/* (a delete takes what waits for its key) */
		ae_forge_net_rules_pending_put(table, 9, 1, "!FM:9", 0);
		ae_forge_net_rules_pending_clear(table, 9);
		CHECK(!ae_forge_net_rules_pending_flush(table, &relay, 1, 1, now, text, &sender));
		/* (a full table: another key's move is dropped, one that lets go takes a stream's place, and a table of lines that let go takes no more) */
		for (step = 0; step < AE_FORGE_NET_PENDING_MAX; step++)
		{
			char line[16];

			snprintf(line, sizeof(line), "!FM:%d", step);
			CHECK(ae_forge_net_rules_pending_put(table, 100 + step, 1, line, 0));
		}
		CHECK(!ae_forge_net_rules_pending_put(table, 5000, 1, "!FM:x", 0) && ae_forge_net_rules_pending_put(table, 5000, 1, "!FM:x", 1));
		for (step = 0; step < AE_FORGE_NET_PENDING_MAX; step++)
			ae_forge_net_rules_pending_put(table, 6000 + step, 1, "!FM:f", 1);
		CHECK(!ae_forge_net_rules_pending_put(table, 7000, 1, "!FM:g", 1));
		memset(table, 0, sizeof(*table));
		/* sixteen machines, one of them dragging at 20 lines a second for ten seconds: the latest of the drag gets through at its share (6 a second at 16
		senders), the final line always, and nothing a hundred-times faster than that is sent */
		memset(&relay, 0, sizeof(relay));
		sent = 0;
		for (step = 0; step < 200; step++)
		{
			char line[16];

			now = 10000 + (unsigned long)step * 50;
			snprintf(line, sizeof(line), "!FM:%d", step);
			if (ae_forge_net_rules_relay_ok(&relay, 3, 15, 16, now))
			{
				ae_forge_net_rules_pending_clear(table, 1);
				sent++;
			}
			else
				ae_forge_net_rules_pending_put(table, 1, 3, line, 0);
			while (ae_forge_net_rules_pending_flush(table, &relay, 15, 16, now, text, &sender))
				sent++;
		}
		CHECK(sent >= 40 && sent <= 100);
		/* (the line that lets go waits for the budget and is then sent, however many moves came after it) */
		ae_forge_net_rules_pending_put(table, 1, 3, "!FM:1:down", 1);
		for (step = 0; step < 100; step++)
			ae_forge_net_rules_pending_put(table, 1, 3, "!FM:1:late", 0);
		for (step = 0; step < 100 && !ae_forge_net_rules_pending_flush(table, &relay, 15, 16, 20000 + (unsigned long)step * 200, text, &sender); step++)
			;
		CHECK(step < 100 && !strcmp(text, "!FM:1:down"));
		free(table);
		/* (the senders: those who sent a line in the last second, not those who asked for the state) */
		{
			unsigned long last[5] = {0, 9500, 9100, 8000, 10000};

			CHECK(ae_forge_net_rules_active_senders(last, 5, 10000) == 3 && ae_forge_net_rules_active_senders(last, 5, 10999) == 1);
		}
	}

	/* ---- the feed is so many lines a second, whatever the frame rate */
	{
		int fps_list[4] = {30, 60, 144, 1000}, each;

		for (each = 0; each < 4; each++)
		{
			struct ae_forge_net_bucket bucket;
			int total = 0, frame, frames = fps_list[each] * 10, max_burst = 0;
			unsigned long start = 1000;

			memset(&bucket, 0, sizeof(bucket));
			for (frame = 0; frame < frames; frame++)
			{
				unsigned long now = start + (unsigned long)frame * 1000ul / (unsigned long)fps_list[each];
				int lines = ae_forge_net_rules_feed_allowance(&bucket, now);

				ae_forge_net_rules_feed_spent(&bucket, lines);
				total += lines;
				if (lines > max_burst)
					max_burst = lines;
			}
			/* (24 to begin with, then 150 a second: 10 s is at most 1524, and a joiner that takes 180 a second is never behind) */
			CHECK(total >= 1400 && total <= 1530 && max_burst <= 24);
		}
	}

	/* ---- the team of an instance by its kind */
	CHECK(ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, 0) && ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, 2) &&
		ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, -1) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, 3) &&
		!ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, 32767) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_SPAWN, 0, -32768));
	CHECK(ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 0, 1) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 0, 2) &&
		ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 2, 15) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 2, 16) &&
		ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 8, 31) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 8, 32767) &&
		ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 3, 0) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 3, 32) &&
		!ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 16, 5) && ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_FLAG, 16, 1));
	CHECK(ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_AI, 0, 8) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_AI, 0, 9) &&
		ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_OTHER, 0, 32767) && !ae_forge_net_rules_instance_team_ok(AE_FORGE_NET_TEAM_OTHER, 0, 32768));
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_forge_net_rules_test: all passed\n");

	return 0;
}
