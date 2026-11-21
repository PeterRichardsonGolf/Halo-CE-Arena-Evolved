/*
CAPTURE_CHILD.H

What the capture (capture.c) needs of the system to run ffmpeg and to make
its files: a child process with a pipe to its standard input, a file made
only if it is not there yet, and an earlier session's leftovers cleared.
capture.c has the first two for Linux and macOS (SDL's process API, its
pipe made to wait), posix_capture.c the last; port/windows/src/
win32_capture.c all of them for Windows (CreateProcessW: no console window,
only the pipe inherited). Plain C types only: every side sees this header.

Paths and arguments are UTF-8.
*/

#ifndef __HALO_LINUX_CAPTURE_CHILD_H
#define __HALO_LINUX_CAPTURE_CHILD_H

#include <stddef.h>

struct capture_child;

/* arguments[0] is the program's path, the list ends with NULL; with_input:
a pipe to its standard input (capture_child_write), else none; its output
goes nowhere. NULL if it cannot start. */
struct capture_child *capture_child_start(const char *const *arguments, int with_input);

/* all of data down the pipe, waiting as long as the child takes; 0 once it
cannot (the child closed it, died or was killed) */
int capture_child_write(struct capture_child *child, const void *data, size_t size);

/* the end of the input */
void capture_child_close_input(struct capture_child *child);

/* waits up to timeout_ms (negative: for ever) for the child to end: 1 and
*exit_code once it has, 0 while it still runs */
int capture_child_wait(struct capture_child *child, int timeout_ms, int *exit_code);

/* ends the child at once (this child only); safe while another thread is
in capture_child_write or capture_child_wait */
void capture_child_kill(struct capture_child *child);

/* the handles; the child, if it still runs, is left running */
void capture_child_free(struct capture_child *child);

/* makes the file, empty, only if nothing of that name is there: 1 made, 0
there already, -1 another failure */
int capture_file_reserve(const char *path);

/* 1 if path is a file (not a folder) that can be run */
int capture_file_executable(const char *path);

/* in folder, the files an earlier session of the capture left: owned(name)
says which names are the capture's (0 not, 1 its, 2 its only while empty);
of those, a regular file (never a link, a folder or a device) untouched
for stale_seconds is removed. The folder is opened once and refused if it
is a link (or, on Windows, any reparse point); each file is looked at and
removed through it, never following a link. Anything unsure is kept. The
files removed, or -1 if the folder was refused or cannot be opened.
(port/linux/src/posix_capture.c, with the C library's own ABI; Windows':
win32_capture.c) */
int capture_folder_clean(const char *folder, int stale_seconds, int (*owned)(const char *name));

#endif
