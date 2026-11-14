/*
AE_DISCS.H

Arena Evolved: modded discs (whole-set map overlays such as Halo 1: NHE, CE+ X
and Patch Edition), each a folder of the data root's discs/ with a maps/ in it,
chosen by game.disc. They were folders of mods/ before (game.mod), which OpenCE's
client-side mods (mod.toml and tags/) now own: a disc found only there is
still played from there, and no player's files are moved.
*/

#ifndef __AE_DISCS_H
#define __AE_DISCS_H

/* the data root's folder a disc's maps/ is in: "discs" (their place now),
or "mods" when only mods/<disc>/maps exists (an earlier release's place) */
char const *ae_disc_folder(char const *disc);

/* the count of a sorted array of names (each width bytes) once a name listed twice (a disc in both discs/ and mods/)
is kept once */
int ae_discs_unique(char *names, int width, int count);

#endif
