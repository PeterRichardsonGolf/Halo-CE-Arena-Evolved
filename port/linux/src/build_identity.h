/*
BUILD_IDENTITY.H

Which Arena Evolved build this is (ChupathingyCE's file, with Arena
Evolved's name: halo_product.h), for its logs and its requests: the
version, channel and commit the build was given (tools/version.py), and the
platform it was compiled for. build_identity.c has it; plain C, for the
platform layer and the system side (posix_*.c, win32_*.c) alike.
*/

#ifndef BUILD_IDENTITY_H
#define BUILD_IDENTITY_H

/* the start of each line of the platform layer's log (platform_log:
standard error, debug.txt; Arena Evolved's) */
#define PLATFORM_LOG_PREFIX "arena-evolved: "

/* "Halo CE: Arena Evolved 0.1.0-beta (release, release config, commit
4f878c8e, built 2026-10-07) Linux x64; ChupathingyCE 0.7.1d, OpenCE
build-157" (HALO_PRODUCT_NAME, and HALO_UPSTREAM_BASE after it) */
const char *build_identity(void);

/* the User-Agent of this build's own requests: "ArenaEvolved/0.1.0-beta
(Linux x64)" */
const char *build_identity_user_agent(void);

/* its parts, as build_identity has them: "0.7.0b", "release" (or nightly,
dev, custom), "61eb5615", "macOS" (Windows, Linux, Android), "arm64" (x64,
x86) */
const char *build_identity_version(void);
const char *build_identity_channel(void);
const char *build_identity_commit(void);
const char *build_identity_platform(void);
const char *build_identity_architecture(void);

/* the log's header, first in debug.txt for this run (before the game's own
"halobeta xbox" line) and on standard error: build_identity, then the
network's numbers (OpenCE network version, Delta, the legacy table,
network.protocol) */
void build_identity_log(void);

#endif
