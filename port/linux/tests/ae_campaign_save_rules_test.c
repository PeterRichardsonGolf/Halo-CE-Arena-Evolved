#include <stdio.h>
#include <string.h>
#include "ae_campaign_save_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char *name(const char *package)
{
	static char file[AE_CAMPAIGN_SAVE_FILE_SIZE];

	if (!ae_campaign_save_file_name(package, file, sizeof(file)))
		return NULL;
	return file;
}

/* a valid file name everywhere: [a-z0-9_.-] only, one '.', never the stock file's */
static int safe(const char *file)
{
	const char *cursor;
	int dots = 0;

	for (cursor = file; *cursor; cursor++)
	{
		char c = *cursor;

		if (c == '.')
			dots++;
		else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
			return 0;
	}
	return dots == 1 && strlen(file) < AE_CAMPAIGN_SAVE_FILE_SIZE && !strncmp(file, "savegame-", 9) &&
		!strcmp(file + strlen(file) - 4, ".bin") && strcmp(file, "savegame.bin");
}

/* two packages' files differ */
static int differ(const char *a, const char *b)
{
	char kept[AE_CAMPAIGN_SAVE_FILE_SIZE];

	snprintf(kept, sizeof(kept), "%s", name(a));
	return strcmp(kept, name(b)) != 0;
}

int main(void)
{
	char file[AE_CAMPAIGN_SAVE_FILE_SIZE], first[AE_CAMPAIGN_SAVE_FILE_SIZE], long_a[80], long_b[80];
	const char *const odd[] = { "My Campaign", "a.b", "../up", "x\\y", "CON", "trail.", "trail ", "caf\xC3\xA9", "a_b" };
	size_t index, other;

	/* a Workshop item's folder (digits) and a plain local folder: as they are */
	CHECK(name("1234567890") && !strcmp(name("1234567890"), "savegame-1234567890.bin"));
	CHECK(!strcmp(name("my-campaign"), "savegame-my-campaign.bin"));
	/* every other byte escaped: no upper case, dot, separator or space is in the name as itself */
	CHECK(!strcmp(name("My Campaign"), "savegame-_4dy_20_43ampaign.bin"));
	CHECK(!strcmp(name("../up"), "savegame-_2e_2e_2fup.bin"));
	CHECK(!strcmp(name("a_b"), "savegame-a_5fb.bin"));
	for (index = 0; index < sizeof(odd) / sizeof(odd[0]); index++)
		CHECK(safe(name(odd[index])));
	/* two different folders never share a file (case too: "Pack" and "pack" are two files) */
	for (index = 0; index < sizeof(odd) / sizeof(odd[0]); index++)
		for (other = index + 1; other < sizeof(odd) / sizeof(odd[0]); other++)
			CHECK(differ(odd[index], odd[other]));
	CHECK(differ("Pack", "pack"));
	/* never empty: no package, no file */
	CHECK(!ae_campaign_save_file_name(NULL, file, sizeof(file)) && !file[0]);
	CHECK(!ae_campaign_save_file_name("", file, sizeof(file)) && !file[0]);
	CHECK(!ae_campaign_save_file_name("x", file, 10) && !file[0]);
	/* a long name: cut, never inside an escape, with the whole name's hash; two long names with the same start differ */
	memset(long_a, 'a', 70);
	long_a[70] = 0;
	strcpy(long_b, long_a);
	long_b[69] = 'b';
	strcpy(first, name(long_a));
	CHECK(safe(first) && strstr(first, "_h") != NULL && strlen(first) <= 9 + AE_CAMPAIGN_SAVE_NAME_KEPT + 4);
	CHECK(differ(long_a, long_b));
	memset(long_a, '.', 70);
	strcpy(first, name(long_a));
	CHECK(safe(first) && strlen(first) <= 9 + AE_CAMPAIGN_SAVE_NAME_KEPT + 4);
	/* (the part kept ends on whole escapes: "_2e" units then "_h") */
	CHECK(!strncmp(first, "savegame-_2e_2e", 15) && strstr(first, "_2e_h") != NULL);
	/* a name just at the limit is not hashed; one more character is */
	memset(long_a, 'z', AE_CAMPAIGN_SAVE_NAME_KEPT);
	long_a[AE_CAMPAIGN_SAVE_NAME_KEPT] = 0;
	CHECK(strstr(name(long_a), "_h") == NULL);
	long_a[AE_CAMPAIGN_SAVE_NAME_KEPT] = 'z';
	long_a[AE_CAMPAIGN_SAVE_NAME_KEPT + 1] = 0;
	CHECK(strstr(name(long_a), "_h") != NULL);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_campaign_save_rules_test: ok\n");
	return failures ? 1 : 0;
}
