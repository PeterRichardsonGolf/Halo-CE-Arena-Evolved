/*
AE_STEAM_WS.C

ae_steam_ws.h. Mbed TLS (port/third_party/mbedtls) over the host's own socket calls (posix_socket_*: the same on every
desktop host, and polled, so a cancel or the budget is seen within a tenth of a second and no thread is left behind).
The certificate and the host name are always verified, against the roots of ae_steam_ws_roots.h only. What is received
(a sign-in's answers hold tokens) is wiped as its buffers go.
*/

#if defined(HALO_STEAM_CLIENT)

#include "ae_steam_ws.h"
#include "ae_steam_ws_rules.h"
#include "ae_steam_ws_roots.h"
#include "posix.h"
#include "steam_crypto.h"
#include "steam_net.h"

#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/psa_util.h"
#include "psa/crypto.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { SLICE = 100, MAXIMUM_MESSAGE = 16 * 1024 * 1024, HEAD_LIMIT = 8192, SEND_MILLISECONDS = 30000 };

/* (posix_socket_getsockopt and posix_socket_last_error speak Winsock's: posix_net.c) */
#define WINSOCK_SOL_SOCKET 0xffff
#define WINSOCK_SO_ERROR 0x1007
/* (mbedtls/net_sockets.h's, which pulls in the system's socket headers: not needed, the sockets are the host's) */
#define BIO_CONN_RESET (-0x0050)
#define BIO_SEND_FAILED (-0x004E)
#define BIO_RECV_FAILED (-0x004C)
#define WSAEWOULDBLOCK 10035
#define WSAEINPROGRESS 10036
#define WSAECONNRESET 10054

struct ae_steam_ws
{
	int socket;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config config;
	int tls;
	unsigned char *in;        /* bytes read, not yet taken as frames */
	size_t in_size, in_capacity;
	unsigned char *message;   /* a fragmented message's parts so far */
	size_t message_size, message_capacity;
	int message_open;
	char error[256];
};

char const *ae_steam_ws_error(struct ae_steam_ws const *ws)
{
	return ws ? ws->error : "";
}

static void fail(struct ae_steam_ws *ws, char const *what, int code)
{
	if (code)
	{
		char reason[160];

		mbedtls_strerror(code, reason, sizeof(reason));
		snprintf(ws->error, sizeof(ws->error), "%s (%s)", what, reason);
	}
	else
		snprintf(ws->error, sizeof(ws->error), "%s", what);
}

/* ---------- the trusted roots */

static pthread_once_t roots_once = PTHREAD_ONCE_INIT;
static mbedtls_x509_crt roots;
static int roots_loaded;

static void roots_load(void)
{
	mbedtls_x509_crt_init(&roots);
	roots_loaded = psa_crypto_init() == PSA_SUCCESS &&
		mbedtls_x509_crt_parse(&roots, (unsigned char const *)ae_steam_ws_root_isrg_x2,
			sizeof(ae_steam_ws_root_isrg_x2)) == 0 &&
		mbedtls_x509_crt_parse(&roots, (unsigned char const *)ae_steam_ws_root_isrg_x1,
			sizeof(ae_steam_ws_root_isrg_x1)) == 0;
}

/* ---------- the socket */

/* a buffer grown into a new one, the old wiped before it is let go (no realloc: it would leave the old bytes behind) */
static int buffer_reserve(unsigned char **buffer, size_t *capacity, size_t used, size_t need)
{
	unsigned char *grown;
	size_t size = *capacity ? *capacity : 16384;

	if (need <= *capacity)
		return 1;
	while (size < need)
		size *= 2;
	grown = malloc(size);
	if (!grown)
		return 0;
	if (*buffer)
	{
		memcpy(grown, *buffer, used);
		steam_wipe(*buffer, *capacity);
		free(*buffer);
	}
	*buffer = grown;
	*capacity = size;
	return 1;
}

/* the time and the stop: 0 go on, 1 stopped, 2 out of time */
static int ended(int (*stop)(void), unsigned long deadline, int has_deadline)
{
	if (stop && stop())
		return 1;
	if (has_deadline && (long)(deadline - steam_milliseconds()) <= 0)
		return 2;
	return 0;
}

/* a wait for the socket, at most milliseconds: 1 ready, 0 not yet, -1 an error */
static int wait_socket(struct ae_steam_ws *ws, int writing, unsigned long milliseconds)
{
	int list[1];
	int count = 1;
	int none = 0;

	list[0] = ws->socket;
	if (posix_socket_select(writing ? NULL : list, writing ? &none : &count, writing ? list : NULL,
		writing ? &count : &none, NULL, &none, (posix_long)(milliseconds / 1000), (posix_long)(milliseconds % 1000) * 1000,
		0) < 0)
		return -1;
	return count > 0;
}

static int bio_send(void *context, unsigned char const *data, size_t size)
{
	struct ae_steam_ws *ws = context;
	int sent = posix_socket_send(ws->socket, data, (int)(size > 65536 ? 65536 : size), 0);

	if (sent >= 0)
		return sent;
	if (posix_socket_last_error() == WSAEWOULDBLOCK)
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	return posix_socket_last_error() == WSAECONNRESET ? BIO_CONN_RESET : BIO_SEND_FAILED;
}

static int bio_receive(void *context, unsigned char *data, size_t size)
{
	struct ae_steam_ws *ws = context;
	int got = posix_socket_recv(ws->socket, data, (int)(size > 65536 ? 65536 : size), 0);

	if (got >= 0)
		return got;
	if (posix_socket_last_error() == WSAEWOULDBLOCK)
		return MBEDTLS_ERR_SSL_WANT_READ;
	return posix_socket_last_error() == WSAECONNRESET ? BIO_CONN_RESET : BIO_RECV_FAILED;
}

/* ---------- the name lookup, on a short-lived worker the caller can walk away from

getaddrinfo cannot be cancelled and a broken resolver (a captive portal, a VPN's DNS) can hold it for tens of seconds: the
lookup runs on a thread of its own, the caller waits for it in slices that see a cancel and the budget, and an abandoned
lookup ends by itself when the resolver gives up (and frees its own job). A few at once at most. */

struct resolve_job
{
	pthread_mutex_t lock;
	pthread_cond_t changed;
	int done;
	int abandoned;
	posix_ulong ip;
	char host[256];
};

static pthread_mutex_t resolving_lock = PTHREAD_MUTEX_INITIALIZER;
static int resolving;   /* lookups running, abandoned ones included */
enum { MAXIMUM_RESOLVING = 4 };

static void *resolve_thread(void *argument)
{
	struct resolve_job *job = argument;
	posix_ulong ip = posix_resolve_ipv4(job->host);
	int abandoned;

	pthread_mutex_lock(&job->lock);
	job->ip = ip;
	job->done = 1;
	abandoned = job->abandoned;
	pthread_cond_broadcast(&job->changed);
	pthread_mutex_unlock(&job->lock);
	pthread_mutex_lock(&resolving_lock);
	resolving--;
	pthread_mutex_unlock(&resolving_lock);
	if (abandoned)
	{
		pthread_cond_destroy(&job->changed);
		pthread_mutex_destroy(&job->lock);
		free(job);
	}
	return NULL;
}

/* the host's IPv4 address: nonzero, or 0 (why in ws->error; *stopped set when a cancel or the budget ended the wait) */
static posix_ulong resolve_host(struct ae_steam_ws *ws, char const *host, int (*stop)(void), unsigned long deadline,
	int has_deadline, int *stopped)
{
	struct resolve_job *job;
	pthread_t thread;
	posix_ulong ip;
	int started;

	pthread_mutex_lock(&resolving_lock);
	started = resolving < MAXIMUM_RESOLVING;
	if (started)
		resolving++;
	pthread_mutex_unlock(&resolving_lock);
	if (!started)
	{
		fail(ws, "name lookups are still pending", 0);
		return 0;
	}
	job = calloc(1, sizeof(*job));
	if (job)
	{
		pthread_mutex_init(&job->lock, NULL);
		pthread_cond_init(&job->changed, NULL);
		snprintf(job->host, sizeof(job->host), "%s", host);
	}
	if (!job || pthread_create(&thread, NULL, resolve_thread, job) != 0)
	{
		if (job)
		{
			pthread_cond_destroy(&job->changed);
			pthread_mutex_destroy(&job->lock);
			free(job);
		}
		pthread_mutex_lock(&resolving_lock);
		resolving--;
		pthread_mutex_unlock(&resolving_lock);
		fail(ws, "no thread for the name lookup", 0);
		return 0;
	}
	pthread_detach(thread);
	for (;;)
	{
		struct timespec until;
		int why;

		pthread_mutex_lock(&job->lock);
		if (job->done)
		{
			ip = job->ip;
			pthread_mutex_unlock(&job->lock);
			pthread_cond_destroy(&job->changed);
			pthread_mutex_destroy(&job->lock);
			free(job);
			if (!ip)
				fail(ws, "the server's name did not resolve", 0);
			return ip;
		}
		why = ended(stop, deadline, has_deadline);
		if (why)
		{
			job->abandoned = 1;
			pthread_mutex_unlock(&job->lock);
			*stopped = why;
			fail(ws, why == 1 ? "Signing in was cancelled." : "the server's name lookup took too long", 0);
			return 0;
		}
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_nsec += SLICE * 1000000L;
		if (until.tv_nsec >= 1000000000L)
		{
			until.tv_sec++;
			until.tv_nsec -= 1000000000L;
		}
		pthread_cond_timedwait(&job->changed, &job->lock, &until);
		pthread_mutex_unlock(&job->lock);
	}
}

static int tcp_connect(struct ae_steam_ws *ws, char const *host, int port, int (*stop)(void), unsigned long deadline,
	int has_deadline, int *stopped)
{
	struct steam_sockaddr address;
	posix_ulong ip = resolve_host(ws, host, stop, deadline, has_deadline, stopped);

	if (!ip)
		return 0;
	ws->socket = posix_socket(2 /* AF_INET */, 1 /* SOCK_STREAM */, 0);
	if (ws->socket < 0)
	{
		fail(ws, "no socket", 0);
		return 0;
	}
	steam_address(&address, (uint32_t)ip, port);
	posix_socket_set_nonblocking(ws->socket, 1);
	if (posix_socket_connect(ws->socket, &address, sizeof(address)) != 0)
	{
		int error = 0, length = sizeof(error);

		/* (in progress: done when it can be written to, if no error) */
		for (;;)
		{
			int why = ended(stop, deadline, has_deadline), ready;

			if (why)
			{
				*stopped = why;
				fail(ws, why == 1 ? "Signing in was cancelled." : "the connection took too long", 0);
				return 0;
			}
			ready = wait_socket(ws, 1, SLICE);
			if (ready < 0)
			{
				fail(ws, "the connection failed", 0);
				return 0;
			}
			if (ready)
				break;
		}
		if (posix_socket_getsockopt(ws->socket, WINSOCK_SOL_SOCKET, WINSOCK_SO_ERROR, &error, &length) != 0 || error)
		{
			fail(ws, "could not connect", 0);
			return 0;
		}
	}
	posix_socket_set_nodelay(ws->socket);
	return 1;
}

/* ---------- the TLS connection and its upgrade */

static int tls_write_all(struct ae_steam_ws *ws, unsigned char const *data, size_t size, int (*stop)(void),
	unsigned long deadline, int has_deadline, int *stopped)
{
	while (size)
	{
		int result = mbedtls_ssl_write(&ws->ssl, data, size);

		if (result == MBEDTLS_ERR_SSL_WANT_WRITE || result == MBEDTLS_ERR_SSL_WANT_READ)
		{
			int why = ended(stop, deadline, has_deadline);

			if (why)
			{
				*stopped = why;
				return 0;
			}
			(void)wait_socket(ws, result == MBEDTLS_ERR_SSL_WANT_WRITE, SLICE);
			continue;
		}
		if (result <= 0)
		{
			fail(ws, "the connection to Steam was lost", result);
			return 0;
		}
		data += result;
		size -= (size_t)result;
	}
	return 1;
}

static void connection_free(struct ae_steam_ws *ws)
{
	mbedtls_ssl_free(&ws->ssl);
	mbedtls_ssl_config_free(&ws->config);
	if (ws->socket >= 0)
		posix_socket_close(ws->socket);
	ws->socket = -1;
	if (ws->in)
		steam_wipe(ws->in, ws->in_capacity);
	if (ws->message)
		steam_wipe(ws->message, ws->message_capacity);
	free(ws->in);
	free(ws->message);
}

void ae_steam_ws_close(struct ae_steam_ws *ws)
{
	if (!ws)
		return;
	if (ws->tls)
	{
		unsigned char mask[4] = { 0, 0, 0, 0 }, frame[8];
		size_t size = ae_ws_encode(frame, sizeof(frame), AE_WS_CLOSE, NULL, 0, mask);
		int stopped = 0;

		/* (a close frame, if the line is still there to take it) */
		(void)tls_write_all(ws, frame, size, NULL, steam_milliseconds() + 200, 1, &stopped);
	}
	connection_free(ws);
	free(ws);
}

static void random_bytes(void *bytes, size_t size)
{
	steam_random(bytes, size);
}

struct ae_steam_ws *ae_steam_ws_open(char const *host, int port, char const *path, int (*stop)(void),
	unsigned long budget_ms, int *stopped, char *error, int error_size)
{
	struct ae_steam_ws *ws;
	unsigned long deadline = steam_milliseconds() + budget_ms;
	int has_deadline = budget_ms != 0;
	unsigned char raw[16];
	char key[32], request[1024], head[HEAD_LIMIT + 1];
	size_t head_size = 0, at, key_length = 0;
	static char const alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int result, why;

	*stopped = 0;
	if (!ae_ws_host_allowed(host))
	{
		snprintf(error, (size_t)error_size, "refused host %.60s: not a steamserver.net name", host);
		return NULL;
	}
	pthread_once(&roots_once, roots_load);
	if (!roots_loaded)
	{
		snprintf(error, (size_t)error_size, "the WebSocket route's trusted roots did not load");
		return NULL;
	}
	ws = calloc(1, sizeof(*ws));
	if (!ws)
	{
		snprintf(error, (size_t)error_size, "out of memory");
		return NULL;
	}
	ws->socket = -1;
	mbedtls_ssl_init(&ws->ssl);
	mbedtls_ssl_config_init(&ws->config);
	if (!tcp_connect(ws, host, port, stop, deadline, has_deadline, stopped))
		goto failed;
	if ((result = mbedtls_ssl_config_defaults(&ws->config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		fail(ws, "could not set up TLS", result);
		goto failed;
	}
	/* (the certificate and the host name checked, always, against the pinned roots only) */
	mbedtls_ssl_conf_authmode(&ws->config, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&ws->config, &roots, NULL);
	mbedtls_ssl_conf_rng(&ws->config, mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
	if ((result = mbedtls_ssl_setup(&ws->ssl, &ws->config)) != 0 ||
		(result = mbedtls_ssl_set_hostname(&ws->ssl, host)) != 0)
	{
		fail(ws, "could not set up TLS", result);
		goto failed;
	}
	mbedtls_ssl_set_bio(&ws->ssl, ws, bio_send, bio_receive, NULL);
	while ((result = mbedtls_ssl_handshake(&ws->ssl)) != 0)
	{
		if (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			why = ended(stop, deadline, has_deadline);
			if (why)
			{
				*stopped = why;
				fail(ws, why == 1 ? "Signing in was cancelled." : "the TLS handshake took too long", 0);
				goto failed;
			}
			(void)wait_socket(ws, result == MBEDTLS_ERR_SSL_WANT_WRITE, SLICE);
			continue;
		}
		{
			unsigned int flags = mbedtls_ssl_get_verify_result(&ws->ssl);

			if (flags && flags != (unsigned int)-1)
			{
				char reason[200];

				mbedtls_x509_crt_verify_info(reason, sizeof(reason), "", flags);
				reason[strcspn(reason, "\n")] = 0;
				snprintf(ws->error, sizeof(ws->error), "the server's certificate does not chain to the pinned roots (%s)",
					reason);
			}
			else
				fail(ws, "the TLS handshake failed", result);
		}
		goto failed;
	}
	ws->tls = 1;
	/* the upgrade: a key of 16 random bytes in base64 */
	random_bytes(raw, sizeof(raw));
	for (at = 0; at + 3 <= sizeof(raw); at += 3)
	{
		unsigned int v = (unsigned int)raw[at] << 16 | (unsigned int)raw[at + 1] << 8 | raw[at + 2];

		key[key_length++] = alphabet[v >> 18];
		key[key_length++] = alphabet[(v >> 12) & 63];
		key[key_length++] = alphabet[(v >> 6) & 63];
		key[key_length++] = alphabet[v & 63];
	}
	{
		unsigned int v = (unsigned int)raw[15] << 16;

		key[key_length++] = alphabet[v >> 18];
		key[key_length++] = alphabet[(v >> 12) & 63];
		key[key_length++] = '=';
		key[key_length++] = '=';
	}
	key[key_length] = 0;
	if (!ae_ws_request(request, sizeof(request), host, port, path, key))
	{
		fail(ws, "the upgrade request does not fit", 0);
		goto failed;
	}
	if (!tls_write_all(ws, (unsigned char const *)request, strlen(request), stop, deadline, has_deadline, stopped))
	{
		if (*stopped)
			fail(ws, *stopped == 1 ? "Signing in was cancelled." : "the connection took too long", 0);
		goto failed;
	}
	/* the answer's head; what follows it stays for the frames */
	for (;;)
	{
		char *end;
		int got;

		head[head_size] = 0;
		end = strstr(head, "\r\n\r\n");
		if (end)
		{
			size_t used = (size_t)(end - head) + 4;

			if (head_size > used && !buffer_reserve(&ws->in, &ws->in_capacity, 0, head_size - used))
			{
				fail(ws, "out of memory", 0);
				goto failed;
			}
			if (head_size > used)
			{
				memcpy(ws->in, head + used, head_size - used);
				ws->in_size = head_size - used;
			}
			head[used] = 0;
			break;
		}
		if (head_size >= HEAD_LIMIT)
		{
			fail(ws, "Steam's answer to the upgrade was too long", 0);
			goto failed;
		}
		got = mbedtls_ssl_read(&ws->ssl, (unsigned char *)head + head_size, HEAD_LIMIT - head_size);
		if (got > 0)
		{
			head_size += (size_t)got;
			continue;
		}
		if (got == MBEDTLS_ERR_SSL_WANT_READ || got == MBEDTLS_ERR_SSL_WANT_WRITE ||
			got == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
		{
			why = ended(stop, deadline, has_deadline);
			if (why)
			{
				*stopped = why;
				fail(ws, why == 1 ? "Signing in was cancelled." : "the server did not answer the upgrade in time", 0);
				goto failed;
			}
			(void)wait_socket(ws, 0, SLICE);
			continue;
		}
		fail(ws, "the server closed the connection at the upgrade", got < 0 ? got : 0);
		goto failed;
	}
	if (!ae_ws_response_ok(head, key))
	{
		int status = 0;

		(void)sscanf(head, "HTTP/%*d.%*d %d", &status);
		snprintf(ws->error, sizeof(ws->error), "the server did not accept the WebSocket (answered %d)", status);
		goto failed;
	}
	return ws;

failed:
	snprintf(error, (size_t)error_size, "%s", ws->error[0] ? ws->error : "the WebSocket could not be opened");
	connection_free(ws);
	free(ws);
	return NULL;
}

/* ---------- messages */

int ae_steam_ws_send(struct ae_steam_ws *ws, unsigned char const *data, size_t size, int (*stop)(void),
	unsigned long budget_ms)
{
	unsigned char mask[4];
	unsigned char *frame = malloc(14 + size);
	size_t length;
	int stopped = 0, ok;

	if (!frame)
		return 0;
	random_bytes(mask, sizeof(mask));
	length = ae_ws_encode(frame, 14 + size, AE_WS_BINARY, data, size, mask);
	ok = length && tls_write_all(ws, frame, length, stop, steam_milliseconds() + (budget_ms ? budget_ms : SEND_MILLISECONDS),
		1, &stopped);
	/* (the frame is the message with a mask on it: wiped, as the TCP route's plain bytes are) */
	steam_wipe(frame, 14 + size);
	free(frame);
	return ok;
}

/* the first n bytes of the input taken off, the bytes they were wiped */
static void input_take(struct ae_steam_ws *ws, size_t n)
{
	memmove(ws->in, ws->in + n, ws->in_size - n);
	steam_wipe(ws->in + ws->in_size - n, n);
	ws->in_size -= n;
}

/* more bytes into ws->in: 1 some, 0 none now, -1 the connection ended */
static int fill(struct ae_steam_ws *ws, unsigned long wait_ms)
{
	int got;

	if (!buffer_reserve(&ws->in, &ws->in_capacity, ws->in_size, ws->in_size + 1))
		return -1;
	if (ws->in_size == ws->in_capacity && !buffer_reserve(&ws->in, &ws->in_capacity, ws->in_size, ws->in_capacity * 2))
		return -1;
	if (!mbedtls_ssl_get_bytes_avail(&ws->ssl) && wait_ms)
		(void)wait_socket(ws, 0, wait_ms);
	got = mbedtls_ssl_read(&ws->ssl, ws->in + ws->in_size, ws->in_capacity - ws->in_size);
	if (got > 0)
	{
		ws->in_size += (size_t)got;
		return 1;
	}
	if (got == MBEDTLS_ERR_SSL_WANT_READ || got == MBEDTLS_ERR_SSL_WANT_WRITE ||
		got == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
		return 0;
	if (got == 0 || got == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || got == BIO_CONN_RESET)
		fail(ws, "the connection to Steam closed", 0);
	else
		fail(ws, "the connection to Steam failed", got);
	return -1;
}

int ae_steam_ws_receive(struct ae_steam_ws *ws, unsigned long wait_ms, int (*stop)(void), unsigned char **data,
	size_t *size, int *stopped)
{
	unsigned long started = steam_milliseconds();

	*data = NULL;
	*size = 0;
	*stopped = 0;
	for (;;)
	{
		struct ae_ws_frame frame;
		long whole = ae_ws_decode(ws->in, ws->in_size, MAXIMUM_MESSAGE, &frame);
		unsigned long waited;
		int got;

		if (whole < 0)
		{
			fail(ws, "Steam sent something that is not a WebSocket frame", 0);
			return -1;
		}
		if (whole > 0)
		{
			unsigned char *payload = ws->in + frame.header_size;

			if (frame.masked)
				ae_ws_unmask(payload, frame.payload_size, payload - 4);
			if (frame.opcode == AE_WS_CLOSE)
			{
				fail(ws, "Steam closed the connection", 0);
				return -1;
			}
			if (frame.opcode == AE_WS_PING)
			{
				unsigned char mask[4], pong[140];
				size_t length;
				int ignored = 0;

				random_bytes(mask, sizeof(mask));
				length = ae_ws_encode(pong, sizeof(pong), AE_WS_PONG, payload, frame.payload_size, mask);
				if (length)
					(void)tls_write_all(ws, pong, length, stop, steam_milliseconds() + 5000, 1, &ignored);
			}
			else if (frame.opcode == AE_WS_BINARY || frame.opcode == AE_WS_CONTINUATION)
			{
				if (frame.opcode == AE_WS_BINARY && ws->message_open)
				{
					fail(ws, "Steam sent a message inside a message", 0);
					return -1;
				}
				if (frame.opcode == AE_WS_CONTINUATION && !ws->message_open)
				{
					fail(ws, "Steam sent a continuation with nothing before it", 0);
					return -1;
				}
				if (ws->message_size + frame.payload_size > MAXIMUM_MESSAGE ||
					!buffer_reserve(&ws->message, &ws->message_capacity, ws->message_size,
						ws->message_size + frame.payload_size + 1))
				{
					fail(ws, "Steam sent too large a message", 0);
					return -1;
				}
				memcpy(ws->message + ws->message_size, payload, frame.payload_size);
				ws->message_size += frame.payload_size;
				ws->message_open = !frame.fin;
				if (frame.fin)
				{
					*data = ws->message;
					*size = ws->message_size;
					ws->message = NULL;
					ws->message_size = ws->message_capacity = 0;
					input_take(ws, (size_t)whole);
					return 1;
				}
			}
			input_take(ws, (size_t)whole);
			continue;
		}
		if (stop && stop())
		{
			*stopped = 1;
			return 0;
		}
		waited = steam_milliseconds() - started;
		if (waited >= wait_ms)
			return 0;
		got = fill(ws, wait_ms - waited < SLICE ? wait_ms - waited : SLICE);
		if (got < 0)
			return -1;
	}
}

#endif
