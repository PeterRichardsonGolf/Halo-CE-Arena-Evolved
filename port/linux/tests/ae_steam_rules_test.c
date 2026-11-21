/* ae_steam_rules.c: STEAM ACCOUNT's state logic (which page the client's state is, what its messages mean, the files'
rows, the staged states of DEV MODE's `menu steam`) */
#include <stdio.h>
#include <string.h>

#include "ae_steam_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_steam_status make(int session)
{
	struct ae_steam_status status;

	memset(&status, 0, sizeof(status));
	status.session = session;
	status.owns = -1;
	return status;
}

int main(void)
{
	struct ae_steam_status status;
	char text[256];
	int stage, index;

	/* the page the client's state is on */
	status = make(AE_STEAM_SESSION_OUT);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT);
	status.session = AE_STEAM_SESSION_UNAVAILABLE;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT);
	status.session = AE_STEAM_SESSION_CONNECTING;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_SIGNING_IN);
	status.session = AE_STEAM_SESSION_SIGNING_IN;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_SIGNING_IN);
	status.session = AE_STEAM_SESSION_IN;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_IN);
	/* the saved sign-in being read: the signing-in page (no SIGN IN), with what is waited for */
	status = make(AE_STEAM_SESSION_OUT);
	CHECK(!ae_steam_checking_text(&status));
	status.checking = 1;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_SIGNING_IN);
	CHECK(!strcmp(ae_steam_checking_text(&status), "CHECKING YOUR SAVED SIGN-IN..."));
	status.store_waiting = 1;
	CHECK(!strcmp(ae_steam_checking_text(&status), "WAITING FOR YOUR SYSTEM'S PASSWORD STORE"));
	status.checking = 0;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT && !ae_steam_checking_text(&status));
	status = make(AE_STEAM_SESSION_OUT);
	/* (a busy Steam's and a waiting store's own words are shown as they are) */
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "Steam is busy: try again in 2 minutes.", text, sizeof(text));
	CHECK(!strcmp(text, "Steam is busy: try again in 2 minutes."));
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "Waiting for your system's password store.", text, sizeof(text));
	CHECK(!strcmp(text, "Waiting for your system's password store."));
	/* a time-out after the password went out says to deny Steam's prompt as well */
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE,
		"Steam did not answer in time. If Steam asks you to approve 'Arena Evolved', deny it.", text, sizeof(text));
	CHECK(!strncmp(text, "Steam did not answer in time. If Steam asks", 43) && strstr(text, "deny it"));
	/* an emailed code: the code's page; the app's approval: its page, until ENTER A CODE INSTEAD */
	status.session = AE_STEAM_SESSION_GUARD;
	status.guard_code = 1;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_GUARD_CODE);
	status.guard_app = 1;
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_GUARD_APP);
	CHECK(ae_steam_page(&status, 1) == AE_STEAM_PAGE_GUARD_CODE);
	/* the app alone offers no code to type instead */
	status.guard_code = 0;
	CHECK(ae_steam_page(&status, 1) == AE_STEAM_PAGE_GUARD_APP);
	CHECK(!ae_steam_code_instead_possible(&status));
	status.guard_code = 1;
	CHECK(ae_steam_code_instead_possible(&status));

	/* what the client's messages mean */
	CHECK(ae_steam_message_kind("") == AE_STEAM_MESSAGE_NONE);
	CHECK(ae_steam_message_kind("Signed in.") == AE_STEAM_MESSAGE_NONE);
	CHECK(ae_steam_message_kind("Connecting to Steam...") == AE_STEAM_MESSAGE_NONE);
	CHECK(ae_steam_message_kind("Steam did not accept that account name and password.") == AE_STEAM_MESSAGE_WRONG_PASSWORD);
	CHECK(ae_steam_message_kind("Wrong code: A types it again.") == AE_STEAM_MESSAGE_WRONG_CODE);
	CHECK(ae_steam_message_kind("Signing in was cancelled.") == AE_STEAM_MESSAGE_CANCELLED);
	CHECK(ae_steam_message_kind("Logged out, and revoked with Steam.") == AE_STEAM_MESSAGE_LOGGED_OUT);
	CHECK(ae_steam_message_kind("Logged out here; Steam was out of reach to revoke it (Manage Steam Guard can).") ==
		AE_STEAM_MESSAGE_LOGGED_OUT);
	CHECK(ae_steam_message_kind("the connection to Steam closed") == AE_STEAM_MESSAGE_UNREACHABLE);
	CHECK(ae_steam_message_kind("Steam no longer accepts this sign-in: sign in again.") == AE_STEAM_MESSAGE_UNREACHABLE);
	/* the words shown for each */
	ae_steam_message_text(AE_STEAM_MESSAGE_WRONG_PASSWORD, "x", text, sizeof(text));
	CHECK(!strcmp(text, "Steam did not accept that account name and password."));
	ae_steam_message_text(AE_STEAM_MESSAGE_WRONG_CODE, "x", text, sizeof(text));
	CHECK(!strcmp(text, "Wrong code: type it again."));
	ae_steam_message_text(AE_STEAM_MESSAGE_CANCELLED, "x", text, sizeof(text));
	CHECK(!strcmp(text, "Signing in was cancelled."));
	/* cancelled after the password went out: Steam may still ask the player to approve, and says so */
	CHECK(ae_steam_message_kind("Signing in was cancelled. If Steam asks you to approve 'Arena Evolved', deny it.") ==
		AE_STEAM_MESSAGE_CANCELLED);
	ae_steam_message_text(AE_STEAM_MESSAGE_CANCELLED,
		"Signing in was cancelled. If Steam asks you to approve 'Arena Evolved', deny it.", text, sizeof(text));
	CHECK(!strcmp(text, "Signing in was cancelled. If Steam asks you to approve 'Arena Evolved', deny it."));
	ae_steam_message_text(AE_STEAM_MESSAGE_CANCELLED, "Signing in was cancelled.", text, sizeof(text));
	CHECK(!strcmp(text, "Signing in was cancelled."));
	ae_steam_message_text(AE_STEAM_MESSAGE_LOGGED_OUT, "Logged out, and revoked with Steam.", text, sizeof(text));
	CHECK(!strcmp(text, "Logged out, and revoked with Steam."));
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "the connection to Steam closed", text, sizeof(text));
	CHECK(!strcmp(text, "Can't reach Steam. Check your connection and try again. (the connection to Steam closed)"));
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "", text, sizeof(text));
	CHECK(!strcmp(text, "Can't reach Steam. Check your connection and try again."));
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "no Steam server answered in 20 seconds", text, sizeof(text));
	CHECK(!strcmp(text, "Can't reach Steam. Check your connection and try again. (no Steam server answered in 20 seconds)"));
	ae_steam_message_text(AE_STEAM_MESSAGE_UNREACHABLE, "Steam no longer accepts this sign-in: sign in again.", text, sizeof(text));
	CHECK(!strcmp(text, "Steam no longer accepts this sign-in: sign in again."));
	/* the answers of a stopped or timed-out sign-in */
	CHECK(ae_steam_message_kind("no Steam server answered in 20 seconds") == AE_STEAM_MESSAGE_UNREACHABLE);
	CHECK(ae_steam_message_kind("Steam did not answer in time") == AE_STEAM_MESSAGE_UNREACHABLE);

	/* SIGN IN needs both fields */
	CHECK(!ae_steam_can_sign_in("", "") && !ae_steam_can_sign_in("arena", "") && !ae_steam_can_sign_in("", "pw"));
	CHECK(ae_steam_can_sign_in("arena", "pw"));
	CHECK(!ae_steam_can_sign_in("   ", "pw"));
	/* the Steam Guard code: 5 characters, never submitted by itself, but not empty */
	CHECK(AE_STEAM_CODE_LIMIT == 5 && ae_steam_can_submit_code("F7K") && !ae_steam_can_submit_code(""));

	/* the settings row's summary */
	status = make(AE_STEAM_SESSION_OUT);
	ae_steam_account_summary(&status, 0, text, sizeof(text));
	CHECK(!strcmp(text, "NOT SIGNED IN"));
	ae_steam_account_summary(&status, 1, text, sizeof(text));
	CHECK(!strcmp(text, "NOT NEEDED: MCC FOUND"));
	status.session = AE_STEAM_SESSION_SIGNING_IN;
	ae_steam_account_summary(&status, 0, text, sizeof(text));
	CHECK(!strcmp(text, "SIGNING IN"));
	status.session = AE_STEAM_SESSION_GUARD;
	ae_steam_account_summary(&status, 0, text, sizeof(text));
	CHECK(!strcmp(text, "SIGNING IN"));
	status.session = AE_STEAM_SESSION_IN;
	snprintf(status.account, sizeof(status.account), "arena_player");
	ae_steam_account_summary(&status, 1, text, sizeof(text));
	CHECK(!strcmp(text, "ar***"));   /* (the login name is half the credential: never shown whole) */
	status.account[0] = 0;
	ae_steam_account_summary(&status, 0, text, sizeof(text));
	CHECK(!strcmp(text, "SIGNED IN"));

	/* the masked login name: its first 2 characters and stars; a short name keeps less */
	ae_steam_account_masked("arena_player", text, sizeof(text));
	CHECK(!strcmp(text, "ar***"));
	ae_steam_account_masked("abc", text, sizeof(text));
	CHECK(!strcmp(text, "ab***"));
	ae_steam_account_masked("ab", text, sizeof(text));
	CHECK(!strcmp(text, "a***"));
	ae_steam_account_masked("a", text, sizeof(text));
	CHECK(!strcmp(text, "***"));
	ae_steam_account_masked("", text, sizeof(text));
	CHECK(!strcmp(text, ""));
	ae_steam_account_masked("arena_player", text, 4);
	CHECK(!strcmp(text, "ar*"));

	/* SHARED MAP FILES */
	status = make(AE_STEAM_SESSION_IN);
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_CHECKING);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "CHECKING"));
	status.files_known = 1;
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_DOWNLOAD);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "DOWNLOAD"));
	status.owns = 0;
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_NOT_OWNED);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "NOT OWNED"));
	status.owns = 1;
	status.queue_count = 1;
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_DOWNLOAD);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "DOWNLOAD"));
	status.shared_queued = 1;
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "QUEUED"));
	status.shared_queued = 0;
	status.queue_count = 0;
	status.downloading = -1;
	status.size = 1126ull * 1024 * 1024;
	status.done = 412ull * 1024 * 1024;
	CHECK(ae_steam_percent(&status) == 37);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "DOWNLOADING  37 %"));
	status.size = 0;
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "STARTING"));
	/* all three here */
	status.downloading = 0;
	status.shared_present[0] = status.shared_present[1] = status.shared_present[2] = 1;
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_READY);
	ae_steam_shared_value(&status, text, sizeof(text));
	CHECK(!strcmp(text, "READY"));
	/* a download of another thing (a Workshop item) is not the shared files' */
	status.shared_present[2] = 0;
	status.downloading = 1;
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_DOWNLOAD);
	status.downloading = 0;
	/* percent never passes 100 and a missing total is 0 */
	status.downloading = -1;
	status.size = 100;
	status.done = 5000;
	CHECK(ae_steam_percent(&status) == 100);
	status.size = 0;
	CHECK(ae_steam_percent(&status) == 0);

	/* each file's state beside the shared download: those here, the one coming, those after it */
	status = make(AE_STEAM_SESSION_IN);
	status.files_known = 1;
	CHECK(ae_steam_file_state(&status, 0) == AE_STEAM_FILE_MISSING);
	status.downloading = -1;
	status.shared_present[0] = 1;
	CHECK(ae_steam_file_state(&status, 0) == AE_STEAM_FILE_HERE);
	CHECK(ae_steam_file_state(&status, 1) == AE_STEAM_FILE_DOWNLOADING);
	CHECK(ae_steam_file_state(&status, 2) == AE_STEAM_FILE_NEXT);
	status.files_known = 0;
	CHECK(ae_steam_file_state(&status, 0) == AE_STEAM_FILE_CHECKING);
	CHECK(!strcmp(ae_steam_file_state_text(AE_STEAM_FILE_HERE), "HERE") &&
		!strcmp(ae_steam_file_state_text(AE_STEAM_FILE_MISSING), "MISSING") &&
		!strcmp(ae_steam_file_state_text(AE_STEAM_FILE_DOWNLOADING), "DOWNLOADING") &&
		!strcmp(ae_steam_file_state_text(AE_STEAM_FILE_NEXT), "NEXT") &&
		!strcmp(ae_steam_file_state_text(AE_STEAM_FILE_CHECKING), "..."));
	CHECK(!strcmp(ae_steam_file_label(0), "CAMPAIGN BITMAPS") && !strcmp(ae_steam_file_label(1), "MULTIPLAYER BITMAPS") &&
		!strcmp(ae_steam_file_label(2), "MULTIPLAYER SOUNDS") && !strcmp(ae_steam_file_label(3), ""));

	/* the progress line under the bar */
	status = make(AE_STEAM_SESSION_IN);
	status.downloading = -1;
	ae_steam_progress_line(&status, text, sizeof(text));
	CHECK(!strcmp(text, "Asking Steam... A cancels."));
	status.size = 1126ull * 1024 * 1024;
	status.done = 412ull * 1024 * 1024;
	status.speed = (unsigned long long)(18.4 * 1024 * 1024);
	ae_steam_progress_line(&status, text, sizeof(text));
	CHECK(!strcmp(text, "412 MB of 1.1 GB (18.4 MB/s). A cancels."));
	status.speed = 0;
	ae_steam_progress_line(&status, text, sizeof(text));
	CHECK(!strcmp(text, "412 MB of 1.1 GB. A cancels."));
	status.queue_count = 2;
	ae_steam_progress_line(&status, text, sizeof(text));
	CHECK(!strcmp(text, "412 MB of 1.1 GB. A cancels. 2 queued."));

	/* sizes and counts as the card shows them */
	ae_steam_size_text(38ull * 1024 * 1024 + 1000, text, sizeof(text));
	CHECK(!strcmp(text, "38 MB"));
	ae_steam_size_text(1126ull * 1024 * 1024, text, sizeof(text));
	CHECK(!strcmp(text, "1.1 GB"));
	ae_steam_size_text(900ull * 1024, text, sizeof(text));
	CHECK(!strcmp(text, "900 KB"));
	ae_steam_count_text(2140, text, sizeof(text));
	CHECK(!strcmp(text, "2,140"));
	ae_steam_count_text(0, text, sizeof(text));
	CHECK(!strcmp(text, "0"));
	ae_steam_count_text(1234567, text, sizeof(text));
	CHECK(!strcmp(text, "1,234,567"));
	CHECK(!strcmp(ae_steam_item_type("Halo1,Multiplayer"), "MULTIPLAYER"));
	CHECK(!strcmp(ae_steam_item_type("Halo1,Campaign"), "CAMPAIGN"));
	CHECK(!strcmp(ae_steam_item_type("Halo1"), ""));
	CHECK(!strcmp(ae_steam_item_type(""), ""));

	/* an item's state on the card */
	status = make(AE_STEAM_SESSION_IN);
	CHECK(!strcmp(ae_steam_item_state_text(&status), "NOT INSTALLED"));
	status.item_installed = 1;
	CHECK(!strcmp(ae_steam_item_state_text(&status), "INSTALLED"));
	status.item_installed = 2;
	CHECK(!strcmp(ae_steam_item_state_text(&status), "UPDATE OUT"));
	status.item_queued = 1;
	CHECK(!strcmp(ae_steam_item_state_text(&status), "QUEUED"));
	status.item_downloading = 1;
	CHECK(!strcmp(ae_steam_item_state_text(&status), "DOWNLOADING"));

	/* leaving while a download runs or waits asks first */
	status = make(AE_STEAM_SESSION_IN);
	CHECK(!ae_steam_leave_asks(&status));
	status.downloading = -1;
	CHECK(ae_steam_leave_asks(&status));
	status.downloading = 0;
	status.queue_count = 1;
	CHECK(ae_steam_leave_asks(&status));
	ae_steam_leave_words(&status, text, sizeof(text));
	CHECK(!strcmp(text, "1 download is going. Wait for it, or cancel it, to leave."));
	status.queue_count = 2;
	status.downloading = 1;
	ae_steam_leave_words(&status, text, sizeof(text));
	CHECK(!strcmp(text, "3 downloads are going. Wait for them, or cancel them, to leave."));

	/* the sign-in's security line by system, and when there is no store */
	CHECK(strstr(ae_steam_store_text(AE_STEAM_SYSTEM_LINUX, 1), "Secret Service"));
	CHECK(strstr(ae_steam_store_text(AE_STEAM_SYSTEM_MACOS, 1), "login Keychain"));
	CHECK(strstr(ae_steam_store_text(AE_STEAM_SYSTEM_WINDOWS, 1), "only your Windows account"));
	CHECK(strstr(ae_steam_store_text(AE_STEAM_SYSTEM_LINUX, 0), "This run only"));
	CHECK(strstr(ae_steam_store_text(AE_STEAM_SYSTEM_MACOS, 0), "This run only"));
	/* (not asked yet: nothing is said to be kept, and no "saved") */
	CHECK(!strstr(ae_steam_store_text(AE_STEAM_SYSTEM_WINDOWS, -1), "kept in a file") &&
		strstr(ae_steam_store_text(AE_STEAM_SYSTEM_WINDOWS, -1), "when there is one"));
	CHECK(strstr(ae_steam_first_store_text(AE_STEAM_SYSTEM_LINUX, -1), "when there is one"));

	/* the first-start page shows only: client built, not asked, no MCC found, not signed in to Steam */
	CHECK(ae_steam_first_start_asks(1, 0, 1, 0));
	CHECK(!ae_steam_first_start_asks(0, 0, 1, 0) && !ae_steam_first_start_asks(1, 1, 1, 0) &&
		!ae_steam_first_start_asks(1, 0, 0, 0) && !ae_steam_first_start_asks(1, 0, 1, 1));
	/* the first-start page's sentence on how the sign-in is kept */
	CHECK(strstr(ae_steam_first_store_text(AE_STEAM_SYSTEM_LINUX, 1), "system's keyring keeps the sign-in until you log out"));
	CHECK(strstr(ae_steam_first_store_text(AE_STEAM_SYSTEM_MACOS, 1), "login Keychain keeps"));
	CHECK(strstr(ae_steam_first_store_text(AE_STEAM_SYSTEM_WINDOWS, 1), "only your Windows account can open keeps"));
	CHECK(strstr(ae_steam_first_store_text(AE_STEAM_SYSTEM_LINUX, 0), "This run only"));

	/* DEV MODE's staged states (menu steam <state>) */
	CHECK(ae_steam_stage_find("signed-out", &stage) && stage == AE_STEAM_STAGE_SIGNED_OUT);
	CHECK(ae_steam_stage_find("TYPING", &stage) && stage == AE_STEAM_STAGE_TYPING);
	CHECK(ae_steam_stage_find("guard-code", &stage) && stage == AE_STEAM_STAGE_GUARD_CODE);
	CHECK(ae_steam_stage_find("guard-app", &stage) && stage == AE_STEAM_STAGE_GUARD_APP);
	CHECK(ae_steam_stage_find("missing", &stage) && stage == AE_STEAM_STAGE_MISSING);
	CHECK(ae_steam_stage_find("downloading", &stage) && stage == AE_STEAM_STAGE_DOWNLOADING);
	CHECK(ae_steam_stage_find("ready", &stage) && stage == AE_STEAM_STAGE_READY);
	CHECK(ae_steam_stage_find("workshop", &stage) && stage == AE_STEAM_STAGE_WORKSHOP);
	CHECK(ae_steam_stage_find("wrong-password", &stage) && stage == AE_STEAM_STAGE_WRONG_PASSWORD);
	CHECK(ae_steam_stage_find("not-owned", &stage) && stage == AE_STEAM_STAGE_NOT_OWNED);
	CHECK(ae_steam_stage_find("logout", &stage) && stage == AE_STEAM_STAGE_LOGOUT);
	CHECK(!ae_steam_stage_find("", &stage) && !ae_steam_stage_find("signedout", &stage) && !ae_steam_stage_find(NULL, &stage));
	CHECK(ae_steam_stage_name(AE_STEAM_STAGE_GUARD_APP) && !strcmp(ae_steam_stage_name(AE_STEAM_STAGE_GUARD_APP), "guard-app"));
	for (stage = 0; stage < AE_STEAM_STAGES; stage++)
	{
		int found;

		CHECK(ae_steam_stage_find(ae_steam_stage_name(stage), &found) && found == stage);
	}
	/* each stage's made-up client state */
	ae_steam_stage_status(AE_STEAM_STAGE_SIGNED_OUT, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT && !status.message[0]);
	ae_steam_stage_status(AE_STEAM_STAGE_TYPING, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT);
	ae_steam_stage_status(AE_STEAM_STAGE_GUARD_CODE, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_GUARD_CODE);
	ae_steam_stage_status(AE_STEAM_STAGE_GUARD_APP, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_GUARD_APP && status.guard_code);
	ae_steam_stage_status(AE_STEAM_STAGE_MISSING, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_IN && ae_steam_shared_state(&status) == AE_STEAM_SHARED_DOWNLOAD &&
		!strcmp(status.account, "arena_player"));
	ae_steam_stage_status(AE_STEAM_STAGE_DOWNLOADING, 37, &status);
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_DOWNLOADING && ae_steam_percent(&status) == 37);
	CHECK(ae_steam_file_state(&status, 0) == AE_STEAM_FILE_HERE && ae_steam_file_state(&status, 1) == AE_STEAM_FILE_DOWNLOADING &&
		ae_steam_file_state(&status, 2) == AE_STEAM_FILE_NEXT);
	/* (no percent: the mockup's 37) */
	ae_steam_stage_status(AE_STEAM_STAGE_DOWNLOADING, 0, &status);
	CHECK(ae_steam_percent(&status) == 37);
	ae_steam_stage_status(AE_STEAM_STAGE_DOWNLOADING, 250, &status);
	CHECK(ae_steam_percent(&status) == 100);
	ae_steam_stage_status(AE_STEAM_STAGE_READY, 0, &status);
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_READY && status.packages == 3);
	ae_steam_stage_status(AE_STEAM_STAGE_WORKSHOP, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_IN && status.total == 236 && status.item_count == AE_STEAM_STAGE_ITEMS);
	ae_steam_stage_status(AE_STEAM_STAGE_WRONG_PASSWORD, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_OUT &&
		ae_steam_message_kind(status.message) == AE_STEAM_MESSAGE_WRONG_PASSWORD);
	ae_steam_stage_status(AE_STEAM_STAGE_NOT_OWNED, 0, &status);
	CHECK(ae_steam_shared_state(&status) == AE_STEAM_SHARED_NOT_OWNED);
	ae_steam_stage_status(AE_STEAM_STAGE_LOGOUT, 0, &status);
	CHECK(ae_steam_page(&status, 0) == AE_STEAM_PAGE_IN);
	/* the staged Workshop page's items: named, sized, the first of them focused with a card */
	for (index = 0; index < AE_STEAM_STAGE_ITEMS; index++)
	{
		char name[96], state[48];

		ae_steam_stage_item(index, name, sizeof(name), state, sizeof(state));
		CHECK(name[0]);
	}
	{
		char name[96], state[48];

		ae_steam_stage_item(1, name, sizeof(name), state, sizeof(state));
		CHECK(!strcmp(state, "INSTALLED"));
		ae_steam_stage_item(2, name, sizeof(name), state, sizeof(state));
		CHECK(!strcmp(state, "UPDATE"));
		ae_steam_stage_item(0, name, sizeof(name), state, sizeof(state));
		CHECK(!state[0]);
	}
	/* the focused staged item's card fills the status */
	ae_steam_stage_status(AE_STEAM_STAGE_WORKSHOP, 0, &status);
	ae_steam_stage_item_card(0, &status);
	CHECK(status.item_valid && status.item_title[0] && status.item_size && status.item_subscribers &&
		!strcmp(ae_steam_item_type(status.item_tags), "MULTIPLAYER"));
	ae_steam_stage_item_card(2, &status);
	CHECK(status.item_installed == 2);

	if (failures)
		return 1;
	printf("ae_steam_rules_test: all passed\n");
	return 0;
}
