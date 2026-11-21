/*
AE_STEAM_SECRET.C

ae_steam_secret.h's: the key that seals the Steam sign-in, in the system's secret store.
*/

#if defined(HALO_STEAM_CLIENT)

#include "ae_steam_secret.h"
#include "steam_crypto.h"
#include "steam_net.h"
#include "steam_seal.h"
#include "posix.h"
#include "ae_steam_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECRET_SIZE 32
#define SECRET_SERVICE "Arena Evolved Steam sign-in"
#define SECRET_ACCOUNT "session key"

static char secret_folder[1024];

void ae_steam_secret_folder(char const *folder)
{
	snprintf(secret_folder, sizeof(secret_folder), "%s", folder);
}

#if !defined(_WIN32)
static void hex_text(unsigned char const *bytes, char *out)
{
	static char const digits[] = "0123456789abcdef";
	int i;

	for (i = 0; i < SECRET_SIZE; i++)
	{
		out[2 * i] = digits[bytes[i] >> 4];
		out[2 * i + 1] = digits[bytes[i] & 15];
	}
	out[2 * SECRET_SIZE] = 0;
}

static int hex_digit(char ch)
{
	return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

/* the 64 hex digits of text into bytes: 1, or 0 */
static int hex_bytes(char const *text, unsigned char *bytes)
{
	int i;

	if (strlen(text) != 2 * SECRET_SIZE)
		return 0;
	for (i = 0; i < SECRET_SIZE; i++)
	{
		int high = hex_digit(text[2 * i]), low = hex_digit(text[2 * i + 1]);

		if (high < 0 || low < 0)
			return 0;
		bytes[i] = (unsigned char)(high * 16 + low);
	}
	return 1;
}
#endif

/* ---------- the stores: each reads the key (1, 0 when there is none, -1 when the store cannot be used), writes one
(1 on success) and forgets it */

#if defined(_WIN32)

#include <windows.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <sddl.h>

/* a security descriptor of a protected DACL: full control for the user running the game and for SYSTEM, nothing
inherited from the folder above it (a folder under C:\\ would otherwise take "Authenticated Users: Modify"). The
caller frees it with LocalFree; NULL when it cannot be made */
static PSECURITY_DESCRIPTOR owner_only_descriptor(void)
{
	HANDLE token = NULL;
	TOKEN_USER *user = NULL;
	DWORD size = 0;
	LPWSTR sid_text = NULL;
	wchar_t sddl[256];
	PSECURITY_DESCRIPTOR descriptor = NULL;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return NULL;
	GetTokenInformation(token, TokenUser, NULL, 0, &size);
	user = size ? (TOKEN_USER *)LocalAlloc(LPTR, size) : NULL;
	if (user && GetTokenInformation(token, TokenUser, user, size, &size) && ConvertSidToStringSidW(user->User.Sid, &sid_text))
	{
		_snwprintf(sddl, 256, L"D:PAI(A;OICI;FA;;;SY)(A;OICI;FA;;;%ls)", sid_text);
		sddl[255] = 0;
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &descriptor, NULL))
			descriptor = NULL;
	}
	if (sid_text)
		LocalFree(sid_text);
	if (user)
		LocalFree(user);
	CloseHandle(token);
	return descriptor;
}

static int wide_of(char const *text, wchar_t *out, int count)
{
	return MultiByteToWideChar(CP_UTF8, 0, text, -1, out, count) != 0;
}

/* an older sign-in in the shared data folder (steam/ under the data root) moved to this user's folder, when its key
is this Windows user's (DPAPI opens it); another user's is left where it is. Once: when this user's folder has no key
of its own yet */
static void user_folder_migrate(char const *old_folder, wchar_t const *folder)
{
	wchar_t old_key[1100], new_key[1100], old_session[1100], new_session[1100], old_wide[1100];
	FILE *file;
	unsigned char wrapped[1024];
	size_t count;
	DATA_BLOB in, out;
	int mine = 0;

	if (!wide_of(old_folder, old_wide, 1100))
		return;
	_snwprintf(old_key, 1100, L"%ls\\session.key", old_wide);
	_snwprintf(new_key, 1100, L"%ls\\session.key", folder);
	_snwprintf(old_session, 1100, L"%ls\\session.txt", old_wide);
	_snwprintf(new_session, 1100, L"%ls\\session.txt", folder);
	old_key[1099] = new_key[1099] = old_session[1099] = new_session[1099] = 0;
	if (GetFileAttributesW(new_key) != INVALID_FILE_ATTRIBUTES)
		return;
	file = _wfopen(old_key, L"rb");
	if (!file)
		return;
	count = fread(wrapped, 1, sizeof(wrapped), file);
	fclose(file);
	in.cbData = (DWORD)count;
	in.pbData = wrapped;
	if (CryptUnprotectData(&in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		mine = out.cbData == SECRET_SIZE;
		SecureZeroMemory(out.pbData, out.cbData);
		LocalFree(out.pbData);
	}
	/* (copied, not moved: the new files take the new folder's own permissions, and the old ones go once they are over) */
	if (mine && CopyFileW(old_key, new_key, TRUE))
	{
		if (GetFileAttributesW(old_session) == INVALID_FILE_ATTRIBUTES || CopyFileW(old_session, new_session, TRUE))
		{
			DeleteFileW(old_session);
			DeleteFileW(old_key);
			steam_log("steam: the saved sign-in moved to this Windows user's own folder");
		}
		else
			DeleteFileW(new_key);
	}
}

/* this Windows user's own folder for the sign-in (%LOCALAPPDATA%\\Arena Evolved\\steam), made once with an owner-only
DACL, as the folder instead of the shared data root's: 1, with the path in folder; 0 leaves folder as it was */
/* this user's own folder could not be made (no %LOCALAPPDATA%, or it cannot be written): there is no store, and no sign-in
is kept past this run, rather than one in the shared data folder with the permissions it has */
static int user_folder_missing;

int ae_steam_user_folder(char const *root, char *folder, size_t size)
{
	static char made[1024];
	static int tried;
	wchar_t base[MAX_PATH], parent[MAX_PATH + 32], wide[MAX_PATH + 64];
	DWORD length;
	SECURITY_ATTRIBUTES attributes;
	PSECURITY_DESCRIPTOR descriptor;
	int ok;

	(void)root;
	if (tried)
	{
		if (made[0])
			snprintf(folder, size, "%s", made);
		return made[0] != 0;
	}
	tried = 1;
	length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
	if (!length || length >= MAX_PATH)
	{
		user_folder_missing = 1;
		steam_log("steam: no %%LOCALAPPDATA%% for this user: the sign-in is not kept past this run");
		return 0;
	}
	_snwprintf(parent, MAX_PATH + 32, L"%ls\\Arena Evolved", base);
	_snwprintf(wide, MAX_PATH + 64, L"%ls\\steam", parent);
	parent[MAX_PATH + 31] = wide[MAX_PATH + 63] = 0;
	CreateDirectoryW(parent, NULL);
	descriptor = owner_only_descriptor();
	memset(&attributes, 0, sizeof(attributes));
	attributes.nLength = sizeof(attributes);
	attributes.lpSecurityDescriptor = descriptor;
	/* (the permissions are set here, once, as the folder is made; a folder that is there already is left as it is) */
	ok = CreateDirectoryW(wide, descriptor ? &attributes : NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
	if (descriptor)
		LocalFree(descriptor);
	if (!descriptor)
		steam_log("steam: the sign-in's folder could not be made owner-only");
	if (!ok || !WideCharToMultiByte(CP_UTF8, 0, wide, -1, made, (int)sizeof(made), NULL, NULL))
	{
		made[0] = 0;
		user_folder_missing = 1;
		steam_log("steam: this user's folder for the sign-in could not be made: it is not kept past this run");
		return 0;
	}
	user_folder_migrate(folder, wide);
	snprintf(folder, size, "%s", made);
	return 1;
}

static int path_of_key(char *path, size_t size)
{
	if (!secret_folder[0] || user_folder_missing)
		return 0;
	snprintf(path, size, "%s/session.key", secret_folder);
	return 1;
}

static int store_read(unsigned char *key)
{
	char path[1100];
	FILE *file;
	unsigned char wrapped[1024];
	size_t count;
	DATA_BLOB in, out;
	int ok = 0;

	if (!path_of_key(path, sizeof(path)))
		return -1;
	file = fopen(path, "rb");
	if (!file)
		return 0;
	count = fread(wrapped, 1, sizeof(wrapped), file);
	fclose(file);
	in.cbData = (DWORD)count;
	in.pbData = wrapped;
	if (CryptUnprotectData(&in, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
	{
		if (out.cbData == SECRET_SIZE)
		{
			memcpy(key, out.pbData, SECRET_SIZE);
			ok = 1;
		}
		SecureZeroMemory(out.pbData, out.cbData);
		LocalFree(out.pbData);
	}
	/* (a file this user cannot open: made anew, which opens nothing sealed before) */
	return ok ? 1 : 0;
}

static int store_write(unsigned char const *key)
{
	char path[1100];
	FILE *file;
	DATA_BLOB in, out;
	int ok = 0;

	if (!path_of_key(path, sizeof(path)))
		return 0;
	in.cbData = SECRET_SIZE;
	in.pbData = (BYTE *)key;
	if (!CryptProtectData(&in, L"Arena Evolved Steam sign-in", NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return 0;
	file = steam_seal_create(path);
	if (file)
	{
		ok = fwrite(out.pbData, 1, out.cbData, file) == out.cbData;
		ok = fclose(file) == 0 && ok;
	}
	LocalFree(out.pbData);
	return ok;
}

static void store_forget(void)
{
	char path[1100];

	if (path_of_key(path, sizeof(path)))
		remove(path);
}

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

static CFMutableDictionaryRef keychain_query(void)
{
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	CFStringRef service = CFStringCreateWithCString(NULL, SECRET_SERVICE, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(NULL, SECRET_ACCOUNT, kCFStringEncodingUTF8);

	CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
	CFDictionarySetValue(query, kSecAttrService, service);
	CFDictionarySetValue(query, kSecAttrAccount, account);
	CFRelease(service);
	CFRelease(account);
	return query;
}

static int store_read(unsigned char *key)
{
	CFMutableDictionaryRef query = keychain_query();
	CFTypeRef found = NULL;
	OSStatus status;
	int result = -1;

	CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
	status = SecItemCopyMatching(query, &found);
	if (status == errSecSuccess && found)
	{
		CFDataRef data = (CFDataRef)found;
		char text[2 * SECRET_SIZE + 2];
		CFIndex length = CFDataGetLength(data);

		result = 0;
		if (length == 2 * SECRET_SIZE)
		{
			memcpy(text, CFDataGetBytePtr(data), (size_t)length);
			text[length] = 0;
			result = hex_bytes(text, key) ? 1 : 0;
			memset(text, 0, sizeof(text));
		}
		CFRelease(found);
	}
	else if (status == errSecItemNotFound)
		result = 0;
	CFRelease(query);
	return result;
}

static int store_write(unsigned char const *key)
{
	CFMutableDictionaryRef query = keychain_query();
	char text[2 * SECRET_SIZE + 1];
	CFDataRef data;
	OSStatus status;

	/* (an older key, if one is there, replaced) */
	SecItemDelete(query);
	hex_text(key, text);
	data = CFDataCreate(NULL, (UInt8 const *)text, 2 * SECRET_SIZE);
	memset(text, 0, sizeof(text));
	CFDictionarySetValue(query, kSecValueData, data);
	status = SecItemAdd(query, NULL);
	CFRelease(data);
	CFRelease(query);
	return status == errSecSuccess;
}

static void store_forget(void)
{
	CFMutableDictionaryRef query = keychain_query();

	SecItemDelete(query);
	CFRelease(query);
}

#else

#include <dlfcn.h>

/* libsecret's schema (secret-schema.h), as that header lays it out: the library is not linked, only asked for by
name when the store is first used */
struct secret_attribute
{
	char const *name;
	int type;
};

struct secret_schema
{
	char const *name;
	int flags;
	struct secret_attribute attributes[32];
	int reserved;
	void *reserved1, *reserved2, *reserved3, *reserved4, *reserved5, *reserved6, *reserved7;
};

static struct secret_schema const schema = { "org.arenaevolved.SteamSession", 0, { { "purpose", 0 }, { NULL, 0 } }, 0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static char *(*secret_lookup)(struct secret_schema const *, void *, void **, ...);
static int (*secret_store)(struct secret_schema const *, char const *, char const *, char const *, void *, void **,
	...);
static int (*secret_clear)(struct secret_schema const *, void *, void **, ...);
static void (*secret_free)(char *);
/* GLib's g_error_free, for the error a lookup reports (from the library libsecret itself loads; NULL: the error is let be) */
static void (*error_free)(void *);

static int secret_service_open(void)
{
	static int tried;
	void *library;

	if (tried)
		return secret_lookup != NULL;
	tried = 1;
	library = dlopen("libsecret-1.so.0", RTLD_LAZY | RTLD_LOCAL);
	if (!library)
		return 0;
	secret_lookup = (char *(*)(struct secret_schema const *, void *, void **, ...))dlsym(library,
		"secret_password_lookup_sync");
	secret_store = (int (*)(struct secret_schema const *, char const *, char const *, char const *, void *, void **,
		...))dlsym(library, "secret_password_store_sync");
	secret_clear = (int (*)(struct secret_schema const *, void *, void **, ...))dlsym(library,
		"secret_password_clear_sync");
	secret_free = (void (*)(char *))dlsym(library, "secret_password_free");
	{
		void *glib = dlopen("libglib-2.0.so.0", RTLD_LAZY | RTLD_LOCAL);

		error_free = glib ? (void (*)(void *))dlsym(glib, "g_error_free") : NULL;
	}
	if (!secret_lookup || !secret_store || !secret_clear || !secret_free)
		secret_lookup = NULL;
	return secret_lookup != NULL;
}

static int store_read(unsigned char *key)
{
	char *text;
	int result = 0;

	void *error = NULL;

	if (!secret_service_open())
		return -1;
	text = secret_lookup(&schema, NULL, &error, "purpose", "session key", (char *)NULL);
	if (error)
	{
		/* (a locked keyring that was not opened, no provider, a bus error: that is not "no key", and a new key must
		never replace the old one on it; no store this run) */
		if (error_free)
			error_free(error);
		if (text)
			secret_free(text);
		return -1;
	}
	if (text)
	{
		result = hex_bytes(text, key) ? 1 : 0;
		secret_free(text);
	}
	return result;
}

static int store_write(unsigned char const *key)
{
	char text[2 * SECRET_SIZE + 1];
	int ok;

	if (!secret_service_open())
		return 0;
	hex_text(key, text);
	ok = secret_store(&schema, "default", SECRET_SERVICE, text, NULL, NULL, "purpose", "session key", (char *)NULL);
	memset(text, 0, sizeof(text));
	return ok;
}

static void store_forget(void)
{
	if (secret_service_open())
		secret_clear(&schema, NULL, NULL, "purpose", "session key", (char *)NULL);
}

#endif

/* ---------- the key, kept for the run once it is had (the sign-in's threads ask for it each time) */

static unsigned char kept[SECRET_SIZE];
static int have_kept;

#if defined(_WIN32)
#define KEPT_LOCK()
#define KEPT_UNLOCK()
#else
#include <pthread.h>
static pthread_mutex_t kept_lock = PTHREAD_MUTEX_INITIALIZER;
#define KEPT_LOCK() pthread_mutex_lock(&kept_lock)
#define KEPT_UNLOCK() pthread_mutex_unlock(&kept_lock)
#endif

/* 1 while the store is being asked (it may be showing a prompt) */
static volatile int store_asking;
/* a lookup of the key failed, or found none under a sign-in that was sealed with one: no new key is made this run (a
new one would leave that sign-in unreadable for good) */
static int no_new_key;

static int secret_get(struct steam_sha1 *sha)
{
	unsigned char key[SECRET_SIZE];
	int state;

	if (have_kept)
	{
		steam_sha1_add(sha, kept, SECRET_SIZE);
		return 1;
	}
	if (no_new_key)
		return 0;
	store_asking = 1;
	state = store_read(key);
	store_asking = 0;
	if (state < 0)
	{
		no_new_key = 1;
		steam_log("steam: no secret store here: the sign-in is not kept past this run");
		return 0;
	}
	if (state == 0)
	{
		char path[1100];

		/* (a keyring that was locked and not opened can answer "none" too: with a sealed sign-in on disk a new key is
		never made over the old one) */
		snprintf(path, sizeof(path), "%s/session.txt", secret_folder);
		if (secret_folder[0] && steam_session_file_sealed(path))
		{
			no_new_key = 1;
			steam_log("steam: the secret store has no key for the saved sign-in: it is not replaced this run");
			return 0;
		}
		posix_random_bytes(key, SECRET_SIZE);
		store_asking = 1;
		state = store_write(key);
		store_asking = 0;
		if (!state)
		{
			steam_wipe(key, sizeof(key));
			steam_log("steam: the secret store would not keep a key: the sign-in is not kept past this run");
			return 0;
		}
	}
	memcpy(kept, key, SECRET_SIZE);
	have_kept = 1;
	steam_sha1_add(sha, key, SECRET_SIZE);
	steam_wipe(key, sizeof(key));
	return 1;
}

/* what the last ask found (STEAM ACCOUNT's card says it): -1 never asked, 0 no store, 1 a store */
static volatile int store_result = -1;

int ae_steam_secret(struct steam_sha1 *sha)
{
	int have;

	KEPT_LOCK();
	have = secret_get(sha);
	KEPT_UNLOCK();
	store_result = have ? 1 : 0;
	return have;
}

int ae_steam_secret_store(void)
{
	return store_result;
}

int ae_steam_secret_waiting(void)
{
	return store_asking;
}

void ae_steam_secret_forget(void)
{
	KEPT_LOCK();
	steam_wipe(kept, sizeof(kept));
	have_kept = 0;
	no_new_key = 0;
	store_forget();
	KEPT_UNLOCK();
}

#endif
