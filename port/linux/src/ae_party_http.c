/*
AE_PARTY_HTTP.C

See ae_party_http.h.
*/

#include <stdlib.h>
#include <string.h>

#include "ae_party_http.h"

enum { MAXIMUM_ANSWER = 65536 };

#if defined(HALO_WEB)

int ae_party_http_post(const char *url, const char *json, int *status, char **body, unsigned long *size)
{
	(void)url;
	(void)json;
	*status = 0;
	*body = NULL;
	*size = 0;
	return 0;
}

#elif defined(HALO_ANDROID)

/* the app's host (port/android/host/host_https.c) */
int host_https_post(const char *url, const char *json, int *status, char *body, int body_capacity, int *body_size, char *error,
	int error_size);

int ae_party_http_post(const char *url, const char *json, int *status, char **body, unsigned long *size)
{
	char error[256];
	int body_size = 0;

	*status = 0;
	*body = malloc(MAXIMUM_ANSWER + 1);
	*size = 0;
	if (!*body || !host_https_post(url, json, status, *body, MAXIMUM_ANSWER, &body_size, error, sizeof(error)))
	{
		free(*body);
		*body = NULL;
		return 0;
	}
	(*body)[body_size] = 0;
	*size = (unsigned long)body_size;
	return 1;
}

#else

#include "update.h"

/* the longest a party request may take, answer included (as the daily challenge's requests) */
#define PARTY_DEADLINE_MILLISECONDS 15000

int ae_party_http_post(const char *url, const char *json, int *status, char **body, unsigned long *size)
{
	struct update_response response;
	char error[256];
	int sent;

	*status = 0;
	*body = NULL;
	*size = 0;
	/* (a whole request, answer included, within PARTY_DEADLINE_MILLISECONDS: a party service that stalls or trickles
	cannot keep the shared TLS lock, which the server browser, map downloads and the updater wait on) */
	update_https_deadline(PARTY_DEADLINE_MILLISECONDS);
	sent = update_https_request("POST", url, "Content-Type: application/json\r\n", json, (unsigned long)strlen(json), MAXIMUM_ANSWER,
		&response, error, sizeof(error));
	update_https_deadline(0);
	if (!sent)
		return 0;
	*status = response.status;
	*body = response.body;
	*size = response.body_size;
	return 1;
}

#endif
