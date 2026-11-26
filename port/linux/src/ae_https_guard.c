/*
AE_HTTPS_GUARD.C

posix_update.c names its request function ae_update_https_request_unlocked (a define at its top); update_https_request is made
here: it takes the game browser's TLS lock around the request, as the game list's requests and the downloads do (Mbed TLS is
built without threading support: two threads setting up its PSA crypto or drawing on its random generator at once crash it).
No caller of update_https_request holds that lock when it calls (the lock is held only inside posix_browser.c's own requests,
update_download and the player key's making, none of which call it).
*/

#include <stdio.h>
#include <string.h>

#include "update.h"
#include "ae_https_guard.h"
#ifdef HALO_GAME_BROWSER
#include "browser_http.h"
#endif

#ifdef _MSC_VER
#define AE_THREAD_LOCAL __declspec(thread)
#else
#define AE_THREAD_LOCAL __thread
#endif

int ae_update_https_request_unlocked(const char *method, const char *url, const char *headers, const void *body,
	unsigned long body_size, unsigned long maximum, struct update_response *response, char *error, int error_size);

/* (the Windows and web builds have their own update_https_request: only posix_update.c's is renamed) */
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
int update_https_request(const char *method, const char *url, const char *headers, const void *body, unsigned long body_size,
	unsigned long maximum, struct update_response *response, char *error, int error_size)
{
	int succeeded;

#ifdef HALO_GAME_BROWSER
	posix_browser_lock();
#endif
	succeeded = ae_update_https_request_unlocked(method, url, headers, body, body_size, maximum, response, error, error_size);
#ifdef HALO_GAME_BROWSER
	posix_browser_unlock();
#endif
	return succeeded;
}
#endif

const char *ae_https_log_address(const char *url)
{
	static AE_THREAD_LOCAL char shown[256];
	size_t length = strcspn(url, "?#");

	if (length >= sizeof(shown))
		length = sizeof(shown) - 1;
	memcpy(shown, url, length);
	shown[length] = 0;
	return shown;
}
