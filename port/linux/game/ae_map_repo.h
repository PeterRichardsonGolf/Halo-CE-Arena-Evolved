/*
AE_MAP_REPO.H

The community map repository's offer in AE's menus (0.7.8). A game left for want of a Custom Edition map its host does
not send (an OpenCE or other host, or map sharing off) is offered the repository's download as the classic Online
Games screen offers it (browser_screen.c: network.map_downloads, the host's version checked, the game joined again),
but as AE's dialogs: the flow's state is browser_screen.c's (ae_map_repo_glue.inc, an include of it), the dialogs and
their words are ae_map_repo.c's.
*/

#ifndef __AE_MAP_REPO_H
#define __AE_MAP_REPO_H

/* what the offer is doing */
enum
{
	AE_REPO_NONE,         /* no offer (or the classic screen's own) */
	AE_REPO_ASKING,       /* download it? */
	AE_REPO_DOWNLOADING,  /* line1: the progress */
	AE_REPO_FAILED,       /* message: why */
	AE_REPO_STOCK,        /* one of the game's own maps, never downloaded: line1 and line2 say where to copy it from */
	AE_REPO_JOINING,      /* the map is in place: the game is joined again */
	AE_REPO_ENDED         /* over; message: the status it ended on, or "" */
};

/* an answer */
enum
{
	AE_REPO_YES,          /* A: download; OK */
	AE_REPO_NO,           /* B: don't; cancel */
	AE_REPO_CLOSE         /* the offer is over: Online Games closed */
};

struct ae_map_repo_view
{
	int phase;
	char file[48];        /* the map as the player knows it (no .map) */
	char line1[160], line2[160];
	char message[200];
};

/* (browser_screen.c) whether the offer is AE's: Online Games open over the main menu with AE's menus on, not drawn */
int browser_screen_ae_offer(void);
/* the flow a step on: the download followed, the game joined again once the map is in place */
void browser_screen_ae_offer_step(void);
/* the offer now (its phase AE_REPO_*) */
void browser_screen_ae_offer_view(struct ae_map_repo_view *view);
void browser_screen_ae_offer_answer(int answer);

/* (ae_hooks.c, each frame) the dialogs follow the offer */
void ae_map_repo_tick(void);

#endif
