/* ae_profile_move_rules.c: export, the file's reading, the merge and replace rules. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../game/ae_bot_record_rules.h"
#include "../game/ae_career_record.h"
#include "../game/ae_profile_move_rules.h"
#include "../game/ae_service_record.h"
#include "../game/mcc_sha256.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)
#define HAS(text, part) do { if (!strstr((text), (part))) { printf("FAIL %s:%d missing \"%s\"\n", __FILE__, __LINE__, (part)); failures++; } } while (0)
#define LACKS(text, part) do { if (strstr((text), (part))) { printf("FAIL %s:%d unexpected \"%s\"\n", __FILE__, __LINE__, (part)); failures++; } } while (0)

enum { BIG = 1 << 20, NOW = 1795283200 };

/* a career.txt with its checksum */
static void sum(char *text)
{
	char line[32];

	snprintf(line, sizeof(line), "checksum = %08lx\n", ae_pm_fnv(text, strlen(text)));
	strcat(text, line);
}

static char const *const career_goat =
	"# AE career\nversion = 2\ncareer = team_slayer | 1450 6 6 12 7 0 31\nonline = ffa | 220 2 2 3 1 1 14 | 14\n"
	"match = goat | team-slayer | 1712 23 1795283100\nsocial = goat | team-slayer | 37 70 33 4 3 1795283100\n"
	"social = goat | ctf | 12 9 5 1 1 1795283000\nexperience = goat | 2\n";
static char const *const service_goat =
	"# AE service record\nversion = 2\nclock = 52\nplayer = goat | Goat | 52 | 52 26 1088 363 217 | 0 0 | 3:2\n"
	"playerx = goat | 2 14 5880\nmap = goat | blood gulch | 9 5 100 40\nlastgame = goat | flag | prisoner | 1 11 7 3 5 31 | battle rifle\n"
	"rival = goat | tiger | Tiger\nversus = goat | tiger | 4 11 6\n";
static char const *const bots_goat =
	"# AE bot records\nversion = 2\nclock = 41\nbot = rook7 | Rook7 | 3 41 team-slayer | 23 12 1 187 171 64 9 | 4 5 9 13 1 3 -1 | x:1\n"
	"botx = rook7 | 5 11 27660\nbotmap = rook7 | prisoner | 6 4 55 46\n"
	"bot = anna | Anna | 1 30 ctf | 8 3 0 40 50 10 2 | 1 1 1 1 1 1 1 | y:2\nversus = goat | rook7 | 14 6 8 9 6 31 23 41\n"
	"lastgame = goat | rook7 | team-slayer | prisoner | 0 12 8 3 5 4 -1\n";

static char c1[BIG], s1[BIG], b1[BIG], file_text[BIG], o1[BIG], o2[BIG], o3[BIG];

static void make_target_texts(void)
{
	snprintf(c1, sizeof(c1), "%s", "");
	snprintf(s1, sizeof(s1), "%s", "");
	snprintf(b1, sizeof(b1), "%s", "");
}

static int export_goat(char *out, int counts[3])
{
	snprintf(c1, sizeof(c1), "%s", career_goat);
	sum(c1);
	snprintf(b1, sizeof(b1), "%s", bots_goat);
	sum(b1);
	return ae_pm_export("Goat", "goat", c1, service_goat, b1, NOW, "1.0.0-rc5", out, BIG, counts);
}

static int merge_all(int mode, struct ae_pm_file const *file, char const *to, char const *c, char const *s, char const *b,
	struct ae_pm_summary *summary)
{
	size_t length;
	int answer = ae_pm_merge_career(mode, file, to, c, o1, BIG, &length, summary);

	if (answer)
		return answer;
	answer = ae_pm_merge_service(mode, file, to, s, o2, BIG, &length, summary);
	if (answer)
		return answer;
	return ae_pm_merge_bots(mode, file, to, b, o3, BIG, &length, summary);
}

static void test_round_trip(void)
{
	struct ae_pm_file file;
	int counts[3];
	struct ae_pm_summary summary;

	EQ(export_goat(file_text, counts), AE_PM_OK);
	HAS(file_text, "# AE profile\nversion = 1\nformat = aeprofile\nprofile = Goat\nkey = goat\n");
	HAS(file_text, "[career]\ncareer = team_slayer");
	LACKS(file_text, "pending");
	EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
	CHECK(!strcmp(file.key, "goat"));
	EQ(file.exported, NOW);
	/* REPLACE into an empty PC gives back the same lines */
	memset(&summary, 0, sizeof(summary));
	EQ(merge_all(AE_PM_REPLACE, &file, "goat", "", "", "", &summary), AE_PM_OK);
	HAS(o1, "match = goat | team-slayer | 1712 23 1795283100\n");
	HAS(o1, "experience = goat | 2\n");
	HAS(o1, "checksum = ");
	HAS(o2, "versus = goat | tiger | 4 11 6\n");
	HAS(o2, "lastgame = goat | flag | prisoner");
	HAS(o3, "bot = rook7 | Rook7");
	HAS(o3, "versus = goat | rook7 | 14 6 8 9 6 31 23 41\n");
	/* the checksum line of the output is the file's own FNV */
	{
		char *cut = strstr(o1, "checksum = ");
		char want[32];

		snprintf(want, sizeof(want), "checksum = %08lx\n", ae_pm_fnv(o1, (size_t)(cut - o1)));
		CHECK(!strcmp(cut, want));
	}
	/* the merged texts export to the same file body (the profile's lines survive the trip) */
	{
		static char again[BIG];
		int counts2[3];

		EQ(ae_pm_export("Goat", "goat", o1, o2, o3, NOW, "1.0.0-rc5", again, BIG, counts2), AE_PM_OK);
		CHECK(!strcmp(again, file_text));
	}
}

static void test_merge_units(void)
{
	struct ae_pm_file file;
	int counts[3];
	static char mine_c[BIG], mine_s[BIG], mine_b[BIG];

	EQ(export_goat(file_text, counts), AE_PM_OK);
	EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
	/* this PC has more games in team-slayer (80 > 70), fewer in ctf (5 < 9), the same in its ladder but a later clock */
	snprintf(mine_c, sizeof(mine_c), "%s",
		"# AE career\nversion = 2\ncareer = team_slayer | 1500 7 7 12 8 0 40\nsocial = goat | team-slayer | 50 80 40 4 3 1795283200\n"
		"social = goat | ctf | 5 5 3 0 0 1795283100\nsocial = tiger | ctf | 9 9 9 0 0 1795283000\nexperience = goat | 3\n"
		"pending = goat | ctf | 1795283000\n");
	sum(mine_c);
	snprintf(mine_s, sizeof(mine_s), "%s",
		"# AE service record\nversion = 2\nclock = 60\nplayer = goat | Goat | 60 | 60 30 900 300 100 | 0 0 | 3:1\n"
		"map = goat | other map | 1 1 1 1\nplayer = tiger | Tiger | 60 | 7 3 1 1 1 | 0 0 | \nrival = goat | cat | Cat\nversus = goat | cat | 9 1 1\n");
	snprintf(mine_b, sizeof(mine_b), "%s",
		"# AE bot records\nversion = 2\nclock = 90\nbot = rook7 | Rook7 | 3 80 ffa | 30 12 1 187 171 64 9 | 4 5 9 13 1 3 -1 | x:1\n"
		"botmap = rook7 | mine map | 1 1 1 1\nbot = zed | Zed | 1 2 ffa | 1 1 0 1 1 1 1 | 1 1 1 1 1 1 1 | z:1\n"
		"versus = goat | rook7 | 3 1 1 2 1 1 1 60\nlastgame = goat | rook7 | ffa | mymap | 0 1 1 1 1 1 -1\n"
		"versus = tiger | zed | 1 1 0 0 0 1 1 5\n");
	sum(mine_b);
	{
		struct ae_pm_summary summary;

		memset(&summary, 0, sizeof(summary));
		EQ(merge_all(AE_PM_MERGE, &file, "goat", mine_c, mine_s, mine_b, &summary), AE_PM_OK);
		/* career: the ladder entry has the same games (12): the later clock (40 over 31) is this PC's */
		HAS(o1, "career = team_slayer | 1500 7 7 12 8 0 40\n");
		LACKS(o1, "career = team_slayer | 1450");
		/* the file's online ladder entry is new here */
		HAS(o1, "online = ffa | 220 2 2 3 1 1 14 | 14\n");
		/* team-slayer: this PC's 80 games over the file's 70; ctf: the file's 9 over this PC's 5; units are not added */
		HAS(o1, "social = goat | team-slayer | 50 80 40 4 3 1795283200\n");
		HAS(o1, "social = goat | ctf | 12 9 5 1 1 1795283000\n");
		LACKS(o1, "social = goat | ctf | 5 5");
		HAS(o1, "social = tiger | ctf | 9 9 9 0 0 1795283000\n");
		/* the match line is only the file's: kept (this PC has none) */
		HAS(o1, "match = goat | team-slayer | 1712 23 1795283100\n");
		/* experience: this PC's answer stays; pending untouched */
		HAS(o1, "experience = goat | 3\n");
		LACKS(o1, "experience = goat | 2\n");
		HAS(o1, "pending = goat | ctf | 1795283000\n");
		/* service: the file's player group (52 games) loses to this PC's 60: group whole, with its own map line */
		HAS(o2, "player = goat | Goat | 60 | 60 30 900");
		HAS(o2, "map = goat | other map | 1 1 1 1\n");
		LACKS(o2, "blood gulch");
		LACKS(o2, "lastgame = goat | flag");
		HAS(o2, "player = tiger | Tiger");
		/* rivals: this PC's first, then the file's; versus lines follow their rival */
		HAS(o2, "rival = goat | cat | Cat\n");
		HAS(o2, "rival = goat | tiger | Tiger\n");
		HAS(o2, "versus = goat | cat | 9 1 1\n");
		HAS(o2, "versus = goat | tiger | 4 11 6\n");
		HAS(o2, "clock = 60\n");
		/* bots: rook7 has more games here (30 over 23): this PC's group, whole (its botmap, none of the file's botx) */
		HAS(o3, "bot = rook7 | Rook7 | 3 80 ffa | 30 12");
		HAS(o3, "botmap = rook7 | mine map | 1 1 1 1\n");
		LACKS(o3, "botx = rook7");
		LACKS(o3, "prisoner | 6 4 55 46");
		/* a bot only in the file is added, a bot only here is kept */
		HAS(o3, "bot = anna | Anna");
		HAS(o3, "bot = zed | Zed");
		/* versus of the profile and rook7: together 3+2 here, 14+9 in the file: the file's, with its lastgame */
		HAS(o3, "versus = goat | rook7 | 14 6 8 9 6 31 23 41\n");
		HAS(o3, "lastgame = goat | rook7 | team-slayer | prisoner |");
		LACKS(o3, "mymap");
		/* another profile's line stays */
		HAS(o3, "versus = tiger | zed | 1 1 0 0 0 1 1 5\n");
		CHECK(summary.from_file > 0 && summary.kept > 0);
		CHECK(summary.rows > 0);
		/* the three outputs read as the record files do: checksum lines right */
		{
			char *cut = strstr(o3, "checksum = ");
			char want[32];

			snprintf(want, sizeof(want), "checksum = %08lx\n", ae_pm_fnv(o3, (size_t)(cut - o3)));
			CHECK(!strcmp(cut, want));
		}
	}
	/* a tie in games and clock keeps this PC's */
	snprintf(mine_c, sizeof(mine_c), "%s", "# AE career\nversion = 2\ncareer = team_slayer | 1 0 0 12 1 0 31\n");
	sum(mine_c);
	EQ(merge_all(AE_PM_MERGE, &file, "goat", mine_c, "", "", NULL), AE_PM_OK);
	HAS(o1, "career = team_slayer | 1 0 0 12 1 0 31\n");
	/* REPLACE: the file's units win whatever the games; this PC's units of the profile the file lacks are dropped */
	snprintf(mine_c, sizeof(mine_c), "%s",
		"# AE career\nversion = 2\ncareer = team_slayer | 1500 7 7 99 8 0 40\nsocial = goat | team-slayer | 50 80 40 4 3 1795283200\n"
		"social = goat | slayer2 | 5 5 3 0 0 1795283100\nsocial = tiger | ctf | 9 9 9 0 0 1795283000\n");
	sum(mine_c);
	EQ(merge_all(AE_PM_REPLACE, &file, "goat", mine_c, "", "", NULL), AE_PM_OK);
	HAS(o1, "career = team_slayer | 1450 6 6 12 7 0 31\n");
	HAS(o1, "social = goat | team-slayer | 37 70 33 4 3 1795283100\n");
	LACKS(o1, "slayer2");
	HAS(o1, "social = tiger | ctf | 9 9 9 0 0 1795283000\n");
	/* the career XP and challenge lines of this PC stay through either merge (an import does not carry them) */
	snprintf(mine_c, sizeof(mine_c), "%s",
		"# AE career\nversion = 3\ncareer = team_slayer | 1 0 0 12 1 0 31\nxp = goat | 5230 1795283000\n"
		"challenges = goat | 20415 2916 | d_win2 1 0 | - 0 0 | - 0 0 | - 0 0 | - 0 0 | - 0 0\n");
	sum(mine_c);
	EQ(merge_all(AE_PM_MERGE, &file, "goat", mine_c, "", "", NULL), AE_PM_OK);
	HAS(o1, "xp = goat | 5230 1795283000\n");
	HAS(o1, "challenges = goat | 20415 2916 | d_win2 1 0");
	EQ(merge_all(AE_PM_REPLACE, &file, "goat", mine_c, "", "", NULL), AE_PM_OK);
	HAS(o1, "xp = goat | 5230 1795283000\n");
	HAS(o1, "career = team_slayer | 1450 6 6 12 7 0 31\n");
}

static void test_rename(void)
{
	struct ae_pm_file file;
	int counts[3];
	static char other_c[BIG];

	EQ(export_goat(file_text, counts), AE_PM_OK);
	EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
	snprintf(other_c, sizeof(other_c), "%s", "# AE career\nversion = 2\nsocial = den | ctf | 1 1 1 0 0 1795283000\n");
	sum(other_c);
	EQ(merge_all(AE_PM_MERGE, &file, "den", other_c, "", "", NULL), AE_PM_OK);
	HAS(o1, "social = den | team-slayer | 37 70 33 4 3 1795283100\n");
	HAS(o1, "social = den | ctf | 12 9 5 1 1 1795283000\n");
	LACKS(o1, "goat");
	HAS(o1, "experience = den | 2\n");
	EQ(merge_all(AE_PM_MERGE, &file, "den", "", "", "", NULL), AE_PM_OK);
	/* the key's own lines are rewritten, the opponent's name is not */
	HAS(o2, "player = den | Goat | 52");
	HAS(o2, "rival = den | tiger | Tiger\n");
	HAS(o2, "versus = den | tiger | 4 11 6\n");
	HAS(o3, "versus = den | rook7 |");
	HAS(o3, "bot = rook7 | Rook7");
	LACKS(o3, "goat");
}

static void test_other_profiles_untouched(void)
{
	struct ae_pm_file file;
	int counts[3];
	static char mine_s[BIG];
	char const *tiger_lines = "player = tiger | Tiger | 60 | 7 3 1 1 1 | 0 0 | \nplayerx = tiger | 1 2 3\nmap = tiger | a | 1 1 1 1\n"
		"rival = tiger | goat | Goat\nversus = tiger | goat | 1 1 0\n";

	EQ(export_goat(file_text, counts), AE_PM_OK);
	EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
	snprintf(mine_s, sizeof(mine_s), "# AE service record\nversion = 2\nclock = 5\n%s", tiger_lines);
	EQ(merge_all(AE_PM_MERGE, &file, "goat", "", mine_s, "", NULL), AE_PM_OK);
	/* every one of tiger's lines is there, whole */
	{
		char const *at = tiger_lines;

		while (*at)
		{
			char line[256];
			size_t length = strcspn(at, "\n");

			memcpy(line, at, length);
			line[length] = 0;
			HAS(o2, line);
			at += length + 1;
		}
	}
	/* a rival of tiger's that is named goat (the opponent) is not rewritten */
	HAS(o2, "rival = tiger | goat | Goat\n");
}

static void test_refusals(void)
{
	struct ae_pm_file file;
	int counts[3];
	static char edited[BIG], big[AE_PM_FILE_MAX + 100];
	char *at;

	EQ(export_goat(file_text, counts), AE_PM_OK);
	/* one changed character in the body: the check fails */
	snprintf(edited, sizeof(edited), "%s", file_text);
	at = strstr(edited, "1712");
	at[0] = '9';
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_CHECK);
	/* the check line's own character */
	snprintf(edited, sizeof(edited), "%s", file_text);
	at = strstr(edited, "check = ") + 9;
	*at = *at == 'a' ? 'b' : 'a';
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_CHECK);
	/* a newer version is its own answer, an unknown format another */
	snprintf(edited, sizeof(edited), "%s", file_text);
	at = strstr(edited, "version = 1");
	at[10] = '2';
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_NEWER);
	snprintf(edited, sizeof(edited), "%s", file_text);
	at = strstr(edited, "aeprofile");
	at[0] = 'x';
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_FORMAT);
	/* not a profile, empty, no [end] */
	EQ(ae_pm_parse("hello\n", 6, NOW, &file), AE_PM_ERR_FORMAT);
	EQ(ae_pm_parse("", 0, NOW, &file), AE_PM_ERR_FORMAT);
	snprintf(edited, sizeof(edited), "%s", file_text);
	*strstr(edited, "[end]") = 0;
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_FORMAT);
	/* a section twice or out of order */
	snprintf(edited, sizeof(edited), "# AE profile\nversion = 1\nformat = aeprofile\nprofile = a\nkey = a\n[service]\n[career]\n[bots]\n[end]\ncheck = 0\n");
	EQ(ae_pm_parse(edited, strlen(edited), NOW, &file), AE_PM_ERR_FORMAT);
	/* the caps: one byte past the file's, a line past 4096, past 12000 lines */
	memset(big, 'x', sizeof(big));
	EQ(ae_pm_parse(big, AE_PM_FILE_MAX + 1, NOW, &file), AE_PM_ERR_SIZE);
	{
		char *long_line = malloc(AE_PM_LINE_MAX + 200);

		snprintf(long_line, AE_PM_LINE_MAX + 200, "# AE profile\nversion = 1\n");
		memset(long_line + strlen(long_line), 'a', AE_PM_LINE_MAX + 1);
		long_line[strlen("# AE profile\nversion = 1\n") + AE_PM_LINE_MAX + 1] = 0;
		EQ(ae_pm_parse(long_line, strlen(long_line), NOW, &file), AE_PM_ERR_SIZE);
		free(long_line);
	}
	{
		char *many = malloc(AE_PM_FILE_MAX);
		int index;
		size_t length = 0;

		length += (size_t)sprintf(many + length, "# AE profile\n");
		for (index = 0; index < AE_PM_LINES_MAX + 5; index++)
			length += (size_t)sprintf(many + length, "# x\n");
		EQ(ae_pm_parse(many, length, NOW, &file), AE_PM_ERR_SIZE);
		free(many);
	}
}

/* a file written by hand (the exporter copies only the lines it should): the sections as given, a right check */
static void hand_file_key(char const *key, char const *career, char const *service, char const *bots, char *out)
{
	struct mcc_sha256 context;
	unsigned char digest[32];
	size_t length;
	int index;

	snprintf(out, BIG, "# AE profile\nversion = 1\nformat = aeprofile\nprofile = Goat\nkey = %s\nexported = 1\nbuild = t\n"
		"[career]\n%s[service]\n%s[bots]\n%s[end]\n", key, career, service, bots);
	length = strlen(out);
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, out, (int)length);
	mcc_sha256_end(&context, digest);
	strcat(out, "check = ");
	for (index = 0; index < 32; index++)
		sprintf(out + strlen(out), "%02x", digest[index]);
	strcat(out, "\n");
}

static void hand_file(char const *career, char const *service, char const *bots, char *out)
{
	hand_file_key("goat", career, service, bots, out);
}

static int parse_body(char const *career, char const *service, char const *bots, unsigned long now)
{
	struct ae_pm_file file;
	static char out[BIG];

	hand_file(career, service, bots, out);
	return ae_pm_parse(out, strlen(out), now, &file);
}

static void test_values(void)
{
	/* a file the exporter writes holds whatever lines the texts have: the reader is the judge */
	EQ(parse_body("match = goat | ctf | 1 5 1795283100\n", "", "", NOW), AE_PM_OK);
	EQ(parse_body("social = goat | ctf | 1 9 10 0 0 1795283100\n", "", "", NOW), AE_PM_ERR_VALUE);   /* wins > games */
	EQ(parse_body("social = goat | ctf | 1 2000000 1 0 0 1795283100\n", "", "", NOW), AE_PM_ERR_VALUE);   /* games */
	EQ(parse_body("social = goat | ctf | 1 5 1 0 0 1895283100\n", "", "", NOW), AE_PM_ERR_VALUE);   /* a clock past now */
	EQ(parse_body("career = a | 1 99 99 5 1 0 3\n", "", "", NOW), AE_PM_ERR_VALUE);   /* a level past 49 */
	EQ(parse_body("career = a | 1 2 2 5 1 0 3\ncareer = a | 1 2 2 5 1 0 3\n", "", "", NOW), AE_PM_ERR_VALUE);   /* a unit twice */
	EQ(parse_body("match = goat | ctf | 1 5 1\nmatch = goat | ctf | 1 6 1\n", "", "", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("career = a | 1 2 2 5 1 0 3\n", "player = goat | G | 1 | 5 9 0 0 0 | 0\n", "", NOW), AE_PM_ERR_VALUE);   /* wins > games */
	EQ(parse_body("career = a | 1 2 2 5 1 0 3\n", "player = goat | G | 1 | 5 1 0 0 0 | 0\n", "bot = a | A | 1 2 x | 5 4 4 1 1 1 1\n", NOW), AE_PM_ERR_VALUE);   /* wins + ties > games */
	EQ(parse_body("career = a | 1 2 2 5 1 0 3\n", "player = goat | G | 1 | 5 1 0 0 0 | 0\n", "bot = a | A | 1 2 x | 5 4 0 1 1 1 1\nbot = a | A | 1 2 x | 5 4 0 1 1 1 1\n", NOW), AE_PM_ERR_VALUE);
	/* a keyed line of another profile in a section */
	EQ(parse_body("match = tiger | ctf | 1 5 1\n", "", "", NOW), AE_PM_ERR_VALUE);
	/* a control character in a line */
	EQ(parse_body("career = a\x01 | 1 2 2 5 1 0 3\n", "", "", NOW), AE_PM_ERR_FORMAT);
	/* unknown line kinds are skipped, not fatal (and not copied by a merge) */
	EQ(parse_body("career = a | 1 2 2 5 1 0 3\nfuture = 1 | 2\n", "", "", NOW), AE_PM_OK);
}

static void test_empty_and_keys(void)
{
	static char out[BIG];
	int counts[3];

	EQ(ae_pm_export("Goat", "goat", "", "", "", NOW, "t", out, BIG, counts), AE_PM_ERR_EMPTY);
	EQ(ae_pm_export("", "goat", "", "", "", NOW, "t", out, BIG, counts), AE_PM_ERR_KEY);
	EQ(ae_pm_export("Goat", "go|at", "", "", "", NOW, "t", out, BIG, counts), AE_PM_ERR_KEY);
	/* only the ladders of the PC: the profile has nothing of its own */
	make_target_texts();
	EQ(ae_pm_export("Goat", "goat", "career = a | 1 2 2 5 1 0 3\n", "", "", NOW, "t", out, BIG, counts), AE_PM_ERR_EMPTY);
	/* a buffer too small */
	snprintf(c1, sizeof(c1), "%s", career_goat);
	sum(c1);
	EQ(ae_pm_export("Goat", "goat", c1, service_goat, "", NOW, "t", out, 100, counts), AE_PM_ERR_ROOM);
	/* a merge into a buffer too small, and with a bad key */
	{
		struct ae_pm_file file;
		size_t length;

		EQ(export_goat(file_text, counts), AE_PM_OK);
		EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
		EQ(ae_pm_merge_career(AE_PM_REPLACE, &file, "goat", "", o1, 20, &length, NULL), AE_PM_ERR_ROOM);
		EQ(ae_pm_merge_career(AE_PM_REPLACE, &file, "", "", o1, BIG, &length, NULL), AE_PM_ERR_KEY);
	}
}

static void test_rival_cap(void)
{
	struct ae_pm_file file;
	static char service[BIG], out[BIG], mine_s[BIG];
	int index, counts[3];
	size_t length = 0;

	length += (size_t)sprintf(service + length, "# AE service record\nversion = 2\nclock = 3\nplayer = goat | G | 3 | 3 1 0 0 0 | 0\n");
	for (index = 0; index < 8; index++)
		length += (size_t)sprintf(service + length, "rival = goat | f%d | F\n", index);
	EQ(ae_pm_export("Goat", "goat", "", service, "", NOW, "t", out, BIG, counts), AE_PM_OK);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	snprintf(mine_s, sizeof(mine_s), "# AE service record\nversion = 2\nclock = 1\nrival = goat | m1 | M\nrival = goat | m2 | M\n");
	EQ(ae_pm_merge_service(AE_PM_MERGE, &file, "goat", mine_s, o2, BIG, &length, NULL), AE_PM_OK);
	/* this PC's two first, the file's fill to 8 */
	HAS(o2, "rival = goat | m1 | M\n");
	HAS(o2, "rival = goat | m2 | M\n");
	HAS(o2, "rival = goat | f5 | F\n");
	LACKS(o2, "rival = goat | f6 | F\n");
}

static void test_backup_restores(void)
{
	/* a backup is a file of this PC's progress: REPLACE from it puts the same tables back */
	struct ae_pm_file file, back;
	int counts[3];
	static char mine_c[BIG], mine_s[BIG], mine_b[BIG], backup[BIG];
	static char r1[BIG], r2[BIG], r3[BIG];

	EQ(export_goat(file_text, counts), AE_PM_OK);
	EQ(ae_pm_parse(file_text, strlen(file_text), NOW, &file), AE_PM_OK);
	snprintf(mine_c, sizeof(mine_c), "%s", "# AE career\nversion = 2\nsocial = goat | ctf | 5 5 3 0 0 1795283100\nexperience = goat | 3\n");
	sum(mine_c);
	snprintf(mine_s, sizeof(mine_s), "%s", "# AE service record\nversion = 2\nclock = 7\nplayer = goat | Goat | 7 | 7 4 1 1 1 | 0\nrival = goat | cat | Cat\n");
	snprintf(mine_b, sizeof(mine_b), "%s", "# AE bot records\nversion = 2\nclock = 3\nbot = zed | Zed | 1 2 ffa | 1 1 0 1 1 1 1 | 1 1 1 1 1 1 1 | z:1\nversus = goat | zed | 1 1 0 0 0 1 1 2\n");
	sum(mine_b);
	EQ(ae_pm_export("Goat", "goat", mine_c, mine_s, mine_b, NOW, "t", backup, BIG, counts), AE_PM_OK);
	EQ(merge_all(AE_PM_MERGE, &file, "goat", mine_c, mine_s, mine_b, NULL), AE_PM_OK);
	memcpy(r1, o1, BIG); memcpy(r2, o2, BIG); memcpy(r3, o3, BIG);
	EQ(ae_pm_parse(backup, strlen(backup), NOW, &back), AE_PM_OK);
	EQ(merge_all(AE_PM_REPLACE, &back, "goat", r1, r2, r3, NULL), AE_PM_OK);
	/* the profile's own lines are back as they were */
	HAS(o1, "social = goat | ctf | 5 5 3 0 0 1795283100\n");
	HAS(o1, "experience = goat | 3\n");
	LACKS(o1, "social = goat | team-slayer");
	HAS(o2, "player = goat | Goat | 7 | 7 4 1 1 1 | 0\n");
	LACKS(o2, "blood gulch");
	HAS(o2, "rival = goat | cat | Cat\n");
	LACKS(o2, "rival = goat | tiger");
}

static void test_empty_profile_section(void)
{
	struct ae_pm_file file;
	static char out[BIG];
	int counts[3];

	/* a profile with a service line only: sections that are empty read as empty */
	EQ(ae_pm_export("Goat", "goat", "", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", "", NOW, "t", out, BIG, counts), AE_PM_OK);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	EQ(file.length[0], 0);
	EQ(merge_all(AE_PM_MERGE, &file, "goat", "", "", "", NULL), AE_PM_OK);
	HAS(o1, "# AE career\nversion = 2\nchecksum = ");
	HAS(o2, "player = goat | G | 1");
	HAS(o3, "# AE bot records\nversion = 2\nclock = 0\nchecksum = ");
}


/* a file with an extra section after [bots] is refused (it once wrote past a three element array) */
static void test_extra_section(void)
{
	struct ae_pm_file file;
	static char out[BIG];

	hand_file("", "", "[xyz]\n", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_FORMAT);
	hand_file("", "", "[bots]\n", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_FORMAT);
	hand_file("[career]\n", "", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_FORMAT);
}

/* a key with spaces round it is not the key: refused, and a rewrite never copies from the key's length */
static void test_key_spaces(void)
{
	struct ae_pm_file file;
	static char out[BIG];
	size_t length;

	hand_file_key("ab", "match =   ab | ctf | 1 5 1795283100\n", "", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_VALUE);
	hand_file_key("ab", "match = ab  | ctf | 1 5 1795283100\n", "", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_VALUE);
	hand_file_key("ab", "match = ab | ctf | 1 5 1795283100\n", "player =  ab | G | 1 | 1 1 0 0 0 | 0\n", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_VALUE);
	hand_file_key("ab", "social = ab |  ctf | 1 5 1 0 0 1795283100\n", "", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_VALUE);
	hand_file_key("ab", "", "", "versus = ab | bot  | 1 1 0 0 0 1 1 1\n", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_VALUE);
	/* a right file goes into another key whole: nothing of the old key stays, nothing is cut or joined */
	hand_file_key("ab", "match = ab | ctf | 1 5 1795283100\nexperience = ab\n", "", "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	EQ(ae_pm_merge_career(AE_PM_MERGE, &file, "goat", "", o1, BIG, &length, NULL), AE_PM_OK);
	HAS(o1, "match = goat | ctf | 1 5 1795283100\n");
	HAS(o1, "experience = goat\n");
	LACKS(o1, "goatab");
	LACKS(o1, "ab");
}

static void test_clock_caps(void)
{
	char const *player = "player = goat | G | 1 | 1 1 0 0 0 | 0\n";
	char service[512];

	snprintf(service, sizeof(service), "clock = 999999999999\n%s", player);
	EQ(parse_body("", service, "", NOW), AE_PM_ERR_VALUE);
	snprintf(service, sizeof(service), "clock = 1000000001\n%s", player);
	EQ(parse_body("", service, "", NOW), AE_PM_ERR_VALUE);
	snprintf(service, sizeof(service), "clock = 1000000000\n%s", player);
	EQ(parse_body("", service, "", NOW), AE_PM_OK);
	EQ(parse_body("", "player = goat | G | 2000000000 | 1 1 0 0 0 | 0\n", "", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("", player, "clock = 4000000000\n", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("", player, "clock = 12\nbot = a | A | 1 2000000000 x | 1 0 0 1 1 1 1\n", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("", player, "clock = 12\nbot = a | A | 1 2 x | 1 0 0 1 1 1 1\nversus = goat | a | 1 0 0 0 0 1 1 2000000000\n", NOW),
		AE_PM_ERR_VALUE);
	EQ(parse_body("career = a | 1 2 2 5 1 0 2000000000\n", player, "", NOW), AE_PM_ERR_VALUE);
	/* a clock does not grow past the cap through a merge either: the larger of two clocks that are both within it */
	{
		struct ae_pm_file file;
		static char out[BIG];
		size_t length;

		snprintf(service, sizeof(service), "clock = 1000000000\n%s", player);
		hand_file("", service, "", out);
		EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
		EQ(ae_pm_merge_service(AE_PM_MERGE, &file, "goat", "version = 2\nclock = 7\n", o2, BIG, &length, NULL), AE_PM_OK);
		HAS(o2, "clock = 1000000000\n");
	}
}

static void test_number_limits(void)
{
	/* a number of more than 31 bits (a long is 32 bits on Windows) is no number: the line is refused, never wrapped */
	EQ(parse_body("social = goat | ctf | 1 99999999999 1 0 0 1795283100\n", "", "", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("social = goat | ctf | 1 4294967296 1 0 0 1795283100\n", "", "", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("social = goat | ctf | 1 2147483648 1 0 0 1795283100\n", "", "", NOW), AE_PM_ERR_VALUE);
	EQ(parse_body("social = goat | ctf | 1 5 1 0 0 1795283100\n", "", "", NOW), AE_PM_OK);
	/* ten digits that fit are a clock (the clock of a game: a unix time) */
	EQ(parse_body("match = goat | ctf | 1 5 2147483647\n", "", "", 0), AE_PM_OK);
	EQ(parse_body("match = goat | ctf | 1 5 2147483648\n", "", "", 0), AE_PM_ERR_VALUE);
}

/* a file as large as the readers keep parses at once: the duplicate check is not quadratic */
static void test_speed(void)
{
	static char bots[BIG], out[BIG];
	struct ae_pm_file file;
	size_t length = 0;
	int index;
	clock_t begin;
	double seconds;

	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\nbotx = b%d | 1 2 3\nbotkinds = b%d | 1 2\n"
			"botmedals = b%d | 1:2\nbotmap = b%d | m1 | 1 1 1 1\n", index, index, index, index, index);
	for (index = 0; index < AE_BOT_RECORD_VERSUS; index++)
		length += (size_t)sprintf(bots + length, "versus = goat | b%d | 1 0 0 0 0 1 1 1\n", index);
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	begin = clock();
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	seconds = (double)(clock() - begin) / CLOCKS_PER_SEC;
	CHECK(seconds < 0.5);
	/* the same unit twice at the far end is still found: the last versus line made the first one's twin */
	{
		char *last = strstr(bots, "versus = goat | b1023 | 1 0 0 0 0 1 1 1\n");

		CHECK(last != NULL);
		memcpy(last, "versus = goat | b1000 | 1 0 0 0 0 1 1 1\n", 40);
	}
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	CHECK(ae_pm_parse(out, strlen(out), NOW, &file) != AE_PM_OK);
}

static void test_dropped_and_versions(void)
{
	struct ae_pm_file file;
	struct ae_pm_summary summary;
	static char service[8192], mine[8192], out[BIG];
	size_t length = 0;
	char head[80];
	int index;

	length += (size_t)sprintf(service + length, "player = goat | G | 1 | 1 1 0 0 0 | 0\n");
	for (index = 0; index < 6; index++)
		length += (size_t)sprintf(service + length, "rival = goat | r%d | R\nversus = goat | r%d | 1 0 0\n", index, index);
	hand_file("", service, "versus = goat | ghost | 1 0 0 0 0 1 1 1\nlastgame = goat | ghost | ffa | m | 0 1 1 1 1 1 -1\n", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	/* this PC already has five rivals: three of the file's fit the readers' eight, the other three go with their versus lines */
	length = (size_t)sprintf(mine, "# AE service record\nversion = 2\nclock = 1\n");
	for (index = 0; index < 5; index++)
		length += (size_t)sprintf(mine + length, "rival = goat | m%d | M\n", index);
	memset(&summary, 0, sizeof(summary));
	EQ(merge_all(AE_PM_MERGE, &file, "goat", "", mine, "", &summary), AE_PM_OK);
	/* three rivals past the cap with their versus lines, and a bot's two lines for a bot that is not there */
	EQ(summary.dropped, 8);
	HAS(o2, "rival = goat | r2");
	LACKS(o2, "rival = goat | r3");
	LACKS(o3, "ghost");
	/* the headers carry the record files' own version */
	snprintf(head, sizeof(head), "# AE career\nversion = %d\n", AE_CAREER_VERSION);
	HAS(o1, head);
	snprintf(head, sizeof(head), "# AE service record\nversion = %d\n", AE_SR_VERSION);
	HAS(o2, head);
	snprintf(head, sizeof(head), "# AE bot records\nversion = %d\n", AE_BOT_RECORD_VERSION);
	HAS(o3, head);
	/* a read error is not "too large" */
	CHECK(strcmp(ae_pm_error_text(AE_PM_ERR_READ), ae_pm_error_text(AE_PM_ERR_SIZE)) != 0);
}

static void test_backup_container(void)
{
	static char big_a[3 << 20], out[(3 << 20) + 4096], again[(3 << 20) + 4096];
	char const *const names[3] = { "career.txt", "service-record.txt", "bot_records.txt" };
	struct ae_pm_raw raw[3], back[3];
	size_t length;
	int index;

	memset(big_a, 'x', sizeof(big_a));
	big_a[sizeof(big_a) - 1] = 0;
	/* a file of 3 MB (past a profile file's cap), an empty one, one that was not there */
	raw[0].text = big_a;
	raw[0].length = (long)strlen(big_a);
	raw[1].text = "";
	raw[1].length = 0;
	raw[2].text = "";
	raw[2].length = -1;
	EQ(ae_pm_backup_write(raw, names, out, sizeof(out), &length), AE_PM_OK);
	EQ(ae_pm_backup_read(out, length, names, back), AE_PM_OK);
	EQ(back[0].length, raw[0].length);
	CHECK(!memcmp(back[0].text, big_a, (size_t)raw[0].length));
	EQ(back[1].length, 0);
	EQ(back[2].length, -1);
	/* bytes with newlines and the marker words inside a file are only bytes */
	raw[1].text = "[end]\ncheck = 00\nfile = career.txt | present | 3\n";
	raw[1].length = (long)strlen(raw[1].text);
	EQ(ae_pm_backup_write(raw, names, again, sizeof(again), &length), AE_PM_OK);
	EQ(ae_pm_backup_read(again, length, names, back), AE_PM_OK);
	EQ(back[1].length, raw[1].length);
	CHECK(!memcmp(back[1].text, raw[1].text, (size_t)raw[1].length));
	/* a byte changed, a cut text, a wrong name, a buffer too small */
	again[length / 2] ^= 1;
	EQ(ae_pm_backup_read(again, length, names, back), AE_PM_ERR_BACKUP);
	EQ(ae_pm_backup_read(out, length / 2, names, back), AE_PM_ERR_BACKUP);
	{
		char const *const other[3] = { "career.txt", "service-record.txt", "other.txt" };

		EQ(ae_pm_backup_read(out, length, other, back), AE_PM_ERR_BACKUP);
	}
	EQ(ae_pm_backup_write(raw, names, again, 1000, &length), AE_PM_ERR_ROOM);
	for (index = 0; index < 3; index++)
		raw[index].length = -1;
	EQ(ae_pm_backup_write(raw, names, again, sizeof(again), &length), AE_PM_OK);
	EQ(ae_pm_backup_read(again, length, names, back), AE_PM_OK);
	EQ(back[0].length, -1);
}

static void test_backup_names(void)
{
	char stem[64], other[64], name[96];
	char const *names[8];
	int victims[8], count;

	ae_pm_backup_stem("goat", stem, sizeof(stem));
	ae_pm_backup_stem("goat-2", other, sizeof(other));
	CHECK(strcmp(stem, other) != 0);
	HAS(stem, "goat-");
	CHECK(ae_pm_backup_name(stem, "20261121-193012", 1, ".aebackup", name, sizeof(name)));
	CHECK(ae_pm_backup_matches(name, stem, ".aebackup"));
	CHECK(!ae_pm_backup_matches(name, stem, ".aeprofile"));
	CHECK(!ae_pm_backup_matches(name, other, ".aebackup"));
	/* a clash adds -2, -3 */
	CHECK(ae_pm_backup_name(stem, "20261121-193012", 2, ".aebackup", name, sizeof(name)));
	HAS(name, "193012-2.aebackup");
	CHECK(ae_pm_backup_matches(name, stem, ".aebackup"));
	/* the other key's backups do not match this stem even where the names start the same */
	{
		char theirs[96];

		CHECK(ae_pm_backup_name(other, "20261121-193012", 1, ".aebackup", theirs, sizeof(theirs)));
		CHECK(!ae_pm_backup_matches(theirs, stem, ".aebackup"));
		CHECK(!ae_pm_backup_matches("goat-extra.aebackup", stem, ".aebackup"));
	}
	/* a key with other characters is a plain file name part */
	ae_pm_backup_stem("a b/c\\d", stem, sizeof(stem));
	CHECK(!strchr(stem, ' ') && !strchr(stem, '/') && !strchr(stem, '\\'));
	ae_pm_backup_stem(".hidden", stem, sizeof(stem));
	CHECK(stem[0] != '.');
	/* the prune keeps the newest keep of the stem's own */
	ae_pm_backup_stem("goat", stem, sizeof(stem));
	ae_pm_backup_stem("goat-2", other, sizeof(other));
	{
		static char list[8][96];

		ae_pm_backup_name(stem, "20261121-100000", 1, ".aebackup", list[0], 96);
		ae_pm_backup_name(stem, "20261121-110000", 1, ".aebackup", list[1], 96);
		ae_pm_backup_name(stem, "20261121-110000", 2, ".aebackup", list[2], 96);
		ae_pm_backup_name(stem, "20261120-090000", 1, ".aebackup", list[3], 96);
		ae_pm_backup_name(other, "20250101-000000", 1, ".aebackup", list[4], 96);
		snprintf(list[5], 96, "%s", "notes.txt");
		for (count = 0; count < 6; count++)
			names[count] = list[count];
		count = ae_pm_prune_select(names, 6, stem, ".aebackup", 2, NULL, victims, 8);
		/* the stem's four: -2 clash newest, 11:00, 10:00, the 20th: the last two go; the other key's one stays */
		EQ(count, 2);
		CHECK((victims[0] == 0 && victims[1] == 3) || (victims[0] == 3 && victims[1] == 0));
		EQ(ae_pm_prune_select(names, 6, stem, ".aebackup", 5, NULL, victims, 8), 0);
	}
}

/* the files' operations faked, each of which can fail */
struct fake
{
	char files[3][40];
	int present[3];
	char staged[3][40];
	int has_staged[3];
	int fail_stage, fail_marker, fail_commit, fail_put;
	int marker;
};

static int f_stage(void *context, int which, char const *text)
{
	struct fake *fake = context;

	if (which == fake->fail_stage)
		return 0;
	if (text)
	{
		snprintf(fake->staged[which], sizeof(fake->staged[0]), "%s", text);
		fake->has_staged[which] = 1;
	}
	return 1;
}

static int f_commit(void *context, int which, int remove)
{
	struct fake *fake = context;

	if (which == fake->fail_commit)
		return 0;
	if (remove)
		fake->present[which] = 0;
	else
	{
		memcpy(fake->files[which], fake->staged[which], sizeof(fake->files[0]));
		fake->present[which] = 1;
		fake->has_staged[which] = 0;
	}
	return 1;
}

static void f_discard(void *context, int which)
{
	((struct fake *)context)->has_staged[which] = 0;
}

static int f_put(void *context, int which, char const *text, long length)
{
	struct fake *fake = context;

	if (which == fake->fail_put)
		return 0;
	if (length < 0)
		fake->present[which] = 0;
	else
	{
		snprintf(fake->files[which], sizeof(fake->files[0]), "%.*s", (int)length, text);
		fake->present[which] = 1;
	}
	return 1;
}

static int f_marker(void *context, int on)
{
	struct fake *fake = context;

	if (on && fake->fail_marker)
		return 0;
	fake->marker = on;
	return 1;
}

static void fake_start(struct fake *fake)
{
	memset(fake, 0, sizeof(*fake));
	fake->fail_stage = fake->fail_commit = fake->fail_put = -1;
	snprintf(fake->files[0], 40, "old career");
	snprintf(fake->files[1], 40, "old service");
	fake->present[0] = fake->present[1] = 1;
	/* (the bot file is not there) */
}

static void test_apply(void)
{
	static struct ae_pm_ops const ops = { f_stage, f_commit, f_discard, f_put, f_marker };
	struct ae_pm_raw raw[3] = { { "old career", 10 }, { "old service", 11 }, { "", -1 } };
	char const *texts[3] = { "new career", "new service", "new bots" };
	struct fake fake;
	int which;

	fake_start(&fake);
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_OK);
	CHECK(!strcmp(fake.files[0], "new career") && !strcmp(fake.files[1], "new service") && !strcmp(fake.files[2], "new bots"));
	EQ(fake.marker, 0);
	/* a file staged that fails: nothing changed, nothing left staged */
	for (which = 0; which < 3; which++)
	{
		int other;

		fake_start(&fake);
		fake.fail_stage = which;
		EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_UNCHANGED);
		CHECK(!strcmp(fake.files[0], "old career") && !strcmp(fake.files[1], "old service") && !fake.present[2]);
		for (other = 0; other < 3; other++)
			EQ(fake.has_staged[other], 0);
		EQ(fake.marker, 0);
	}
	fake_start(&fake);
	fake.fail_marker = 1;
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_UNCHANGED);
	CHECK(!strcmp(fake.files[0], "old career") && !fake.has_staged[0] && !fake.has_staged[1] && !fake.has_staged[2]);
	/* a commit that fails: the files already changed are put back, the absent one is taken away again */
	for (which = 0; which < 3; which++)
	{
		fake_start(&fake);
		fake.fail_commit = which;
		EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_ROLLED_BACK);
		CHECK(!strcmp(fake.files[0], "old career") && !strcmp(fake.files[1], "old service") && !fake.present[2]);
		EQ(fake.has_staged[0] + fake.has_staged[1] + fake.has_staged[2], 0);
		EQ(fake.marker, 0);
	}
	/* a commit that fails and a file that cannot be put back: said so, the mark stays for the next start */
	fake_start(&fake);
	fake.fail_commit = 2;
	fake.fail_put = 1;
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_BROKEN);
	EQ(fake.marker, 1);
	/* the recovery of a cut off write puts the three back from the backup's bytes */
	fake.fail_put = -1;
	CHECK(ae_pm_rollback(&ops, &fake, raw));
	CHECK(!strcmp(fake.files[0], "old career") && !strcmp(fake.files[1], "old service") && !fake.present[2]);
	fake.fail_put = 0;
	CHECK(!ae_pm_rollback(&ops, &fake, raw));
	/* UNDO writes the bytes of a backup: a file that the import added is taken away */
	fake_start(&fake);
	fake.present[2] = 1;
	snprintf(fake.files[2], 40, "added by the import");
	{
		char const *undo[3] = { "old career", "old service", NULL };
		struct ae_pm_raw now[3] = { { "x", 1 }, { "y", 1 }, { "added by the import", 19 } };

		EQ(ae_pm_apply(&ops, &fake, undo, now), AE_PM_APPLY_OK);
		CHECK(!strcmp(fake.files[0], "old career") && !fake.present[2]);
	}
}


/* the backup just written is never pruned, whatever its stamp; -10 is after -9 */
static void test_prune_protects(void)
{
	static char list[10][96];
	char const *names[10];
	char stem[64];
	int victims[10], count, index;

	ae_pm_backup_stem("goat", stem, sizeof(stem));
	/* five backups with later stamps (a clock set back, or a backup folder from another PC) and the one just written, the oldest */
	for (index = 0; index < 5; index++)
	{
		char stamp[40];

		snprintf(stamp, sizeof(stamp), "2030010%d-120000", (index + 1) % 10);
		ae_pm_backup_name(stem, stamp, 1, ".aebackup", list[index], 96);
	}
	ae_pm_backup_name(stem, "20261121-193012", 1, ".aebackup", list[5], 96);
	for (index = 0; index < 6; index++)
		names[index] = list[index];
	count = ae_pm_prune_select(names, 6, stem, ".aebackup", 5, list[5], victims, 10);
	EQ(count, 0);
	/* with nothing protected the new one would be the victim: the protection is what keeps it */
	count = ae_pm_prune_select(names, 6, stem, ".aebackup", 5, NULL, victims, 10);
	EQ(count, 1);
	EQ(victims[0], 5);
	/* the clash numbers sort as numbers: -10 is newer than -9 and -2 */
	ae_pm_backup_name(stem, "20261121-193012", 1, ".aebackup", list[0], 96);
	ae_pm_backup_name(stem, "20261121-193012", 2, ".aebackup", list[1], 96);
	ae_pm_backup_name(stem, "20261121-193012", 9, ".aebackup", list[2], 96);
	ae_pm_backup_name(stem, "20261121-193012", 10, ".aebackup", list[3], 96);
	ae_pm_backup_name(stem, "20261121-193011", 10, ".aebackup", list[4], 96);
	for (index = 0; index < 5; index++)
		names[index] = list[index];
	count = ae_pm_prune_select(names, 5, stem, ".aebackup", 2, NULL, victims, 10);
	EQ(count, 3);
	/* kept: -10 and -9 of the later second; gone: -2, the one without a suffix, the earlier second's */
	for (index = 0; index < count; index++)
		CHECK(victims[index] == 0 || victims[index] == 1 || victims[index] == 4);
}

static void test_marker(void)
{
	char const *texts[3] = { "new career\n", "new service\n", NULL };
	char marker[512], name[96];
	struct ae_pm_raw now[3];

	EQ(ae_pm_marker_write("goat-1-20261121-193012.aebackup", texts, marker, sizeof(marker)), AE_PM_OK);
	HAS(marker, "backup = goat-1-20261121-193012.aebackup\n");
	HAS(marker, "bots = absent\n");
	/* the files on disk are the new texts (the write was done, the mark was left): the mark is dropped, nothing put back */
	now[0].text = "new career\n";
	now[0].length = 11;
	now[1].text = "new service\n";
	now[1].length = 12;
	now[2].text = "";
	now[2].length = -1;
	EQ(ae_pm_marker_decide(marker, strlen(marker), now, name, sizeof(name)), AE_PM_RECOVER_DONE);
	CHECK(!strcmp(name, "goat-1-20261121-193012.aebackup"));
	/* a file that is not the new text (the write was cut off between the renames): rolled back */
	now[1].text = "old service\n";
	EQ(ae_pm_marker_decide(marker, strlen(marker), now, name, sizeof(name)), AE_PM_RECOVER_ROLLBACK);
	now[1].text = "new service\n";
	now[2].text = "an added file";
	now[2].length = 13;
	EQ(ae_pm_marker_decide(marker, strlen(marker), now, name, sizeof(name)), AE_PM_RECOVER_ROLLBACK);
	/* a mark that is not one, cut, or without a name or a hash */
	EQ(ae_pm_marker_decide("hello", 5, now, name, sizeof(name)), AE_PM_RECOVER_BAD);
	EQ(ae_pm_marker_decide(marker, 40, now, name, sizeof(name)), AE_PM_RECOVER_BAD);
	EQ(ae_pm_marker_decide("# AE profile move\nversion = 1\ncareer = x\n", 40, now, name, sizeof(name)), AE_PM_RECOVER_BAD);
	/* a mark that could not be taken away after a write that was done: the apply says so */
}

static int g_remove_fails;
static int g_marker_off(void *context, int on)
{
	(void)context;
	return on ? 1 : !g_remove_fails;
}

static void test_marker_left(void)
{
	static struct ae_pm_ops const ops = { f_stage, f_commit, f_discard, f_put, g_marker_off };
	struct ae_pm_raw raw[3] = { { "old career", 10 }, { "old service", 11 }, { "", -1 } };
	char const *texts[3] = { "new career", "new service", "new bots" };
	struct fake fake;

	fake_start(&fake);
	g_remove_fails = 1;
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_MARKER_LEFT);
	CHECK(!strcmp(fake.files[0], "new career") && !strcmp(fake.files[2], "new bots"));
	g_remove_fails = 0;
	fake_start(&fake);
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_OK);
}


/* a mark left after a done write is taken away once the files are the new texts, tried every few seconds */
static void test_marker_retry(void)
{
	static struct ae_pm_ops const ops = { f_stage, f_commit, f_discard, f_put, g_marker_off };
	struct ae_pm_raw raw[3] = { { "old career", 10 }, { "old service", 11 }, { "", -1 } };
	char const *texts[3] = { "new career", "new service", "new bots" };
	char marker[512], name[96];
	struct ae_pm_raw now[3];
	struct fake fake;
	unsigned long last = 0;
	int polls = 0, due = 0, tick, pending = 1;

	/* the first ask is due, then one in every three seconds; a clock that went back is due again */
	CHECK(ae_pm_retry_due(&last, 1000, 3));
	CHECK(!ae_pm_retry_due(&last, 1001, 3) && !ae_pm_retry_due(&last, 1002, 3));
	CHECK(ae_pm_retry_due(&last, 1003, 3));
	CHECK(!ae_pm_retry_due(&last, 1005, 3) && ae_pm_retry_due(&last, 1006, 3));
	CHECK(ae_pm_retry_due(&last, 500, 3));
	/* the write is done but its mark would not go: pending until a retry finds the files are the texts the mark names */
	fake_start(&fake);
	g_remove_fails = 1;
	EQ(ae_pm_apply(&ops, &fake, texts, raw), AE_PM_APPLY_MARKER_LEFT);
	EQ(ae_pm_marker_write("b.aebackup", texts, marker, sizeof(marker)), AE_PM_OK);
	last = 0;
	for (tick = 0; tick < 12 && pending; tick++)
	{
		if (tick == 5)
			g_remove_fails = 0;
		if (!ae_pm_retry_due(&last, 2000 + (unsigned long)tick, 3))
			continue;
		due++;
		now[0].text = fake.files[0];
		now[0].length = (long)strlen(fake.files[0]);
		now[1].text = fake.files[1];
		now[1].length = (long)strlen(fake.files[1]);
		now[2].text = fake.files[2];
		now[2].length = fake.present[2] ? (long)strlen(fake.files[2]) : -1;
		polls++;
		if (ae_pm_marker_decide(marker, strlen(marker), now, name, sizeof(name)) == AE_PM_RECOVER_DONE && g_marker_off(&fake, 0))
			pending = 0;
	}
	/* (retried at ticks 0, 3, 6: the removal fails until tick 5, so tick 6's retry takes the mark away) */
	EQ(pending, 0);
	EQ(due, 3);
	EQ(polls, 3);
	/* a file that is not the new text keeps the mark: not a done write */
	fake.files[1][0] = 'x';
	now[1].text = fake.files[1];
	EQ(ae_pm_marker_decide(marker, strlen(marker), now, name, sizeof(name)), AE_PM_RECOVER_ROLLBACK);
}

/* a file over the readers' caps is refused at once as too large (a merge of it would take long), and a file as large as the
readers keep still passes and merges in time */
static void test_caps(void)
{
	static char bots[BIG], service[BIG / 4], out[BIG], mine[BIG];
	struct ae_pm_file file;
	size_t length = 0;
	int index;
	clock_t begin;
	double seconds;
	size_t out_length;

	/* 11,000 bot lines: refused, quickly */
	for (index = 0; index < 11000; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\n", index);
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	begin = clock();
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_SIZE);
	CHECK((double)(clock() - begin) / CLOCKS_PER_SEC < 0.5);
	/* one past the readers' bots; versus; rivals; maps */
	length = 0;
	for (index = 0; index < AE_BOT_RECORD_BOTS + 1; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\n", index);
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_SIZE);
	length = 0;
	for (index = 0; index < 10; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\n", index);
	for (index = 0; index < AE_BOT_RECORD_VERSUS + 1; index++)
		length += (size_t)sprintf(bots + length, "versus = goat | b%d | 1 0 0 0 0 1 1 1\n", index % 10 + 1000 * (index / 10));
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_SIZE);
	length = 0;
	for (index = 0; index < AE_SR_RIVALS_PER_OWNER + 1; index++)
		length += (size_t)sprintf(service + length, "rival = goat | r%d | R\n", index);
	hand_file("", service, "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_SIZE);
	length = 0;
	for (index = 0; index < AE_SR_MAPS + 1; index++)
		length += (size_t)sprintf(service + length, "map = goat | m%d | 1 1 1 1\n", index);
	hand_file("", service, "", out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_ERR_SIZE);
	/* a worst case file the readers keep: every bot with its lines, every versus and lastgame line; into a target as full */
	length = 0;
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\nbotx = b%d | 1 2 3\nbotkinds = b%d | 1 2\n"
			"botmedals = b%d | 1:2\nbotmap = b%d | m1 | 1 1 1 1\nbotmap = b%d | m2 | 1 1 1 1\n", index, index, index, index, index, index);
	for (index = 0; index < AE_BOT_RECORD_VERSUS; index++)
		length += (size_t)sprintf(bots + length, "versus = goat | b%d | 1 0 0 0 0 1 1 1\nlastgame = goat | b%d | ffa | m | 0 1 1 1 1 1 -1\n",
			index % AE_BOT_RECORD_BOTS, index % AE_BOT_RECORD_BOTS);
	/* (a bot's versus line once per pair: the 512 bots take 512 of them, the rest are the cap's room) */
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	{
		int answer = ae_pm_parse(out, strlen(out), NOW, &file);

		/* (the same bot twice in versus is a duplicate: the real file never has it) */
		CHECK(answer == AE_PM_ERR_VALUE || answer == AE_PM_OK);
	}
	length = 0;
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
		length += (size_t)sprintf(bots + length, "bot = b%d | B | 1 2 x | 1 0 0 1 1 1 1\nbotx = b%d | 1 2 3\nbotkinds = b%d | 1 2\n"
			"botmedals = b%d | 1:2\nbotmap = b%d | m1 | 1 1 1 1\nbotmap = b%d | m2 | 1 1 1 1\n", index, index, index, index, index, index);
	for (index = 0; index < AE_BOT_RECORD_BOTS; index++)
		length += (size_t)sprintf(bots + length, "versus = goat | b%d | 1 0 0 0 0 1 1 1\nlastgame = goat | b%d | ffa | m | 0 1 1 1 1 1 -1\n", index, index);
	hand_file("", "player = goat | G | 1 | 1 1 0 0 0 | 0\n", bots, out);
	EQ(ae_pm_parse(out, strlen(out), NOW, &file), AE_PM_OK);
	/* (a target with the same bots, all of them each side's) */
	strcpy(mine, "# AE bot records\nversion = 2\nclock = 5\n");
	strcat(mine, bots);
	begin = clock();
	EQ(ae_pm_merge_bots(AE_PM_MERGE, &file, "goat", mine, o3, BIG, &out_length, NULL), AE_PM_OK);
	seconds = (double)(clock() - begin) / CLOCKS_PER_SEC;
	CHECK(seconds < 3.0);
}

/* a backup that is cut in the middle of its "file = ..." line is a refusal, not a read past the text (ASan judges) */
static void test_backup_truncated(void)
{
	char const *const names[3] = { "career.txt", "service-record.txt", "bot_records.txt" };
	struct ae_pm_raw back[3];
	static char const cut1[] = "# AE records backup\nversion = 1\nfile = career.txt | prese\n";
	static char const cut2[] = "# AE records backup\nversion = 1\nfile = career.txt | absent |";
	static char const cut3[] = "# AE records backup\nversion = 1\nfile = career.txt | present | 5\nabcde\nfile = ser\n";
	static char const cut4[] = "# AE records backup\nversion = 1\nfile = career.txt | absen\n";
	char *copy;
	size_t length;
	int index;

	/* (copied to buffers of exactly their length so that a read past the end is a heap overflow) */
	char const *const cuts[4] = { cut1, cut2, cut3, cut4 };

	for (index = 0; index < 4; index++)
	{
		length = strlen(cuts[index]);
		copy = malloc(length);
		memcpy(copy, cuts[index], length);
		EQ(ae_pm_backup_read(copy, length, names, back), AE_PM_ERR_BACKUP);
		free(copy);
	}
	/* every prefix of a good backup is refused the same way */
	{
		struct ae_pm_raw raw[3] = { { "abc", 3 }, { "", -1 }, { "xy", 2 } };
		static char good[4096];
		size_t good_length;

		EQ(ae_pm_backup_write(raw, names, good, sizeof(good), &good_length), AE_PM_OK);
		for (length = 0; length < good_length; length++)
		{
			copy = malloc(length ? length : 1);
			memcpy(copy, good, length);
			EQ(ae_pm_backup_read(copy, length, names, back), AE_PM_ERR_BACKUP);
			free(copy);
		}
	}
}

static void test_url(void)
{
	char url[256];

	CHECK(ae_pm_file_url("/srv/me/.local/share/g/exports", url, sizeof(url)));
	CHECK(!strcmp(url, "file:///srv/me/.local/share/g/exports"));
	CHECK(ae_pm_file_url("C:\\Users\\An Ne\\50%#?\\exports", url, sizeof(url)));
	CHECK(!strcmp(url, "file:///C:/Users/An%20Ne/50%25%23%3F/exports"));
	CHECK(ae_pm_file_url("/srv/\xc3\xa9/x", url, sizeof(url)));
	CHECK(!strcmp(url, "file:///srv/%C3%A9/x"));
	CHECK(!ae_pm_file_url("/a/very/long/path", url, 20));
	CHECK(!ae_pm_file_url("", url, sizeof(url)));
}

/* a file's check line made right again (so that a mutated body reaches the value checks and the merge) */
static size_t rehash(char *text, size_t size)
{
	struct mcc_sha256 context;
	unsigned char digest[32];
	char *end = NULL, *at = text;
	size_t length;
	int index;

	while ((at = memchr(at, '[', size - (size_t)(at - text))) != NULL)
	{
		if (!strncmp(at, "[end]\n", 6))
			end = at;
		at++;
	}
	if (!end)
		return size;
	length = (size_t)(end - text) + 6;
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, text, (int)length);
	mcc_sha256_end(&context, digest);
	memcpy(text + length, "check = ", 8);
	for (index = 0; index < 32; index++)
		sprintf(text + length + 8 + index * 2, "%02x", digest[index]);
	strcpy(text + length + 72, "\n");
	return length + 73;
}

/* mutated files: a byte changed, deleted or inserted anywhere in a good file never crashes the reader or the merge (the
sanitizers judge); a file that still reads as a good one merges into three texts that fit */
static void test_mutations(void)
{
	struct ae_pm_file file;
	static char good[BIG], text[BIG];
	unsigned long seed = 12345;
	size_t length;
	int counts[3], round, accepted = 0, rounds = getenv("AE_PM_FUZZ_ROUNDS") ? atoi(getenv("AE_PM_FUZZ_ROUNDS")) : 4000;

	EQ(export_goat(good, counts), AE_PM_OK);
	length = strlen(good);
	for (round = 0; round < rounds; round++)
	{
		size_t at, size = length;
		int kind;

		memcpy(text, good, length + 1);
		seed = seed * 1103515245UL + 12345UL;
		at = (size_t)((seed >> 8) % (unsigned long)length);
		seed = seed * 1103515245UL + 12345UL;
		kind = (int)((seed >> 8) % 4);
		if (kind == 0)
			text[at] = (char)(seed >> 16);
		else if (kind == 1)
		{
			memmove(text + at, text + at + 1, size - at);
			size--;
		}
		else if (kind == 2)
		{
			memmove(text + at + 1, text + at, size - at + 1);
			text[at] = (char)(seed >> 16);
			size++;
		}
		else
			size = at;
		if (round & 1)
			size = rehash(text, size);
		if (ae_pm_parse(text, size, NOW, &file) == AE_PM_OK)
		{
			accepted++;
			EQ(merge_all(AE_PM_MERGE, &file, "den", career_goat, service_goat, bots_goat, NULL) == AE_PM_OK ||
				merge_all(AE_PM_MERGE, &file, "den", career_goat, service_goat, bots_goat, NULL) == AE_PM_ERR_ROOM, 1);
		}
	}
	/* (the check line makes nearly every mutation a refusal; the ones inside the check's own spelling that parse are the same file) */
	CHECK(accepted < rounds);
}

int main(void)
{
	test_round_trip();
	test_merge_units();
	test_rename();
	test_other_profiles_untouched();
	test_refusals();
	test_values();
	test_empty_and_keys();
	test_rival_cap();
	test_backup_restores();
	test_empty_profile_section();
	test_extra_section();
	test_key_spaces();
	test_clock_caps();
	test_number_limits();
	test_speed();
	test_dropped_and_versions();
	test_backup_container();
	test_backup_names();
	test_apply();
	test_url();
	test_prune_protects();
	test_marker();
	test_marker_left();
	test_marker_retry();
	test_caps();
	test_backup_truncated();
	test_mutations();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_profile_move_rules: ok\n");
	return 0;
}
