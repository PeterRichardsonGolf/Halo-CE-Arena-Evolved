/*
AE_EXTRAS.H

The game's own folders of a download that cannot hold them in the data root. A release carries packs/ (the weapon packs,
the vehicle and bomb packs), voices/ (the callout voices) and sounds/ (the horn) beside the game, and the game reads
them from its data root. The macOS download is an application whose data root is its folder in Application Support, so
the three folders sit beside the application (the folder the download was unpacked into) or in its Resources, and the
game copies what the data root lacks, once at each start (ae_extras_copy: a file is copied when the data root has none
or an older one; a newer one is the player's and stays). The release's own signature forbids putting them in the
application, and the player's copy of it can be moved anywhere after one start.

HALO_EXTRAS_ROOT (any platform) names one more folder to copy from, the way tests give a platform's second place.
Windows and Linux builds copy nothing without it.
*/

#ifndef __AE_EXTRAS_H
#define __AE_EXTRAS_H

/* copies what the data root lacks of packs/, voices/ and sounds/ from the places above, logging where it found them
and what it copied; data_root is the platform's (platform_data_root, which calls this once it has it) */
void ae_extras_copy(const char *data_root);

#endif
