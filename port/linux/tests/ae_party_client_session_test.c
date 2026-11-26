/* The party client's view across a session (ae_party_client.c, with the service's answers stubbed): none before a session,
the service's party once polled, none again after stop (a post-game CONTINUE must not divert on an old view). */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ae_party_client.h"
#include "ae_worker.h"

#undef vsnprintf

/* (the game's headers rename snprintf) */
int halo_linux_snprintf(char *buffer, size_t size, const char *format, ...)
{
	va_list arguments;
	int count;

	va_start(arguments, format);
	count = vsnprintf(buffer, size, format, arguments);
	va_end(arguments);
	return count;
}

void platform_log(const char *format, ...) { (void)format; }
const char *config_string(const char *name) { (void)name; return "http://127.0.0.1:1/party"; }
int ae_party_key_public(unsigned char *public_key) { memset(public_key, 7, 32); return 1; }
int ae_party_key_sign(const char *text, unsigned char *signature) { (void)text; memset(signature, 1, 64); return 1; }

int ae_party_http_post(const char *url, const char *json, int *status, char **body, unsigned long *size)
{
	const char *answer = "{\"ok\":true}";

	/* (every call is a POST of a JSON body with the session in it; none has a query) */
	assert(!strchr(url, '?') && json[0] == '{');
	if (strstr(url, "/challenge"))
		answer = "{\"nonce\":\"0123456789abcdef0123456789abcdef\",\"expires_s\":60}";
	else if (strstr(url, "/hello"))
	{
		assert(strstr(json, "\"nonce\":\"0123456789abcdef0123456789abcdef\"") && strstr(json, "\"wire\":\"2\""));
		answer = "{\"sid\":\"abc\",\"code\":\"ZZZZZ\"}";
	}
	else if (strstr(url, "/poll"))
	{
		assert(strstr(json, "\"sid\":\"abc\""));
		answer = "{\"rev\":1,\"party\":{\"id\":\"p1\",\"leader\":\"ZZZZ\",\"seats\":1,\"members\":[]}}";
	}
	*status = 200;
	*body = malloc(strlen(answer) + 1);
	strcpy(*body, answer);
	*size = (unsigned long)strlen(answer);
	return 1;
}

static int wait_for(int (*done)(void))
{
	int tries;

	for (tries = 0; tries < 400; tries++)
	{
		if (done())
			return 1;
		ae_worker_sleep(10);
	}
	return 0;
}

static int has_view(void) { struct ae_pc_view view; return ae_party_client_view(&view) != 0; }
static int no_view(void) { return !has_view(); }
static int is_off(void) { return ae_party_client_state() == AE_PC_OFF; }

int main(void)
{
	struct ae_pc_view view;

	assert(ae_party_client_view(&view) == 0);
	assert(ae_party_client_start());
	assert(wait_for(has_view));
	assert(ae_party_client_view(&view) && view.has_party);
	ae_party_client_stop();
	assert(wait_for(is_off));
	assert(no_view());
	puts("ae_party_client_session_test: ok");
	return 0;
}
