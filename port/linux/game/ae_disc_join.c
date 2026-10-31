/*
AE_DISC_JOIN.C

Arena Evolved: a modded disc in the join token (ae_disc_join.h; the rules are
ae_disc_join_rules.c's).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "interface/ui_widget.h"
#include "networking/network_game_protocol.h"
#include "../src/ae_discs.h"

#include "ae_disc_join.h"
#include "ae_disc_join_rules.h"

#include <xtl.h>

#define DISC_NAME_BYTES 128

static struct
{
	/* the disc the identity is for ("" none), and whether it was worked out */
	char disc[DISC_NAME_BYTES];
	boolean known;
	/* 0: the stock token */
	unsigned long long identity;
} disc_join;

/* the disc the game started with, or "" (cache_files_mod_started: game.disc,
frozen at the start) */
static char const *started_disc(
	void)
{
	char const *disc = cache_files_mod_started();

	return disc ? disc : "";
}

/* the disc's identity when its maps folder has a stock map's replacement, else
0; said once a disc in debug.txt */
static unsigned long long disc_identity(
	char const *disc)
{
	WIN32_FIND_DATAA data;
	char pattern[512];
	char first[64] = "";
	long replaced = 0;
	HANDLE find;

	if (!disc[0])
		return 0;
	if (snprintf(pattern, sizeof(pattern), "d:\\%s\\%s\\maps\\*.map", ae_disc_folder(disc), disc) >=
		(int)sizeof(pattern))
	{
		return 0;
	}
	find = FindFirstFileA(pattern, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				ae_disc_join_replaces_stock_map(data.cFileName))
			{
				if (!replaced++)
					snprintf(first, sizeof(first), "%s", data.cFileName);
			}
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
	if (!replaced)
	{
		error(_error_silent, "modded discs: %s replaces no stock map: machines join this one's games, and it "
			"theirs, as without a disc", disc);
		return 0;
	}
	error(_error_silent, "modded discs: %s replaces %ld stock maps (%s ...): only machines playing %s join this "
		"one's games, and it joins only theirs", disc, replaced, first, disc);
	return ae_disc_join_identity(disc);
}

void ae_disc_join_token(
	unsigned char *token,
	long size)
{
	char const *disc = started_disc();

	if (!disc_join.known || strcmp(disc_join.disc, disc))
	{
		snprintf(disc_join.disc, sizeof(disc_join.disc), "%s", disc);
		disc_join.identity = disc_identity(disc_join.disc);
		disc_join.known = TRUE;
	}
	ae_disc_join_fold(token, (int)size, disc_join.identity);
}

void ae_disc_join_rejected(
	unsigned short rejection_code)
{
	/* (short: the error box shows five lines) */
	static wchar_t const stock_text[] = L"The host plays a modded disc. Choose it in Settings > Game Options > "
		L"MODDED DISCS, then join again.";
	static wchar_t const disc_start[] = L"The host plays another disc than ";
	static wchar_t const disc_end[] = L", or an older build. Choose the host's disc (or none) in "
		L"Settings > Game Options > MODDED DISCS.";
	wchar_t text[512];
	long length = 0;
	char const *c;

	if (rejection_code != _rejection_code_bad_join_token)
		return;
	if (!disc_join.known || !disc_join.identity)
	{
		error(_error_silent, "modded discs: the host refused this machine's join token: it plays a modded disc, "
			"this machine the stock maps");
		display_error_text_when_main_menu_loaded(stock_text);
		return;
	}
	error(_error_silent, "modded discs: the host refused this machine's join token: it plays another disc than %s, or an "
		"older build",
		disc_join.disc);
	for (; length < (long)NUMBEROF(disc_start) - 1; length++)
		text[length] = disc_start[length];
	/* (a long name cut to fit the box) */
	for (c = disc_join.disc; *c && c < disc_join.disc + 16; c++)
		text[length++] = (wchar_t)(unsigned char)*c;
	csmemcpy(text + length, disc_end, sizeof(disc_end));
	display_error_text_when_main_menu_loaded(text);
}
