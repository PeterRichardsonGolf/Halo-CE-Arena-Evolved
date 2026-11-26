#ifndef AE_GARBAGE_LOG_H
#define AE_GARBAGE_LOG_H

/* (objects.c, objects_garbage_collection) a dead body the collection is about to delete: one line, "garbage: a body
collected", with what made it count as unseen (ae_garbage_log.c) */
void ae_garbage_log_body(long object_index, long active_garbage, short mode);

#endif
