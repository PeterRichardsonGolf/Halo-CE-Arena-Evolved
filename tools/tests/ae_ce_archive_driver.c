/* The community map archive's library (port/linux/src/ae_ce_archive.c), run natively for tools/test_ae_ce_archive.py
 * against a made-up archive on 127.0.0.1: the platform's calls it makes are small stand-ins here (BSD sockets for the
 * posix_socket_* calls, a settable config, no HTTPS: update_https_request fails). One command a word on the command
 * line; every answer is a line "key=value ..." on stdout. Nothing here reaches the real archive: the address is
 * given, and the library takes only this machine's.
 *
 *   driver sign <seed byte> <list file> <signature file>     prints the public key (64 hex digits)
 *   driver <base> <key hex> <maps folder> <cache folder> <command words ...>
 *     start | query <text> | get <file> | getover <file> | getcancel <file> <bytes> | limits <stall ms> <deadline ms> <free bytes>
 *     config <enabled 0|1> <consent 0|1> | poll | status | wait <ms> */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ae_ce_archive.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"
#include "posix.h"
#include "update.h"

/* ---------- the platform's calls, as stand-ins */

static int config_enabled = 1, config_consent = 1;

void platform_log(char const *format, ...)
{
	va_list arguments;

	fputs("log: ", stderr);
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fputc('\n', stderr);
}

char const *platform_data_root(void)
{
	char const *root = getenv("AE_TEST_DATA_ROOT");

	return root ? root : "/nonexistent";
}
void platform_translate_path(char const *xbox_path, char *host_path, unsigned long host_path_size)
{
	snprintf(host_path, host_path_size, "/nonexistent/%s", xbox_path);
}
int config_boolean(char const *name)
{
	return !strcmp(name, "game.ce_archive") ? config_enabled : !strcmp(name, "game.ce_archive_consented") ? config_consent : 0;
}
char const *config_string(char const *name) { (void)name; return ""; }
int config_write_boolean(char const *name, int value) { (void)name; (void)value; return 1; }
static int map_loaded_flag;
static int map_loaded(void) { return map_loaded_flag; }

int update_https_request(char const *method, char const *url, char const *headers, void const *body,
	unsigned long body_size, unsigned long maximum, struct update_response *response, char *error, int error_size)
{
	(void)method; (void)url; (void)headers; (void)body; (void)body_size; (void)maximum;
	memset(response, 0, sizeof(*response));
	snprintf(error, (size_t)error_size, "https is not available in this test");
	return 0;
}

int posix_socket(int family, int type, int protocol) { return socket(family, type, protocol); }
int posix_socket_close(int s) { return close(s); }
int posix_socket_connect(int s, void const *address, int length) { return connect(s, (struct sockaddr const *)address, (socklen_t)length); }
int posix_socket_send(int s, void const *buffer, int length, int flags) { (void)flags; return (int)send(s, buffer, (size_t)length, MSG_NOSIGNAL); }
int posix_socket_recv(int s, void *buffer, int length, int flags) { (void)flags; return (int)recv(s, buffer, (size_t)length, 0); }
int posix_socket_set_nonblocking(int s, int on)
{
	int flags = fcntl(s, F_GETFL, 0);

	return fcntl(s, F_SETFL, on ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
}
int posix_socket_getsockopt(int s, int level, int name, void *value, int *length)
{
	socklen_t size = (socklen_t)*length;
	int result;

	/* (Winsock's SOL_SOCKET and SO_ERROR) */
	result = getsockopt(s, level == 0xffff ? SOL_SOCKET : level, name == 0x1007 ? SO_ERROR : name, value, &size);
	*length = (int)size;
	return result;
}
int posix_socket_select(int *read, int *read_count, int *write, int *write_count, int *error, int *error_count,
	posix_long seconds, posix_long microseconds, int infinite)
{
	struct pollfd entry;
	int result;

	(void)error; (void)error_count; (void)infinite;
	entry.fd = write && write_count && *write_count ? write[0] : read[0];
	entry.events = write && write_count && *write_count ? POLLOUT : POLLIN;
	entry.revents = 0;
	result = poll(&entry, 1, (int)(seconds * 1000 + microseconds / 1000));
	if (result < 0)
		return -1;
	if (read_count && !(write && write_count && *write_count))
		*read_count = result > 0 ? 1 : 0;
	if (write_count && write && *write_count)
		*write_count = result > 0 ? 1 : 0;
	return result;
}
posix_ulong posix_resolve_ipv4(char const *host)
{
	struct addrinfo hints, *results;
	posix_ulong address = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, NULL, &hints, &results) != 0)
		return 0;
	address = ((struct sockaddr_in *)results->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(results);
	return address;
}
int posix_make_directory(char const *path) { return mkdir(path, 0777) == 0 || errno == EEXIST; }

/* ---------- the driver */

static void installed_hook(char const *file) { printf("installed file=%s\n", file); }

static unsigned long long now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned long long)t.tv_sec * 1000ULL + (unsigned long long)t.tv_nsec / 1000000ULL;
}

static void nap(unsigned milliseconds)
{
	struct timespec t;

	t.tv_sec = milliseconds / 1000;
	t.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	nanosleep(&t, NULL);
}

static void show_status(void)
{
	struct ae_ce_archive_status status;

	ae_ce_archive_get_status(&status);
	printf("status state=%s network=%d count=%d skipped=%d serial=%llu view=%d error=%s\n",
		status.state == AE_CE_ARCHIVE_READY ? "READY" : status.state == AE_CE_ARCHIVE_FAILED ? "FAILED" :
		status.state == AE_CE_ARCHIVE_LOADING ? "LOADING" : "IDLE", status.from_network, status.count, status.skipped,
		status.serial, status.view_count, status.error);
}

static void show_download(void)
{
	struct ae_ce_archive_download d;

	ae_ce_archive_download_status(&d);
	printf("download state=%s file=%s done=%llu total=%llu error=%s\n",
		d.state == AE_CE_ARCHIVE_DOWNLOAD_DONE ? "DONE" : d.state == AE_CE_ARCHIVE_DOWNLOAD_FAILED ? "FAILED" :
		d.state == AE_CE_ARCHIVE_DOWNLOAD_CANCELLED ? "CANCELLED" : d.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING ?
		"RUNNING" : "NONE", d.file, d.done, d.total, d.error);
}

static int wait_download(unsigned long long limit_ms)
{
	unsigned long long until = now_ms() + limit_ms;
	struct ae_ce_archive_download d;

	for (;;)
	{
		ae_ce_archive_download_status(&d);
		if (d.state != AE_CE_ARCHIVE_DOWNLOAD_RUNNING)
			return 1;
		if (now_ms() > until)
			return 0;
		nap(20);
	}
}

static int sign_mode(int argc, char **argv)
{
	uint8_t seed[32], secret[64], key[32], signature[64];
	static unsigned char message[16 * 1024 * 1024];
	char const context[] = "fractum-halo-map-index 1\n";
	size_t used = strlen(context), size;
	FILE *file;
	int i;

	if (argc < 5)
		return 2;
	memset(seed, atoi(argv[2]), sizeof(seed));
	crypto_ed25519_key_pair(secret, key, seed);
	file = fopen(argv[3], "rb");
	if (!file)
		return 2;
	memcpy(message, context, used);
	size = fread(message + used, 1, sizeof(message) - used, file);
	fclose(file);
	crypto_ed25519_sign(signature, secret, message, used + size);
	file = fopen(argv[4], "wb");
	for (i = 0; i < 64; i++)
		fprintf(file, "%02x", signature[i]);
	fclose(file);
	for (i = 0; i < 32; i++)
		printf("%02x", key[i]);
	printf("\n");
	return 0;
}

int main(int argc, char **argv)
{
	char why[200];
	int at;
	struct ae_ce_archive_limits limits;

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc >= 2 && !strcmp(argv[1], "sign"))
		return sign_mode(argc, argv);
	if (argc >= 2 && !strcmp(argv[1], "hosts"))
	{
		char list_host[128], file_host[128];

		ae_ce_archive_hosts(list_host, (int)sizeof(list_host), file_host, (int)sizeof(file_host));
		printf("hosts list=%s files=%s\n", list_host, file_host);
		return 0;
	}
	if (argc < 5)
		return 2;
	ae_ce_archive_limits_default(&limits);
	ae_ce_archive_set_folders(argv[3], argv[4]);
	if (!ae_ce_archive_set_test_server(argv[1], argv[2], why, (int)sizeof(why)))
	{
		printf("test_server refused: %s\n", why);
		return 0;
	}
	ae_ce_archive_set_installed_hook(installed_hook);
	ae_ce_archive_set_map_loaded_proc(map_loaded);
	for (at = 5; at < argc; at++)
	{
		char const *word = argv[at];

		if (!strcmp(word, "start"))
		{
			unsigned long long until = now_ms() + 20000;
			struct ae_ce_archive_status status;

			if (!ae_ce_archive_start(why, (int)sizeof(why)))
			{
				printf("start refused: %s\n", why);
				continue;
			}
			do
			{
				nap(20);
				ae_ce_archive_get_status(&status);
			} while (status.state == AE_CE_ARCHIVE_LOADING && now_ms() < until);
			show_status();
		}
		else if (!strcmp(word, "query") && at + 1 < argc)
		{
			struct ae_ce_archive_query query;
			struct ae_ce_archive_row row;
			int rows, place;

			memset(&query, 0, sizeof(query));
			snprintf(query.text, sizeof(query.text), "%s", argv[++at]);
			rows = ae_ce_archive_query(&query);
			printf("query rows=%d files=", rows);
			for (place = 0; ae_ce_archive_row(place, &row); place++)
				printf("%s%s%s", place ? "," : "", row.file, row.installed ? "*" : "");
			printf("\n");
		}
		else if ((!strcmp(word, "get") || !strcmp(word, "getover")) && at + 1 < argc)
		{
			char const *file = argv[++at];

			if (!ae_ce_archive_download(file, !strcmp(word, "getover"), why, (int)sizeof(why)))
			{
				printf("get refused: %s\n", why);
				continue;
			}
			if (!wait_download(60000))
				printf("get timed out\n");
			show_download();
		}
		else if (!strcmp(word, "getcancel") && at + 2 < argc)
		{
			char const *file = argv[++at];
			unsigned long long bytes = strtoull(argv[++at], NULL, 10), until = now_ms() + 30000;
			struct ae_ce_archive_download d;

			if (!ae_ce_archive_download(file, 0, why, (int)sizeof(why)))
			{
				printf("get refused: %s\n", why);
				continue;
			}
			do
			{
				nap(10);
				ae_ce_archive_download_status(&d);
			} while (d.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING && d.done < bytes && now_ms() < until);
			printf("cancelling at done=%llu\n", d.done);
			ae_ce_archive_cancel();
			wait_download(10000);
			show_download();
		}
		else if (!strcmp(word, "limits") && at + 3 < argc)
		{
			limits.stall_ms = strtoul(argv[++at], NULL, 10);
			limits.file_deadline_ms = strtoul(argv[++at], NULL, 10);
			limits.list_deadline_ms = limits.file_deadline_ms;
			limits.fake_free_bytes = strtoull(argv[++at], NULL, 10);
			limits.fake_free_set = limits.fake_free_bytes != 0;
			ae_ce_archive_set_limits(&limits);
		}
		else if (!strcmp(word, "config") && at + 2 < argc)
		{
			config_enabled = atoi(argv[++at]);
			config_consent = atoi(argv[++at]);
		}
		else if (!strcmp(word, "marks") && at + 1 < argc)
			ae_ce_archive_set_marks_file(argv[++at]);
		else if (!strcmp(word, "maploaded") && at + 1 < argc)
			map_loaded_flag = atoi(argv[++at]);
		else if (!strcmp(word, "hosts"))
		{
			char list_host[128], file_host[128];

			ae_ce_archive_hosts(list_host, (int)sizeof(list_host), file_host, (int)sizeof(file_host));
			printf("hosts list=%s files=%s\n", list_host, file_host);
		}
		else if (!strcmp(word, "poll"))
			ae_ce_archive_poll();
		else if (!strcmp(word, "status"))
		{
			show_status();
			show_download();
		}
		else if (!strcmp(word, "wait") && at + 1 < argc)
			nap((unsigned)atoi(argv[++at]));
		else
		{
			printf("unknown command %s\n", word);
			return 3;
		}
	}
	return 0;
}
