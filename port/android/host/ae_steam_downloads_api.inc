/*
AE_STEAM_DOWNLOADS_API.INC

(AE) The desktop STEAM ACCOUNT screen's calls (ae_steam_downloads.h), written inside steam_downloads.c's own unit
(it is included at its end) because it reads the client's private state.
*/

#include "ae_steam_secret.h"

/* whether the manifest's halo1 list was logged in this sign-in */
static int ae_manifest_logged;
/* (the collections block at the end of this file) */
static void ae_collection_drop_all(void);

/* (state_lock held) the desktop screen's whole view of the client */
static void ae_status_build(struct steam_ae_status *out)
{
	int i, index = -1;
	unsigned long now = steam_milliseconds();
	struct steam_workshop_file const *file = NULL;

	memset(out, 0, sizeof(*out));
	out->session = (int)steam.state;
	/* (the saved sign-in is still being read, or the player gave up waiting for it) */
	out->checking = !steam.loaded && !steam.skip_resume;
	out->store_waiting = ae_steam_secret_waiting();
	if (steam.state != SESSION_IN)
		ae_manifest_logged = 0;
	out->guard_app = (steam.confirmations & (STEAM_GUARD_DEVICE_CONFIRMATION | STEAM_GUARD_EMAIL_CONFIRMATION)) != 0;
	out->guard_code = (steam.confirmations & (STEAM_GUARD_DEVICE_CODE | STEAM_GUARD_EMAIL_CODE)) != 0;
	snprintf(out->account, sizeof(out->account), "%s", steam.account);
	/* (a download's error first while it is held, else the latest message) */
	if (steam.held_error[0] && (long)(steam.held_until - now) > 0)
		snprintf(out->message, sizeof(out->message), "%s", steam.held_error);
	else
		snprintf(out->message, sizeof(out->message), "%s", steam.message);
	out->owns = steam.owns_halo1;
	out->files_known = steam.files_known;
	for (i = 0; i < SHARED_FILES && i < 3; i++)
		out->shared_present[i] = steam.shared_present[i];
	out->downloading = steam.downloading;
	out->queue_count = steam.queue_count;
	out->shared_queued = queue_find(-1, 0) >= 0;
	out->done = steam.done;
	out->size = steam.size;
	out->speed = steam.speed;
	out->packages = steam.package_count;
	out->loading = steam.loading;
	out->total = steam.total;
	out->page = steam.page;
	out->item_count = steam.item_count;
	out->sort = steam.sort;
	out->type = steam.type;
	snprintf(out->search, sizeof(out->search), "%s", steam.search);
	if (steam.view == VIEW_BROWSE && row_kind(steam.chosen, &index) == ROW_ITEM && index >= 0)
		file = &steam.items[index];
	if (file)
	{
		out->item_valid = 1;
		snprintf(out->item_title, sizeof(out->item_title), "%s", file->title);
		snprintf(out->item_description, sizeof(out->item_description), "%s", file->description);
		snprintf(out->item_tags, sizeof(out->item_tags), "%s", file->tags);
		out->item_size = file->size;
		out->item_subscribers = file->subscriptions;
		out->item_installed = item_installed(file->id, file->manifest);
		out->item_downloading = steam.downloading > 0 && steam.downloading_id == file->id;
		out->item_queued = queue_find(1, file->id) >= 0;
		{
			char id[24];

			snprintf(id, sizeof(id), "%llu", (unsigned long long)file->id);
			workshop_authors_get(id, out->item_author, (int)sizeof(out->item_author));
		}
	}
}

/* ---------- (AE) the desktop STEAM ACCOUNT screen's calls: its state and commands (steam_downloads.h) */

void steam_downloads_ae_status(struct steam_ae_status *out)
{
	*out = snapshot.ae;
}

void steam_downloads_ae_sign_in(char const *account, char const *password)
{
	pthread_mutex_lock(&steam.state_lock);
	if (!steam.loaded)
	{
		/* (the saved sign-in is still being read: nothing is taken yet, and it is said) */
		snprintf(steam.message, sizeof(steam.message), "Waiting for your system's password store.");
	}
	else if (steam.state == SESSION_OUT && account[0] && password[0])
	{
		snprintf(steam.typed_account, sizeof(steam.typed_account), "%s", account);
		snprintf(steam.typed_password, sizeof(steam.typed_password), "%s", password);
		steam.want_password = 1;
		steam.message[0] = 0;
		steam_log("steam: sign-in asked for");
	}
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_guard_code(char const *code)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.state == SESSION_GUARD && code[0])
	{
		snprintf(steam.typed_code, sizeof(steam.typed_code), "%s", code);
		steam.code_typed = 1;
		steam_log("steam: Steam Guard code typed");
	}
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_cancel_sign_in(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.state == SESSION_CONNECTING || steam.state == SESSION_GUARD)
		steam.cancel_sign_in = 1;
	/* (still reading the saved sign-in: it is not resumed when read) */
	if (!steam.loaded)
		steam.skip_resume = 1;
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_log_out(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.state == SESSION_IN)
		steam.want_sign_out = 1;
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_download_shared(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.state == SESSION_IN && steam.downloading >= 0 && queue_find(-1, 0) < 0)
		download_ask(-1, NULL);
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_cancel_shared(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.downloading < 0)
		steam.cancel_download = 1;
	else if (queue_find(-1, 0) >= 0)
		queue_remove(queue_find(-1, 0));
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_cancel_all(void)
{
	pthread_mutex_lock(&steam.state_lock);
	steam.queue_count = 0;
	/* (the collections' feeders stop with it: nothing is queued again) */
	ae_collection_drop_all();
	if (steam.downloading)
		steam.cancel_download = 1;
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_workshop(int open)
{
	pthread_mutex_lock(&steam.state_lock);
	if (open && steam.state == SESSION_IN)
		view_open(VIEW_BROWSE);
	else if (!open)
		view_open(VIEW_HOME);
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_search(char const *text)
{
	pthread_mutex_lock(&steam.state_lock);
	snprintf(steam.search, sizeof(steam.search), "%s", text);
	steam.page = 1;
	steam.want_page = 1;
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_filter_step(int which)
{
	pthread_mutex_lock(&steam.state_lock);
	if (which == 0)
		steam.sort = (steam.sort + 1) % (int)(sizeof(sorts) / sizeof(sorts[0]));
	else
		steam.type = (steam.type + 1) % (int)(sizeof(type_names) / sizeof(type_names[0]));
	steam.page = 1;
	steam.want_page = 1;
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_page(int delta)
{
	pthread_mutex_lock(&steam.state_lock);
	if (steam.page + delta >= 1 && (steam.page + delta - 1) * WORKSHOP_PAGE_ITEMS < (int)steam.total)
	{
		steam.page += delta;
		steam.want_page = 1;
		steam.page_jump = 1;
	}
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_item_focus(int index)
{
	pthread_mutex_lock(&steam.state_lock);
	if (index >= 0 && index < steam.item_count)
		steam.chosen = (short)(BROWSE_FILTER_ROWS + index);
	pthread_mutex_unlock(&steam.state_lock);
}

/* the Workshop number of the page's item at that place (its page on Steam is named by it); 0: no such item */
uint64_t steam_downloads_ae_item_id(int index)
{
	uint64_t id = 0;

	pthread_mutex_lock(&steam.state_lock);
	if (steam.view == VIEW_BROWSE && index >= 0 && index < steam.item_count)
		id = steam.items[index].id;
	pthread_mutex_unlock(&steam.state_lock);
	return id;
}

int steam_downloads_ae_item_remove(uint64_t id)
{
	int i, asked = 0;

	pthread_mutex_lock(&steam.state_lock);
	/* (not one being downloaded or waiting for it, and one removal at a time) */
	if (id && !steam.want_remove[0] && queue_find(1, id) < 0 && !(steam.downloading > 0 && steam.downloading_id == id))
	{
		for (i = 0; i < steam.package_count && !asked; i++)
		{
			/* (only the package this game's downloader made, named by the item's id and recorded: any other folder stays) */
			if (&steam.packages[i] == own_package_of(id))
			{
				snprintf(steam.want_remove, sizeof(steam.want_remove), "%s", steam.packages[i].folder);
				asked = 1;
			}
		}
	}
	wake();
	pthread_mutex_unlock(&steam.state_lock);
	return asked;
}

void steam_downloads_ae_item_choose(int index)
{
	if (index >= 0 && snapshot.ae.item_count > index && snapshot.view == VIEW_BROWSE)
		steam_downloads_choose(BROWSE_FILTER_ROWS + index);
}

void steam_downloads_ae_item_row(int index, char *name, int name_size, char *state, int state_size)
{
	steam_downloads_row(BROWSE_FILTER_ROWS + index, name, name_size, state, state_size);
}

/* Halo: CE Anniversary's depot, once a sign-in: every path under halo1/ with its size, and the count and bytes, in the log
(what a download of MCC's own Halo 1 maps would be: nothing is downloaded). Read again after a log out and a new sign-in. */
static void ae_manifest_log(struct steam_depot_manifest const *files)
{
	int index, count = 0;
	unsigned long long bytes = 0;

	if (ae_manifest_logged)
		return;
	ae_manifest_logged = 1;
	for (index = 0; index < files->file_count; index++)
	{
		struct steam_depot_file const *file = &files->files[index];

		if (file->directory || strncasecmp(file->name, "halo1/", 6))
			continue;
		steam_log("steam: manifest halo1: %s %llu", file->name, (unsigned long long)file->size);
		count++;
		bytes += file->size;
	}
	steam_log("steam: manifest halo1: %d files, %llu bytes", count, bytes);
}

/* ---------- (AE) Workshop collections: sizes and titles asked of Steam, the maps fed to the download queue a few at a time */

enum { AE_COLLECTION_CACHE = 1280, AE_COLLECTION_IDS = 64, AE_COLLECTION_JOBS = 8, AE_COLLECTION_FEED = 4,
	AE_COLLECTION_RETRY_MS = 30000 };

struct ae_collection_entry
{
	struct steam_workshop_file file;
	int have, failed;
	unsigned long retry;
};
static struct ae_collection_entry *ae_collection_cache;
static int ae_collection_asking;
/* (state_lock) each get's ids, taken out one at a time by a stop; a slot is free when not active */
static struct
{
	int active;
	int count;
	uint64_t ids[AE_COLLECTION_IDS];
} ae_collection_jobs[AE_COLLECTION_JOBS];

/* (state_lock held) the cache's entry for an id, made when there is room (NULL when none: the cache holds twenty collections
of sixty-four) */
static struct ae_collection_entry *ae_collection_entry_of(uint64_t id, int make)
{
	int i, free_slot = -1;

	if (!ae_collection_cache)
		ae_collection_cache = calloc(AE_COLLECTION_CACHE, sizeof(*ae_collection_cache));
	if (!ae_collection_cache)
		return NULL;
	for (i = 0; i < AE_COLLECTION_CACHE; i++)
	{
		if (ae_collection_cache[i].have && ae_collection_cache[i].file.id == id)
			return &ae_collection_cache[i];
		if (!ae_collection_cache[i].have && free_slot < 0)
			free_slot = i;
	}
	if (!make || free_slot < 0)
		return NULL;
	memset(&ae_collection_cache[free_slot], 0, sizeof(ae_collection_cache[free_slot]));
	ae_collection_cache[free_slot].have = 1;
	ae_collection_cache[free_slot].file.id = id;
	return &ae_collection_cache[free_slot];
}

/* Steam's details of each id that has none yet (a failed one again after a pause); a lookup that fails is marked, and says
so in the log */
static void ae_collection_resolve(uint64_t const *ids, int count, int const *job)
{
	int index;

	for (index = 0; index < count; index++)
	{
		struct steam_workshop_file file;
		char error[256];
		int need, found = 0, signed_in;
		uint64_t id;

		pthread_mutex_lock(&steam.state_lock);
		id = job ? ae_collection_jobs[*job].ids[index] : ids[index];
		{
			struct ae_collection_entry *entry = id ? ae_collection_entry_of(id, 0) : NULL;

			need = id != 0 && !(entry && entry->file.manifest) &&
				!(entry && entry->failed && (long)(entry->retry - steam_milliseconds()) > 0);
		}
		signed_in = steam.state == SESSION_IN;
		pthread_mutex_unlock(&steam.state_lock);
		if (!need || !signed_in)
			continue;
		pthread_mutex_lock(&steam.session_lock);
		if (session_ensure(error, sizeof(error)))
			found = steam_cm_workshop_details(steam.cm, id, &file);
		pthread_mutex_unlock(&steam.session_lock);
		pthread_mutex_lock(&steam.state_lock);
		{
			struct ae_collection_entry *entry = ae_collection_entry_of(id, 1);

			if (entry && found)
			{
				entry->file = file;
				entry->failed = 0;
			}
			else if (entry)
			{
				entry->failed = 1;
				entry->retry = steam_milliseconds() + AE_COLLECTION_RETRY_MS;
			}
		}
		pthread_mutex_unlock(&steam.state_lock);
		if (!found)
			steam_log("steam: collection item %llu: Steam gave no details", (unsigned long long)id);
	}
}

static void *ae_collection_ask_thread(void *argument)
{
	uint64_t *ids = argument;

	ae_collection_resolve(ids + 1, (int)ids[0], NULL);
	free(ids);
	pthread_mutex_lock(&steam.state_lock);
	ae_collection_asking = 0;
	pthread_mutex_unlock(&steam.state_lock);
	return NULL;
}

void steam_downloads_ae_collection_ask(uint64_t const *ids, int count)
{
	uint64_t *copy;
	pthread_t thread;
	pthread_attr_t attributes;
	int index, kept = 0;

	if (!ids || count <= 0)
		return;
	copy = calloc((size_t)AE_COLLECTION_IDS + 1, sizeof(*copy));
	if (!copy)
		return;
	for (index = 0; index < count && kept < AE_COLLECTION_IDS; index++)
		if (ids[index])
			copy[1 + kept++] = ids[index];
	copy[0] = (uint64_t)kept;
	pthread_mutex_lock(&steam.state_lock);
	if (ae_collection_asking || steam.state != SESSION_IN)
	{
		pthread_mutex_unlock(&steam.state_lock);
		free(copy);
		return;
	}
	ae_collection_asking = 1;
	pthread_mutex_unlock(&steam.state_lock);
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thread, &attributes, ae_collection_ask_thread, copy))
	{
		free(copy);
		pthread_mutex_lock(&steam.state_lock);
		ae_collection_asking = 0;
		pthread_mutex_unlock(&steam.state_lock);
	}
	pthread_attr_destroy(&attributes);
}

/* a get's feeder: the details first, then each id into the download queue while fewer than AE_COLLECTION_FEED wait there, so
a single install and another get still find room. Ends when every id is queued, dropped by a stop or a sign-out */
static void *ae_collection_get_thread(void *argument)
{
	int slot = (int)(intptr_t)argument, index;

	ae_collection_resolve(NULL, ae_collection_jobs[slot].count, &slot);
	for (index = 0; index < ae_collection_jobs[slot].count; index++)
	{
		struct timespec pause = { 0, 500 * 1000 * 1000 };

		for (;;)
		{
			struct ae_collection_entry const *entry;
			uint64_t id;
			int done = 0, stopped;

			pthread_mutex_lock(&steam.state_lock);
			id = ae_collection_jobs[slot].ids[index];
			stopped = steam.state != SESSION_IN;
			entry = id ? ae_collection_entry_of(id, 0) : NULL;
			if (stopped)
				ae_collection_jobs[slot].count = 0;
			else if (!id || !entry || !entry->file.manifest)
				done = 1;
			else if (queue_find(1, id) >= 0 || (steam.downloading > 0 && steam.downloading_id == id))
				done = 1;
			else if (steam.queue_count < AE_COLLECTION_FEED)
			{
				download_ask(1, &entry->file);
				ae_collection_jobs[slot].ids[index] = 0;
				wake();
				done = 1;
			}
			pthread_mutex_unlock(&steam.state_lock);
			if (stopped || done)
				break;
			nanosleep(&pause, NULL);
		}
	}
	pthread_mutex_lock(&steam.state_lock);
	ae_collection_jobs[slot].active = 0;
	ae_collection_jobs[slot].count = 0;
	pthread_mutex_unlock(&steam.state_lock);
	return NULL;
}

int steam_downloads_ae_collection_get(uint64_t const *ids, int count)
{
	pthread_t thread;
	pthread_attr_t attributes;
	int index, other, slot = -1, taken = 0;

	if (!ids || count <= 0)
		return 0;
	pthread_mutex_lock(&steam.state_lock);
	if (steam.state != SESSION_IN)
	{
		pthread_mutex_unlock(&steam.state_lock);
		return 0;
	}
	for (other = 0; other < AE_COLLECTION_JOBS && slot < 0; other++)
		if (!ae_collection_jobs[other].active)
			slot = other;
	if (slot < 0)
	{
		pthread_mutex_unlock(&steam.state_lock);
		return 0;
	}
	ae_collection_jobs[slot].count = 0;
	for (index = 0; index < count && ae_collection_jobs[slot].count < AE_COLLECTION_IDS; index++)
	{
		struct package const *package = ids[index] ? own_package_of(ids[index]) : NULL;
		int busy = 0;

		if (!ids[index])
			continue;
		for (other = 0; other < AE_COLLECTION_JOBS && !busy; other++)
		{
			int each;

			if (!ae_collection_jobs[other].active)
				continue;
			for (each = 0; each < ae_collection_jobs[other].count; each++)
				busy |= ae_collection_jobs[other].ids[each] == ids[index];
		}
		/* (installed here by the downloader and current, queued, being downloaded, or in another get: left out) */
		if (busy || (package && recorded_manifest(ids[index]) != PARTIAL_MANIFEST && !package_outdated(package)) || queue_find(1, ids[index]) >= 0 ||
			(steam.downloading > 0 && steam.downloading_id == ids[index]))
			continue;
		ae_collection_jobs[slot].ids[ae_collection_jobs[slot].count++] = ids[index];
	}
	taken = ae_collection_jobs[slot].count;
	if (taken)
		ae_collection_jobs[slot].active = 1;
	pthread_mutex_unlock(&steam.state_lock);
	if (!taken)
		return 0;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thread, &attributes, ae_collection_get_thread, (void *)(intptr_t)slot))
	{
		pthread_mutex_lock(&steam.state_lock);
		ae_collection_jobs[slot].active = 0;
		ae_collection_jobs[slot].count = 0;
		pthread_mutex_unlock(&steam.state_lock);
		taken = 0;
	}
	pthread_attr_destroy(&attributes);
	return taken;
}

/* (state_lock held) every get's ids dropped: the queue is cleared by the caller */
static void ae_collection_drop_all(void)
{
	int slot, each;

	for (slot = 0; slot < AE_COLLECTION_JOBS; slot++)
		for (each = 0; each < ae_collection_jobs[slot].count; each++)
			ae_collection_jobs[slot].ids[each] = 0;
}

void steam_downloads_ae_collection_stop(uint64_t const *ids, int count)
{
	int index, slot, each;

	pthread_mutex_lock(&steam.state_lock);
	for (index = 0; ids && index < count; index++)
	{
		int place = queue_find(1, ids[index]);

		if (!ids[index])
			continue;
		for (slot = 0; slot < AE_COLLECTION_JOBS; slot++)
			for (each = 0; each < ae_collection_jobs[slot].count; each++)
				if (ae_collection_jobs[slot].ids[each] == ids[index])
					ae_collection_jobs[slot].ids[each] = 0;
		if (place >= 0)
			queue_remove(place);
		if (steam.downloading > 0 && steam.downloading_id == ids[index])
			steam.cancel_download = 1;
	}
	wake();
	pthread_mutex_unlock(&steam.state_lock);
}

/* ---------- the removals: one batch at a time, one package after another, on a thread of the client's */

enum { AE_REMOVAL_MOST = 128, AE_REMOVAL_STEPS = 600 };

static struct
{
	uint64_t ids[AE_REMOVAL_MOST];
	int result[AE_REMOVAL_MOST];  /* 0 waiting, 1 done, 2 failed */
	int count;
	int running;
} ae_removal;

/* one package: handed to the files thread (which takes one removal at a time) once nothing else waits there, then waited for
until its download record is gone (the files thread deletes the folder first). 1 removed (or already gone), 2 not removed */
static int ae_removal_one(uint64_t id)
{
	struct timespec pause = { 0, 100 * 1000 * 1000 };
	int step, submitted = 0;

	for (step = 0; step < AE_REMOVAL_STEPS && !submitted; step++)
	{
		struct package const *package;

		pthread_mutex_lock(&steam.state_lock);
		package = own_package_of(id);
		if (!package)
		{
			/* (a folder the downloader did not make is not removed: that is a failure, not a removal) */
			submitted = package_of(id) != NULL;
			pthread_mutex_unlock(&steam.state_lock);
			return submitted ? 2 : 1;
		}
		if (!recorded_manifest(id) || queue_find(1, id) >= 0 || (steam.downloading > 0 && steam.downloading_id == id))
		{
			pthread_mutex_unlock(&steam.state_lock);
			return 2;
		}
		if (!steam.want_remove[0])
		{
			snprintf(steam.want_remove, sizeof(steam.want_remove), "%s", package->folder);
			remove_failed_id = 0;
			submitted = 1;
			wake();
		}
		pthread_mutex_unlock(&steam.state_lock);
		if (!submitted)
			nanosleep(&pause, NULL);
	}
	if (!submitted)
		return 2;
	for (step = 0; step < AE_REMOVAL_STEPS; step++)
	{
		int recorded, failed;

		pthread_mutex_lock(&steam.state_lock);
		recorded = recorded_manifest(id) != 0;
		failed = remove_failed_id == id;
		pthread_mutex_unlock(&steam.state_lock);
		if (failed)
			return 2;
		if (!recorded)
			return 1;
		nanosleep(&pause, NULL);
	}
	return 2;
}

static void *ae_collection_remove_thread(void *unused)
{
	int index;

	(void)unused;
	for (index = 0;; index++)
	{
		uint64_t id;
		int result;

		pthread_mutex_lock(&steam.state_lock);
		if (index >= ae_removal.count)
		{
			ae_removal.running = 0;
			pthread_mutex_unlock(&steam.state_lock);
			return NULL;
		}
		id = ae_removal.ids[index];
		pthread_mutex_unlock(&steam.state_lock);
		result = ae_removal_one(id);
		pthread_mutex_lock(&steam.state_lock);
		ae_removal.result[index] = result;
		pthread_mutex_unlock(&steam.state_lock);
		if (result == 2)
			steam_log("steam: collection item %llu was not removed", (unsigned long long)id);
	}
}

int steam_downloads_ae_collection_remove(uint64_t const *ids, int count)
{
	pthread_t thread;
	pthread_attr_t attributes;
	int index, each, taken = 0, start = 0;

	pthread_mutex_lock(&steam.state_lock);
	/* (a finished batch is forgotten when the next starts) */
	if (!ae_removal.running)
		ae_removal.count = 0;
	for (index = 0; ids && index < count && ae_removal.count < AE_REMOVAL_MOST; index++)
	{
		int again = 0;

		if (!ids[index] || !own_package_of(ids[index]) || queue_find(1, ids[index]) >= 0 ||
			(steam.downloading > 0 && steam.downloading_id == ids[index]))
			continue;
		for (each = 0; each < ae_removal.count; each++)
			again |= ae_removal.ids[each] == ids[index] && !ae_removal.result[each];
		if (again)
			continue;
		ae_removal.ids[ae_removal.count] = ids[index];
		ae_removal.result[ae_removal.count++] = 0;
		taken++;
	}
	if (taken && !ae_removal.running)
	{
		ae_removal.running = 1;
		start = 1;
	}
	pthread_mutex_unlock(&steam.state_lock);
	if (!start)
		return taken;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thread, &attributes, ae_collection_remove_thread, NULL))
	{
		pthread_mutex_lock(&steam.state_lock);
		for (index = 0; index < ae_removal.count; index++)
			if (!ae_removal.result[index])
				ae_removal.result[index] = 2;
		ae_removal.running = 0;
		pthread_mutex_unlock(&steam.state_lock);
		taken = 0;
	}
	pthread_attr_destroy(&attributes);
	return taken;
}

int steam_downloads_ae_removal_result(uint64_t id)
{
	int index, result = -1;

	pthread_mutex_lock(&steam.state_lock);
	for (index = 0; index < ae_removal.count; index++)
		if (ae_removal.ids[index] == id)
			result = ae_removal.result[index];
	pthread_mutex_unlock(&steam.state_lock);
	return result;
}

void steam_downloads_ae_collection_remove_status(int *pending, int *done, int *failed)
{
	int index;

	*pending = *done = *failed = 0;
	pthread_mutex_lock(&steam.state_lock);
	for (index = 0; index < ae_removal.count; index++)
	{
		if (ae_removal.result[index] == 1)
			(*done)++;
		else if (ae_removal.result[index] == 2)
			(*failed)++;
		else
			(*pending)++;
	}
	pthread_mutex_unlock(&steam.state_lock);
}

int steam_downloads_ae_collection_item(uint64_t id, struct steam_ae_collection_item *out)
{
	struct ae_collection_entry const *entry;
	struct package const *package;
	int known = 0;

	memset(out, 0, sizeof(*out));
	out->id = id;
	pthread_mutex_lock(&steam.state_lock);
	entry = ae_collection_entry_of(id, 0);
	if (entry && entry->file.manifest)
	{
		known = 1;
		out->size = entry->file.size;
		out->updated = entry->file.updated;
		snprintf(out->title, sizeof(out->title), "%s", entry->file.title);
	}
	out->known = known;
	out->failed = entry && entry->failed;
	out->queued = queue_find(1, id) >= 0;
	if (steam.downloading > 0 && steam.downloading_id == id)
	{
		out->getting = 1;
		out->percent = steam.size ? (int)(steam.done * 100u / steam.size) : 0;
		if (out->percent > 100)
			out->percent = 100;
	}
	package = package_of(id);
	out->outdated = package && package_outdated(package);
	out->recorded = own_package_of(id) != NULL;
	pthread_mutex_unlock(&steam.state_lock);
	return known;
}

/* the Workshop picture of a collection's item, from the client's kept previews (the same twelve the focused item's picture
lives in): its side (PREVIEW_SIDE, pixels filled when not NULL) or 0 while there is none yet (not signed in, details not
here, still being fetched, or Steam has none). One picture is asked at a time, behind any other under way, so a tile of
eight fills one after another; asking for a picture that is not here yet is how it is queued */
int steam_downloads_ae_collection_picture(uint64_t id, uint32_t *pixels)
{
	struct ae_collection_entry const *entry;
	struct preview *preview;
	int side = 0, slot, busy = 0;

	if (!id)
		return 0;
	pthread_mutex_lock(&steam.state_lock);
	entry = ae_collection_entry_of(id, 0);
	preview = preview_find(id);
	if (preview && preview->state == 1 && preview->pixels)
	{
		preview->used = ++steam.preview_clock;
		if (pixels)
			memcpy(pixels, preview->pixels, sizeof(uint32_t) * PREVIEW_SIDE * PREVIEW_SIDE);
		side = PREVIEW_SIDE;
	}
	else if (entry && entry->file.preview_url[0] && !(preview && preview->state < 0))
	{
		for (slot = 0; slot < PREVIEWS; slot++)
			busy |= steam.previews[slot].id != 0 && steam.previews[slot].state == 0 && steam.previews[slot].taken;
		if (!busy && (steam.want_preview == 0 || steam.want_preview == id))
			preview_want(&entry->file);
	}
	pthread_mutex_unlock(&steam.state_lock);
	return side;
}

/* ---------- (AE) a Workshop item asked for by a game server a player joined

One item at a time. Its details are asked of Steam on a thread of its own by the number alone (title, tags, app, size and
manifest are Steam's words, never the server's), checked against the ceilings given, and then queued on the files thread
like the screen's DOWNLOAD, so one downloader runs download_item: a fetch and a screen download never run it together, a
sign-out or a cancel reaches both, and the screen shows the download. The manifest's total is checked against the same
ceilings by download_item's hook (ae_fetch_over_cap). An item that is a package in mcc_maps already is not downloaded
again. */

static struct
{
	int state;
	int why;
	uint64_t id;
	uint64_t item_cap;
	uint64_t bytes_left;
	char title[96];
	uint64_t taken;
	int cancel;
	/* its item is in the downloader's queue or being downloaded: asked by this fetch (owned) or by the screen or a collection */
	int queued, owned;
} ae_fetch;

/* download_item's: an item of this fetch whose manifest is over a ceiling is not downloaded (state_lock not held) */
static int ae_fetch_over_cap(uint64_t id, uint64_t total)
{
	int over = 0;

	pthread_mutex_lock(&steam.state_lock);
	if (ae_fetch.state == STEAM_AE_FETCH_RUNNING && ae_fetch.owned && ae_fetch.id == id)
	{
		int verdict = steam_fetch_size_check(total, ae_fetch.item_cap, ae_fetch.bytes_left);

		if (verdict != STEAM_FETCH_OK)
		{
			over = 1;
			ae_fetch.why = verdict == STEAM_FETCH_TOO_LARGE ? STEAM_AE_FETCH_TOO_LARGE : STEAM_AE_FETCH_LIMIT_BYTES;
		}
		else
			ae_fetch.taken = total;
	}
	pthread_mutex_unlock(&steam.state_lock);
	return over;
}

/* the files thread's: download_item of an item ended (state_lock not held) */
static void ae_fetch_note(uint64_t id, int installed)
{
	pthread_mutex_lock(&steam.state_lock);
	if (ae_fetch.state == STEAM_AE_FETCH_RUNNING && ae_fetch.id == id)
	{
		if (installed)
		{
			ae_fetch.state = STEAM_AE_FETCH_DONE;
			ae_fetch.why = STEAM_AE_FETCH_NONE;
		}
		else
		{
			if (ae_fetch.why != STEAM_AE_FETCH_TOO_LARGE && ae_fetch.why != STEAM_AE_FETCH_LIMIT_BYTES)
			{
				ae_fetch.why = ae_fetch.cancel ? STEAM_AE_FETCH_CANCELLED : strstr(steam.message, "enough free disk") ?
					STEAM_AE_FETCH_NO_ROOM : STEAM_AE_FETCH_REFUSED;
			}
			ae_fetch.state = STEAM_AE_FETCH_FAILED;
		}
	}
	pthread_mutex_unlock(&steam.state_lock);
}

static void ae_fetch_finish(int state, int why)
{
	pthread_mutex_lock(&steam.state_lock);
	ae_fetch.state = state;
	ae_fetch.why = why;
	pthread_mutex_unlock(&steam.state_lock);
}

static void *ae_fetch_thread(void *unused)
{
	char error[256];
	struct steam_workshop_file item;
	uint64_t id;
	int have, found = 0, here;

	(void)unused;
	pthread_mutex_lock(&steam.state_lock);
	id = ae_fetch.id;
	pthread_mutex_unlock(&steam.state_lock);
	memset(&item, 0, sizeof(item));
	error[0] = 0;
	pthread_mutex_lock(&steam.session_lock);
	have = steam.token[0] != 0;
	if (have && session_ensure(error, sizeof(error)))
		found = steam_cm_workshop_details(steam.cm, id, &item);
	pthread_mutex_unlock(&steam.session_lock);
	if (!have)
	{
		ae_fetch_finish(STEAM_AE_FETCH_FAILED, STEAM_AE_FETCH_NOT_SIGNED_IN);
		return NULL;
	}
	if (!found || item.id != id || !item.manifest)
	{
		steam_log("steam fetch: item %llu: %s", (unsigned long long)id, error[0] ? error : "not found on the Workshop");
		ae_fetch_finish(STEAM_AE_FETCH_FAILED, STEAM_AE_FETCH_REFUSED);
		return NULL;
	}
	/* (an item of another game's Workshop is never asked for; the depot call refuses it too) */
	if (item.app && item.app != STEAM_APP_MCC)
	{
		steam_log("steam fetch: item %llu belongs to app %u", (unsigned long long)id, (unsigned)item.app);
		ae_fetch_finish(STEAM_AE_FETCH_FAILED, STEAM_AE_FETCH_NOT_MULTIPLAYER);
		return NULL;
	}
	if (!steam_item_tags_multiplayer(item.tags))
	{
		steam_log("steam fetch: item %llu is not a Halo 1 multiplayer item", (unsigned long long)id);
		ae_fetch_finish(STEAM_AE_FETCH_FAILED, STEAM_AE_FETCH_NOT_MULTIPLAYER);
		return NULL;
	}
	pthread_mutex_lock(&steam.state_lock);
	snprintf(ae_fetch.title, sizeof(ae_fetch.title), "%s", item.title);
	here = own_package_of(id) != NULL && recorded_manifest(id) != PARTIAL_MANIFEST;
	if (here)
	{
		ae_fetch.state = STEAM_AE_FETCH_DONE;
		ae_fetch.why = STEAM_AE_FETCH_ALREADY;
	}
	else if (ae_fetch.cancel)
	{
		/* (cancelled while Steam was asked: nothing is queued) */
		ae_fetch.state = STEAM_AE_FETCH_FAILED;
		ae_fetch.why = STEAM_AE_FETCH_CANCELLED;
	}
	else
	{
		int verdict = item.size ? steam_fetch_size_check(item.size, ae_fetch.item_cap, ae_fetch.bytes_left) : STEAM_FETCH_OK;

		if (verdict != STEAM_FETCH_OK)
		{
			ae_fetch.state = STEAM_AE_FETCH_FAILED;
			ae_fetch.why = verdict == STEAM_FETCH_TOO_LARGE ? STEAM_AE_FETCH_TOO_LARGE : STEAM_AE_FETCH_LIMIT_BYTES;
		}
		else if (steam.queue_count == DOWNLOAD_QUEUE)
		{
			ae_fetch.state = STEAM_AE_FETCH_FAILED;
			ae_fetch.why = STEAM_AE_FETCH_BUSY;
		}
		else if (queue_find(1, id) >= 0 || (steam.downloading > 0 && steam.downloading_id == id))
		{
			/* (the screen or a collection has it queued or going: not asked again; this fetch follows that download) */
			ae_fetch.queued = 1;
		}
		else
		{
			download_ask(1, &item);
			if (queue_find(1, id) < 0)
			{
				ae_fetch.state = STEAM_AE_FETCH_FAILED;
				ae_fetch.why = STEAM_AE_FETCH_BUSY;
			}
			else
			{
				ae_fetch.queued = 1;
				ae_fetch.owned = 1;
			}
			wake();
		}
	}
	pthread_mutex_unlock(&steam.state_lock);
	return NULL;
}

int steam_downloads_ae_fetch_begin(uint64_t id, uint64_t item_cap, uint64_t bytes_left)
{
	pthread_t thread;
	int started = 0;

	if (!id)
		return 0;
	pthread_mutex_lock(&steam.state_lock);
	if (steam.threads_started && ae_fetch.state != STEAM_AE_FETCH_RUNNING)
	{
		memset(&ae_fetch, 0, sizeof(ae_fetch));
		ae_fetch.state = STEAM_AE_FETCH_RUNNING;
		ae_fetch.id = id;
		ae_fetch.item_cap = item_cap;
		ae_fetch.bytes_left = bytes_left;
		started = 1;
	}
	pthread_mutex_unlock(&steam.state_lock);
	if (started && (pthread_create(&thread, NULL, ae_fetch_thread, NULL) != 0 || pthread_detach(thread) != 0))
		ae_fetch_finish(STEAM_AE_FETCH_FAILED, STEAM_AE_FETCH_REFUSED);
	return started;
}

void steam_downloads_ae_fetch_status(struct steam_ae_fetch *out)
{
	memset(out, 0, sizeof(*out));
	pthread_mutex_lock(&steam.state_lock);
	/* (its entry taken out of the queue by a sign-out, CANCEL ALL, a collection's stop or the screen, and not being
	downloaded: it is over) */
	if (ae_fetch.state == STEAM_AE_FETCH_RUNNING && ae_fetch.queued && queue_find(1, ae_fetch.id) < 0 &&
		!(steam.downloading > 0 && steam.downloading_id == ae_fetch.id))
	{
		ae_fetch.state = STEAM_AE_FETCH_FAILED;
		ae_fetch.why = STEAM_AE_FETCH_CANCELLED;
	}
	out->state = ae_fetch.state;
	out->why = ae_fetch.why;
	out->id = ae_fetch.id;
	out->taken = ae_fetch.taken;
	snprintf(out->title, sizeof(out->title), "%s", ae_fetch.title);
	if (ae_fetch.state == STEAM_AE_FETCH_RUNNING && steam.downloading > 0 && steam.downloading_id == ae_fetch.id)
	{
		out->done = steam.done;
		out->size = steam.size;
	}
	pthread_mutex_unlock(&steam.state_lock);
}

void steam_downloads_ae_fetch_cancel(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (ae_fetch.state == STEAM_AE_FETCH_RUNNING)
	{
		int place = ae_fetch.owned ? queue_find(1, ae_fetch.id) : -1;

		ae_fetch.cancel = 1;
		if (place >= 0)
		{
			/* (still waiting its turn) */
			queue_remove(place);
			ae_fetch.state = STEAM_AE_FETCH_FAILED;
			ae_fetch.why = STEAM_AE_FETCH_CANCELLED;
		}
		else if (ae_fetch.owned && steam.downloading > 0 && steam.downloading_id == ae_fetch.id)
			steam.cancel_download = 1;
		else if (!ae_fetch.owned && ae_fetch.queued)
		{
			/* (not this fetch's entry: left to whoever asked it) */
			ae_fetch.state = STEAM_AE_FETCH_FAILED;
			ae_fetch.why = STEAM_AE_FETCH_CANCELLED;
		}
	}
	pthread_mutex_unlock(&steam.state_lock);
}

/* a finished fetch's answer taken: the state goes back to idle (the item's folder stays) */
void steam_downloads_ae_fetch_clear(void)
{
	pthread_mutex_lock(&steam.state_lock);
	if (ae_fetch.state != STEAM_AE_FETCH_RUNNING)
		ae_fetch.state = STEAM_AE_FETCH_IDLE;
	pthread_mutex_unlock(&steam.state_lock);
}
