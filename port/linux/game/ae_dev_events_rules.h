/*
AE_DEV_EVENTS_RULES.H

DEV MODE's event files (owner 2026-11-03): <config folder>/dev keeps only the newest AE_DEV_EVENTS_KEEP streams. When
a new stream opens, the oldest beyond that are removed: only files named exactly as the stream names them,
events-YYYYmmdd-HHMMSS.jsonl, whose names sort as their times do. Pure: string only, no engine headers.
*/

#ifndef AE_DEV_EVENTS_RULES_H
#define AE_DEV_EVENTS_RULES_H

#define AE_DEV_EVENTS_KEEP 50

/* 1: name is an event stream's file name, events-<8 digits>-<6 digits>.jsonl; 0: anything else */
int ae_dev_events_is_stream(const char *name);

/* of the count names (a folder's, in any order), the streams to remove so that the keep newest stay: their indices
in remove (room for count), the oldest first; how many. A name that is not a stream's is never picked. Of a part of
the folder's names, it picks only streams that at least keep newer ones of that part outlive, so never one of the
whole folder's keep newest */
int ae_dev_events_to_remove(const char *const *names, int count, int keep, int *remove);

#endif
