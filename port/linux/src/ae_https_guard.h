/*
AE_HTTPS_GUARD.H

Two helpers for update_https_request (posix_update.c): the request made in turn with the other users of Mbed TLS (the game
list, the downloads, the key's first making), and an address shortened for the log (no query string, no fragment).
*/

#ifndef __AE_HTTPS_GUARD_H
#define __AE_HTTPS_GUARD_H

/* the address up to its query string or fragment, in a buffer of this thread's own (valid until its next call) */
const char *ae_https_log_address(const char *url);

#endif
