#include <stdio.h>
#include <string.h>

#include "ae_weaponset_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int read_text(char const *text, struct ae_weaponset *set, char *error, int size)
{
	return ae_weaponset_read(text, (long)strlen(text), set, error, size);
}

static char const small[] =
	"# a small set\n"
	"weaponset 1 small\n"
	"format 1\n"
	"draw random\n"
	"tier_weight BASE 60 POWER 40\n"
	"map_time off\n"
	"rack_ammo fresh\n"
	"scan off\n"
	"swap stock:shotgun auto_shotgun\n"
	"row PRECISION BASE logic pickup respawn 30 weapons battle_rifle*3 carbine   # the BR three times as often\n"
	"row CLOSE POWER logic static respawn 120 delay 60 ammo 50 weapons stock:shotgun\n"
	"row HEAVY POWER logic trigger channel DROP_A weapons stock:rocket_launcher\n"
	"row MELEE POWER mute\n"
	"row ANY ANY logic expired respawn 3 weapons sidearm\n"
	"channel DROP_A every 120 variance 25\n"
	"override global delay 10\n"
	"override tier POWER respawn 90\n"
	"override class PRECISION logic expired\n"
	"override cell CLOSE POWER respawn 150 ammo 100\n";

/* Every line kind reads into its place. */
static void test_reads(void)
{
	struct ae_weaponset set;
	char error[96];
	struct ae_weaponset_row const *row;

	CHECK(read_text(small, &set, error, sizeof error) == 0);
	CHECK(strcmp(set.name, "small") == 0);
	CHECK(set.draw == AE_WEAPONSET_RANDOM);
	CHECK(set.tier_weight[AE_SPAWNER_BASE] == 60 && set.tier_weight[AE_SPAWNER_POWER] == 40);
	CHECK(!set.map_time && set.rack_ammo_fresh && !set.scan);
	CHECK(set.has == (AE_WEAPONSET_HAS_DRAW | AE_WEAPONSET_HAS_TIER_WEIGHT | AE_WEAPONSET_HAS_MAP_TIME |
		AE_WEAPONSET_HAS_RACK_AMMO | AE_WEAPONSET_HAS_SCAN));
	CHECK(set.row_count == 5);
	row = ae_weaponset_row_of(&set, AE_SPAWNER_PRECISION, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_PICKUP && row->seconds == 30 && row->delay == 0);
	CHECK(row && row->weapon_count == 2 && strcmp(row->weapons[0], "battle_rifle") == 0 && row->weights[0] == 3);
	CHECK(row && strcmp(row->weapons[1], "carbine") == 0 && row->weights[1] == 1);
	row = ae_weaponset_row_of(&set, AE_SPAWNER_CLOSE, AE_SPAWNER_POWER);
	CHECK(row && row->logic == AE_SPAWNER_STATIC && row->seconds == 120 && row->delay == 60 && row->ammo_percent == 50);
	row = ae_weaponset_row_of(&set, AE_SPAWNER_HEAVY, AE_SPAWNER_POWER);
	CHECK(row && row->logic == AE_SPAWNER_TRIGGER && strcmp(row->channel, "DROP_A") == 0 && row->seconds == 0);
	row = ae_weaponset_row_of(&set, AE_SPAWNER_MELEE, AE_SPAWNER_POWER);
	CHECK(row && row->mute);
	row = ae_weaponset_row_of(&set, AE_SPAWNER_AUTOMATIC, AE_SPAWNER_BASE);	/* falls to ANY ANY */
	CHECK(row && row->cls == AE_SPAWNER_CLASS_ANY && row->seconds == 5);	/* the floor on respawn 3 */
	CHECK(set.channel_count == 1 && ae_weaponset_channel_of(&set, "drop_a") &&
		ae_weaponset_channel_of(&set, "DROP_A")->every_seconds == 120 &&
		ae_weaponset_channel_of(&set, "DROP_A")->variance_percent == 25);
	CHECK(!ae_weaponset_channel_of(&set, "DROP_B"));
	CHECK(set.swap_count == 1 && strcmp(ae_weaponset_swapped(&set, "stock:shotgun"), "auto_shotgun") == 0);
	CHECK(strcmp(ae_weaponset_swapped(&set, "carbine"), "carbine") == 0);
	CHECK(set.override_count == 4);
}

/* The overrides: most specific wins; a mute row stays mute. */
static void test_cell(void)
{
	struct ae_weaponset set;
	struct ae_weaponset_row row;
	char error[96];

	CHECK(read_text(small, &set, error, sizeof error) == 0);
	CHECK(ae_weaponset_cell(&set, AE_SPAWNER_PRECISION, AE_SPAWNER_BASE, &row));
	CHECK(row.logic == AE_SPAWNER_EXPIRED);	/* the class override */
	CHECK(row.seconds == 30 && row.delay == 10);	/* the global delay */
	CHECK(ae_weaponset_cell(&set, AE_SPAWNER_CLOSE, AE_SPAWNER_POWER, &row));
	CHECK(row.seconds == 150 && row.ammo_percent == 100 && row.delay == 10);	/* cell beats tier (90) */
	CHECK(ae_weaponset_cell(&set, AE_SPAWNER_HEAVY, AE_SPAWNER_POWER, &row));
	CHECK(row.seconds == 90 && row.logic == AE_SPAWNER_TRIGGER);	/* the tier override */
	CHECK(ae_weaponset_cell(&set, AE_SPAWNER_MELEE, AE_SPAWNER_POWER, &row));
	CHECK(row.mute);
	CHECK(ae_weaponset_cell(&set, AE_SPAWNER_PISTOL, AE_SPAWNER_BASE, &row));	/* ANY ANY + global */
	CHECK(row.cls == AE_SPAWNER_CLASS_ANY && row.delay == 10 && row.seconds == 5);
	set.row_count = 0;
	CHECK(!ae_weaponset_cell(&set, AE_SPAWNER_PISTOL, AE_SPAWNER_BASE, &row));
	CHECK(!ae_weaponset_row_of(0, 0, 0));
}

/* A line the reader does not know, or a bad value, refuses the whole file and names the line. */
static void test_refusals(void)
{
	struct ae_weaponset set;
	char error[96];

	CHECK(read_text("weaponset 1 x\nformat 1\nrowx a b\n", &set, error, sizeof error) == 3);
	CHECK(strstr(error, "rowx") != 0);
	CHECK(read_text("weaponset 1 x\nformat 2\n", &set, error, sizeof error) == 2);
	CHECK(read_text("weaponset 1 x\n", &set, error, sizeof error) == 1);	/* no format line */
	CHECK(strstr(error, "format") != 0);
	CHECK(read_text("format 1\n", &set, error, sizeof error) == 1);	/* no header */
	CHECK(read_text("weaponset 2 x\nformat 1\n", &set, error, sizeof error) == 1);
	CHECK(read_text("", &set, error, sizeof error) == 1);
	CHECK(read_text("# only a comment\n\n", &set, error, sizeof error) == 1);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup weapons br\n", &set, error, sizeof error) == 3);
	CHECK(strstr(error, "respawn") != 0);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic trigger weapons br\n", &set, error, sizeof error) == 3);
	CHECK(strstr(error, "channel") != 0);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE respawn 30 weapons br\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic melt respawn 30 weapons br\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30 weapons br*0\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30 weapons br*10\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30 weapons \"a b\"\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE mute now\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow MID BASE logic pickup respawn 30 weapons br\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\ndraw sideways\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\ntier_weight BASE\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\ntier_weight ANY 5\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nmap_time maybe\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrack_ammo both\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrules strict\n", &set, error, sizeof error) == 3);
	CHECK(strstr(error, "rules") != 0);
	CHECK(read_text("weaponset 1 x\nformat 1\nrules\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrules ae reference\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nextends x\n", &set, error, sizeof error) == 3);	/* itself */
	CHECK(read_text("weaponset 1 x\nformat 1\nchannel A every 120\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nchannel A every 2 variance 10\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\noverride cell CLOSE respawn 10\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\noverride global weapons br\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\noverride global respawn\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nswap br\n", &set, error, sizeof error) == 3);
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30 weapons br # unclosed \"quote\n", &set, error, sizeof error) == 0);	/* a comment is cut first */
	CHECK(read_text("weaponset 1 x\nformat 1\nrow PRECISION BASE logic pickup respawn 30 weapons \"br\n", &set, error, sizeof error) == 3);
	CHECK(ae_weaponset_read(0, 0, &set, error, sizeof error) == 1);
	CHECK(ae_weaponset_read(small, 70000, &set, error, sizeof error) == 1);
	/* a long line, a NUL */
	{
		char text[320];
		int k;

		strcpy(text, "weaponset 1 x\nformat 1\n");
		for (k = 0; k < 210; k++) strcat(text, "a");
		strcat(text, "\n");
		CHECK(read_text(text, &set, error, sizeof error) == 3);
		strcpy(text, "weaponset 1 x\nformat 1\nrow PRECISION BASE\n");
		text[25] = 0;
		CHECK(ae_weaponset_read(text, 44, &set, error, sizeof error) == 3);
	}
}

/* A child takes its parent's rows it lacks, channels, swaps and scalars; its own rows win. */
static void test_extends(void)
{
	struct ae_weaponset parent, child;
	struct ae_weaponset_row const *row;
	char error[96];

	CHECK(read_text(small, &parent, error, sizeof error) == 0);
	CHECK(read_text("weaponset 1 kid\nformat 1\nextends small\nscan on\n"
		"row PRECISION BASE logic static respawn 45 weapons dmr\n"
		"channel DROP_B every 60 variance 0\n", &child, error, sizeof error) == 0);
	CHECK(strcmp(child.extends, "small") == 0);
	CHECK(child.row_count == 1 && child.scan);
	CHECK(!ae_weaponset_extend(&child, 0));
	CHECK(ae_weaponset_extend(&child, &parent));
	CHECK(child.extends[0] == 0);
	CHECK(child.row_count == 5);
	row = ae_weaponset_row_of(&child, AE_SPAWNER_PRECISION, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_STATIC && row->seconds == 45 && row->weapon_count == 1);
	row = ae_weaponset_row_of(&child, AE_SPAWNER_CLOSE, AE_SPAWNER_POWER);
	CHECK(row && row->seconds == 120);
	CHECK(child.channel_count == 2 && ae_weaponset_channel_of(&child, "DROP_A") && ae_weaponset_channel_of(&child, "DROP_B"));
	CHECK(child.swap_count == 1);
	CHECK(child.draw == AE_WEAPONSET_RANDOM && child.rack_ammo_fresh && !child.map_time && child.scan);	/* scan: its own */
	CHECK(child.tier_weight[AE_SPAWNER_BASE] == 60);
	CHECK(child.override_count == 4);
	CHECK(!ae_weaponset_extend(&child, &parent));	/* done once */
	/* a child naming another parent takes nothing */
	CHECK(read_text("weaponset 1 kid\nformat 1\nextends other\n", &child, error, sizeof error) == 0);
	CHECK(!ae_weaponset_extend(&child, &parent));
	CHECK(child.row_count == 0);
	/* `rules`: ae by default; a child without the line takes its parent's, one with it keeps its own */
	CHECK(read_text("weaponset 1 small\nformat 1\n", &parent, error, sizeof error) == 0 && !parent.rules_reference);
	CHECK(read_text("weaponset 1 small\nformat 1\nrules reference\n", &parent, error, sizeof error) == 0 &&
		parent.rules_reference && (parent.has & AE_WEAPONSET_HAS_RULES));
	CHECK(read_text("weaponset 1 kid\nformat 1\nextends small\n", &child, error, sizeof error) == 0);
	CHECK(ae_weaponset_extend(&child, &parent) && child.rules_reference);
	CHECK(read_text("weaponset 1 kid\nformat 1\nextends small\nrules ae\n", &child, error, sizeof error) == 0);
	CHECK(ae_weaponset_extend(&child, &parent) && !child.rules_reference);
}

/* The three built-in sets read, and say what the design says. */
static void test_builtin(void)
{
	struct ae_weaponset casual, comp, btb;
	struct ae_weaponset_row const *row;
	char error[96];
	char const *text;

	CHECK(ae_weaponsets_builtin_id("casual") == AE_WEAPONSET_CASUAL);
	CHECK(ae_weaponsets_builtin_id("COMP") == AE_WEAPONSET_COMP);
	CHECK(ae_weaponsets_builtin_id("btb") == AE_WEAPONSET_BTB);
	CHECK(ae_weaponsets_builtin_id("ranked") == -1);
	CHECK(ae_weaponsets_builtin_id(0) == -1);
	CHECK(strcmp(ae_weaponsets_builtin_name(AE_WEAPONSET_BTB), "btb") == 0);
	CHECK(strcmp(ae_weaponsets_builtin_name(7), "") == 0);
	CHECK(!ae_weaponsets_builtin_text("custom"));

	text = ae_weaponsets_builtin_text("casual");
	CHECK(text && read_text(text, &casual, error, sizeof error) == 0);
	CHECK(casual.row_count == 10 && casual.map_time && !casual.rack_ammo_fresh && casual.scan);
	CHECK(!casual.rules_reference && (casual.has & AE_WEAPONSET_HAS_RULES));	/* `rules ae` */
	CHECK(casual.draw == AE_WEAPONSET_ROTATE && casual.tier_weight[AE_SPAWNER_BASE] == 70);
	row = ae_weaponset_row_of(&casual, AE_SPAWNER_CLOSE, AE_SPAWNER_POWER);
	CHECK(row && row->logic == AE_SPAWNER_STATIC && row->seconds == 120 && row->weapon_count == 1 &&
		strcmp(row->weapons[0], "stock:shotgun") == 0);
	row = ae_weaponset_row_of(&casual, AE_SPAWNER_CLOSE, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_EXPIRED && row->seconds == 15 && strcmp(row->weapons[0], "auto_shotgun") == 0);
	row = ae_weaponset_row_of(&casual, AE_SPAWNER_AUTOMATIC, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_PICKUP && row->weapon_count == 8);
	CHECK(!ae_weaponset_row_of(&casual, AE_SPAWNER_PISTOL, AE_SPAWNER_POWER));	/* no ANY rows: nothing there */

	text = ae_weaponsets_builtin_text("comp");
	CHECK(text && read_text(text, &comp, error, sizeof error) == 0);
	CHECK(comp.row_count == 10 && !comp.map_time && comp.rack_ammo_fresh);
	CHECK(comp.rules_reference);	/* `rules reference` */
	row = ae_weaponset_row_of(&comp, AE_SPAWNER_PRECISION, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_EXPIRED && row->seconds == 30 && row->weapon_count == 1 &&
		strcmp(row->weapons[0], "battle_rifle") == 0);
	row = ae_weaponset_row_of(&comp, AE_SPAWNER_MELEE, AE_SPAWNER_POWER);
	CHECK(row && row->delay == 60 && row->seconds == 240);

	text = ae_weaponsets_builtin_text("btb");
	CHECK(text && read_text(text, &btb, error, sizeof error) == 0);
	CHECK(btb.row_count == 7 && btb.channel_count == 3 && strcmp(btb.extends, "casual") == 0);
	CHECK(ae_weaponset_extend(&btb, &casual));
	CHECK(btb.row_count == 10 && btb.map_time && !btb.rack_ammo_fresh && btb.scan);
	CHECK(!btb.rules_reference);	/* casual's */
	row = ae_weaponset_row_of(&btb, AE_SPAWNER_HEAVY, AE_SPAWNER_POWER);
	CHECK(row && row->logic == AE_SPAWNER_TRIGGER && strcmp(row->channel, "DROP_A") == 0);
	CHECK(ae_weaponset_channel_of(&btb, "DROP_A")->every_seconds == 120);
	row = ae_weaponset_row_of(&btb, AE_SPAWNER_PRECISION, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_PICKUP && row->weapon_count == 4);	/* casual's */
	row = ae_weaponset_row_of(&btb, AE_SPAWNER_LONG_RANGE, AE_SPAWNER_BASE);
	CHECK(row && row->logic == AE_SPAWNER_PICKUP && row->seconds == 90);	/* its own */
	/* every row of every built-in set names a channel the set has, when it triggers */
	{
		int index;

		for (index = 0; index < btb.row_count; index++)
			if (btb.rows[index].logic == AE_SPAWNER_TRIGGER)
				CHECK(ae_weaponset_channel_of(&btb, btb.rows[index].channel) != 0);
	}
}

int main(void)
{
	test_reads();
	test_cell();
	test_refusals();
	test_extends();
	test_builtin();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
