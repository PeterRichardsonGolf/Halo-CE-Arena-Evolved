#include <stdio.h>
#include <string.h>

#include "ae_spawner_file_rules.h"
#include "ae_spawner_text.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int read_text(char const *text, struct ae_spawner_file *file, char *error, int size)
{
	return ae_spawner_file_read(text, (long)strlen(text), file, error, size);
}

#define HEAD "spawners 1 bloodgulch 1a2b3c4d\nformat 1\n"

static char const sample[] =
	"# Blood Gulch's spawners\n"
	HEAD
	"auto on\n"
	"spawner 3 class CLOSE tier POWER logic static respawn 120 delay 30\n"
	"spawner skip 4\n"
	"spawner 9 split\n"
	"spawner 7 weapon stock:shotgun\n"
	"spawner at 1 2.5 -3 facing 90 kind pad class HEAVY tier POWER channel DROP_A sets comp btb games ctf slayer\n"
	"spawner 21 kind hidden sets custom\n"
	"spawner 22 respawn 2 games all\n"
	"channel DROP_A symmetrical\n";

/* Every line kind reads into its place, with the defaults where an option is absent. */
static void test_reads(void)
{
	struct ae_spawner_file file;
	struct ae_spawner_edit const *e;
	char error[96];

	CHECK(read_text(sample, &file, error, sizeof error) == 0);
	CHECK(strcmp(file.map, "bloodgulch") == 0 && file.hash == 0x1a2b3c4dul && file.auto_convert);
	CHECK(file.edit_count == 7);
	e = &file.edits[0];
	CHECK(e->edit == AE_SPAWNER_EDIT_RETAG && e->n == 3);
	CHECK(e->cls == AE_SPAWNER_CLOSE && e->tier == AE_SPAWNER_POWER && e->logic == AE_SPAWNER_STATIC);
	CHECK(e->respawn == 120 && e->delay == 30 && e->kind == AE_SPAWNER_KIND_NONE);
	CHECK(e->has == (AE_SPAWNER_EDIT_HAS_CLASS | AE_SPAWNER_EDIT_HAS_TIER | AE_SPAWNER_EDIT_HAS_LOGIC |
		AE_SPAWNER_EDIT_HAS_RESPAWN | AE_SPAWNER_EDIT_HAS_DELAY));
	CHECK(e->sets == AE_SPAWNER_SETS_ALL && e->games == AE_SPAWNER_GAMES_ALL);
	e = &file.edits[1];
	CHECK(e->edit == AE_SPAWNER_EDIT_SKIP && e->n == 4);
	e = &file.edits[2];
	CHECK(e->edit == AE_SPAWNER_EDIT_SPLIT && e->n == 9);
	e = &file.edits[3];
	CHECK(e->edit == AE_SPAWNER_EDIT_PIN && e->n == 7 && strcmp(e->weapon, "stock:shotgun") == 0);
	e = &file.edits[4];
	CHECK(e->edit == AE_SPAWNER_EDIT_ADD && e->n == -1);
	CHECK(e->position[0] == 1.0f && e->position[1] == 2.5f && e->position[2] == -3.0f && e->facing == 90.0f);
	CHECK((e->has & AE_SPAWNER_EDIT_HAS_FACING) && e->kind == AE_SPAWNER_PAD && e->cls == AE_SPAWNER_HEAVY &&
		e->tier == AE_SPAWNER_POWER);
	CHECK(strcmp(e->channel, "DROP_A") == 0 && (e->has & AE_SPAWNER_EDIT_HAS_CHANNEL));
	CHECK(e->sets == 6ul && (e->has & AE_SPAWNER_EDIT_HAS_SETS));	/* comp 1<<1, btb 1<<2 */
	CHECK(e->games == (AE_SPAWNER_GAMES_CTF | AE_SPAWNER_GAMES_SLAYER) && (e->has & AE_SPAWNER_EDIT_HAS_GAMES));
	e = &file.edits[5];
	CHECK(e->edit == AE_SPAWNER_EDIT_RETAG && e->n == 21 && e->kind == AE_SPAWNER_HIDDEN && e->sets == (1ul << 15));
	e = &file.edits[6];
	CHECK(e->n == 22 && e->respawn == 5 && e->games == AE_SPAWNER_GAMES_ALL && (e->has & AE_SPAWNER_EDIT_HAS_GAMES));
	CHECK(file.symmetrical_count == 1 && ae_spawner_file_symmetrical(&file, "drop_a"));
	CHECK(!ae_spawner_file_symmetrical(&file, "DROP_B"));
	CHECK(!ae_spawner_file_symmetrical(0, "DROP_A"));
	CHECK(ae_spawner_file_for_map(&file, "BloodGulch", 0x1a2b3c4dul));
	CHECK(!ae_spawner_file_for_map(&file, "bloodgulch", 0x1a2b3c4eul));
	CHECK(!ae_spawner_file_for_map(&file, "hangemhigh", 0x1a2b3c4dul));
	CHECK(!ae_spawner_file_for_map(&file, 0, 0x1a2b3c4dul));
	/* auto off, an empty but valid file */
	CHECK(read_text(HEAD "auto off\n", &file, error, sizeof error) == 0);
	CHECK(!file.auto_convert && file.edit_count == 0);
	CHECK(read_text(HEAD, &file, error, sizeof error) == 0);
}

/* A line the reader does not know, or a bad value, refuses the whole file and names the line. */
static void test_refusals(void)
{
	struct ae_spawner_file file;
	char error[96];

	CHECK(read_text("spawners 1 bloodgulch 1a2b3c4\nformat 1\n", &file, error, sizeof error) == 1);	/* 7 hex digits */
	CHECK(read_text("spawners 1 bloodgulch 1a2b3c4g\nformat 1\n", &file, error, sizeof error) == 1);
	CHECK(read_text("spawners 2 bloodgulch 1a2b3c4d\nformat 1\n", &file, error, sizeof error) == 1);
	CHECK(read_text("layout 1 bloodgulch 1a2b3c4d\nformat 1\n", &file, error, sizeof error) == 1);
	CHECK(read_text("spawners 1 bloodgulch 1a2b3c4d\n", &file, error, sizeof error) == 1);	/* no format */
	CHECK(strstr(error, "format") != 0);
	CHECK(read_text("spawners 1 bloodgulch 1a2b3c4d\nformat 2\n", &file, error, sizeof error) == 2);
	CHECK(read_text("", &file, error, sizeof error) == 1);
	CHECK(read_text(HEAD "auto maybe\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "item rockets at 1 2 3\n", &file, error, sizeof error) == 3);	/* a layout line */
	CHECK(strstr(error, "item") != 0);
	CHECK(read_text(HEAD "spawner\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner skip\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner skip x\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 class\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 class MID\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 tier T1\n", &file, error, sizeof error) == 3);	/* not yet a tier */
	CHECK(read_text(HEAD "spawner 5 logic melt\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 kind pod\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 respawn 9999\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 colour red\n", &file, error, sizeof error) == 3);
	CHECK(strstr(error, "colour") != 0);
	CHECK(read_text(HEAD "spawner 5 sets\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 sets ranked\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 games ctf elimination\n", &file, error, sizeof error) == 3);	/* the second word */
	CHECK(read_text(HEAD "spawner 5 split now\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 weapon\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 weapon \"a b\"\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 5 weapon br extra\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner at 1 2\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner at 1 2 3\n", &file, error, sizeof error) == 3);	/* kind class tier needed */
	CHECK(read_text(HEAD "spawner at 1 2 3 kind rack class CLOSE\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner at 1 2 3 facing 400 kind rack class CLOSE tier BASE\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner at 1 2 3 kind rack class CLOSE tier BASE\n", &file, error, sizeof error) == 0);
	CHECK(read_text(HEAD "channel DROP_A\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "channel DROP_A mirrored\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner 4096 split\n", &file, error, sizeof error) == 3);
	CHECK(read_text(HEAD "spawner -1 split\n", &file, error, sizeof error) == 3);
	CHECK(ae_spawner_file_read(0, 0, &file, error, sizeof error) == 1);
	/* 256 lines are fine, the 257th spawner line is not */
	{
		static char text[AE_SPAWNER_TEXT_MAXIMUM_FILE_SIZE];
		int k;

		strcpy(text, HEAD);
		for (k = 0; k < 256; k++) strcat(text, "spawner 1 split\n");
		CHECK(read_text(text, &file, error, sizeof error) == 0 && file.edit_count == 256);
		strcat(text, "spawner 1 split\n");
		CHECK(read_text(text, &file, error, sizeof error) == 259);
	}
}

/* The words. */
static void test_words(void)
{
	CHECK(ae_spawner_kind_of_word("PAD") == AE_SPAWNER_PAD);
	CHECK(ae_spawner_kind_of_word("hidden") == AE_SPAWNER_HIDDEN);
	CHECK(ae_spawner_kind_of_word("plate") == -1);
	CHECK(ae_spawner_kind_of_word(0) == -1);
	CHECK(strcmp(ae_spawner_kind_word(AE_SPAWNER_CRATE), "crate") == 0);
	CHECK(strcmp(ae_spawner_kind_word(AE_SPAWNER_KIND_NONE), "") == 0);
	CHECK(ae_spawner_kind_tier(AE_SPAWNER_RACK) == AE_SPAWNER_BASE);
	CHECK(ae_spawner_kind_tier(AE_SPAWNER_PAD) == AE_SPAWNER_POWER);
	CHECK(ae_spawner_kind_tier(AE_SPAWNER_CRATE) == AE_SPAWNER_TIER_ANY);
	CHECK(ae_spawner_kind_tier(AE_SPAWNER_KIND_NONE) == AE_SPAWNER_TIER_ANY);
	CHECK(ae_spawner_games_bit("ALL") == AE_SPAWNER_GAMES_ALL);
	CHECK(ae_spawner_games_bit("king") == AE_SPAWNER_GAMES_KING);
	CHECK(ae_spawner_games_bit("race") == AE_SPAWNER_GAMES_RACE);
	CHECK(ae_spawner_games_bit("snd") == 0);
	CHECK(ae_spawner_sets_bit("casual") == 1ul && ae_spawner_sets_bit("comp") == 2ul && ae_spawner_sets_bit("btb") == 4ul);
	CHECK(ae_spawner_sets_bit("custom") == (1ul << 15));
	CHECK(ae_spawner_sets_bit("ranked") == 0);
}

int main(void)
{
	test_reads();
	test_refusals();
	test_words();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
