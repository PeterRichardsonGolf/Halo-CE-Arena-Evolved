/*
COMMUNITY_NET.C

The community server's requests (community_net.h): a slot each, its request
made on a detached thread of its own (update_https_request: Mbed TLS or WinHTTP,
update.h), its answer
kept in the slot until the game's code picks it up by its ticket. A request
whose ticket was forgotten is let finish, and its answer dropped.

This install's key (community.key) is 32 random bytes as hex, made and
written to config.toml the first time a request is sent; the server keeps
only its hash, and tells players apart by it (database/halo_sql.js).
*/

#include "community_net.h"
#include "ae_community.h"
#include "p2p_internal.h"
#include "ae_worker.h"
#include "platform.h"
#include "port_config.h"
#include "posix.h"
#include "update.h"
#ifdef HALO_GAME_BROWSER
#include "browser_http.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMUNITY_NET_SLOTS 8
#define COMMUNITY_NET_URL_MAXIMUM 1024
#define COMMUNITY_NET_KEY_BYTES 32
/* the whole of a request: connect, handshake and answer */
#define COMMUNITY_NET_DEADLINE_MILLISECONDS 15000

enum
{
	_slot_free = 0,
	_slot_running,
	_slot_done,
	/* (its ticket forgotten: freed by its thread when it is done) */
	_slot_abandoned,
};

struct community_net_slot
{
	int state;
	int ticket;
	int status;
	char url[COMMUNITY_NET_URL_MAXIMUM];
	char headers[256];
	char *body;
	char *answer;
	int answer_size;
	char error[256];
};

static struct ae_worker_lock *community_net_lock_object;
static struct community_net_slot community_net_slots[COMMUNITY_NET_SLOTS];
static int community_net_next_ticket = 1;

char const *community_net_platform(void)
{
#if defined(HALO_WEB)
	return "web";
#elif defined(HALO_SWITCH)
	return "switch";
#elif defined(HALO_ANDROID)
	return "android";
#elif defined(HALO_PS5)
	return "ps5";
#elif defined(_WIN32)
	return "windows";
#else
	return "linux";
#endif
}

int community_net_custom(void)
{
	return !strcmp(config_string("community.server_mode"), "custom");
}

/* the server asked: the default, or community.custom_server's, without a
trailing slash (an address without a scheme taken as https://); FALSE when
it is not https:// */
static int community_net_server(char *server, size_t size)
{
	char const *configured = community_net_custom() ? config_string("community.custom_server") :
		COMMUNITY_NET_DEFAULT_SERVER;
	size_t length;

	while (*configured == ' ')
		configured++;
	if (!strstr(configured, "://"))
		snprintf(server, size, "https://%s", configured);
	else
		snprintf(server, size, "%s", configured);
	length = strlen(server);
	while (length > 8 && (server[length - 1] == '/' || server[length - 1] == ' '))
		server[--length] = 0;
	return !strncmp(server, "https://", 8) && length > 8;
}

void community_net_server_text(char *text, int size)
{
	if (!community_net_server(text, (size_t)size))
	{
		snprintf(text, (size_t)size, "%s", community_net_custom() ? config_string("community.custom_server") :
			COMMUNITY_NET_DEFAULT_SERVER);
	}
}

int community_net_enabled(void)
{
	/* (both the setting and the community services' consent: neither alone sends anything) */
	return config_boolean("community.enabled") && ae_community_services_on();
}

/* this install's key: community.key's, else a new one, written there (64
hex digits; the server takes 32 to 128 letters, digits, - and _) */
static int community_net_key(char *key, size_t size)
{
	char const *configured = config_string("community.key");
	size_t length = strlen(configured);
	size_t index;
	int valid = length >= 32 && length <= 128;

	for (index = 0; valid && index < length; index++)
	{
		char c = configured[index];

		valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
	}
	if (!valid)
	{
		unsigned char bytes[COMMUNITY_NET_KEY_BYTES];
		char made[COMMUNITY_NET_KEY_BYTES * 2 + 1];

		posix_random_bytes(bytes, sizeof(bytes));
		for (index = 0; index < sizeof(bytes); index++)
			snprintf(made + index * 2, 3, "%02x", bytes[index]);
		if (!config_write("community.key", made))
		{
			platform_log("community: could not write community.key");
			return 0;
		}
		configured = config_string("community.key");
	}
	snprintf(key, size, "%s", configured);
	return 1;
}

/* the key a server is sent: the install's key to the default server; to any other server (a custom one) a key made from the install's
and that server's address by HMAC-SHA256 (64 hex digits), so it tells the player apart on its own and never holds the key the
default server knows */
static int community_net_key_for(char const *server, char *key, size_t size)
{
	char install[160], message[COMMUNITY_NET_URL_MAXIMUM + 32];
	unsigned char digest[P2P_SHA256_SIZE];
	size_t index;

	if (!community_net_key(install, sizeof(install)))
		return 0;
	if (!strcmp(server, COMMUNITY_NET_DEFAULT_SERVER))
	{
		snprintf(key, size, "%s", install);
		return 1;
	}
	if (size < P2P_SHA256_SIZE * 2 + 1)
		return 0;
	snprintf(message, sizeof(message), "community-server-key:%s", server);
	p2p_hmac_sha256((unsigned char const *)install, (int)strlen(install), message, (int)strlen(message), digest);
	for (index = 0; index < sizeof(digest); index++)
		snprintf(key + index * 2, 3, "%02x", digest[index]);
	return 1;
}

static void community_net_slot_free(struct community_net_slot *slot)
{
	free(slot->body);
	free(slot->answer);
	memset(slot, 0, sizeof(*slot));
}

/* the lock taken (made the first time: the game's thread asks before any worker runs) */
static void community_net_lock_take(void)
{
	if (!community_net_lock_object)
		community_net_lock_object = ae_worker_lock_new();
	ae_worker_lock_take(community_net_lock_object);
}

/* one request made, the answer's body kept in the slot (status 0 and why in error when none came) */
static int community_net_make(struct community_net_slot *slot)
{
#if defined(HALO_WEB) || defined(HALO_ANDROID)
	/* (the web build and the Android app make no request of their own here yet: the challenge plays offline) */
	snprintf(slot->error, sizeof(slot->error), "online boards are not in this build");
	return 0;
#else
	struct update_response response;
	int status;

	/* (update_https_request takes the Mbed TLS lock itself (ae_https_guard.c): taking it here too would wait on this
	thread's own lock forever. The deadline is this thread's: the wait for the lock counts toward it, so a slow server
	holds the lock for this long at most, whatever it sends) */
	update_https_deadline(COMMUNITY_NET_DEADLINE_MILLISECONDS);
	status = update_https_request(slot->body ? "POST" : "GET", slot->url, slot->headers, slot->body,
		slot->body ? (unsigned long)strlen(slot->body) : 0, (unsigned long)(slot->answer_size - 1), &response,
		slot->error, (int)sizeof(slot->error));
	update_https_deadline(0);
	if (!status)
		return 0;
	if (response.body)
		snprintf(slot->answer, (size_t)slot->answer_size, "%s", response.body);
	free(response.body);
	return response.status > 0 ? response.status : 0;
#endif
}

static void *community_net_thread(void *context)
{
	struct community_net_slot *slot = context;
	int status = community_net_make(slot);

	community_net_lock_take();
	slot->status = status;
	if (slot->state == _slot_abandoned)
		community_net_slot_free(slot);
	else
		slot->state = _slot_done;
	ae_worker_lock_give(community_net_lock_object);
	return NULL;
}

/* a request in a free slot, on a thread of its own: its ticket, 0 for none */
static int community_net_start(const char *url, const char *headers, const char *body, int answer_size)
{
	struct community_net_slot *slot = NULL;
	int index;
	int ticket;

	community_net_lock_take();
	for (index = 0; index < COMMUNITY_NET_SLOTS && !slot; index++)
	{
		if (community_net_slots[index].state == _slot_free)
			slot = &community_net_slots[index];
	}
	if (!slot)
	{
		ae_worker_lock_give(community_net_lock_object);
		platform_log("community: too many requests at once; %s not sent", url);
		return 0;
	}
	slot->answer = malloc((size_t)answer_size);
	slot->answer_size = answer_size;
	slot->body = body ? strdup(body) : NULL;
	if (!slot->answer || (body && !slot->body))
	{
		community_net_slot_free(slot);
		ae_worker_lock_give(community_net_lock_object);
		return 0;
	}
	slot->answer[0] = 0;
	snprintf(slot->url, sizeof(slot->url), "%s", url);
	snprintf(slot->headers, sizeof(slot->headers), "%s", headers);
	ticket = slot->ticket = community_net_next_ticket++;
	if (community_net_next_ticket <= 0)
		community_net_next_ticket = 1;
	slot->state = _slot_running;
	if (!ae_worker_start(community_net_thread, slot))
	{
		community_net_slot_free(slot);
		ae_worker_lock_give(community_net_lock_object);
		platform_log("community: could not start a request's thread");
		return 0;
	}
	ae_worker_lock_give(community_net_lock_object);
	return ticket;
}

int community_net_address(char *server, int size)
{
	return size > 0 && community_net_server(server, (size_t)size);
}

int community_net_send(const char *path, const char *body, const char *player)
{
	char server[COMMUNITY_NET_URL_MAXIMUM];
	char url[COMMUNITY_NET_URL_MAXIMUM];
	char headers[256];
	char key[160];

	if (!community_net_enabled() || !community_net_server(server, sizeof(server)) ||
		!community_net_key_for(server, key, sizeof(key)))
	{
		return 0;
	}
	snprintf(url, sizeof(url), "%s%s", server, path);
	/* (a profile's own: the install's key and the profile's, as the server
	tells players apart) */
	snprintf(headers, sizeof(headers), "%sX-Halo-Key: %s%s%.16s\r\n",
		body ? "Content-Type: application/json\r\n" : "", key, player && player[0] ? "-" : "", player ? player : "");
	return community_net_start(url, headers, body, COMMUNITY_NET_ANSWER_MAXIMUM);
}

/* (the web build's page-close beacon: this build sends none) */
void community_net_close_beacon(const char *path)
{
	(void)path;
}

static struct community_net_slot *community_net_slot_of(int ticket)
{
	int index;

	for (index = 0; ticket > 0 && index < COMMUNITY_NET_SLOTS; index++)
	{
		struct community_net_slot *slot = &community_net_slots[index];

		if (slot->ticket == ticket && (slot->state == _slot_running || slot->state == _slot_done))
			return slot;
	}
	return NULL;
}

int community_net_poll(int ticket, int *status, char *answer, int size)
{
	struct community_net_slot *slot;
	int result;

	community_net_lock_take();
	slot = community_net_slot_of(ticket);
	if (!slot)
		result = _community_net_unknown;
	else if (slot->state == _slot_running)
		result = _community_net_waiting;
	else
	{
		if (status)
			*status = slot->status;
		if (answer && size > 0)
			snprintf(answer, (size_t)size, "%s", slot->status ? slot->answer : slot->error);
		result = slot->status ? _community_net_answered : _community_net_failed;
		if (!slot->status)
			platform_log("community: %s: %s", slot->url, slot->error);
		community_net_slot_free(slot);
	}
	ae_worker_lock_give(community_net_lock_object);
	return result;
}

void community_net_forget(int ticket)
{
	struct community_net_slot *slot;

	community_net_lock_take();
	slot = community_net_slot_of(ticket);
	if (slot && slot->state == _slot_running)
		slot->state = _slot_abandoned;
	else if (slot)
		community_net_slot_free(slot);
	ae_worker_lock_give(community_net_lock_object);
}
