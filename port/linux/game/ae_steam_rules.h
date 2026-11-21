/*
AE_STEAM_RULES.H

STEAM ACCOUNT's state logic (ae_screen_steam.c draws it).
The Steam client (port/android/host/steam_downloads.c, built into the desktop game: ae_steam_client.h) says what it is
doing as a struct ae_steam_status; this turns that into which page the screen is on, what the client's messages mean,
the shared files' rows, the sizes and counts the card shows, and the made-up states DEV MODE's `menu steam <state>`
stages for a TEST RUN (no Steam traffic). Pure: string and stdlib only, no engine headers (unit test:
port/linux/tests/ae_steam_rules_test.c).
*/

#ifndef AE_STEAM_RULES_H
#define AE_STEAM_RULES_H

#include <stddef.h>

/* the client's session states (steam_downloads.c's enum session_state, in its order) */
enum
{
	AE_STEAM_SESSION_OUT, AE_STEAM_SESSION_CONNECTING, AE_STEAM_SESSION_GUARD, AE_STEAM_SESSION_SIGNING_IN,
	AE_STEAM_SESSION_IN, AE_STEAM_SESSION_UNAVAILABLE
};

/* MCC's shared files: Halo: CE Anniversary's three (the client's shared_files, in its order) */
enum { AE_STEAM_FILES = 3 };
/* the Steam Guard code's length */
enum { AE_STEAM_CODE_LIMIT = 5 };

/* what the client is doing, copied out once a frame (ae_steam_client.c) */
struct ae_steam_status
{
	int session;                      /* AE_STEAM_SESSION_* */
	int checking;                     /* the saved sign-in is being read (SIGN IN waits; CANCEL gives up on it) */
	int store_waiting;                /* the system's password store is being asked (it may be showing a prompt) */
	int guard_app;                    /* Steam Guard: the app can approve it (or an email's link) */
	int guard_code;                   /* Steam Guard: a code can be typed (the app's, or the emailed one) */
	char account[64];                 /* the account's name as typed, once signed in */
	char message[256];                /* the client's latest message ("" none) */
	int owns;                         /* -1 not known, 0 no, 1 yes: the account owns Halo: CE Anniversary */
	int files_known;
	int shared_present[AE_STEAM_FILES];
	int downloading;                  /* 0 none, -1 the shared files, 1 a Workshop item */
	int queue_count;                  /* downloads waiting behind it */
	int shared_queued;                /* the shared files' download is one of them */
	unsigned long long done, size, speed;   /* bytes of the one going, its total, bytes a second (0: not measured) */
	int packages;                     /* Workshop items in mcc_maps */
	int loading;                      /* the Workshop's page is being asked for */
	unsigned int total;               /* the Workshop's items for the filters */
	int page;                         /* the Workshop's page, from 1 */
	int item_count;                   /* the items of this page */
	int sort, type;                   /* the Workshop's SORT and TYPE (0 first) */
	char search[128];
	/* the focused Workshop item */
	int item_valid;
	char item_title[96], item_description[600], item_tags[160], item_author[100];
	unsigned long long item_size;
	unsigned int item_subscribers;
	int item_installed;               /* 0 not here, 1 here, 2 here but older */
	int item_downloading, item_queued;
};
/* the struct is filled by ae_steam_client.c (the host's ABI) and read by the game's units (the Xbox's ABI, through
lp64_rewrite): both include this header, so a field whose size or place differs between them stops the build, here, on
the 64-bit targets (an array of size -1; 1.0 rc2's stack smash was a long in it). Change the numbers with the struct. */
#if defined(__LP64__) || defined(_WIN64)
typedef char ae_steam_status_size_check[(sizeof(struct ae_steam_status) == 1544 &&
	offsetof(struct ae_steam_status, done) == 376 && offsetof(struct ae_steam_status, total) == 408 &&
	offsetof(struct ae_steam_status, item_size) == 1520 && offsetof(struct ae_steam_status, item_subscribers) == 1528) ? 1 : -1];
#endif

/* the pages the screen shows */
enum
{
	AE_STEAM_PAGE_OUT, AE_STEAM_PAGE_SIGNING_IN, AE_STEAM_PAGE_GUARD_CODE, AE_STEAM_PAGE_GUARD_APP, AE_STEAM_PAGE_IN
};
/* the page the client's state is on; code_instead: the player asked to type a code where the app could approve it */
int ae_steam_page(struct ae_steam_status const *status, int code_instead);
/* the signing-in page's line while nothing is under way with Steam: what is being waited for, or NULL (the page is
about a sign-in with Steam) */
char const *ae_steam_checking_text(struct ae_steam_status const *status);
/* whether ENTER A CODE INSTEAD means anything (a code can be typed at all) */
int ae_steam_code_instead_possible(struct ae_steam_status const *status);

/* what the client's latest message means for the screen */
enum
{
	AE_STEAM_MESSAGE_NONE,           /* "", "Signed in.", "Connecting to Steam...": nothing to say */
	AE_STEAM_MESSAGE_WRONG_PASSWORD, /* Steam refused the name and password: the password field's error */
	AE_STEAM_MESSAGE_WRONG_CODE,     /* the Steam Guard code field's error */
	AE_STEAM_MESSAGE_CANCELLED,      /* "Signing in was cancelled." */
	AE_STEAM_MESSAGE_LOGGED_OUT,     /* "Logged out, and revoked with Steam." / "...here; Steam was out of reach ..." */
	AE_STEAM_MESSAGE_UNREACHABLE     /* anything else, said as Steam not being reached, with its reason */
};
int ae_steam_message_kind(char const *message);
/* the words the screen shows for a kind (message: the client's own, for the kinds that carry it) */
void ae_steam_message_text(int kind, char const *message, char *out, size_t size);

/* SIGN IN's rule: both fields hold something (an account name of spaces alone is nothing); a Steam Guard code: not empty */
int ae_steam_can_sign_in(char const *account, char const *password);
int ae_steam_can_submit_code(char const *code);

/* the settings row STEAM ACCOUNT's summary: NOT SIGNED IN, SIGNING IN, the account's name, or NOT NEEDED: MCC FOUND
(signed out with MCC on this PC) */
/* the login name for the screen: its first 2 characters and "***" (the client has no persona name; the login name is
half the credential, and a screenshot or a stream would show it); "" for none */
void ae_steam_account_masked(char const *account, char *out, size_t size);
void ae_steam_account_summary(struct ae_steam_status const *status, int mcc_found, char *out, size_t size);

/* SHARED MAP FILES */
enum
{
	AE_STEAM_SHARED_CHECKING, AE_STEAM_SHARED_DOWNLOAD, AE_STEAM_SHARED_DOWNLOADING, AE_STEAM_SHARED_READY,
	AE_STEAM_SHARED_NOT_OWNED
};
int ae_steam_shared_state(struct ae_steam_status const *status);
/* the row's value: CHECKING, DOWNLOAD, STARTING / "DOWNLOADING  37 %", READY, NOT OWNED (a queued one is DOWNLOAD's
state: the value says QUEUED then) */
void ae_steam_shared_value(struct ae_steam_status const *status, char *out, size_t size);
/* the going download's percent, rounded, 0 to 100 (0 until the total is known) */
int ae_steam_percent(struct ae_steam_status const *status);

/* each of the three files on the card */
enum { AE_STEAM_FILE_CHECKING, AE_STEAM_FILE_HERE, AE_STEAM_FILE_MISSING, AE_STEAM_FILE_DOWNLOADING, AE_STEAM_FILE_NEXT };
int ae_steam_file_state(struct ae_steam_status const *status, int index);
char const *ae_steam_file_state_text(int state);
char const *ae_steam_file_label(int index);
/* "412 MB of 1.1 GB (18.4 MB/s). A cancels." under the bar */
void ae_steam_progress_line(struct ae_steam_status const *status, char *out, size_t size);

/* the Workshop card's texts */
void ae_steam_size_text(unsigned long long bytes, char *out, size_t size);
void ae_steam_count_text(unsigned long number, char *out, size_t size);
/* CAMPAIGN or MULTIPLAYER by the item's tags, "" when neither */
char const *ae_steam_item_type(char const *tags);
/* the focused item's state: NOT INSTALLED, INSTALLED, UPDATE OUT, QUEUED, DOWNLOADING */
char const *ae_steam_item_state_text(struct ae_steam_status const *status);

/* leaving the screen while downloads run or wait asks first */
int ae_steam_leave_asks(struct ae_steam_status const *status);
void ae_steam_leave_words(struct ae_steam_status const *status, char *out, size_t size);

/* the card's line on how the sign-in is kept, by system; store: the secret store is there at run time */
enum { AE_STEAM_SYSTEM_LINUX, AE_STEAM_SYSTEM_MACOS, AE_STEAM_SYSTEM_WINDOWS };
char const *ae_steam_store_text(int system, int store);

/* whether the first-start page shows: the build has the client, the question was not asked yet, no MCC was found on
this PC, and Steam is not signed in */
int ae_steam_first_start_asks(int client_built, int asked, int mcc_not_found, int signed_in);
/* the first-start page's sentence on the same */
char const *ae_steam_first_store_text(int system, int store);

/* DEV MODE's staged states: `menu steam <name> [<percent>]` */
enum
{
	AE_STEAM_STAGE_SIGNED_OUT, AE_STEAM_STAGE_TYPING, AE_STEAM_STAGE_GUARD_CODE, AE_STEAM_STAGE_GUARD_APP,
	AE_STEAM_STAGE_MISSING, AE_STEAM_STAGE_DOWNLOADING, AE_STEAM_STAGE_READY, AE_STEAM_STAGE_WORKSHOP,
	AE_STEAM_STAGE_WRONG_PASSWORD, AE_STEAM_STAGE_NOT_OWNED, AE_STEAM_STAGE_LOGOUT, AE_STEAM_STAGES
};
enum { AE_STEAM_STAGE_ITEMS = 12 };
/* the stage a word names (any case); 1 and *stage, or 0 */
int ae_steam_stage_find(char const *word, int *stage);
char const *ae_steam_stage_name(int stage);
/* the client's made-up state for a stage (percent: DOWNLOADING's, 0: 37) */
void ae_steam_stage_status(int stage, int percent, struct ae_steam_status *status);
/* the made-up Workshop page's item rows (name, state as the client words them) and the card of the focused one */
void ae_steam_stage_item(int index, char *name, size_t name_size, char *state, size_t state_size);
void ae_steam_stage_item_card(int index, struct ae_steam_status *status);

#endif
