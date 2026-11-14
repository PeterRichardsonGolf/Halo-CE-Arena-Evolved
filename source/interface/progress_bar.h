/*
PROGRESS_BAR.H

header included in hcex build.
*/

#ifndef __PROGRESS_BAR_H
#define __PROGRESS_BAR_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"
#include "real_math.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

/* ---------- prototypes/PROGRESS_BAR.C */

void progress_bar_initialize(
	void);
void progress_bar_dispose(
	void);
void progress_bar_begin(
	boolean skip_frame_capture);
/* AE (owner 11-11): a load with no loading screen wait (display.loading_screen off or its seconds 0) plays no loading
tones; game.c sets it for each load */
void progress_bar_set_quiet(boolean quiet); /* AE hook */
void progress_bar_end(
	void);
boolean progress_bar_is_active(
	void);
void progress_bar_enable(
	boolean enabled);
void progress_bar_eachframe(
	void);
void progress_bar_display(
	real progress);
boolean progress_bar_is_stuff_ready(
	void);

/* ---------- globals */

/* ---------- public code */

#endif // __PROGRESS_BAR_H
