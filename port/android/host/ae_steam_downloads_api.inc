/*
AE_STEAM_DOWNLOADS_API.INC

(AE) The desktop STEAM ACCOUNT screen's calls (ae_steam_downloads.h), written inside steam_downloads.c's own unit
(it is included at its end) because it reads the client's private state.
*/

#include "ae_steam_secret.h"

/* whether the manifest's halo1 list was logged in this sign-in */
static int ae_manifest_logged;

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
