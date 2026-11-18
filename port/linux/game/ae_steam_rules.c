/*
AE_STEAM_RULES.C

STEAM ACCOUNT's state logic (ae_steam_rules.h).
*/

#include "ae_steam_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void put(char *out, size_t size, char const *text)
{
	if (size)
		snprintf(out, size, "%s", text);
}

/* ---------- the page */

int ae_steam_code_instead_possible(struct ae_steam_status const *status)
{
	return status->session == AE_STEAM_SESSION_GUARD && status->guard_code;
}

char const *ae_steam_checking_text(struct ae_steam_status const *status)
{
	if (!status->checking)
		return NULL;
	return status->store_waiting ? "WAITING FOR YOUR SYSTEM'S PASSWORD STORE" : "CHECKING YOUR SAVED SIGN-IN...";
}

int ae_steam_page(struct ae_steam_status const *status, int code_instead)
{
	/* (the saved sign-in is still being read: the signing-in page, whose CANCEL gives up on it; no SIGN IN) */
	if (status->checking)
		return AE_STEAM_PAGE_SIGNING_IN;
	switch (status->session)
	{
	case AE_STEAM_SESSION_CONNECTING:
	case AE_STEAM_SESSION_SIGNING_IN:
		return AE_STEAM_PAGE_SIGNING_IN;
	case AE_STEAM_SESSION_GUARD:
		/* (the app's approval first; a code when the app cannot approve, or when the player asked for it) */
		if (status->guard_app && !(code_instead && status->guard_code))
			return AE_STEAM_PAGE_GUARD_APP;
		return status->guard_code || status->guard_app ? AE_STEAM_PAGE_GUARD_CODE : AE_STEAM_PAGE_SIGNING_IN;
	case AE_STEAM_SESSION_IN:
		return AE_STEAM_PAGE_IN;
	default:
		return AE_STEAM_PAGE_OUT;
	}
}

/* ---------- messages */

int ae_steam_message_kind(char const *message)
{
	if (!message || !message[0])
		return AE_STEAM_MESSAGE_NONE;
	if (!strncmp(message, "Wrong code", 10))
		return AE_STEAM_MESSAGE_WRONG_CODE;
	if (strstr(message, "account name and password"))
		return AE_STEAM_MESSAGE_WRONG_PASSWORD;
	if (strstr(message, "was cancelled"))
		return AE_STEAM_MESSAGE_CANCELLED;
	if (!strncmp(message, "Logged out", 10))
		return AE_STEAM_MESSAGE_LOGGED_OUT;
	if (!strncmp(message, "Signed in", 9) || !strncmp(message, "Connecting to Steam", 19) ||
		!strncmp(message, "Reaching Steam", 14))
		return AE_STEAM_MESSAGE_NONE;
	return AE_STEAM_MESSAGE_UNREACHABLE;
}

void ae_steam_message_text(int kind, char const *message, char *out, size_t size)
{
	switch (kind)
	{
	case AE_STEAM_MESSAGE_WRONG_PASSWORD:
		put(out, size, "Steam did not accept that account name and password.");
		break;
	case AE_STEAM_MESSAGE_WRONG_CODE:
		put(out, size, "Wrong code: type it again.");
		break;
	case AE_STEAM_MESSAGE_CANCELLED:
		/* (cancelled after the password went out: Steam may still ask the player to approve it) */
		if (message && strstr(message, "deny it"))
			put(out, size, "Signing in was cancelled. If Steam asks you to approve 'Arena Evolved', deny it.");
		else
			put(out, size, "Signing in was cancelled.");
		break;
	case AE_STEAM_MESSAGE_LOGGED_OUT:
		put(out, size, message ? message : "");
		break;
	case AE_STEAM_MESSAGE_UNREACHABLE:
		/* (a refusal of the saved sign-in is not a lack of connection: its own words) */
		if (message && (!strncmp(message, "Steam no longer accepts", 23) || !strncmp(message, "Steam is busy", 13) ||
			!strncmp(message, "Waiting for your system", 23) ||
			!strncmp(message, "Steam did not answer", 20)))
			put(out, size, message);
		else if (message && message[0])
			snprintf(out, size, "Can't reach Steam. Check your connection and try again. (%s)", message);
		else
			put(out, size, "Can't reach Steam. Check your connection and try again.");
		break;
	default:
		put(out, size, "");
		break;
	}
}

/* ---------- fields */

int ae_steam_can_sign_in(char const *account, char const *password)
{
	int named = 0;

	for (; account && *account; account++)
		if (!isspace((unsigned char)*account))
			named = 1;
	return named && password && password[0];
}

int ae_steam_can_submit_code(char const *code)
{
	return code && code[0];
}

void ae_steam_account_masked(char const *account, char *out, size_t size)
{
	size_t length = account ? strlen(account) : 0;
	size_t shown = length > 2 ? 2 : length > 1 ? 1 : 0;
	char text[8];

	if (!size)
		return;
	if (!length)
	{
		out[0] = 0;
		return;
	}
	memcpy(text, account, shown);
	text[shown] = 0;
	snprintf(out, size, "%s***", text);
}

void ae_steam_account_summary(struct ae_steam_status const *status, int mcc_found, char *out, size_t size)
{
	switch (status->session)
	{
	case AE_STEAM_SESSION_IN:
		if (status->account[0])
			ae_steam_account_masked(status->account, out, size);
		else
			put(out, size, "SIGNED IN");
		break;
	case AE_STEAM_SESSION_CONNECTING:
	case AE_STEAM_SESSION_GUARD:
	case AE_STEAM_SESSION_SIGNING_IN:
		put(out, size, "SIGNING IN");
		break;
	default:
		put(out, size, mcc_found ? "NOT NEEDED: MCC FOUND" : "NOT SIGNED IN");
		break;
	}
}

/* ---------- shared files */

static int all_here(struct ae_steam_status const *status)
{
	int index;

	for (index = 0; index < AE_STEAM_FILES; index++)
		if (!status->shared_present[index])
			return 0;
	return 1;
}

int ae_steam_percent(struct ae_steam_status const *status)
{
	unsigned long long percent;

	if (!status->size)
		return 0;
	if (status->done >= status->size)
		return 100;
	percent = (status->done * 100ull + status->size / 2) / status->size;
	return percent > 100 ? 100 : (int)percent;
}

int ae_steam_shared_state(struct ae_steam_status const *status)
{
	if (status->downloading < 0)
		return AE_STEAM_SHARED_DOWNLOADING;
	if (!status->files_known)
		return AE_STEAM_SHARED_CHECKING;
	if (all_here(status))
		return AE_STEAM_SHARED_READY;
	if (status->owns == 0)
		return AE_STEAM_SHARED_NOT_OWNED;
	return AE_STEAM_SHARED_DOWNLOAD;
}

void ae_steam_shared_value(struct ae_steam_status const *status, char *out, size_t size)
{
	switch (ae_steam_shared_state(status))
	{
	case AE_STEAM_SHARED_DOWNLOADING:
		if (!status->size)
			put(out, size, "STARTING");
		else
			snprintf(out, size, "DOWNLOADING  %d %%", ae_steam_percent(status));
		break;
	case AE_STEAM_SHARED_READY:
		put(out, size, "READY");
		break;
	case AE_STEAM_SHARED_NOT_OWNED:
		put(out, size, "NOT OWNED");
		break;
	case AE_STEAM_SHARED_DOWNLOAD:
		put(out, size, status->shared_queued ? "QUEUED" : "DOWNLOAD");
		break;
	default:
		put(out, size, "CHECKING");
		break;
	}
}

int ae_steam_file_state(struct ae_steam_status const *status, int index)
{
	int before;

	if (index < 0 || index >= AE_STEAM_FILES || !status->files_known)
		return AE_STEAM_FILE_CHECKING;
	if (status->shared_present[index])
		return AE_STEAM_FILE_HERE;
	if (status->downloading >= 0)
		return AE_STEAM_FILE_MISSING;
	/* (the files come in order: the first one not here is the one coming) */
	for (before = 0; before < index; before++)
		if (!status->shared_present[before])
			return AE_STEAM_FILE_NEXT;
	return AE_STEAM_FILE_DOWNLOADING;
}

char const *ae_steam_file_state_text(int state)
{
	switch (state)
	{
	case AE_STEAM_FILE_HERE: return "HERE";
	case AE_STEAM_FILE_MISSING: return "MISSING";
	case AE_STEAM_FILE_DOWNLOADING: return "DOWNLOADING";
	case AE_STEAM_FILE_NEXT: return "NEXT";
	default: return "...";
	}
}

char const *ae_steam_file_label(int index)
{
	static char const *const labels[AE_STEAM_FILES] = { "CAMPAIGN BITMAPS", "MULTIPLAYER BITMAPS", "MULTIPLAYER SOUNDS" };

	return index >= 0 && index < AE_STEAM_FILES ? labels[index] : "";
}

void ae_steam_size_text(unsigned long long bytes, char *out, size_t size)
{
	if (bytes >= 1024ull * 1024 * 1024)
		snprintf(out, size, "%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
	else if (bytes >= 1024ull * 1024)
		snprintf(out, size, "%.0f MB", (double)bytes / (1024.0 * 1024));
	else
		snprintf(out, size, "%.0f KB", (double)bytes / 1024.0);
}

void ae_steam_progress_line(struct ae_steam_status const *status, char *out, size_t size)
{
	char done[32], total[32], speed[32];
	size_t used;

	if (!status->size)
	{
		put(out, size, "Asking Steam... A cancels.");
		return;
	}
	ae_steam_size_text(status->done, done, sizeof(done));
	ae_steam_size_text(status->size, total, sizeof(total));
	if (status->speed)
	{
		if (status->speed >= 1024ull * 1024)
			snprintf(speed, sizeof(speed), "%.1f MB", (double)status->speed / (1024.0 * 1024));
		else
			snprintf(speed, sizeof(speed), "%.0f KB", (double)status->speed / 1024.0);
		snprintf(out, size, "%s of %s (%s/s). A cancels.", done, total, speed);
	}
	else
		snprintf(out, size, "%s of %s. A cancels.", done, total);
	if (status->queue_count)
	{
		used = strlen(out);
		if (used < size)
			snprintf(out + used, size - used, " %d queued.", status->queue_count);
	}
}

void ae_steam_count_text(unsigned long number, char *out, size_t size)
{
	char digits[32], grouped[48];
	size_t length, index, at = 0;

	snprintf(digits, sizeof(digits), "%lu", number);
	length = strlen(digits);
	for (index = 0; index < length; index++)
	{
		if (index && (length - index) % 3 == 0)
			grouped[at++] = ',';
		grouped[at++] = digits[index];
	}
	grouped[at] = 0;
	put(out, size, grouped);
}

char const *ae_steam_item_type(char const *tags)
{
	if (!tags)
		return "";
	if (strstr(tags, "Campaign"))
		return "CAMPAIGN";
	if (strstr(tags, "Multiplayer"))
		return "MULTIPLAYER";
	return "";
}

char const *ae_steam_item_state_text(struct ae_steam_status const *status)
{
	if (status->item_downloading)
		return "DOWNLOADING";
	if (status->item_queued)
		return "QUEUED";
	if (status->item_installed == 2)
		return "UPDATE OUT";
	return status->item_installed == 1 ? "INSTALLED" : "NOT INSTALLED";
}

int ae_steam_leave_asks(struct ae_steam_status const *status)
{
	return status->downloading != 0 || status->queue_count > 0;
}

void ae_steam_leave_words(struct ae_steam_status const *status, char *out, size_t size)
{
	int count = (status->downloading ? 1 : 0) + status->queue_count;

	if (count == 1)
		put(out, size, "1 download is going. Wait for it, or cancel it, to leave.");
	else
		snprintf(out, size, "%d downloads are going. Wait for them, or cancel them, to leave.", count);
}

char const *ae_steam_store_text(int system, int store)
{
	/* (not asked yet: nothing is said to be kept) */
	if (store < 0)
		return "Steam's sign-in is kept in your system's password store when there is one, and ends when the game closes "
			"when there is not.";
	if (!store)
		return "This run only: there is no keyring here, so the sign-in ends when the game closes.";
	switch (system)
	{
	case AE_STEAM_SYSTEM_MACOS:
		return "Steam's sign-in is kept in your login Keychain. LOG OUT ends it with Steam too.";
	case AE_STEAM_SYSTEM_WINDOWS:
		return "Steam's sign-in is kept in a file only your Windows account can open. LOG OUT ends it with Steam too.";
	default:
		return "Steam's sign-in is kept in your system's keyring (Secret Service). With no keyring it lasts until the "
			"game closes. LOG OUT ends it with Steam too.";
	}
}

int ae_steam_first_start_asks(int client_built, int asked, int mcc_not_found, int signed_in)
{
	return client_built && !asked && mcc_not_found && !signed_in;
}

char const *ae_steam_first_store_text(int system, int store)
{
	if (store < 0)
		return "Your system's password store keeps the sign-in when there is one; without one it ends when the game closes.";
	if (!store)
		return "This run only: there is no keyring here, so the sign-in ends when the game closes.";
	switch (system)
	{
	case AE_STEAM_SYSTEM_MACOS:
		return "Your login Keychain keeps the sign-in until you log out.";
	case AE_STEAM_SYSTEM_WINDOWS:
		return "A file only your Windows account can open keeps the sign-in until you log out.";
	default:
		return "Your system's keyring keeps the sign-in until you log out.";
	}
}

/* ---------- staged states */

static char const *const stage_names[AE_STEAM_STAGES] =
{
	"signed-out", "typing", "guard-code", "guard-app", "missing", "downloading", "ready", "workshop", "wrong-password",
	"not-owned", "logout"
};

static struct { char const *name, *state; unsigned long long size; unsigned long subscribers; char const *tags; }
const stage_items[AE_STEAM_STAGE_ITEMS] =
{
	{ "Sympathy", "", 38ull * 1024 * 1024, 2140, "Halo1,Multiplayer" },
	{ "Warren", "INSTALLED", 24ull * 1024 * 1024, 1860, "Halo1,Multiplayer" },
	{ "Gothra", "UPDATE", 51ull * 1024 * 1024, 1204, "Halo1,Multiplayer" },
	{ "Ascetic", "", 19ull * 1024 * 1024, 988, "Halo1,Multiplayer" },
	{ "Hektik", "INSTALLED", 33ull * 1024 * 1024, 870, "Halo1,Multiplayer" },
	{ "Fissurefall", "", 46ull * 1024 * 1024, 744, "Halo1,Multiplayer" },
	{ "Cold Shoulder", "", 28ull * 1024 * 1024, 611, "Halo1,Multiplayer" },
	{ "Longest Night", "", 212ull * 1024 * 1024, 590, "Halo1,Campaign" },
	{ "Rubble", "", 22ull * 1024 * 1024, 402, "Halo1,Multiplayer" },
	{ "Zanzibar Rising", "", 41ull * 1024 * 1024, 388, "Halo1,Multiplayer" },
	{ "Narrows", "", 17ull * 1024 * 1024, 310, "Halo1,Multiplayer" },
	{ "The Pit Again", "", 26ull * 1024 * 1024, 255, "Halo1,Multiplayer" }
};

int ae_steam_stage_find(char const *word, int *stage)
{
	int index;

	if (!word || !word[0])
		return 0;
	for (index = 0; index < AE_STEAM_STAGES; index++)
	{
		char const *name = stage_names[index];
		size_t at = 0;

		while (name[at] && word[at] && tolower((unsigned char)word[at]) == name[at])
			at++;
		if (!name[at] && !word[at])
		{
			*stage = index;
			return 1;
		}
	}
	return 0;
}

char const *ae_steam_stage_name(int stage)
{
	return stage >= 0 && stage < AE_STEAM_STAGES ? stage_names[stage] : "";
}

void ae_steam_stage_status(int stage, int percent, struct ae_steam_status *status)
{
	memset(status, 0, sizeof(*status));
	status->owns = -1;
	status->page = 1;
	switch (stage)
	{
	case AE_STEAM_STAGE_GUARD_CODE:
		status->session = AE_STEAM_SESSION_GUARD;
		status->guard_code = 1;
		put(status->account, sizeof(status->account), "arena_player");
		return;
	case AE_STEAM_STAGE_GUARD_APP:
		status->session = AE_STEAM_SESSION_GUARD;
		status->guard_app = 1;
		status->guard_code = 1;
		put(status->account, sizeof(status->account), "arena_player");
		return;
	case AE_STEAM_STAGE_WRONG_PASSWORD:
		put(status->message, sizeof(status->message), "Steam did not accept that account name and password.");
		return;
	case AE_STEAM_STAGE_SIGNED_OUT:
	case AE_STEAM_STAGE_TYPING:
		return;
	default:
		break;
	}
	status->session = AE_STEAM_SESSION_IN;
	put(status->account, sizeof(status->account), "arena_player");
	status->owns = 1;
	status->files_known = 1;
	status->packages = 3;
	switch (stage)
	{
	case AE_STEAM_STAGE_MISSING:
		status->packages = 0;
		break;
	case AE_STEAM_STAGE_DOWNLOADING:
		status->packages = 0;
		status->downloading = -1;
		status->shared_present[0] = 1;
		status->size = 1126ull * 1024 * 1024;
		if (percent <= 0)
			percent = 37;
		if (percent > 100)
			percent = 100;
		status->done = status->size * (unsigned long long)percent / 100;
		status->speed = (unsigned long long)(18.4 * 1024 * 1024);
		break;
	case AE_STEAM_STAGE_READY:
		status->shared_present[0] = status->shared_present[1] = status->shared_present[2] = 1;
		break;
	case AE_STEAM_STAGE_WORKSHOP:
		status->shared_present[0] = status->shared_present[1] = status->shared_present[2] = 1;
		status->total = 236;
		status->item_count = AE_STEAM_STAGE_ITEMS;
		ae_steam_stage_item_card(0, status);
		break;
	case AE_STEAM_STAGE_NOT_OWNED:
		status->owns = 0;
		status->packages = 0;
		break;
	default:
		break;
	}
}

void ae_steam_stage_item(int index, char *name, size_t name_size, char *state, size_t state_size)
{
	if (index < 0 || index >= AE_STEAM_STAGE_ITEMS)
	{
		put(name, name_size, "");
		put(state, state_size, "");
		return;
	}
	put(name, name_size, stage_items[index].name);
	put(state, state_size, stage_items[index].state);
}

void ae_steam_stage_item_card(int index, struct ae_steam_status *status)
{
	if (index < 0 || index >= AE_STEAM_STAGE_ITEMS)
	{
		status->item_valid = 0;
		return;
	}
	status->item_valid = 1;
	put(status->item_title, sizeof(status->item_title), stage_items[index].name);
	put(status->item_description, sizeof(status->item_description),
		"(Sample) The item's own description from its Workshop page, cut to three lines here; the whole text is on Steam.");
	put(status->item_tags, sizeof(status->item_tags), stage_items[index].tags);
	status->item_size = stage_items[index].size;
	status->item_subscribers = stage_items[index].subscribers;
	status->item_installed = !strcmp(stage_items[index].state, "INSTALLED") ? 1 :
		!strcmp(stage_items[index].state, "UPDATE") ? 2 : 0;
	status->item_downloading = status->item_queued = 0;
}
