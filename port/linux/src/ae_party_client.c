/*
AE_PARTY_CLIENT.C

See ae_party_client.h. The thread owns every request; the game's thread queues calls and copies the newest view under a lock.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ae_party_client.h"
#include "ae_worker.h"
#include "ae_party_http.h"
#include "ae_party_key.h"
#include "../game/ae_party_follow_rules.h"
#include "port_config.h"
#include "halo_port_limits.h"

#ifndef HALO_VERSION
#define HALO_VERSION "dev"
#endif

void platform_log(const char *format, ...);

enum { QUEUE = 8, CALL_TEXT = 700, URL = 1100, BODY = 1100 };

static struct
{
	struct ae_worker_lock *lock;
	struct ae_worker_signal *signal;
	int started, stopping, state;
	char name[AE_PC_NAME];
	int seats, identity_changed;
	char sid[40];
	char code[AE_FRIEND_CODE_SIZE];
	long rev;
	unsigned long generation, last_generation;
	struct ae_pc_view view;
	int have_view; /* a view arrived in this session: cleared when the session ends */
	char queue[QUEUE][CALL_TEXT];
	int queued;
	char last[200];
	int autojoin;
	int poll_ms;                    /* the service's poll interval (its hello's config), 0 until one is heard */
} client = { .name = "Player", .seats = 1 };

int ae_party_client_enabled(void)
{
	const char *base = config_string("network.party_service");

	return base && base[0];
}

int ae_party_client_service_local(void)
{
	char origin[AE_PC_ORIGIN];

	return ae_pc_origin(config_string("network.party_service"), origin, sizeof(origin)) && ae_follow_service_is_loopback(origin);
}

static void hex(const unsigned char *bytes, int size, char *text)
{
	static const char digits[] = "0123456789abcdef";
	int index;

	for (index = 0; index < size; index++)
	{
		text[2 * index] = digits[bytes[index] >> 4];
		text[2 * index + 1] = digits[bytes[index] & 15];
	}
	text[2 * size] = 0;
}

int ae_party_client_code(char *code, int size)
{
	unsigned char pub[32];
	char text[AE_FRIEND_CODE_SIZE];

	if (!ae_party_key_public(pub) || !ae_friend_code_from_key(pub, text))
	{
		if (size)
			code[0] = 0;
		return 0;
	}
	snprintf(code, (size_t)size, "%s", text);
	return 1;
}

/* a POST of the JSON body to url: its parsed answer and status, 0 when no answer came */
static int ask(const char *url, const char *body, struct ae_pc_view *reply, int *status)
{
	char *answer = NULL;
	unsigned long size = 0;
	int ok;

	*status = 0;
	if (!ae_party_http_post(url, body, status, &answer, &size))
		return 0;
	ok = ae_pc_view_parse(answer, (int)size, reply);
	free(answer);
	return ok;
}

/* "<op>" and a call's body: the session first, then the pairs queued with it */
static int session_body(char *out, int size, const char *sid, const char *queued)
{
	out[0] = 0;
	if (!ae_pc_body_add(out, size, "sid", sid))
		return 0;
	if (queued && queued[0] == '{' && queued[1] != '}')
	{
		int length = (int)strlen(out), rest = (int)strlen(queued + 1);

		if (length - 1 + 1 + rest + 1 > size)
			return 0;
		out[length - 1] = ',';
		memcpy(out + length, queued + 1, (size_t)rest + 1);
	}
	return 1;
}

/* a challenge, then the hello signed over this service's address and the challenge's nonce */
static int say_hello(struct ae_pc_view *reply)
{
	unsigned char pub[32], signature[64];
	char pub_hex[65], signature_hex[129], text[260], url[URL], body[BODY], number[16], origin[AE_PC_ORIGIN];
	const char *base = config_string("network.party_service");
	char name[AE_PC_NAME];
	int status, seats;

	if (!ae_party_key_public(pub) || !ae_pc_origin(base, origin, sizeof(origin)))
		return 0;
	hex(pub, 32, pub_hex);
	if (!ae_pc_url(url, sizeof(url), base, "challenge"))
		return 0;
	if (!ask(url, "{}", reply, &status) || !reply->ok || strlen(reply->nonce) != 32)
		return 0;
	ae_pc_hello_text(origin, reply->nonce, pub_hex, text, sizeof(text));
	if (!ae_party_key_sign(text, signature))
		return 0;
	hex(signature, 64, signature_hex);
	ae_worker_lock_take(client.lock);
	snprintf(name, sizeof(name), "%s", client.name);
	seats = client.seats;
	client.identity_changed = 0;
	ae_worker_lock_give(client.lock);
	body[0] = 0;
	ae_pc_body_add(body, sizeof(body), "key", pub_hex);
	ae_pc_body_add(body, sizeof(body), "nonce", reply->nonce);
	ae_pc_body_add(body, sizeof(body), "sig", signature_hex);
	ae_pc_body_add(body, sizeof(body), "name", name);
	snprintf(number, sizeof(number), "%d", seats);
	ae_pc_body_add(body, sizeof(body), "seats", number);
	ae_pc_body_add(body, sizeof(body), "build", HALO_VERSION);
	snprintf(number, sizeof(number), "%d", HALO_PORT_NETWORK_VERSION);
	ae_pc_body_add(body, sizeof(body), "net", number);
	snprintf(number, sizeof(number), "%d", AE_PC_WIRE);
	ae_pc_body_add(body, sizeof(body), "wire", number);
	if (!ae_pc_url(url, sizeof(url), base, "hello"))
		return 0;
	memset(reply, 0, sizeof(*reply));
	return ask(url, body, reply, &status) && reply->ok && reply->sid[0];
}

static void set_last(const char *op, const struct ae_pc_view *reply, int answered)
{
	ae_worker_lock_take(client.lock);
	if (!answered)
		snprintf(client.last, sizeof(client.last), "%s: no answer", op);
	else if (reply->ok)
		snprintf(client.last, sizeof(client.last), "%s: ok", op);
	else
		snprintf(client.last, sizeof(client.last), "%s: refused %s%s%s", op, reply->error, reply->detail[0] ? ": " : "", reply->detail);
	client.last_generation++;
	ae_worker_lock_give(client.lock);
	platform_log("ae party: %s", client.last);
}

static void take_view(const struct ae_pc_view *view)
{
	char summary[120];

	ae_worker_lock_take(client.lock);
	if (view->same)
	{
		ae_worker_lock_give(client.lock);
		return;
	}
	client.view = *view;
	client.rev = view->rev;
	client.generation++;
	client.have_view = 1;
	ae_worker_lock_give(client.lock);
	ae_pc_view_summary(view, summary, sizeof(summary));
	platform_log("ae party: view %s", summary);
}

static void set_state(int state)
{
	ae_worker_lock_take(client.lock);
	client.state = state;
	ae_worker_lock_give(client.lock);
}

static void drop_session(void)
{
	ae_worker_lock_take(client.lock);
	client.sid[0] = 0;
	client.rev = 0;
	client.have_view = 0;
	memset(&client.view, 0, sizeof(client.view));
	ae_worker_lock_give(client.lock);
}

static void *client_run(void *argument)
{
	static const int backoff[] = { 1, 2, 5, 10, 30 };
	int failures = 0;

	(void)argument;
	for (;;)
	{
		struct ae_pc_view reply;
		char url[URL], body[BODY], sid[40], number[24], pending[CALL_TEXT];
		const char *base = config_string("network.party_service");
		unsigned int seen;
		int status, answered, wait_ms;
		long rev;

		memset(&reply, 0, sizeof(reply));
		ae_worker_lock_take(client.lock);
		if (client.stopping)
		{
			snprintf(sid, sizeof(sid), "%s", client.sid);
			client.sid[0] = 0;
			client.have_view = 0;
			memset(&client.view, 0, sizeof(client.view));
			client.started = client.stopping = 0;
			client.state = AE_PC_OFF;
			ae_worker_lock_give(client.lock);
			if (sid[0] && ae_pc_url(url, sizeof(url), base, "bye") && session_body(body, sizeof(body), sid, NULL))
				ask(url, body, &reply, &status);
			return NULL;
		}
		snprintf(sid, sizeof(sid), "%s", client.sid);
		rev = client.rev;
		ae_worker_lock_give(client.lock);

		if (!sid[0])
		{
			set_state(AE_PC_CONNECTING);
			if (!say_hello(&reply))
			{
				int wait = backoff[failures < 4 ? failures : 4];

				if (failures++ == 0 || failures > 5)
					platform_log("ae party: no hello (%s); trying again in %d s", reply.error[0] ? reply.error : "no answer", wait);
				set_state(AE_PC_BACKOFF);
				ae_worker_signal_wait(client.signal, ae_worker_signal_seen(client.signal), (unsigned)wait * 1000);
				continue;
			}
			failures = 0;
			ae_worker_lock_take(client.lock);
			snprintf(client.sid, sizeof(client.sid), "%s", reply.sid);
			snprintf(client.code, sizeof(client.code), "%s", reply.code);
			client.poll_ms = reply.poll_ms;
			client.rev = 0;
			ae_worker_lock_give(client.lock);
			set_state(AE_PC_UP);
			platform_log("ae party: connected as %s", reply.code);
			continue;
		}

		/* the calls the game queued, then a poll */
		for (;;)
		{
			int index;

			ae_worker_lock_take(client.lock);
			if (!client.queued)
			{
				ae_worker_lock_give(client.lock);
				break;
			}
			snprintf(pending, sizeof(pending), "%s", client.queue[0]);
			for (index = 1; index < client.queued; index++)
				memcpy(client.queue[index - 1], client.queue[index], CALL_TEXT);
			client.queued--;
			ae_worker_lock_give(client.lock);
			/* pending is "op", a space, then the JSON object of the call's pairs */
			{
				char op[40];
				const char *queued = strchr(pending, ' ');
				int op_length = queued ? (int)(queued - pending) : (int)strlen(pending);

				snprintf(op, sizeof(op), "%.*s", op_length, pending);
				if (!ae_pc_url(url, sizeof(url), base, op) || !session_body(body, sizeof(body), sid, queued ? queued + 1 : NULL))
					continue;
				answered = ask(url, body, &reply, &status);
				set_last(op, &reply, answered);
				if (answered && !strcmp(reply.error, "no_session"))
				{
					drop_session();
					break;
				}
				if (answered && reply.ok)
					rev = -1;
			}
		}
		ae_worker_lock_take(client.lock);
		if (!client.sid[0])
		{
			ae_worker_lock_give(client.lock);
			continue;
		}
		if (client.identity_changed)
		{
			/* a changed name goes with a new hello */
			client.sid[0] = 0;
			ae_worker_lock_give(client.lock);
			continue;
		}
		ae_worker_lock_give(client.lock);
		if (!ae_pc_url(url, sizeof(url), base, "poll") || !session_body(body, sizeof(body), sid, NULL))
			continue;
		snprintf(number, sizeof(number), "%ld", rev < 0 ? 0 : rev);
		ae_pc_body_add(body, sizeof(body), "rev", number);
		answered = ask(url, body, &reply, &status);
		if (!answered)
		{
			set_state(AE_PC_BACKOFF);
			ae_worker_signal_wait(client.signal, ae_worker_signal_seen(client.signal), 5000);
			continue;
		}
		set_state(AE_PC_UP);
		if (!reply.ok && !strcmp(reply.error, "no_session"))
		{
			drop_session();
			continue;
		}
		if (reply.ok)
		{
			take_view(&reply);
			if (!reply.same && reply.invite_count && client.autojoin)
				ae_party_client_call("party_answer", "party", reply.invite[0].party, "join", "1", NULL);
		}
		seen = ae_worker_signal_seen(client.signal);
		ae_worker_lock_take(client.lock);
		wait_ms = ae_pc_poll_ms(client.poll_ms, client.view.has_party || client.view.search.present);
		if (client.queued)
			wait_ms = 0;
		ae_worker_lock_give(client.lock);
		if (wait_ms)
			ae_worker_signal_wait(client.signal, seen, (unsigned)wait_ms);
	}
}

int ae_party_client_start(void)
{
	char code[AE_FRIEND_CODE_SIZE];

	if (!ae_party_client_enabled())
		return 0;
	if (!client.lock)
		client.lock = ae_worker_lock_new();
	if (!client.signal)
		client.signal = ae_worker_signal_new();
	if (!client.lock || !client.signal)
		return 0;
	ae_worker_lock_take(client.lock);
	if (client.started)
	{
		ae_worker_lock_give(client.lock);
		return 1;
	}
	client.started = 1;
	client.state = AE_PC_CONNECTING;
	ae_worker_lock_give(client.lock);
	if (!ae_party_client_code(code, sizeof(code)) || !ae_worker_start(client_run, NULL))
	{
		ae_worker_lock_take(client.lock);
		client.started = 0;
		client.state = AE_PC_OFF;
		ae_worker_lock_give(client.lock);
		platform_log("ae party: not started (no party key or no thread)");
		return 0;
	}
	platform_log("ae party: started, service %s, code %s", config_string("network.party_service"), code);
	return 1;
}

void ae_party_client_identity(const char *name, int seats)
{
	if (!client.lock)
		client.lock = ae_worker_lock_new();
	if (!client.lock)
		return;
	ae_worker_lock_take(client.lock);
	if (name && name[0])
		snprintf(client.name, sizeof(client.name), "%s", name);
	if (seats >= 1 && seats <= 4)
		client.seats = seats;
	client.identity_changed = client.started;
	ae_worker_lock_give(client.lock);
	if (client.signal)
		ae_worker_signal_wake(client.signal);
}

int ae_party_client_state(void)
{
	int state;

	if (!client.lock)
		return AE_PC_OFF;
	ae_worker_lock_take(client.lock);
	state = client.state;
	ae_worker_lock_give(client.lock);
	return state;
}

unsigned long ae_party_client_view(struct ae_pc_view *view)
{
	unsigned long generation;

	if (!client.lock)
		return 0;
	ae_worker_lock_take(client.lock);
	generation = client.have_view ? client.generation : 0;
	if (generation && view)
		*view = client.view;
	ae_worker_lock_give(client.lock);
	return generation;
}

int ae_party_client_call(const char *op, ...)
{
	char text[CALL_TEXT];
	va_list arguments;
	const char *key;
	int ok = 1, op_length;

	if (!client.lock || !op || !client.started)
		return 0;
	if (strlen(op) + 1 >= sizeof(text) || strchr(op, ' '))
		return 0;
	op_length = (int)strlen(op);
	snprintf(text, sizeof(text), "%s ", op);
	text[op_length + 1] = 0;
	va_start(arguments, op);
	while ((key = va_arg(arguments, const char *)) != NULL)
	{
		const char *value = va_arg(arguments, const char *);

		if (!ae_pc_body_add(text + op_length + 1, (int)sizeof(text) - op_length - 1, key, value))
		{
			ok = 0;
			break;
		}
	}
	va_end(arguments);
	if (!ok)
		return 0;
	ae_worker_lock_take(client.lock);
	if (client.queued < QUEUE)
		snprintf(client.queue[client.queued++], CALL_TEXT, "%s", text);
	else
		ok = 0;
	ae_worker_lock_give(client.lock);
	if (ok)
		ae_worker_signal_wake(client.signal);
	return ok;
}

unsigned long ae_party_client_last(char *text, int size)
{
	unsigned long generation;

	if (!client.lock)
	{
		if (size)
			text[0] = 0;
		return 0;
	}
	ae_worker_lock_take(client.lock);
	snprintf(text, (size_t)size, "%s", client.last);
	generation = client.last_generation;
	ae_worker_lock_give(client.lock);
	return generation;
}

void ae_party_client_dev_autojoin(int on)
{
	client.autojoin = on != 0;
}

void ae_party_client_stop(void)
{
	if (!client.lock)
		return;
	ae_worker_lock_take(client.lock);
	if (client.started)
		client.stopping = 1;
	ae_worker_lock_give(client.lock);
	if (client.signal)
		ae_worker_signal_wake(client.signal);
}
