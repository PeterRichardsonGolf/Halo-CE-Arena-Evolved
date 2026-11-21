/*
PTHREAD.H (Mbed TLS's)

Mbed TLS's threading.h (MBEDTLS_THREADING_PTHREAD) includes <pthread.h>. The Windows build gives the library's units this
one folder with that header alone, so they see the platform's pthread shim (port/windows/include/posix/pthread.h) and
none of the other POSIX shims beside it, which would shadow the Windows SDK's own headers.
*/
#include "../posix/pthread.h"
