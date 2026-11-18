/* ae_load_refuse_rules.c: see ae_load_refuse_rules.h */
#include "ae_load_refuse_rules.h"

#include <stdio.h>
#include <string.h>

void ae_lr_reset(struct ae_lr_state *state)
{
	memset(state, 0, sizeof(*state));
	state->last_log = -1e9;
}

static int ae_lr_valid(long index)
{
	return index >= 0 && index < AE_LR_ENTRIES;
}

double ae_lr_note_failure(struct ae_lr_state *state, long index, double now)
{
	double wait = AE_LR_FIRST_WAIT;
	int step;

	if (!ae_lr_valid(index))
		return 0.0;
	if (state->failures[index] < 1000)
		state->failures[index]++;
	for (step = 1; step < state->failures[index] && wait < AE_LR_MAX_WAIT; step++)
		wait *= 2.0;
	if (wait > AE_LR_MAX_WAIT)
		wait = AE_LR_MAX_WAIT;
	state->retry_at[index] = now + wait;
	return wait;
}

void ae_lr_note_success(struct ae_lr_state *state, long index)
{
	if (ae_lr_valid(index))
	{
		state->failures[index] = 0;
		state->retry_at[index] = 0.0;
	}
}

int ae_lr_refused(struct ae_lr_state const *state, long index)
{
	return ae_lr_valid(index) && state->retry_at[index] > 0.0;
}

int ae_lr_due(struct ae_lr_state const *state, long index, double now)
{
	return ae_lr_refused(state, index) && now >= state->retry_at[index];
}

void ae_lr_release(struct ae_lr_state *state, long index)
{
	if (ae_lr_valid(index))
		state->retry_at[index] = 0.0;
}

long ae_lr_waiting(struct ae_lr_state const *state, long count, double now, double *soonest)
{
	long index, waiting = 0;

	if (soonest)
		*soonest = -1.0;
	for (index = 0; index < count && index < AE_LR_ENTRIES; index++)
	{
		if (ae_lr_refused(state, index))
		{
			double left = state->retry_at[index] - now;

			if (left < 0.0)
				left = 0.0;
			waiting++;
			if (soonest && (*soonest < 0.0 || left < *soonest))
				*soonest = left;
		}
	}
	return waiting;
}

int ae_lr_log_due(struct ae_lr_state *state, double now)
{
	if (now - state->last_log < AE_LR_LOG_SECONDS)
		return 0;
	state->last_log = now;
	return 1;
}

char const *ae_lr_resource_name(unsigned type)
{
	return type == 1 ? "bitmaps.map" : type == 2 ? "sounds.map" : "resource.map";
}

void ae_lr_resource_failure_text(char *out, unsigned long size, unsigned type, char const *path,
	unsigned long file_bytes, char const *reference, unsigned long failed_count, char const *level)
{
	char const *leaf = path ? path : "(no file)";
	char const *cursor;
	char detail[400] = "";
	char const *file = ae_lr_resource_name(type);

	if (!out || !size)
		return;
	for (cursor = leaf; *cursor; cursor++)
	{
		if ((*cursor == '/' || *cursor == '\\') && cursor[1])
			leaf = cursor + 1;
	}
	if (failed_count && reference && reference[0])
		snprintf(detail, sizeof(detail), "; %lu reference%s fail, the first is %s", failed_count,
			failed_count == 1 ? "" : "s", reference);
	else if (reference && reference[0])
		snprintf(detail, sizeof(detail), "; the first reference to fail is %s", reference);
	else if (failed_count)
		snprintf(detail, sizeof(detail), "; %lu reference%s fail", failed_count, failed_count == 1 ? "" : "s");
	snprintf(out, size, "%s (%s, %lu bytes) does not satisfy %s%s. It is not the %s this map was made with: update "
		"MCC in Steam (verify its files) or install the map's own %s",
		file, leaf, file_bytes, level && level[0] ? level : "the map", detail, file, file);
}
