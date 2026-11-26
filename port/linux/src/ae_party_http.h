/*
AE_PARTY_HTTP.H

The party client's one kind of request: a POST of a JSON body to the party service (every call of wire 2). The same function
for the desktop builds (update_https_request, Mbed TLS), Android (the app's host) and the web build (none).
*/

#ifndef __AE_PARTY_HTTP_H
#define __AE_PARTY_HTTP_H

/* POSTs json (Content-Type: application/json) to url, https:// or http:// to this machine: 1 when an answer came, with its status
and its body (malloc'd, NUL terminated: the caller frees it); 0 when none did. No query string, no redirect */
int ae_party_http_post(const char *url, const char *json, int *status, char **body, unsigned long *size);

#endif
