/* The LAN list's firewall hint (ae_lan_hint.c): when it is due, and the firewalls found in fake system trees */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ae_lan_hint.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char base[256];

/* a fresh fake root: <base>/<name> */
static char const *tree(char const *name)
{
	static char root[512];
	char command[2048];

	snprintf(root, sizeof(root), "%s/%s", base, name);
	snprintf(command, sizeof(command), "rm -rf '%s' && mkdir -p '%s'", root, root);
	if (system(command))
		printf("could not make %s\n", root);
	return root;
}

static void file_put(char const *root, char const *path, char const *text)
{
	char full[1024], command[2048];
	FILE *file;

	snprintf(full, sizeof(full), "%s%s", root, path);
	snprintf(command, sizeof(command), "mkdir -p \"$(dirname '%s')\"", full);
	if (system(command))
		printf("could not make the folder of %s\n", full);
	file = fopen(full, "w");
	if (file)
	{
		fputs(text, file);
		fclose(file);
	}
}

static void folder_make(char const *root, char const *path)
{
	char command[2048];

	snprintf(command, sizeof(command), "mkdir -p '%s%s'", root, path);
	if (system(command))
		printf("could not make %s%s\n", root, path);
}

/* ae_lan_firewall_in's answer for the root, and its name */
static int found(char const *root, char *what)
{
	return ae_lan_firewall_in(root, what, 32);
}

int main(void)
{
	char what[32];
	char const *root;

	/* (tools/test_ae_units.py's scratch folder) */
	snprintf(base, sizeof(base), "%s/ae_lan_hint_test.%ld", getenv("AE_TEST_SCRATCH") ? getenv("AE_TEST_SCRATCH") :
		getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (long)getpid());

	/* due: only with nothing found, a firewall on, and from the delay on */
	CHECK(!ae_lan_hint_due(0, 1, 1));
	CHECK(!ae_lan_hint_due(AE_LAN_HINT_DELAY_MS - 1, 1, 1));
	CHECK(ae_lan_hint_due(AE_LAN_HINT_DELAY_MS, 1, 1));
	CHECK(ae_lan_hint_due(60000, 1, 1));
	CHECK(!ae_lan_hint_due(60000, 0, 1));
	CHECK(!ae_lan_hint_due(60000, 1, 0));
	/* (the lines fit the list's: 63 characters) */
	CHECK(strlen(AE_LAN_HINT_LINE_1) <= 63);
	CHECK(strlen(AE_LAN_HINT_LINE_2) <= 63);

	/* nothing at all */
	root = tree("empty");
	strcpy(what, "x");
	CHECK(!found(root, what));
	CHECK(!strcmp(what, ""));

	/* ufw on, as Ubuntu writes it */
	root = tree("ufw-on");
	file_put(root, "/etc/ufw/ufw.conf", "# /etc/ufw/ufw.conf\n#\n\n# Set to yes to start on boot.\nENABLED=yes\n\n"
		"# Please use the 'ufw' command to set the loglevel.\nLOGLEVEL=low\n");
	CHECK(found(root, what));
	CHECK(!strcmp(what, "ufw"));
	/* ufw off (installed, as Ubuntu ships it) */
	root = tree("ufw-off");
	file_put(root, "/etc/ufw/ufw.conf", "# Set to yes to start on boot.\nENABLED=no\nLOGLEVEL=low\n");
	CHECK(!found(root, what));
	/* a comment that says yes is not the setting */
	root = tree("ufw-comment");
	file_put(root, "/etc/ufw/ufw.conf", "#ENABLED=yes\nENABLED=no\n");
	CHECK(!found(root, what));
	/* quotes, spaces, case; the last line wins */
	root = tree("ufw-quoted");
	file_put(root, "/etc/ufw/ufw.conf", "ENABLED=no\n  ENABLED = \"Yes\"\n");
	CHECK(found(root, what));
	root = tree("ufw-last-no");
	file_put(root, "/etc/ufw/ufw.conf", "ENABLED=yes\nENABLED=no\n");
	CHECK(!found(root, what));
	/* a longer word is not yes; an empty file; a line with no end */
	root = tree("ufw-yesterday");
	file_put(root, "/etc/ufw/ufw.conf", "ENABLED=yesterday");
	CHECK(!found(root, what));
	root = tree("ufw-empty");
	file_put(root, "/etc/ufw/ufw.conf", "");
	CHECK(!found(root, what));
	root = tree("ufw-no-newline");
	file_put(root, "/etc/ufw/ufw.conf", "ENABLED=yes");
	CHECK(found(root, what));

	/* firewalld: its pid file names a live process */
	root = tree("firewalld-pid");
	file_put(root, "/run/firewalld.pid", "812\n");
	folder_make(root, "/proc/812");
	CHECK(found(root, what));
	CHECK(!strcmp(what, "firewalld"));
	/* in /var/run, as older systems have it */
	root = tree("firewalld-var-run");
	file_put(root, "/var/run/firewalld.pid", "77");
	folder_make(root, "/proc/77");
	CHECK(found(root, what));
	/* a stale pid file (firewalld stopped, the file left) */
	root = tree("firewalld-stale");
	file_put(root, "/run/firewalld.pid", "812\n");
	folder_make(root, "/proc/1");
	CHECK(!found(root, what));
	/* garbage, and a pid of 0 or less */
	root = tree("firewalld-garbage");
	file_put(root, "/run/firewalld.pid", "firewalld\n");
	file_put(root, "/var/run/firewalld.pid", "-5\n");
	folder_make(root, "/proc/-5");
	CHECK(!found(root, what));
	/* no pid file, but a process named firewalld */
	root = tree("firewalld-comm");
	file_put(root, "/proc/1/comm", "systemd\n");
	file_put(root, "/proc/950/comm", "firewalld\n");
	file_put(root, "/proc/self/comm", "firewalld\n");
	CHECK(found(root, what));
	CHECK(!strcmp(what, "firewalld"));
	/* other processes only (a name that only starts so, a non-numbered folder) */
	root = tree("no-firewalld");
	file_put(root, "/proc/1/comm", "systemd\n");
	file_put(root, "/proc/42/comm", "firewalld-helper\n");
	file_put(root, "/proc/self/comm", "firewalld\n");
	CHECK(!found(root, what));
	/* ufw first when both */
	root = tree("both");
	file_put(root, "/etc/ufw/ufw.conf", "ENABLED=yes\n");
	file_put(root, "/proc/950/comm", "firewalld\n");
	CHECK(found(root, what));
	CHECK(!strcmp(what, "ufw"));
	/* no room for the name: still found; a NULL root is this system's (no crash) */
	CHECK(ae_lan_firewall_in(root, NULL, 0));
	(void)ae_lan_firewall_in(NULL, what, sizeof(what));

	{
		char command[1024];

		snprintf(command, sizeof(command), "rm -rf '%s'", base);
		if (system(command))
			printf("could not remove %s\n", base);
	}
	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_lan_hint_test: all passed\n");
	return failures ? 1 : 0;
}
