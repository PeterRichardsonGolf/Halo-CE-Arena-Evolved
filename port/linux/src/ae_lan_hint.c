/*
AE_LAN_HINT.C

Whether the LAN list's firewall hint is due, and whether a firewall is on (ae_lan_hint.h). It only reads files that
everyone may read; it runs nothing (no systemctl, no ufw: those may ask for a password, or need root).
*/

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "ae_lan_hint.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <dirent.h>
#define AE_LAN_HINT_READS 1
#endif

int ae_lan_hint_due(unsigned long elapsed_ms, int empty, int firewall)
{
	return empty && firewall && elapsed_ms >= AE_LAN_HINT_DELAY_MS;
}

static void name_set(char *what, int size, char const *name)
{
	if (what && size > 0)
		snprintf(what, (size_t)size, "%s", name);
}

#ifdef AE_LAN_HINT_READS
/* ufw.conf's ENABLED=yes (a line of its own; quotes and spaces allowed, case ignored, # comments skipped) */
static int ufw_enabled(char const *root)
{
	char path[512], line[256];
	FILE *file;
	int enabled = 0;

	snprintf(path, sizeof(path), "%s/etc/ufw/ufw.conf", root);
	file = fopen(path, "r");
	if (!file)
		return 0;
	while (fgets(line, sizeof(line), file))
	{
		char *text = line;
		char value[8];
		int length = 0;

		while (isspace((unsigned char)*text))
			text++;
		if (strncmp(text, "ENABLED", 7))
			continue;
		text += 7;
		while (isspace((unsigned char)*text))
			text++;
		if (*text++ != '=')
			continue;
		while (isspace((unsigned char)*text) || *text == '"' || *text == '\'')
			text++;
		while (length < (int)sizeof(value) - 1 && isalpha((unsigned char)text[length]))
		{
			value[length] = (char)tolower((unsigned char)text[length]);
			length++;
		}
		value[length] = 0;
		/* (the last ENABLED= line counts, as the shell that reads the file has it) */
		enabled = !strcmp(value, "yes");
	}
	fclose(file);
	return enabled;
}

/* a process of that number is alive (its /proc folder) */
static int process_alive(char const *root, long pid)
{
	char path[512];
	DIR *folder;

	if (pid <= 0)
		return 0;
	snprintf(path, sizeof(path), "%s/proc/%ld", root, pid);
	/* (opened as a folder: the game's own sys/stat.h is the Windows one's) */
	folder = opendir(path);
	if (folder)
		closedir(folder);
	return folder != NULL;
}

/* firewalld's pid file names a live process */
static int firewalld_pid_file(char const *root)
{
	static char const *const files[] = { "/run/firewalld.pid", "/var/run/firewalld.pid" };
	unsigned int index;

	for (index = 0; index < sizeof(files) / sizeof(files[0]); index++)
	{
		char path[512];
		FILE *file;
		long pid = 0;

		snprintf(path, sizeof(path), "%s%s", root, files[index]);
		file = fopen(path, "r");
		if (!file)
			continue;
		if (fscanf(file, "%ld", &pid) != 1)
			pid = 0;
		fclose(file);
		if (process_alive(root, pid))
			return 1;
	}
	return 0;
}

/* a process named firewalld (its comm: a script run by its #! line is named for the script) */
static int firewalld_process(char const *root)
{
	char path[512];
	DIR *folder;
	struct dirent *entry;
	int found = 0;

	snprintf(path, sizeof(path), "%s/proc", root);
	folder = opendir(path);
	if (!folder)
		return 0;
	while (!found && (entry = readdir(folder)) != NULL)
	{
		char name[64];
		FILE *file;

		if (!isdigit((unsigned char)entry->d_name[0]))
			continue;
		snprintf(path, sizeof(path), "%s/proc/%s/comm", root, entry->d_name);
		file = fopen(path, "r");
		if (!file)
			continue;
		if (fgets(name, sizeof(name), file))
		{
			name[strcspn(name, "\r\n")] = 0;
			found = !strcmp(name, "firewalld");
		}
		fclose(file);
	}
	closedir(folder);
	return found;
}
#endif

int ae_lan_firewall_in(char const *root, char *what, int size)
{
	name_set(what, size, "");
#ifdef AE_LAN_HINT_READS
	if (!root)
		root = "";
	if (ufw_enabled(root))
	{
		name_set(what, size, "ufw");
		return 1;
	}
	if (firewalld_pid_file(root) || firewalld_process(root))
	{
		name_set(what, size, "firewalld");
		return 1;
	}
#else
	(void)root;
#endif
	return 0;
}

int ae_lan_firewall(char *what, int size)
{
#if defined(AE_LAN_HINT_READS) && !defined(HALO_ANDROID) && !defined(HALO_SERVER) && !defined(HALO_WEB)
	/* (a game test's fake system tree in place of this system's: tools/test_ae_lan_lobby.py) */
	char const *root = getenv("AE_TEST_FIREWALL_ROOT");

	return ae_lan_firewall_in(root ? root : "", what, size);
#else
	name_set(what, size, "");
	return 0;
#endif
}
