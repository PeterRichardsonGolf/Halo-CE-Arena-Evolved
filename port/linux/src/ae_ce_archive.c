/*
AE_CE_ARCHIVE.C

ae_ce_archive.h's library: the community map archive's signed list and its maps, fetched, checked and installed.

The list's source and keys, its format, its signature's check and the download's shape are from the Android host's
fractum_archive.c (thelinkin3000), as Halo Content Evolved's port/linux/src/map_archive.c (Watanoge) brought them to
the desktop; both AGPL-3.0-or-later. What this port changed: the pinned keys and the signature are checked for the
kept copy as for the network's, a list older than the one in use is refused, a list or map is taken only with a
Content-Length that is its size, no answer is followed to another address, a transfer has a stall time and a
deadline and can be cancelled at any point, the file name comes from the signed list and is checked against the
game's rules, the free disk is checked first, the map's header is checked, and a map is never put over another one
unless the caller says so. The archive's own catalog server is not used.

THE TRANSPORT
- The list: https://raw.githubusercontent.com/FractumSeraph/OpenCE/map-index/map_torrents.txt and its ".sig", over
  HTTPS with the certificate and host name checked (update_https_request, the updater's). A GET with the updater's
  User-Agent and no other header of ours. What is trusted of the answer: its body, if the signature by a pinned key
  verifies over it. Nothing else.
- The maps: http://halomaps.fractumseraph.net/maps/<name> over plain HTTP, port 80, a GET with Host, the updater's
  User-Agent, an Accept header for any type and "Connection: close". The host is not known to offer HTTPS, so the BLAKE2b of the
  signed list is the only trust a map has: the bytes are written to a temporary file while they are hashed, and the
  file is given its name only when size and hash are the list's. The answer must be a 200 with a Content-Length
  that is the listed size exactly; a redirect, a chunked or encoded body, any other status is refused, and no
  address an answer names is ever contacted.
- A test's server (ae_ce_archive_set_test_server) is on this machine only.
*/

#include "ae_ce_archive.h"

#include "map_archive.h"
#include "monocypher.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HALO_ANDROID) || defined(HALO_WEB)

/* (no HTTPS client of the platform layer, or no sockets: the feature is not there) */
int ae_ce_archive_available(void) { return 0; }
int ae_ce_archive_enabled(void) { return 0; }
int ae_ce_archive_consented(void) { return 0; }
void ae_ce_archive_set_consent(int yes) { (void)yes; }
int ae_ce_archive_start(char *why, int why_size) { if (why && why_size > 0) snprintf(why, (size_t)why_size, "not in this build"); return 0; }
void ae_ce_archive_get_status(struct ae_ce_archive_status *status) { memset(status, 0, sizeof(*status)); }
int ae_ce_archive_query(struct ae_ce_archive_query const *query) { (void)query; return 0; }
int ae_ce_archive_row(int place, struct ae_ce_archive_row *row) { (void)place; (void)row; return 0; }
int ae_ce_archive_download(char const *file, int overwrite, char *why, int why_size) { (void)file; (void)overwrite; if (why && why_size > 0) snprintf(why, (size_t)why_size, "not in this build"); return 0; }
void ae_ce_archive_cancel(void) {}
void ae_ce_archive_download_status(struct ae_ce_archive_download *status) { memset(status, 0, sizeof(*status)); }
void ae_ce_archive_download_reset(void) {}
void ae_ce_archive_set_installed_hook(ae_ce_archive_installed_proc hook) { (void)hook; }
void ae_ce_archive_set_map_loaded_proc(ae_ce_archive_map_loaded_proc proc) { (void)proc; }
void ae_ce_archive_set_marks_file(char const *path) { (void)path; }
void ae_ce_archive_hosts(char *list_host, int list_size, char *file_host, int file_size) { if (list_host && list_size > 0) list_host[0] = 0; if (file_host && file_size > 0) file_host[0] = 0; }
void ae_ce_archive_poll(void) {}
void ae_ce_archive_rescan(void) {}
int ae_ce_archive_set_test_server(char const *base, char const *key_hex, char *why, int why_size) { (void)base; (void)key_hex; (void)why; (void)why_size; return 0; }
int ae_ce_archive_use_dev_environment(char *text, int text_size) { (void)text; (void)text_size; return 0; }
void ae_ce_archive_limits_default(struct ae_ce_archive_limits *limits) { memset(limits, 0, sizeof(*limits)); }
void ae_ce_archive_set_limits(struct ae_ce_archive_limits const *limits) { (void)limits; }
void ae_ce_archive_set_folders(char const *custom_maps, char const *cache) { (void)custom_maps; (void)cache; }
void ae_ce_archive_set_disk_free(ae_ce_archive_disk_free_proc proc) { (void)proc; }

#else

#include "ae_marks_file.h"
#include "posix.h"
#include "update.h"

#include <pthread.h>
#include <time.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <sys/statvfs.h>
#endif

void platform_log(char const *format, ...) __attribute__((format(printf, 1, 2)));
char const *platform_data_root(void);
void platform_translate_path(char const *xbox_path, char *host_path, unsigned long host_path_size);
int config_boolean(char const *name);
char const *config_string(char const *name);
int config_write_boolean(char const *name, int value);

/* ---------- where it asks, and the keys it trusts */

#define LIST_URL "https://raw.githubusercontent.com/FractumSeraph/OpenCE/map-index/map_torrents.txt"
#define MAPS_BASE "http://halomaps.fractumseraph.net/maps/"
#define LIST_FILE "map_torrents.txt"
#define SIGNATURE_SUFFIX ".sig"
#define CACHE_LIST "map_list.txt"
#define CACHE_SIGNATURE "map_list.txt.sig"
/* a name no more specific than the program: the plain-HTTP request is in the clear, and tells the path nothing of the player's
build, system or machine */
#define USER_AGENT "HaloMapClient/1"
/* the downloaded maps' list the game reads (map_families.c) */
#define MARKS_PATH "d:\\maps_ce\\downloaded_maps.txt"
#define RECORD_FILE "highest_serial.txt"
#define ENABLED_KEY "game.ce_archive"
#define CONSENT_KEY "game.ce_archive_consented"

/* the keys the list may be signed by: the archive maintainer's release key and its recovery key (his fork's
port/linux/src/map_index_key.h) */
static const uint8_t pinned_keys[][32] =
{
	{
		0xc1, 0x20, 0x6c, 0xa2, 0xbe, 0xca, 0x78, 0xe1, 0x1c, 0xb1, 0x05, 0x30, 0xdd, 0x6e, 0x73, 0x3c,
		0x45, 0x61, 0xa6, 0xf1, 0x76, 0x2d, 0x49, 0x5d, 0xb1, 0xa6, 0x62, 0x53, 0xf3, 0x5a, 0x24, 0x3e,
	},
	{
		0xbb, 0x14, 0x21, 0xe0, 0x14, 0x7f, 0x12, 0x10, 0xa8, 0x44, 0x65, 0x32, 0xf6, 0x70, 0x96, 0xfc,
		0xe8, 0x87, 0xc7, 0xb5, 0x97, 0x87, 0x5c, 0x55, 0x27, 0x12, 0x98, 0xe4, 0x71, 0x88, 0xf8, 0xd3,
	},
};

enum
{
	PATH_SIZE = 1024,
	BLOCK = 64 * 1024,
	HEAD_MAXIMUM = 8192,
	CONNECT_MILLISECONDS = 10000,
	SLICE_MILLISECONDS = 200,
	PENDING_MAXIMUM = 16,
	RATE_WINDOW_MILLISECONDS = 3000
};

static struct
{
	pthread_mutex_t lock;

	/* the sources: the archive's, or a test's */
	int test_server;
	char list_url[512];
	char signature_url[520];
	char maps_base[512];
	uint8_t test_key[32];
	struct ae_ce_archive_limits limits;
	char custom_maps[PATH_SIZE];
	char cache[PATH_SIZE];
	ae_ce_archive_disk_free_proc disk_free;
	ae_ce_archive_installed_proc hook;
	ae_ce_archive_map_loaded_proc map_loaded;
	char marks[PATH_SIZE];

	/* the list */
	int state;
	int from_network;
	int list_busy;
	char error[160];
	unsigned long long serial;
	struct ae_ce_archive_entry *entries;
	int count;
	int skipped;
	int *view;
	int view_count;
	struct ae_ce_archive_query query;
	/* a number that changes with every list taken: a rescan's results belong to the list they were made for */
	unsigned generation;
	int rescan_busy;
	int rescan_again;

	/* the download */
	int dl_busy;
	/* (set by the menus' thread, read by the worker: only through cancel_set / cancel_asked) */
	int cancel;
	struct ae_ce_archive_download dl;
	struct ae_ce_archive_entry dl_entry;
	int dl_overwrite;
	unsigned long long dl_started;
	unsigned long long dl_window_at;
	unsigned long long dl_window_done;

	char pending[PENDING_MAXIMUM][AE_CE_ARCHIVE_FILE_SIZE];
	int pending_count;
} S = { .lock = PTHREAD_MUTEX_INITIALIZER };

static unsigned long long now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned long long)t.tv_sec * 1000ULL + (unsigned long long)t.tv_nsec / 1000000ULL;
}

static void say(char *text, int size, char const *message)
{
	if (text && size > 0)
		snprintf(text, (size_t)size, "%s", message);
}

/* the defaults are set before the first use */
static void init_once(void)
{
	pthread_mutex_lock(&S.lock);
	if (!S.list_url[0])
	{
		ae_ce_archive_limits_default(&S.limits);
		snprintf(S.list_url, sizeof(S.list_url), "%s", LIST_URL);
		snprintf(S.signature_url, sizeof(S.signature_url), "%s%s", LIST_URL, SIGNATURE_SUFFIX);
		snprintf(S.maps_base, sizeof(S.maps_base), "%s", MAPS_BASE);
		S.query.show = AE_CE_ARCHIVE_SHOW_ALL;
	}
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_limits_default(struct ae_ce_archive_limits *limits)
{
	memset(limits, 0, sizeof(*limits));
	limits->stall_ms = 20000;
	limits->file_deadline_ms = 60U * 60 * 1000;
	limits->list_deadline_ms = 2U * 60 * 1000;
}

void ae_ce_archive_set_limits(struct ae_ce_archive_limits const *limits)
{
	init_once();
	pthread_mutex_lock(&S.lock);
	S.limits = *limits;
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_set_folders(char const *custom_maps, char const *cache)
{
	init_once();
	pthread_mutex_lock(&S.lock);
	snprintf(S.custom_maps, sizeof(S.custom_maps), "%s", custom_maps ? custom_maps : "");
	snprintf(S.cache, sizeof(S.cache), "%s", cache ? cache : "");
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_set_disk_free(ae_ce_archive_disk_free_proc proc)
{
	pthread_mutex_lock(&S.lock);
	S.disk_free = proc;
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_set_map_loaded_proc(ae_ce_archive_map_loaded_proc proc)
{
	pthread_mutex_lock(&S.lock);
	S.map_loaded = proc;
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_set_marks_file(char const *path)
{
	pthread_mutex_lock(&S.lock);
	snprintf(S.marks, sizeof(S.marks), "%s", path ? path : "");
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_hosts(char *list_host, int list_size, char *file_host, int file_size)
{
	struct ae_ce_archive_url list, files;
	char list_url[512], files_url[512];

	init_once();
	pthread_mutex_lock(&S.lock);
	snprintf(list_url, sizeof(list_url), "%s", S.list_url);
	snprintf(files_url, sizeof(files_url), "%s", S.maps_base);
	pthread_mutex_unlock(&S.lock);
	if (list_host && list_size > 0)
		snprintf(list_host, (size_t)list_size, "%s", ae_ce_archive_url_parse(list_url, &list) ? list.host : "");
	if (file_host && file_size > 0)
		snprintf(file_host, (size_t)file_size, "%s", ae_ce_archive_url_parse(files_url, &files) ? files.host : "");
}

void ae_ce_archive_set_installed_hook(ae_ce_archive_installed_proc hook)
{
	pthread_mutex_lock(&S.lock);
	S.hook = hook;
	pthread_mutex_unlock(&S.lock);
}

int ae_ce_archive_available(void)
{
	return 1;
}

int ae_ce_archive_enabled(void)
{
	return config_boolean(ENABLED_KEY);
}

int ae_ce_archive_consented(void)
{
	return config_boolean(CONSENT_KEY);
}

void ae_ce_archive_set_consent(int yes)
{
	config_write_boolean(CONSENT_KEY, yes ? 1 : 0);
}

/* whether the feature may touch the network, else why */
static int permitted(char *why, int why_size)
{
	if (!ae_ce_archive_enabled())
	{
		say(why, why_size, "browsing maps online is off (game.ce_archive)");
		return 0;
	}
	if (!ae_ce_archive_consented())
	{
		say(why, why_size, "the first-use consent is not given (game.ce_archive_consented)");
		return 0;
	}
	return 1;
}

/* ---------- folders and files */

static void path_join(char *path, size_t size, char const *folder, char const *name)
{
	size_t length = strlen(folder);

	if (length && (folder[length - 1] == '/' || folder[length - 1] == '\\'))
		snprintf(path, size, "%s%s", folder, name);
	else
		snprintf(path, size, "%s/%s", folder, name);
}

static void custom_maps_folder(char *folder, size_t size)
{
	pthread_mutex_lock(&S.lock);
	snprintf(folder, size, "%s", S.custom_maps);
	pthread_mutex_unlock(&S.lock);
	if (!folder[0])
	{
		char const *setting = config_string("paths.custom_maps");

		if (setting && setting[0])
			snprintf(folder, size, "%s", setting);
		else
			platform_translate_path("d:\\custom_maps", folder, (unsigned long)size);
	}
}

static void cache_folder(char *folder, size_t size)
{
	int test;

	pthread_mutex_lock(&S.lock);
	snprintf(folder, size, "%s", S.cache);
	test = S.test_server;
	pthread_mutex_unlock(&S.lock);
	/* (a test archive's list is never kept where the real one is) */
	if (!folder[0])
		path_join(folder, size, platform_data_root(), test ? "ce_archive_test" : "ce_archive");
}

/* a new file for writing at path, never through a link or over a file that is there: any file of that name is unlinked
first, then the file is made with O_EXCL (CREATE_NEW's fopen mode on Windows), and a link planted between is refused */
static FILE *file_create(char const *path)
{
	remove(path);
#if defined(_WIN32)
	return fopen(path, "wbx");
#else
	{
		int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
		FILE *file;

		if (descriptor < 0)
			return NULL;
		file = fdopen(descriptor, "wb");
		if (!file)
			close(descriptor);
		return file;
	}
#endif
}

/* part given the name path, replacing the file there in one step: 1. The old file is never gone before the new one has its
name (rename over on POSIX; MoveFileEx with replace and write-through on Windows) */
static int replace_file(char const *part, char const *path)
{
#if defined(_WIN32)
	return MoveFileExA(part, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(part, path) == 0;
#endif
}

/* the folder of the file at path flushed to disk, so a name just given is there after a power loss */
static void sync_directory(char const *path)
{
#if !defined(_WIN32)
	char folder[PATH_SIZE];
	char *slash;
	int descriptor;

	snprintf(folder, sizeof(folder), "%s", path);
	slash = strrchr(folder, '/');
	if (!slash)
		return;
	*(slash == folder ? slash + 1 : slash) = 0;
	descriptor = open(folder, O_RDONLY);
	if (descriptor >= 0)
	{
		fsync(descriptor);
		close(descriptor);
	}
#else
	(void)path;
#endif
}

/* part given the name path when no file has it: 1. Never replaces one (a link and an unlink where the file system has
links, else a check and a rename) */
static int rename_no_replace(char const *part, char const *path)
{
#if defined(_WIN32)
	return rename(part, path) == 0;
#else
	if (link(part, path) == 0)
	{
		unlink(part);
		return 1;
	}
	if (errno == EEXIST)
		return 0;
	/* (no links here: FAT and the like) */
	{
		FILE *there = fopen(path, "rb");

		if (there)
		{
			fclose(there);
			return 0;
		}
	}
	return rename(part, path) == 0;
#endif
}

static int file_exists(char const *path)
{
	FILE *file = fopen(path, "rb");

	if (!file)
		return 0;
	fclose(file);
	return 1;
}

static int file_read(char const *path, unsigned long maximum, unsigned char **data, size_t *size)
{
	FILE *file = fopen(path, "rb");
	long length;

	*data = NULL;
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || (unsigned long)length > maximum ||
		fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return 0;
	}
	*data = (unsigned char *)malloc((size_t)length + 1);
	if (!*data || fread(*data, 1, (size_t)length, file) != (size_t)length)
	{
		free(*data);
		*data = NULL;
		fclose(file);
		return 0;
	}
	(*data)[length] = 0;
	*size = (size_t)length;
	fclose(file);
	return 1;
}

/* data into path through a temporary file beside it, so a kept list is never half written */
static int file_write(char const *path, unsigned char const *data, size_t size)
{
	char temporary[PATH_SIZE + 8];
	FILE *file;
	int good;

	snprintf(temporary, sizeof(temporary), "%s.part", path);
	file = file_create(temporary);
	if (!file)
		return 0;
	good = fwrite(data, 1, size, file) == size;
	good = fflush(file) == 0 && good;
#if !defined(_WIN32)
	good = fsync(fileno(file)) == 0 && good;
#endif
	if (fclose(file) != 0)
		good = 0;
	if (good)
	{
#if defined(_WIN32)
		/* (rename does not replace a file there: one call replaces it and is on disk when it returns) */
		good = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		good = rename(temporary, path) == 0;
#endif
	}
	if (!good)
		remove(temporary);
	return good;
}

static unsigned long long disk_free_bytes(char const *folder, int *known)
{
	ae_ce_archive_disk_free_proc proc;
	struct ae_ce_archive_limits limits;

	pthread_mutex_lock(&S.lock);
	proc = S.disk_free;
	limits = S.limits;
	pthread_mutex_unlock(&S.lock);
	*known = 1;
	if (limits.fake_free_set)
		return limits.fake_free_bytes;
	if (proc)
		return proc(folder);
#if defined(_WIN32)
	{
		ULARGE_INTEGER available;

		if (GetDiskFreeSpaceExA(folder, &available, NULL, NULL))
			return (unsigned long long)available.QuadPart;
	}
#elif defined(__linux__) || defined(__APPLE__)
	{
		struct statvfs info;

		if (statvfs(folder, &info) == 0)
			return (unsigned long long)info.f_bavail * (unsigned long long)info.f_frsize;
	}
#endif
	/* (not known: the download is refused) */
	*known = 0;
	return 0;
}

/* ---------- the transport */

/* Winsock's sockaddr_in, which every platform's posix_net.c reads (posix.h) */
struct address
{
	uint16_t family;
	uint16_t port;
	uint32_t ip;
	unsigned char zero[8];
};

#define WINSOCK_SOL_SOCKET 0xffff
#define WINSOCK_SO_ERROR 0x1007

static int socket_wait(int socket, unsigned long milliseconds, int writing)
{
	int list[1];
	int count = 1;
	int none = 0;

	list[0] = socket;
	if (posix_socket_select(writing ? NULL : list, writing ? &none : &count, writing ? list : NULL,
		writing ? &count : &none, NULL, &none, (posix_long)(milliseconds / 1000),
		(posix_long)(milliseconds % 1000) * 1000, 0) < 0)
		return -1;
	return count > 0;
}

static int cancel_asked(int const volatile *flag)
{
	return __atomic_load_n((int const *)flag, __ATOMIC_ACQUIRE);
}

struct http_job
{
	struct ae_ce_archive_url url;
	/* 1: the body must be exactly size bytes; 0: any size up to size */
	int exact;
	unsigned long long size;
	int (*write)(void *context, unsigned char const *data, size_t length);
	void *context;
	int volatile *cancel;
	unsigned int stall_ms;
	unsigned int deadline_ms;
	char error[160];
};

static int job_fail(struct http_job *job, char const *message)
{
	snprintf(job->error, sizeof(job->error), "%s", message);
	return 0;
}

/* one GET over plain HTTP to job->url, its body to job->write: 1, else 0 with why in job->error. No redirect
is followed, and the body is read only to the Content-Length the answer states */
static int http_get(struct http_job *job)
{
	char request[1024];
	char head[HEAD_MAXIMUM + 1];
	unsigned char *block;
	struct address address;
	struct ae_ce_archive_head parsed;
	unsigned long long started = now_ms(), last_data, received = 0;
	size_t have = 0, head_end = 0, used;
	int socket = -1, good = 0, blocking;
	char const *marker;
	uint32_t ip;

	job->error[0] = 0;
	block = (unsigned char *)malloc(BLOCK);
	if (!block)
		return job_fail(job, "out of memory");
	if (ae_ce_archive_host_is_loopback(job->url.host))
		ip = 0x0100007fU;
	else
		ip = (uint32_t)posix_resolve_ipv4(job->url.host);
	if (!ip)
	{
		free(block);
		return job_fail(job, "the archive's address could not be found");
	}
	/* (a resolver that answers with this machine's or a private address is not the archive: only a test's own host may be) */
	if (!ae_ce_archive_host_is_loopback(job->url.host))
	{
		unsigned char octets[4];

		memcpy(octets, &ip, 4);
		if (!ae_ce_archive_address_public(octets))
		{
			free(block);
			return job_fail(job, "the archive's name answered with a private address");
		}
	}
	socket = posix_socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 0);
	if (socket < 0)
	{
		free(block);
		return job_fail(job, "no network connection could be made");
	}
	memset(&address, 0, sizeof(address));
	address.family = 2;
	address.port = (uint16_t)(((job->url.port & 255) << 8) | ((job->url.port >> 8) & 255));
	address.ip = ip;
	posix_socket_set_nonblocking(socket, 1);
	if (posix_socket_connect(socket, &address, sizeof(address)) != 0)
	{
		unsigned long waited = 0;
		int ready = 0, error = 0, length = (int)sizeof(error);

		while (waited < CONNECT_MILLISECONDS && !ready)
		{
			if (cancel_asked(job->cancel))
			{
				job_fail(job, "cancelled");
				goto done;
			}
			ready = socket_wait(socket, SLICE_MILLISECONDS, 1);
			if (ready < 0)
				break;
			waited += SLICE_MILLISECONDS;
		}
		if (ready <= 0 || posix_socket_getsockopt(socket, WINSOCK_SOL_SOCKET, WINSOCK_SO_ERROR, &error, &length) != 0 ||
			error)
		{
			job_fail(job, "the archive could not be reached");
			goto done;
		}
	}
	posix_socket_set_nonblocking(socket, 0);
	{
		char host_header[160];
		int made;

		if (job->url.port != 80)
			made = snprintf(host_header, sizeof(host_header), "%s:%d", job->url.host, job->url.port);
		else
			made = snprintf(host_header, sizeof(host_header), "%s", job->url.host);
		if (made < 0 || (size_t)made >= sizeof(host_header))
		{
			job_fail(job, "the request is too long");
			goto done;
		}
		made = snprintf(request, sizeof(request),
			"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " USER_AGENT "\r\nAccept: */*\r\nConnection: close\r\n\r\n",
			job->url.path, host_header);
		if (made < 0 || (size_t)made >= sizeof(request))
		{
			job_fail(job, "the request is too long");
			goto done;
		}
		used = (size_t)made;
	}
	last_data = now_ms();
	{
		size_t sent = 0;

		while (sent < used)
		{
			int count;

			if (cancel_asked(job->cancel))
			{
				job_fail(job, "cancelled");
				goto done;
			}
			if (now_ms() - last_data > job->stall_ms)
			{
				job_fail(job, "the archive took no request");
				goto done;
			}
			if (socket_wait(socket, SLICE_MILLISECONDS, 1) <= 0)
				continue;
			count = posix_socket_send(socket, request + sent, (int)(used - sent), 0);
			if (count <= 0)
			{
				job_fail(job, "the archive took no request");
				goto done;
			}
			sent += (size_t)count;
		}
	}
	/* the head */
	marker = NULL;
	while (!marker)
	{
		int count;

		if (cancel_asked(job->cancel))
		{
			job_fail(job, "cancelled");
			goto done;
		}
		if (now_ms() - started > job->deadline_ms)
		{
			job_fail(job, "the archive took too long");
			goto done;
		}
		if (now_ms() - last_data > job->stall_ms)
		{
			job_fail(job, "the archive stopped answering");
			goto done;
		}
		if (have >= HEAD_MAXIMUM)
		{
			job_fail(job, "the archive's answer is not HTTP");
			goto done;
		}
		blocking = socket_wait(socket, SLICE_MILLISECONDS, 0);
		if (blocking < 0)
		{
			job_fail(job, "the connection failed");
			goto done;
		}
		if (!blocking)
			continue;
		count = posix_socket_recv(socket, head + have, (int)(HEAD_MAXIMUM - have), 0);
		if (count <= 0)
		{
			job_fail(job, "the archive closed the connection");
			goto done;
		}
		last_data = now_ms();
		have += (size_t)count;
		head[have] = 0;
		marker = strstr(head, "\r\n\r\n");
	}
	head_end = (size_t)(marker - head);
	if (!ae_ce_archive_head_parse(head, head_end, &parsed))
	{
		job_fail(job, "the archive's answer is not HTTP");
		goto done;
	}
	if (job->exact)
	{
		if (!ae_ce_archive_head_allows(&parsed, job->size, job->error, (int)sizeof(job->error)))
			goto done;
	}
	else
	{
		if (parsed.status != 200 || parsed.chunked || parsed.encoded || parsed.content_length < 0)
		{
			snprintf(job->error, sizeof(job->error), "the archive answered %d", parsed.status);
			goto done;
		}
		if ((unsigned long long)parsed.content_length > job->size)
		{
			job_fail(job, "the answer is larger than allowed");
			goto done;
		}
	}
	/* the body: what came with the head, then the rest, to the stated length and no further */
	{
		unsigned long long length = (unsigned long long)parsed.content_length;
		size_t extra = have - (head_end + 4);

		if (extra > length)
			extra = (size_t)length;
		if (extra && !job->write(job->context, (unsigned char const *)head + head_end + 4, extra))
		{
			job_fail(job, "the file could not be written");
			goto done;
		}
		received = extra;
		last_data = now_ms();
		while (received < length)
		{
			unsigned long long want = length - received;
			int count;

			if (cancel_asked(job->cancel))
			{
				job_fail(job, "cancelled");
				goto done;
			}
			if (now_ms() - started > job->deadline_ms)
			{
				job_fail(job, "the archive took too long");
				goto done;
			}
			if (now_ms() - last_data > job->stall_ms)
			{
				job_fail(job, "the archive stopped sending");
				goto done;
			}
			blocking = socket_wait(socket, SLICE_MILLISECONDS, 0);
			if (blocking < 0)
			{
				job_fail(job, "the connection failed");
				goto done;
			}
			if (!blocking)
				continue;
			count = posix_socket_recv(socket, block, want > (unsigned long long)BLOCK ? BLOCK : (int)want, 0);
			if (count <= 0)
			{
				job_fail(job, "the download was cut short");
				goto done;
			}
			last_data = now_ms();
			if (!job->write(job->context, block, (size_t)count))
			{
				if (!job->error[0])
					job_fail(job, "the file could not be written");
				goto done;
			}
			received += (unsigned long long)count;
		}
	}
	good = 1;
done:
	if (socket >= 0)
		posix_socket_close(socket);
	free(block);
	return good;
}

struct memory
{
	unsigned char *data;
	size_t size;
	size_t capacity;
};

static int memory_write(void *context, unsigned char const *data, size_t length)
{
	struct memory *memory = (struct memory *)context;

	if (memory->size + length > memory->capacity)
		return 0;
	memcpy(memory->data + memory->size, data, length);
	memory->size += length;
	return 1;
}

/* the body of a GET of url (an https:// address, or a test server's http:// one on this machine), at most maximum
bytes of it, into memory (malloc'd, NUL terminated: the caller frees it) */
static int fetch_memory(char const *url, unsigned long maximum, unsigned char **data, size_t *size,
	int volatile *cancel, char *error, int error_size)
{
	struct ae_ce_archive_url parts;

	*data = NULL;
	*size = 0;
	if (!ae_ce_archive_url_parse(url, &parts))
	{
		say(error, error_size, "not an address the archive may be asked at");
		return 0;
	}
	if (parts.secure)
	{
		struct update_response response;

		if (!update_https_request("GET", url, NULL, NULL, 0, maximum, &response, error, error_size))
			return 0;
		if (response.status != 200 || !response.body)
		{
			snprintf(error, (size_t)error_size, "the list's host answered %d", response.status);
			free(response.body);
			return 0;
		}
		if (response.body_size > maximum)
		{
			say(error, error_size, "the answer is larger than allowed");
			free(response.body);
			return 0;
		}
		*data = (unsigned char *)response.body;
		*size = response.body_size;
		return 1;
	}
	if (!ae_ce_archive_host_is_loopback(parts.host))
	{
		say(error, error_size, "a list is fetched over HTTPS only");
		return 0;
	}
	{
		struct http_job job;
		struct memory memory;
		struct ae_ce_archive_limits limits;

		pthread_mutex_lock(&S.lock);
		limits = S.limits;
		pthread_mutex_unlock(&S.lock);
		memset(&job, 0, sizeof(job));
		job.url = parts;
		job.exact = 0;
		job.size = maximum;
		job.write = memory_write;
		job.context = &memory;
		job.cancel = cancel;
		job.stall_ms = limits.stall_ms;
		job.deadline_ms = limits.list_deadline_ms;
		memory.capacity = maximum;
		memory.size = 0;
		memory.data = (unsigned char *)malloc((size_t)maximum + 1);
		if (!memory.data)
		{
			say(error, error_size, "out of memory");
			return 0;
		}
		if (!http_get(&job))
		{
			say(error, error_size, job.error);
			free(memory.data);
			return 0;
		}
		memory.data[memory.size] = 0;
		*data = memory.data;
		*size = memory.size;
		return 1;
	}
}

/* ---------- the list */

static void installed_mark(struct ae_ce_archive_entry *entries, int count, char const *folder)
{
	int i;

	for (i = 0; i < count; i++)
	{
		char path[PATH_SIZE];

		path_join(path, sizeof(path), folder, entries[i].file);
		entries[i].installed = (unsigned char)file_exists(path);
	}
}

static void view_rebuild_locked(void)
{
	if (S.entries && S.view)
		S.view_count = ae_ce_archive_view_build(S.entries, S.count, &S.query, S.view);
	else
		S.view_count = 0;
}

/* a list and its signature taken: verified, parsed, and in use when it is not older than the one in use: 1 if in
use, else 0 with why in error */
static int list_take(unsigned char const *list, size_t list_size, char const *signature, size_t signature_size,
	char const *source, char *error, int error_size)
{
	struct ae_ce_archive_entry *entries;
	int count = 0, skipped = 0, *view;
	unsigned long long serial = 0;
	char folder[PATH_SIZE];
	uint8_t const (*keys)[32] = pinned_keys;
	uint8_t test_keys[1][32];
	int key_count = (int)(sizeof(pinned_keys) / sizeof(pinned_keys[0]));

	pthread_mutex_lock(&S.lock);
	if (S.test_server)
	{
		memcpy(test_keys[0], S.test_key, 32);
		keys = (uint8_t const (*)[32])test_keys;
		key_count = 1;
	}
	pthread_mutex_unlock(&S.lock);
	if (!ae_ce_archive_list_signed(list, list_size, signature, signature_size, keys, key_count))
	{
		snprintf(error, (size_t)error_size, "the list from %s is not signed by the archive's key", source);
		return 0;
	}
	entries = (struct ae_ce_archive_entry *)calloc(AE_CE_ARCHIVE_ENTRIES_MAXIMUM, sizeof(*entries));
	if (!entries)
	{
		snprintf(error, (size_t)error_size, "out of memory");
		return 0;
	}
	if (!ae_ce_archive_list_parse(list, list_size, entries, AE_CE_ARCHIVE_ENTRIES_MAXIMUM, &count, &skipped, &serial,
		error, error_size))
	{
		free(entries);
		return 0;
	}
	/* (Halo PC's own maps are the player's to copy from their own install, never fetched from here) */
	{
		int from, to = 0;

		for (from = 0; from < count; from++)
		{
			char stem[AE_CE_ARCHIVE_FILE_SIZE];

			snprintf(stem, sizeof(stem), "%.*s", (int)strlen(entries[from].file) - 4, entries[from].file);
			if (map_archive_stock_name(stem))
			{
				skipped++;
				continue;
			}
			entries[to++] = entries[from];
		}
		count = to;
		if (count <= 0)
		{
			free(entries);
			snprintf(error, (size_t)error_size, "the list names no map");
			return 0;
		}
	}
	/* (a serial is Unix seconds: one far ahead of this computer's clock cannot be a real list's, and would lock every
	later list out) */
	if (!ae_ce_archive_serial_in_time(serial, (unsigned long long)(time(NULL) > 0 ? time(NULL) : 0)))
	{
		free(entries);
		snprintf(error, (size_t)error_size, "the list's serial is more than %d days ahead of this computer's clock (or the clock is not set)",
			AE_CE_ARCHIVE_SERIAL_DAYS_AHEAD);
		return 0;
	}
	/* (the highest serial and its bytes' hash seen before, from a file of its own: no list that is older, or has its serial
	and other bytes, is taken, whatever the kept copy is or was) */
	{
		char cache[PATH_SIZE], record_path[PATH_SIZE], text[160];
		unsigned char *record_data = NULL;
		size_t record_size = 0;
		uint8_t hash[32], record_hash[32];
		unsigned long long record_serial = 0;
		int have = 0;

		crypto_blake2b(hash, 32, list, list_size);
		cache_folder(cache, sizeof(cache));
		path_join(record_path, sizeof(record_path), cache, RECORD_FILE);
		if (file_exists(record_path))
		{
			/* (a record that is there and cannot be read is not "first use": lists are refused until it is deleted) */
			if (file_read(record_path, 256, &record_data, &record_size))
				have = ae_ce_archive_record_parse((char const *)record_data, &record_serial, record_hash);
			free(record_data);
			if (!have)
			{
				free(entries);
				snprintf(error, (size_t)error_size, "the record of the highest list seen is damaged: delete %s in the data folder to start again",
					"ce_archive/" RECORD_FILE);
				return 0;
			}
		}
		if (!ae_ce_archive_serial_allows(have, record_serial, record_hash, serial, hash, error, error_size))
		{
			free(entries);
			return 0;
		}
		if (!have || serial > record_serial)
		{
			ae_ce_archive_record_format(serial, hash, text, sizeof(text));
			posix_make_directory(cache);
			if (!file_write(record_path, (unsigned char const *)text, strlen(text)))
				platform_log("map archive: the highest serial could not be kept");
		}
	}
	pthread_mutex_lock(&S.lock);
	if (S.state == AE_CE_ARCHIVE_READY && serial < S.serial)
	{
		pthread_mutex_unlock(&S.lock);
		free(entries);
		snprintf(error, (size_t)error_size, "the list from %s is older than the one in use", source);
		return 0;
	}
	pthread_mutex_unlock(&S.lock);
	custom_maps_folder(folder, sizeof(folder));
	installed_mark(entries, count, folder);
	view = (int *)malloc((size_t)count * sizeof(int));
	if (!view)
	{
		free(entries);
		snprintf(error, (size_t)error_size, "out of memory");
		return 0;
	}
	pthread_mutex_lock(&S.lock);
	free(S.entries);
	free(S.view);
	S.entries = (struct ae_ce_archive_entry *)realloc(entries, (size_t)count * sizeof(*entries));
	if (!S.entries)
		S.entries = entries;
	S.view = view;
	S.count = count;
	S.skipped = skipped;
	S.serial = serial;
	S.generation++;
	S.state = AE_CE_ARCHIVE_READY;
	view_rebuild_locked();
	pthread_mutex_unlock(&S.lock);
	platform_log("map archive: the list from %s is in use: serial %llu, %d maps (%d lines left out)", source, serial, count,
		skipped);
	return 1;
}

static void *list_thread(void *unused)
{
	char folder[PATH_SIZE], list_path[PATH_SIZE], signature_path[PATH_SIZE], error[160];
	unsigned char *list = NULL, *signature = NULL;
	size_t list_size = 0, signature_size = 0;
	char list_url[512], signature_url[520];
	static volatile int never_cancelled;
	int kept = 0, network = 0;

	(void)unused;
	error[0] = 0;
	pthread_mutex_lock(&S.lock);
	snprintf(list_url, sizeof(list_url), "%s", S.list_url);
	snprintf(signature_url, sizeof(signature_url), "%s", S.signature_url);
	pthread_mutex_unlock(&S.lock);
	cache_folder(folder, sizeof(folder));
	path_join(list_path, sizeof(list_path), folder, CACHE_LIST);
	path_join(signature_path, sizeof(signature_path), folder, CACHE_SIGNATURE);
	if (file_read(list_path, AE_CE_ARCHIVE_LIST_MAXIMUM, &list, &list_size) &&
		file_read(signature_path, AE_CE_ARCHIVE_SIGNATURE_MAXIMUM, &signature, &signature_size))
	{
		char ignored[160];

		kept = list_take(list, list_size, (char const *)signature, signature_size, "this device", ignored,
			(int)sizeof(ignored));
	}
	free(list);
	free(signature);
	list = signature = NULL;
	if (fetch_memory(list_url, AE_CE_ARCHIVE_LIST_MAXIMUM, &list, &list_size, &never_cancelled, error,
		(int)sizeof(error)) &&
		fetch_memory(signature_url, AE_CE_ARCHIVE_SIGNATURE_MAXIMUM, &signature, &signature_size, &never_cancelled,
		error, (int)sizeof(error)))
	{
		network = list_take(list, list_size, (char const *)signature, signature_size, "the archive", error,
			(int)sizeof(error));
		if (network)
		{
			/* (written only after it verified and parsed) */
			posix_make_directory(folder);
			if (!file_write(list_path, list, list_size) || !file_write(signature_path, signature, signature_size))
				platform_log("map archive: the list could not be kept");
		}
	}
	free(list);
	free(signature);
	pthread_mutex_lock(&S.lock);
	if (S.state != AE_CE_ARCHIVE_READY)
	{
		S.state = AE_CE_ARCHIVE_FAILED;
		snprintf(S.error, sizeof(S.error), "%s", error[0] ? error : "no list");
		platform_log("map archive: no list: %s", S.error);
	}
	else
	{
		S.from_network = network;
		snprintf(S.error, sizeof(S.error), "%s", network ? "" : error);
		if (!network && error[0])
			platform_log("map archive: the kept list stays in use (%s)", error);
	}
	(void)kept;
	S.list_busy = 0;
	pthread_mutex_unlock(&S.lock);
	return NULL;
}

int ae_ce_archive_start(char *why, int why_size)
{
	pthread_t thread;

	init_once();
	if (!permitted(why, why_size))
		return 0;
	pthread_mutex_lock(&S.lock);
	if (S.list_busy)
	{
		pthread_mutex_unlock(&S.lock);
		return 1;
	}
	if (S.state == AE_CE_ARCHIVE_READY)
	{
		pthread_mutex_unlock(&S.lock);
		return 1;
	}
	S.list_busy = 1;
	S.state = AE_CE_ARCHIVE_LOADING;
	S.error[0] = 0;
	pthread_mutex_unlock(&S.lock);
	if (pthread_create(&thread, NULL, list_thread, NULL) != 0)
	{
		pthread_mutex_lock(&S.lock);
		S.list_busy = 0;
		S.state = AE_CE_ARCHIVE_FAILED;
		snprintf(S.error, sizeof(S.error), "could not start the thread");
		pthread_mutex_unlock(&S.lock);
		say(why, why_size, "could not start the thread");
		return 0;
	}
	pthread_detach(thread);
	return 1;
}

void ae_ce_archive_get_status(struct ae_ce_archive_status *status)
{
	init_once();
	pthread_mutex_lock(&S.lock);
	status->state = S.state;
	status->from_network = S.from_network;
	snprintf(status->error, sizeof(status->error), "%s", S.error);
	status->serial = S.serial;
	status->count = S.count;
	status->skipped = S.skipped;
	status->view_count = S.view_count;
	pthread_mutex_unlock(&S.lock);
}

int ae_ce_archive_query(struct ae_ce_archive_query const *query)
{
	int rows;

	init_once();
	pthread_mutex_lock(&S.lock);
	S.query = *query;
	S.query.text[sizeof(S.query.text) - 1] = 0;
	if (S.query.sort < 0 || S.query.sort >= AE_CE_ARCHIVE_SORT_COUNT)
		S.query.sort = AE_CE_ARCHIVE_SORT_NAME;
	if (S.query.show < 0 || S.query.show >= AE_CE_ARCHIVE_SHOW_COUNT)
		S.query.show = AE_CE_ARCHIVE_SHOW_ALL;
	view_rebuild_locked();
	rows = S.view_count;
	pthread_mutex_unlock(&S.lock);
	return rows;
}

int ae_ce_archive_row(int place, struct ae_ce_archive_row *row)
{
	int found = 0;

	init_once();
	pthread_mutex_lock(&S.lock);
	if (place >= 0 && place < S.view_count)
	{
		struct ae_ce_archive_entry const *entry = S.entries + S.view[place];

		memcpy(row->file, entry->file, sizeof(row->file));
		row->size = entry->size;
		row->installed = entry->installed;
		found = 1;
	}
	pthread_mutex_unlock(&S.lock);
	return found;
}

static void *rescan_thread(void *unused)
{
	char folder[PATH_SIZE];

	(void)unused;
	for (;;)
	{
		struct ae_ce_archive_entry *copy;
		unsigned generation;
		int count, i;

		custom_maps_folder(folder, sizeof(folder));
		pthread_mutex_lock(&S.lock);
		S.rescan_again = 0;
		count = S.count;
		generation = S.generation;
		copy = (struct ae_ce_archive_entry *)malloc((size_t)(count ? count : 1) * sizeof(*copy));
		if (copy && count)
			memcpy(copy, S.entries, (size_t)count * sizeof(*copy));
		pthread_mutex_unlock(&S.lock);
		if (copy)
		{
			installed_mark(copy, count, folder);
			pthread_mutex_lock(&S.lock);
			/* (a list taken in the meantime has its own marks, made when it was taken) */
			if (S.generation == generation && S.count == count)
			{
				for (i = 0; i < count; i++)
					S.entries[i].installed = copy[i].installed;
				view_rebuild_locked();
			}
			pthread_mutex_unlock(&S.lock);
			free(copy);
		}
		pthread_mutex_lock(&S.lock);
		if (!S.rescan_again)
		{
			S.rescan_busy = 0;
			pthread_mutex_unlock(&S.lock);
			return NULL;
		}
		pthread_mutex_unlock(&S.lock);
	}
}

void ae_ce_archive_rescan(void)
{
	pthread_t thread;

	init_once();
	pthread_mutex_lock(&S.lock);
	if (S.rescan_busy)
	{
		/* (one under way: it goes round once more) */
		S.rescan_again = 1;
		pthread_mutex_unlock(&S.lock);
		return;
	}
	S.rescan_busy = 1;
	pthread_mutex_unlock(&S.lock);
	if (pthread_create(&thread, NULL, rescan_thread, NULL) == 0)
	{
		pthread_detach(thread);
		return;
	}
	pthread_mutex_lock(&S.lock);
	S.rescan_busy = 0;
	pthread_mutex_unlock(&S.lock);
}

/* ---------- a download */

struct download_sink
{
	FILE *file;
	crypto_blake2b_ctx hash;
	unsigned long long written;
	unsigned long long size;
	char error[160];
};

static int download_write(void *context, unsigned char const *data, size_t length)
{
	struct download_sink *sink = (struct download_sink *)context;
	unsigned long long now = now_ms();

	if (sink->written + length > sink->size)
	{
		snprintf(sink->error, sizeof(sink->error), "the archive's file is larger than the list's");
		return 0;
	}
	if (fwrite(data, 1, length, sink->file) != length)
	{
		snprintf(sink->error, sizeof(sink->error), "custom_maps is full, or could not be written to");
		return 0;
	}
	crypto_blake2b_update(&sink->hash, data, length);
	sink->written += length;
	pthread_mutex_lock(&S.lock);
	S.dl.done = sink->written;
	if (now - S.dl_window_at >= RATE_WINDOW_MILLISECONDS)
	{
		unsigned long long span = now - S.dl_window_at;

		S.dl.rate = span ? (unsigned int)((sink->written - S.dl_window_done) * 1000ULL / span) : 0;
		S.dl_window_at = now;
		S.dl_window_done = sink->written;
	}
	pthread_mutex_unlock(&S.lock);
	return 1;
}

static void download_end(int state, char const *error, char const *installed)
{
	pthread_mutex_lock(&S.lock);
	S.dl.state = state;
	snprintf(S.dl.error, sizeof(S.dl.error), "%s", error ? error : "");
	if (state == AE_CE_ARCHIVE_DOWNLOAD_DONE && installed)
	{
		if (S.pending_count == PENDING_MAXIMUM)
		{
			memmove(S.pending[0], S.pending[1], sizeof(S.pending[0]) * (PENDING_MAXIMUM - 1));
			S.pending_count--;
		}
		snprintf(S.pending[S.pending_count++], sizeof(S.pending[0]), "%s", installed);
		/* (the list's mark of it) */
		{
			int i;

			for (i = 0; i < S.count; i++)
			{
				if (!strcmp(S.entries[i].file, installed))
					S.entries[i].installed = 1;
			}
			view_rebuild_locked();
		}
	}
	S.dl_busy = 0;
	pthread_mutex_unlock(&S.lock);
}

/* whether a map may be replaced now: only where the game says no map is loaded (none says: not) */
static int overwrite_allowed(void)
{
	ae_ce_archive_map_loaded_proc proc;

	pthread_mutex_lock(&S.lock);
	proc = S.map_loaded;
	pthread_mutex_unlock(&S.lock);
	return proc && !proc();
}

/* the map's name (its file, less ".map", then "@ce") appended to the list of downloaded maps the game reads, and flushed to
disk, before the map has its final name: whatever happens next (a quit, a crash, a screen that never polls) the map is
played by the downloaded maps' rules. 1 when it is there; else 0 and why (also when the list holds as many names as the
game's table takes) */
static int mark_downloaded(char const *file, char *error, int error_size)
{
	char path[PATH_SIZE], directory[PATH_SIZE], name[AE_CE_ARCHIVE_FILE_SIZE + 4];
	size_t stem = strlen(file) - 4, cut;
	int good;

	pthread_mutex_lock(&S.lock);
	snprintf(path, sizeof(path), "%s", S.marks);
	pthread_mutex_unlock(&S.lock);
	if (!path[0])
		platform_translate_path(MARKS_PATH, path, (unsigned long)sizeof(path));
	snprintf(name, sizeof(name), "%.*s@ce", (int)stem, file);
	snprintf(directory, sizeof(directory), "%s", path);
	for (cut = strlen(directory); cut > 0 && directory[cut - 1] != '/' && directory[cut - 1] != '\\'; cut--)
		;
	if (cut > 1)
	{
		directory[cut - 1] = 0;
		posix_make_directory(directory);
	}
	good = ae_marks_append_host(path, name, error, error_size);
	return good;
}

static void *download_thread(void *unused)
{
	struct ae_ce_archive_entry entry;
	struct download_sink sink;
	struct http_job job;
	char folder[PATH_SIZE], path[PATH_SIZE], part[PATH_SIZE + 8], error[160], request_path[512];
	char maps_base[512];
	struct ae_ce_archive_url base;
	unsigned char digest[32], header[AE_CE_ARCHIVE_HEADER_BYTES];
	struct ae_ce_archive_limits limits;
	int overwrite, header_read = 0, good_flush, there;
	FILE *check;

	(void)unused;
	error[0] = 0;
	pthread_mutex_lock(&S.lock);
	entry = S.dl_entry;
	overwrite = S.dl_overwrite;
	limits = S.limits;
	snprintf(maps_base, sizeof(maps_base), "%s", S.maps_base);
	pthread_mutex_unlock(&S.lock);
	custom_maps_folder(folder, sizeof(folder));
	path_join(path, sizeof(path), folder, entry.file);
	snprintf(part, sizeof(part), "%s.part", path);
	memset(&sink, 0, sizeof(sink));
	if (!ae_ce_archive_url_parse(maps_base, &base) ||
		!ae_ce_archive_map_path(base.path, entry.listed, request_path, sizeof(request_path)))
	{
		download_end(AE_CE_ARCHIVE_DOWNLOAD_FAILED, "the archive's address is not usable", NULL);
		return NULL;
	}
	memset(&job, 0, sizeof(job));
	job.url = base;
	snprintf(job.url.path, sizeof(job.url.path), "%s", request_path);
	job.exact = 1;
	job.size = entry.size;
	job.write = download_write;
	job.context = &sink;
	job.cancel = &S.cancel;
	job.stall_ms = limits.stall_ms;
	job.deadline_ms = limits.file_deadline_ms;
	sink.size = entry.size;
	sink.file = file_create(part);
	if (!sink.file)
	{
		download_end(AE_CE_ARCHIVE_DOWNLOAD_FAILED, "custom_maps could not be written to", NULL);
		return NULL;
	}
	crypto_blake2b_init(&sink.hash, 32);
	if (!http_get(&job))
	{
		snprintf(error, sizeof(error), "%s", sink.error[0] ? sink.error : job.error);
		goto failed;
	}
	if (sink.written != entry.size)
	{
		snprintf(error, sizeof(error), "the download was cut short");
		goto failed;
	}
	good_flush = fflush(sink.file) == 0;
#if !defined(_WIN32)
	good_flush = good_flush && fsync(fileno(sink.file)) == 0;
#endif
	if (fclose(sink.file) != 0 || !good_flush)
	{
		sink.file = NULL;
		snprintf(error, sizeof(error), "custom_maps is full, or could not be written to");
		goto failed;
	}
	sink.file = NULL;
	crypto_blake2b_final(&sink.hash, digest);
	if (crypto_verify32(digest, entry.blake2b) != 0)
	{
		snprintf(error, sizeof(error), "the file is not the list's (its BLAKE2b differs)");
		goto failed;
	}
	check = fopen(part, "rb");
	if (check)
	{
		header_read = fread(header, 1, sizeof(header), check) == sizeof(header);
		fclose(check);
	}
	if (!header_read)
	{
		snprintf(error, sizeof(error), "the map could not be read back");
		goto failed;
	}
	if (!ae_ce_archive_header_ok(header, entry.size, error, (int)sizeof(error)))
		goto failed;
	if (cancel_asked(&S.cancel))
	{
		snprintf(error, sizeof(error), "cancelled");
		goto failed;
	}
	/* (never over a map there unless the caller said so, and then only while no map is loaded) */
	there = file_exists(path);
	if (there && !overwrite)
	{
		snprintf(error, sizeof(error), "a file named %s is in custom_maps", entry.file);
		goto failed;
	}
	if (there && !overwrite_allowed())
	{
		snprintf(error, sizeof(error), "a map is loaded: nothing is replaced now");
		goto failed;
	}
	/* (the map is a downloaded map before it has its name: written to the game's list, and refused if it cannot be. The
	checks that can refuse the install are all above, so a name is listed for a map that did not come only if the last step
	fails, which makes that name's map stricter and nothing else) */
	if (!mark_downloaded(entry.file, error, (int)sizeof(error)))
		goto failed;
	/* (a cancel that came while the list was written: the map there is kept, as the cancel's notice says) */
	if (cancel_asked(&S.cancel))
	{
		snprintf(error, sizeof(error), "cancelled");
		goto failed;
	}
	if (there)
	{
		/* (one step: the new file takes the old one's name, so there is no moment without a map there) */
		if (!replace_file(part, path))
		{
			snprintf(error, sizeof(error), "the map there could not be replaced (is it in use?)");
			goto failed;
		}
	}
	else if (!rename_no_replace(part, path))
	{
		snprintf(error, sizeof(error), "a file named %s is in custom_maps, or the map could not be put there", entry.file);
		goto failed;
	}
	sync_directory(path);
	platform_log("map archive: %s installed in custom_maps (%llu bytes, BLAKE2b and header checked)", entry.file,
		entry.size);
	download_end(AE_CE_ARCHIVE_DOWNLOAD_DONE, NULL, entry.file);
	return NULL;
failed:
	if (sink.file)
		fclose(sink.file);
	remove(part);
	platform_log("map archive: %s not installed: %s", entry.file, error);
	download_end(cancel_asked(&S.cancel) || !strcmp(error, "cancelled") ? AE_CE_ARCHIVE_DOWNLOAD_CANCELLED :
		AE_CE_ARCHIVE_DOWNLOAD_FAILED, error, NULL);
	return NULL;
}

int ae_ce_archive_download(char const *file, int overwrite, char *why, int why_size)
{
	char wanted[AE_CE_ARCHIVE_FILE_SIZE], folder[PATH_SIZE], path[PATH_SIZE];
	unsigned long long free_bytes, size;
	pthread_t thread;
	int index, known;

	init_once();
	if (!permitted(why, why_size))
		return 0;
	if (!file || !ae_ce_archive_file_name_ok(file, wanted))
	{
		say(why, why_size, "that is not a map file name");
		return 0;
	}
	if (overwrite && !overwrite_allowed())
	{
		say(why, why_size, "a map is loaded: nothing is replaced now");
		return 0;
	}
	custom_maps_folder(folder, sizeof(folder));
	pthread_mutex_lock(&S.lock);
	if (S.dl_busy)
	{
		pthread_mutex_unlock(&S.lock);
		say(why, why_size, "a download is under way");
		return 0;
	}
	for (index = 0; index < S.count && strcmp(S.entries[index].file, wanted); index++)
		;
	if (index >= S.count)
	{
		pthread_mutex_unlock(&S.lock);
		say(why, why_size, "that map is not in the list");
		return 0;
	}
	size = S.entries[index].size;
	pthread_mutex_unlock(&S.lock);
	path_join(path, sizeof(path), folder, wanted);
	if (!overwrite && file_exists(path))
	{
		say(why, why_size, "a file of that name is in custom_maps already");
		return 0;
	}
	free_bytes = disk_free_bytes(folder, &known);
	if (!known)
	{
		say(why, why_size, "the free disk space is not known");
		return 0;
	}
	if (!ae_ce_archive_disk_allows(free_bytes, size))
	{
		say(why, why_size, "there is not enough free disk space");
		return 0;
	}
	pthread_mutex_lock(&S.lock);
	if (S.dl_busy)
	{
		pthread_mutex_unlock(&S.lock);
		say(why, why_size, "a download is under way");
		return 0;
	}
	for (index = 0; index < S.count && strcmp(S.entries[index].file, wanted); index++)
		;
	if (index >= S.count)
	{
		pthread_mutex_unlock(&S.lock);
		say(why, why_size, "that map is not in the list");
		return 0;
	}
	S.dl_entry = S.entries[index];
	S.dl_busy = 1;
	S.dl_overwrite = overwrite;
	__atomic_store_n(&S.cancel, 0, __ATOMIC_RELEASE);
	memset(&S.dl, 0, sizeof(S.dl));
	S.dl.state = AE_CE_ARCHIVE_DOWNLOAD_RUNNING;
	snprintf(S.dl.file, sizeof(S.dl.file), "%s", wanted);
	S.dl.total = S.dl_entry.size;
	S.dl_started = S.dl_window_at = now_ms();
	S.dl_window_done = 0;
	pthread_mutex_unlock(&S.lock);
	if (pthread_create(&thread, NULL, download_thread, NULL) != 0)
	{
		download_end(AE_CE_ARCHIVE_DOWNLOAD_FAILED, "could not start the thread", NULL);
		say(why, why_size, "could not start the thread");
		return 0;
	}
	pthread_detach(thread);
	return 1;
}

void ae_ce_archive_cancel(void)
{
	__atomic_store_n(&S.cancel, 1, __ATOMIC_RELEASE);
}

void ae_ce_archive_download_status(struct ae_ce_archive_download *status)
{
	init_once();
	pthread_mutex_lock(&S.lock);
	*status = S.dl;
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_download_reset(void)
{
	pthread_mutex_lock(&S.lock);
	if (!S.dl_busy)
		memset(&S.dl, 0, sizeof(S.dl));
	pthread_mutex_unlock(&S.lock);
}

void ae_ce_archive_poll(void)
{
	for (;;)
	{
		char file[AE_CE_ARCHIVE_FILE_SIZE];
		ae_ce_archive_installed_proc hook;

		pthread_mutex_lock(&S.lock);
		hook = S.hook;
		if (!S.pending_count || !hook)
		{
			pthread_mutex_unlock(&S.lock);
			return;
		}
		snprintf(file, sizeof(file), "%s", S.pending[0]);
		memmove(S.pending[0], S.pending[1], sizeof(S.pending[0]) * (size_t)(PENDING_MAXIMUM - 1));
		S.pending_count--;
		pthread_mutex_unlock(&S.lock);
		hook(file);
	}
}

/* ---------- DEV MODE and the tests */

int ae_ce_archive_set_test_server(char const *base, char const *key_hex, char *why, int why_size)
{
	struct ae_ce_archive_url parts;
	uint8_t key[32];
	size_t length;

	init_once();
	if (!base || !key_hex || !ae_ce_archive_url_parse(base, &parts) || parts.secure ||
		!ae_ce_archive_host_is_loopback(parts.host))
	{
		say(why, why_size, "a test archive is http://127.0.0.1:<port> or http://localhost:<port> only");
		return 0;
	}
	length = strlen(key_hex);
	if (!ae_ce_archive_hex(key_hex, length, key, 32))
	{
		say(why, why_size, "the test key is 64 hex digits");
		return 0;
	}
	length = strlen(parts.path);
	while (length > 1 && parts.path[length - 1] == '/')
		parts.path[--length] = 0;
	if (length == 1)
		parts.path[0] = 0;
	pthread_mutex_lock(&S.lock);
	S.test_server = 1;
	memcpy(S.test_key, key, 32);
	snprintf(S.list_url, sizeof(S.list_url), "http://%s:%d%s/" LIST_FILE, parts.host, parts.port, parts.path);
	snprintf(S.signature_url, sizeof(S.signature_url), "%s" SIGNATURE_SUFFIX, S.list_url);
	snprintf(S.maps_base, sizeof(S.maps_base), "http://%s:%d%s/maps/", parts.host, parts.port, parts.path);
	pthread_mutex_unlock(&S.lock);
	return 1;
}

int ae_ce_archive_use_dev_environment(char *text, int text_size)
{
	char const *base = getenv("AE_CE_ARCHIVE_BASE");
	char const *key = getenv("AE_CE_ARCHIVE_TEST_KEY");
	char why[160];

	if (!base || !base[0])
		return 0;
	if (!key || !key[0])
	{
		say(text, text_size, "AE_CE_ARCHIVE_BASE needs AE_CE_ARCHIVE_TEST_KEY (64 hex digits)");
		return -1;
	}
	if (!ae_ce_archive_set_test_server(base, key, why, (int)sizeof(why)))
	{
		say(text, text_size, why);
		return -1;
	}
	if (text && text_size > 0)
		snprintf(text, (size_t)text_size, "test archive at %s", base);
	return 1;
}

#endif
