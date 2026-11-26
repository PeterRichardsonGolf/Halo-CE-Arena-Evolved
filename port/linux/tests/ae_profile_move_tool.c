/* A command line for ae_profile_move_rules.c (tools/test_ae_units.py compares it with a second implementation of the rules):
	export <name> <key> <career> <service> <bots> <out> [now]
	merge <merge|replace> <to key> <file> <career> <service> <bots> <outdir>
A record file that is not there is taken as none. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../game/ae_profile_move_rules.h"

static char *slurp(char const *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	char *text;
	long size;

	if (!file)
	{
		text = calloc(1, 1);
		*length = 0;
		return text;
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	text = calloc(1, (size_t)size + 1);
	if (fread(text, 1, (size_t)size, file) != (size_t)size)
		exit(3);
	fclose(file);
	*length = (size_t)size;
	return text;
}

static void spit(char const *directory, char const *name, char const *text, size_t length)
{
	char path[1024];
	FILE *file;

	snprintf(path, sizeof(path), "%s/%s", directory, name);
	file = fopen(path, "wb");
	if (!file || fwrite(text, 1, length, file) != length)
		exit(4);
	fclose(file);
}

int main(int argc, char **argv)
{
	size_t length;

	if (argc >= 8 && !strcmp(argv[1], "export"))
	{
		char *career = slurp(argv[4], &length), *service = slurp(argv[5], &length), *bots = slurp(argv[6], &length);
		char *out = calloc(1, 1 << 21);
		int answer = ae_pm_export(argv[2], argv[3], career, service, bots, argc > 8 ? (unsigned long)atol(argv[8]) : 1795283200UL,
			"test", out, 1 << 21, NULL);

		if (answer)
		{
			fprintf(stderr, "export: %s\n", ae_pm_error_text(answer));
			return 2;
		}
		{
			FILE *file = fopen(argv[7], "wb");

			fwrite(out, 1, strlen(out), file);
			fclose(file);
		}
		return 0;
	}
	if (argc >= 9 && !strcmp(argv[1], "merge"))
	{
		size_t file_length;
		char *text = slurp(argv[4], &file_length);
		struct ae_pm_file file;
		char *career = slurp(argv[5], &length), *service = slurp(argv[6], &length), *bots = slurp(argv[7], &length);
		char *out = calloc(1, 1 << 22);
		int mode = !strcmp(argv[2], "replace") ? AE_PM_REPLACE : AE_PM_MERGE;
		int answer = ae_pm_parse(text, file_length, 0, &file);

		if (answer)
		{
			fprintf(stderr, "parse: %s\n", ae_pm_error_text(answer));
			return 2;
		}
		answer = ae_pm_merge_career(mode, &file, argv[3], career, out, 1 << 22, &length, NULL);
		if (answer)
			return 2;
		spit(argv[8], "career.txt", out, length);
		answer = ae_pm_merge_service(mode, &file, argv[3], service, out, 1 << 22, &length, NULL);
		if (answer)
			return 2;
		spit(argv[8], "service-record.txt", out, length);
		answer = ae_pm_merge_bots(mode, &file, argv[3], bots, out, 1 << 22, &length, NULL);
		if (answer)
			return 2;
		spit(argv[8], "bot_records.txt", out, length);
		return 0;
	}
	fprintf(stderr, "usage: see the file's head\n");
	return 1;
}
