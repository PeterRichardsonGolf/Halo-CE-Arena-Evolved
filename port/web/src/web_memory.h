/*
WEB_MEMORY.H

The browser build's memory size, for mcc_memory.c's MCC map window (upstream
keeps it in its own web_memory.c; this build's browser memory layout is
web_main.c's, and it plays no MCC maps: the window is never made). As
xbox_memory.c's: the WebAssembly memory's size in bytes.
*/

#ifndef HALO_WEB_MEMORY_H
#define HALO_WEB_MEMORY_H

#include <stddef.h>

static inline size_t web_memory_bytes(void)
{
	return (size_t)__builtin_wasm_memory_size(0) << 16;
}

#endif
