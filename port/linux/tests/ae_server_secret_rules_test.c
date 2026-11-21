#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ae_server_secret_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char path[64], text[64];
	struct stat info;
	FILE *file;
	char name[] = "./ae_secret_test_XXXXXX";
	int descriptor;

	CHECK(ae_server_secret_path("/srv/data", "a.txt", path, sizeof(path)) && !strcmp(path, "/srv/data/a.txt"));
	CHECK(ae_server_secret_path("/srv/data/", "a.txt", path, sizeof(path)) && !strcmp(path, "/srv/data/a.txt"));
	CHECK(!ae_server_secret_path("", "a.txt", path, sizeof(path)) && !path[0]);
	CHECK(!ae_server_secret_path("/srv", "", path, sizeof(path)) && !ae_server_secret_path(NULL, "a", path, sizeof(path)));
	CHECK(!ae_server_secret_path("/a-very-long-folder-name-that-will-not-fit", "file-name-too.txt", path, 32) && !path[0]);
	/* the file: made 0600 (the process umask 0 must not widen it), an older, wider one cut and made 0600 */
	descriptor = mkstemp(name);
	CHECK(descriptor >= 0);
	if (descriptor >= 0)
	{
		close(descriptor);
		unlink(name);
		umask(0);
		CHECK(ae_server_secret_write(name, "first-secret"));
		CHECK(stat(name, &info) == 0 && (info.st_mode & 0777) == 0600);
		chmod(name, 0644);
		CHECK(ae_server_secret_write(name, "two"));
		CHECK(stat(name, &info) == 0 && (info.st_mode & 0777) == 0600 && info.st_size == 4);
		file = fopen(name, "r");
		CHECK(file && fgets(text, sizeof(text), file) && !strcmp(text, "two\n"));
		if (file)
			fclose(file);
		unlink(name);
	}
	CHECK(!ae_server_secret_write("/nonexistent-folder/x", "a") && !ae_server_secret_write("", "a") && !ae_server_secret_write("x", NULL));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_server_secret_rules_test: ok\n");
	return failures != 0;
}
