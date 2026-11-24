/*
WEB_MCC_STORAGE.C

The browser build's side of MCC package storage (web_mcc_storage.h), for
mcc_package.c, mcc_share.c, mcc_share_storage.c and mcc_manifest.c.

Arena Evolved's browser build plays no Custom Edition or MCC maps
(web_main.c sets HALO_CUSTOM_EDITION false), so it keeps no MCC packages:
none is visible, there is no room to download one, and nothing is removed.
Upstream's browser build stores them in the page (its own
web_mcc_storage.c and web_mcc_library.js); this build does not take that
page side.
*/

#include "web_mcc_storage.h"

void web_mcc_share_io(struct mcc_share_io *io)
{
	/* (the native file functions stay: nothing is ever staged here) */
	(void)io;
}

uint64_t web_mcc_free_bytes(void)
{
	return 0;
}

int web_mcc_package_visible(char const *level)
{
	(void)level;
	return 0;
}

int web_mcc_directory_visible(char const *directory)
{
	(void)directory;
	return 0;
}

int web_mcc_remove_begin(char const *folder)
{
	(void)folder;
	return 0;
}

void web_mcc_remove_end(char const *folder)
{
	(void)folder;
}
