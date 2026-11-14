/*
AE_MOTD_FETCH.C

See ae_motd_fetch.h. The worker's result is handed over under a lock; the game's thread copies it out.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ae_motd_fetch.h"
#include "ae_worker.h"
#include "master_list.h"
#include "../game/ae_motd_rules.h"

void platform_log(const char *format, ...);

enum { FETCH_IDLE, FETCH_ASKING, FETCH_DONE };

static struct
{
	struct ae_worker_lock *lock;
	int state, have, offline;
	struct ae_motd motd;
	char body[AE_MOTD_TEXT_LIMIT * 2 + 512];
	char signature[132];
} fetch;

static void *fetch_run(void *argument)
{
	static char urls[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_URL_SIZE];
	static unsigned char keys[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_KEY_SIZE];
	int count = master_list_client_masters(urls, keys), index, have = 0;
	struct ae_motd motd;

	(void)argument;
	for (index = 0; index < count && !have; index++)
	{
		char url[MASTER_LIST_URL_SIZE + 16];
		char *body = NULL, *signature = NULL;
		unsigned long size = 0, signature_size = 0;
		int status = 0, why;

		snprintf(url, sizeof(url), "%s/v1/motd", urls[index]);
		if (!master_list_get(url, &status, &body, &size) || status != 200 || !body)
		{
			free(body);
			continue;
		}
		snprintf(url, sizeof(url), "%s/v1/motd.sig", urls[index]);
		if (!master_list_get(url, &status, &signature, &signature_size) || status != 200 || !signature)
		{
			free(body);
			free(signature);
			continue;
		}
		while (signature_size && (signature[signature_size - 1] == '\n' || signature[signature_size - 1] == '\r' ||
			signature[signature_size - 1] == ' '))
		{
			signature[--signature_size] = 0;
		}
		why = ae_motd_check(body, size, signature, keys[index], (unsigned long)time(NULL), &motd);
		if (why == AE_MOTD_OK && size < sizeof(fetch.body))
		{
			ae_worker_lock_take(fetch.lock);
			fetch.motd = motd;
			memcpy(fetch.body, body, size);
			fetch.body[size] = 0;
			snprintf(fetch.signature, sizeof(fetch.signature), "%s", signature);
			fetch.have = have = 1;
			ae_worker_lock_give(fetch.lock);
			platform_log("ae motd: fetched rev %ld", motd.rev);
		}
		else
			platform_log("ae motd: rejected (%s)", why == AE_MOTD_BAD_SIGNATURE ? "signature" : why == AE_MOTD_EXPIRED ?
				"expired" : "malformed");
		free(body);
		free(signature);
	}
	if (!have)
		platform_log("ae motd: no answer");
	ae_worker_lock_take(fetch.lock);
	fetch.state = FETCH_DONE;
	ae_worker_lock_give(fetch.lock);
	return NULL;
}

void ae_motd_fetch_start(void)
{
	if (fetch.state != FETCH_IDLE)
		return;
	if (!fetch.lock)
		fetch.lock = ae_worker_lock_new();
	if (fetch.offline || !fetch.lock || getenv("HALO_AE_MOTD_OFFLINE"))
	{
		fetch.state = FETCH_DONE;
		platform_log("ae motd: offline");
		return;
	}
	fetch.state = FETCH_ASKING;
	if (!ae_worker_start(fetch_run, NULL))
		fetch.state = FETCH_DONE;
}

int ae_motd_fetch_result(struct ae_motd *out, char *body, int body_size, char *signature, int signature_size)
{
	int result;

	if (!fetch.lock)
		return fetch.state == FETCH_DONE ? 0 : -1;
	ae_worker_lock_take(fetch.lock);
	result = fetch.state != FETCH_DONE ? -1 : fetch.have ? 1 : 0;
	if (result == 1)
	{
		*out = fetch.motd;
		if (body)
			snprintf(body, (size_t)body_size, "%s", fetch.body);
		if (signature)
			snprintf(signature, (size_t)signature_size, "%s", fetch.signature);
	}
	ae_worker_lock_give(fetch.lock);
	return result;
}

int ae_motd_fetch_check_saved(const char *body, const char *signature, struct ae_motd *out)
{
	static char urls[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_URL_SIZE];
	static unsigned char keys[MASTER_LIST_MAXIMUM_MASTERS][MASTER_LIST_KEY_SIZE];
	int count = master_list_client_masters(urls, keys), index, why = AE_MOTD_BAD_SIGNATURE;

	for (index = 0; index < count; index++)
	{
		why = ae_motd_check(body, strlen(body), signature, keys[index], (unsigned long)time(NULL), out);
		if (why != AE_MOTD_BAD_SIGNATURE)
			return why;
	}
	return why;
}

void ae_motd_fetch_offline(void)
{
	fetch.offline = 1;
	if (fetch.state == FETCH_ASKING && fetch.lock)
	{
		ae_worker_lock_take(fetch.lock);
		fetch.have = 0;
		ae_worker_lock_give(fetch.lock);
	}
	platform_log("ae motd: offline (motd_offline)");
}
