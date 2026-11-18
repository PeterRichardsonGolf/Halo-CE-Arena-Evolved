/*
AE_STEAM_CLIENT.C

ae_steam_client.h: the client's calls for STEAM ACCOUNT. Built with the Steam client's units (the host's ABI,
tools/steam_client_build.py), so it includes the client's own header.
*/

#if defined(HALO_STEAM_CLIENT)

#include "ae_steam_client.h"

#include "steam_downloads.h"
#include "ae_steam_downloads.h"
#include "ae_steam_secret.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *platform_data_root(void);

static int opened;

int ae_steam_client_built(void)
{
	return 1;
}

void ae_steam_client_open(void)
{
	static char root[1024];

	if (opened)
		return;
	opened = 1;
	snprintf(root, sizeof(root), "%s", platform_data_root());
	steam_downloads_open(root);
}

void ae_steam_client_frame(void)
{
	if (opened)
		(void)steam_downloads_poll();
}

void ae_steam_client_status(struct ae_steam_status *out)
{
	struct steam_ae_status client;

	memset(out, 0, sizeof(*out));
	out->owns = -1;
	if (!opened)
		return;
	steam_downloads_ae_status(&client);
	out->session = client.session;
	out->checking = client.checking;
	out->store_waiting = client.store_waiting;
	out->guard_app = client.guard_app;
	out->guard_code = client.guard_code;
	snprintf(out->account, sizeof(out->account), "%s", client.account);
	snprintf(out->message, sizeof(out->message), "%s", client.message);
	out->owns = client.owns;
	out->files_known = client.files_known;
	memcpy(out->shared_present, client.shared_present, sizeof(out->shared_present));
	out->downloading = client.downloading;
	out->queue_count = client.queue_count;
	out->shared_queued = client.shared_queued;
	out->done = client.done;
	out->size = client.size;
	out->speed = client.speed;
	out->packages = client.packages;
	out->loading = client.loading;
	out->total = client.total;
	out->page = client.page;
	out->item_count = client.item_count;
	out->sort = client.sort;
	out->type = client.type;
	snprintf(out->search, sizeof(out->search), "%s", client.search);
	out->item_valid = client.item_valid;
	snprintf(out->item_title, sizeof(out->item_title), "%s", client.item_title);
	snprintf(out->item_description, sizeof(out->item_description), "%s", client.item_description);
	snprintf(out->item_tags, sizeof(out->item_tags), "%s", client.item_tags);
	snprintf(out->item_author, sizeof(out->item_author), "%s", client.item_author);
	out->item_size = client.item_size;
	out->item_subscribers = client.item_subscribers;
	out->item_installed = client.item_installed;
	out->item_downloading = client.item_downloading;
	out->item_queued = client.item_queued;
}

void ae_steam_client_item_row(int index, char *name, int name_size, char *state, int state_size)
{
	steam_downloads_ae_item_row(index, name, name_size, state, state_size);
}

void ae_steam_client_sign_in(char const *account, char const *password)
{
	steam_downloads_ae_sign_in(account, password);
}

void ae_steam_client_guard_code(char const *code) { steam_downloads_ae_guard_code(code); }
void ae_steam_client_cancel_sign_in(void) { steam_downloads_ae_cancel_sign_in(); }
void ae_steam_client_log_out(void) { steam_downloads_ae_log_out(); }
void ae_steam_client_download_shared(void) { steam_downloads_ae_download_shared(); }
void ae_steam_client_cancel_shared(void) { steam_downloads_ae_cancel_shared(); }
void ae_steam_client_cancel_all(void) { steam_downloads_ae_cancel_all(); }
void ae_steam_client_workshop(int open) { steam_downloads_ae_workshop(open); }
void ae_steam_client_search(char const *text) { steam_downloads_ae_search(text); }
void ae_steam_client_filter_step(int which) { steam_downloads_ae_filter_step(which); }
void ae_steam_client_page(int delta) { steam_downloads_ae_page(delta); }
void ae_steam_client_item_focus(int index) { steam_downloads_ae_item_focus(index); }
void ae_steam_client_item_choose(int index) { steam_downloads_ae_item_choose(index); }
int ae_steam_client_store(void) { return ae_steam_secret_store(); }

int ae_steam_client_picture(unsigned long long *key, unsigned int *pixels)
{
	uint64_t item = 0;
	int side;

	*key = 0;
	if (!opened)
		return 0;
	side = steam_downloads_picture(&item, (uint32_t *)pixels);
	*key = side ? (unsigned long long)item : 0;
	return item ? side : 0;
}

#endif
