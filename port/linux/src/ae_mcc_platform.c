/*
AE_MCC_PLATFORM.C

The game's use of MCC's Custom Edition resource maps (ae_mcc.h): the
detection run once a process, the first time a resource map is looked for or
Settings shows MAP FILES (game.mcc_path, HALO_MCC_PATH, then Steam's roots:
Linux's folders, Windows' registry: win32_ae_mcc.c), its line in debug.txt,
the "mcc:\<name>.map" paths map_family_resource hands the engine (and
platform_translate_path turns into the file in MCC's folder), the answers to
the menus' question, and Settings' BROWSE.

Nothing here writes in the MCC folder: it is only read, by the engine's own
read-only opens. Its path stays here and in config.toml (game.mcc_path): the
engine, the netcode, the saves and the map cache see only "mcc:\<name>.map".
*/

#include "platform.h"
#include "port_config.h"
#include "ae_mcc.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID)
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keyboard.h>
#define AE_MCC_FOLDER_DIALOG 1
#endif

#ifdef _WIN32
/* (port/windows/src/win32_ae_mcc.c: the registry's SteamPath, the
InstallPath fallback, the default folder) */
int ae_mcc_windows_steam_roots(char (*roots)[AE_MCC_PATH_SIZE], int maximum);
#endif

/* the prefix of the paths the engine opens MCC's files by */
#define MCC_PREFIX "mcc:\\" /* (ae_mcc.c's ae_mcc_virtual_path_type reads it back) */

static pthread_mutex_t mcc_lock = PTHREAD_MUTEX_INITIALIZER;

static struct
{
	int detected;
	struct ae_mcc_result result;
	/* map_family_resource asked for the player's own folders only */
	int probing;
	/* the log's lines said once */
	int ask_logged;
	int read_logged[3];
	/* BROWSE: a folder the dialog gave, waiting for the menus' poll; the
	last result's words (0: none yet) */
	int dialog_open;
	int chosen;
	int dialog_failed;
	char chosen_path[AE_MCC_PATH_SIZE];
	char browse_message[512];
	/* the status line, UTF-8 and wide */
	char status[600];
	unsigned short status_wide[600];
} mcc;

/* ---------- the detection */

static int folder_exists(const char *path)
{
	return platform_folder_exists(path);
}

static const char *source_name(int source)
{
	switch (source)
	{
	case AE_MCC_SOURCE_CONFIG: return "game.mcc_path";
	case AE_MCC_SOURCE_ENVIRONMENT: return "HALO_MCC_PATH";
	case AE_MCC_SOURCE_STEAM: return "Steam";
	case AE_MCC_SOURCE_BROWSE: return "BROWSE";
	default: return "nowhere";
	}
}

static void log_result(const char *when)
{
	switch (mcc.result.state)
	{
	case AE_MCC_FOUND:
		platform_log("mcc: %s: Custom Edition files found in %s (from %s: %s); game.mcc_use %s", when,
			mcc.result.root, source_name(mcc.result.source), mcc.result.reason, config_string("game.mcc_use"));
		break;
	case AE_MCC_INCOMPLETE:
		platform_log("mcc: %s: MCC found, but not its Custom Edition files: %s", when, mcc.result.reason);
		break;
	default:
		platform_log("mcc: %s: not found: %s", when, mcc.result.reason);
		break;
	}
}

/* (with the lock held) */
static void detect_locked(void)
{
	char roots[AE_MCC_MAXIMUM_ROOTS][AE_MCC_PATH_SIZE];
	const char *root_pointers[AE_MCC_MAXIMUM_ROOTS];
	struct ae_mcc_inputs inputs;
	int count = 0, index;

	if (mcc.detected)
		return;
	mcc.detected = 1;
#if defined(_WIN32)
	count = ae_mcc_windows_steam_roots(roots, AE_MCC_MAXIMUM_ROOTS);
#elif !defined(HALO_ANDROID)
	count = ae_mcc_linux_steam_roots(getenv("XDG_DATA_HOME"), getenv("HOME"), roots, AE_MCC_MAXIMUM_ROOTS);
#endif
	for (index = 0; index < count; index++)
		root_pointers[index] = roots[index];
	memset(&inputs, 0, sizeof(inputs));
	inputs.config_path = config_string("game.mcc_path");
	inputs.environment_path = getenv("HALO_MCC_PATH");
	inputs.steam_roots = root_pointers;
	inputs.steam_root_count = count;
	inputs.directory_exists = folder_exists;
	ae_mcc_detect(&inputs, &mcc.result);
	log_result("detected");
	/* (a library moved, or MCC installed again elsewhere: the folder in use kept as it is now) */
	if (mcc.result.state == AE_MCC_FOUND && mcc.result.source == AE_MCC_SOURCE_STEAM &&
		ae_mcc_use_from_text(config_string("game.mcc_use")) == AE_MCC_USE_YES &&
		strcmp(config_string("game.mcc_path"), mcc.result.root) && ae_mcc_path_storable(mcc.result.root))
	{
		if (config_write("game.mcc_path", mcc.result.root))
			platform_log("mcc: game.mcc_path is now %s", mcc.result.root);
	}
}

static void detect(void)
{
	pthread_mutex_lock(&mcc_lock);
	detect_locked();
	pthread_mutex_unlock(&mcc_lock);
}

const char *ae_mcc_root(void)
{
	detect();
	return mcc.result.state == AE_MCC_FOUND ? mcc.result.root : NULL;
}

int ae_mcc_copy_root(char *root, size_t size)
{
	int found;

	pthread_mutex_lock(&mcc_lock);
	detect_locked();
	found = mcc.result.state == AE_MCC_FOUND;
	snprintf(root, size, "%s", found ? mcc.result.root : "");
	pthread_mutex_unlock(&mcc_lock);
	return found;
}

void ae_mcc_probe_own_files(int on)
{
	mcc.probing = on;
}

int ae_mcc_state(void)
{
	detect();
	return mcc.result.state;
}

int ae_mcc_use(void)
{
	return ae_mcc_use_from_text(config_string("game.mcc_use"));
}

/* ---------- the engine's reads */

int ae_mcc_resource(const char *name, char *path, long size)
{
	int type = ae_mcc_resource_type(name);

	/* (with "no" nothing of Steam's is read for a map: no detection; only Settings' MAP FILES looks) */
	if (type < 0 || size <= 0 || mcc.probing || ae_mcc_use() == AE_MCC_USE_NO || ae_mcc_state() != AE_MCC_FOUND)
		return 0;
	switch (ae_mcc_use())
	{
	case AE_MCC_USE_ASK:
		/* (outside the menus' question: a test host, a joined game; ask counts as no) */
		pthread_mutex_lock(&mcc_lock);
		if (!mcc.ask_logged)
		{
			mcc.ask_logged = 1;
			platform_log("mcc: Custom Edition's %s.map is missing; MCC's in %s is not read: game.mcc_use is "
				"\"ask\" (the menus ask when a map is picked; \"yes\" reads them)", name, mcc.result.root);
		}
		pthread_mutex_unlock(&mcc_lock);
		return 0;
	default:
		break;
	}
	snprintf(path, (size_t)size, MCC_PREFIX "%s.map", ae_mcc_resource_name(type));
	pthread_mutex_lock(&mcc_lock);
	if (!mcc.read_logged[type])
	{
		mcc.read_logged[type] = 1;
		platform_log("mcc: %s.map read from MCC's folder (%s)", ae_mcc_resource_name(type), mcc.result.root);
	}
	pthread_mutex_unlock(&mcc_lock);
	return 1;
}

int ae_mcc_translate_path(const char *xbox_path, char *host_path, unsigned long size)
{
	char root[AE_MCC_PATH_SIZE];
	int type = ae_mcc_virtual_path_type(xbox_path);

	if (type == AE_MCC_VIRTUAL_NOT)
		return 0;
	if (size)
		host_path[0] = 0;
	/* (any thread opens files: the folder copied under the lock, which BROWSE writes it under) */
	if (type >= 0 && ae_mcc_copy_root(root, sizeof(root)))
		ae_mcc_resource_path(root, ae_mcc_resource_name(type), host_path, size);
	return 1;
}

const char *ae_mcc_refusal_note(void)
{
	if (ae_mcc_use() == AE_MCC_USE_NO)
		return "";
	switch (ae_mcc_state())
	{
	case AE_MCC_INCOMPLETE:
		return ". MCC found, but its Custom Edition files are missing. Check the install in Steam.";
	case AE_MCC_FOUND:
		/* (a load outside the menus' question: a game joined, the Xbox menus, a test host) */
		return ae_mcc_use() == AE_MCC_USE_ASK ? ". MCC found: turn on Settings > MAP FILES to use its files." : "";
	default:
		return "";
	}
}

/* ---------- the menus */

void ae_mcc_answer_use(void)
{
	const char *root = ae_mcc_root();

	if (!root)
		return;
	config_write("game.mcc_use", "yes");
	/* (HALO_MCC_PATH's folder is one run's: not kept) */
	if (mcc.result.source != AE_MCC_SOURCE_ENVIRONMENT && ae_mcc_path_storable(root) &&
		strcmp(config_string("game.mcc_path"), root))
	{
		config_write("game.mcc_path", root);
	}
	platform_log("mcc: USE MCC FILES: game.mcc_use = \"yes\", reading from %s", root);
}

void ae_mcc_answer_never(void)
{
	config_write("game.mcc_use", "no");
	platform_log("mcc: NEVER ASK AGAIN: game.mcc_use = \"no\"");
}

void ae_mcc_answer_not_now(void)
{
	platform_log("mcc: NOT NOW: game.mcc_use stays \"%s\"", config_string("game.mcc_use"));
}

/* the folder BROWSE chose, or one of its own folders' parents: the MCC folder
(the one holding halo1), its halo1, halo1/maps or the custom_edition folder
itself. 1 with the MCC folder in root */
static int browsed_root(const char *chosen, char *root, size_t size)
{
	char candidate[AE_MCC_PATH_SIZE], why[384], first_why[384] = "";
	int level;

	snprintf(candidate, sizeof(candidate), "%s", chosen);
	for (level = 0; level < 4; level++)
	{
		size_t length = strlen(candidate);
		char *cut;

		while (length > 1 && (candidate[length - 1] == '/' || candidate[length - 1] == '\\'))
			candidate[--length] = 0;
		if (ae_mcc_root_valid(candidate, why, sizeof(why)))
		{
			snprintf(root, size, "%s", candidate);
			return 1;
		}
		if (!level)
			snprintf(first_why, sizeof(first_why), "%s", why);
		cut = strrchr(candidate, '/');
		if (!cut || (strrchr(candidate, '\\') && strrchr(candidate, '\\') > cut))
			cut = strrchr(candidate, '\\');
		if (!cut || cut == candidate)
			break;
		*cut = 0;
	}
	platform_log("mcc: BROWSE: %s is not an MCC folder, nor in one (%s)", chosen, first_why);
	return 0;
}

/* a folder chosen, checked and kept (the menus' thread, with the lock held) */
static void browse_take_locked(const char *chosen)
{
	char root[AE_MCC_PATH_SIZE];

	if (!browsed_root(chosen, root, sizeof(root)))
	{
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THAT FOLDER HAS NO CUSTOM EDITION FILES. PICK\nTHE MCC FOLDER, THE ONE THAT HOLDS halo1.");
		return;
	}
	if (!ae_mcc_path_storable(root))
	{
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THAT FOLDER'S PATH IS TOO LONG TO KEEP\n(UNDER 256 CHARACTERS).");
		return;
	}
	config_write("game.mcc_path", root);
	mcc.detected = 1;
	mcc.result.state = AE_MCC_FOUND;
	mcc.result.source = AE_MCC_SOURCE_BROWSE;
	snprintf(mcc.result.root, sizeof(mcc.result.root), "%s", root);
	snprintf(mcc.result.reason, sizeof(mcc.result.reason), "the folder BROWSE chose");
	log_result("BROWSE");
	mcc.browse_message[0] = 0;
}

#ifdef AE_MCC_FOLDER_DIALOG
static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter)
{
	(void)userdata;
	(void)filter;
	pthread_mutex_lock(&mcc_lock);
	mcc.dialog_open = 0;
	if (!files)
	{
		mcc.dialog_failed = 1;
		platform_log("mcc: BROWSE: the folder dialog failed: %s", SDL_GetError());
	}
	else if (files[0])
	{
		snprintf(mcc.chosen_path, sizeof(mcc.chosen_path), "%s", files[0]);
		mcc.chosen = 1;
	}
	else
	{
		platform_log("mcc: BROWSE: cancelled");
	}
	pthread_mutex_unlock(&mcc_lock);
}
#endif

void ae_mcc_browse(void)
{
	const char *test_result = getenv("HALO_MCC_BROWSE_RESULT");

	detect();
	pthread_mutex_lock(&mcc_lock);
	if (mcc.dialog_open)
	{
		pthread_mutex_unlock(&mcc_lock);
		return;
	}
	mcc.browse_message[0] = 0;
	mcc.dialog_failed = 0;
	/* (the tests' folder, chosen as the dialog would) */
	if (test_result && *test_result)
	{
		platform_log("mcc: BROWSE: HALO_MCC_BROWSE_RESULT %s", test_result);
		snprintf(mcc.chosen_path, sizeof(mcc.chosen_path), "%s", test_result);
		mcc.chosen = 1;
		pthread_mutex_unlock(&mcc_lock);
		return;
	}
#ifdef AE_MCC_FOLDER_DIALOG
	mcc.dialog_open = 1;
	pthread_mutex_unlock(&mcc_lock);
	platform_log("mcc: BROWSE: the folder dialog opened");
	SDL_ShowOpenFolderDialog(dialog_done, NULL, SDL_GetKeyboardFocus(),
		mcc.result.state == AE_MCC_FOUND ? mcc.result.root : NULL, false);
#else
	snprintf(mcc.browse_message, sizeof(mcc.browse_message), "THIS BUILD HAS NO FOLDER DIALOG:\nSET game.mcc_path.");
	pthread_mutex_unlock(&mcc_lock);
#endif
}

/* the path, shortened from its start to fit the line */
static void shortened(const char *path, char *out, size_t size, size_t most)
{
	size_t length = strlen(path);

	if (length <= most)
		snprintf(out, size, "%s", path);
	else
		snprintf(out, size, "...%s", path + length - (most - 3));
}

const char *ae_mcc_status_text(void)
{
	char path[128];

	detect();
	pthread_mutex_lock(&mcc_lock);
	if (mcc.chosen)
	{
		mcc.chosen = 0;
		browse_take_locked(mcc.chosen_path);
	}
	if (mcc.dialog_failed)
	{
		mcc.dialog_failed = 0;
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THE FOLDER DIALOG COULD NOT OPEN.\nSET game.mcc_path IN config.toml INSTEAD.");
	}
	if (mcc.dialog_open)
		snprintf(mcc.status, sizeof(mcc.status), "CHOOSING A FOLDER...");
	else if (mcc.browse_message[0])
		snprintf(mcc.status, sizeof(mcc.status), "%s", mcc.browse_message);
	else if (mcc.result.state == AE_MCC_FOUND)
	{
		shortened(mcc.result.root, path, sizeof(path), 48);
		snprintf(mcc.status, sizeof(mcc.status), "%s\n%s",
			ae_mcc_use() == AE_MCC_USE_YES ? "IN USE:" : "FOUND (SET MCC FILES TO ON TO USE IT):", path);
	}
	else if (mcc.result.state == AE_MCC_INCOMPLETE)
		snprintf(mcc.status, sizeof(mcc.status), "MCC FOUND, BUT ITS CUSTOM EDITION FILES ARE\nMISSING. CHECK THE INSTALL IN STEAM.");
	else
		snprintf(mcc.status, sizeof(mcc.status), "NOT FOUND");
	pthread_mutex_unlock(&mcc_lock);
	return mcc.status;
}

const unsigned short *ae_mcc_status_wide(void)
{
	const unsigned char *text = (const unsigned char *)ae_mcc_status_text();
	size_t length = 0;

	/* (UTF-8 to UTF-16, the basic plane; anything else a ?) */
	while (*text && length + 1 < sizeof(mcc.status_wide) / sizeof(mcc.status_wide[0]))
	{
		unsigned long code = *text++;
		int more = code >= 0xf0 ? 3 : code >= 0xe0 ? 2 : code >= 0xc0 ? 1 : 0;

		if (more)
		{
			code &= 0x3f >> more;
			for (; more && (*text & 0xc0) == 0x80; more--)
				code = code << 6 | (*text++ & 0x3f);
			if (more || code > 0xffff)
				code = '?';
		}
		else if (code >= 0x80)
		{
			code = '?';
		}
		/* (a line break as the menus' text has it: menu_files.c's halo_menus_utf16) */
		else if (code == '\n' && length + 2 < sizeof(mcc.status_wide) / sizeof(mcc.status_wide[0]))
		{
			mcc.status_wide[length++] = '\r';
		}
		mcc.status_wide[length++] = (unsigned short)code;
	}
	mcc.status_wide[length] = 0;
	return mcc.status_wide;
}
