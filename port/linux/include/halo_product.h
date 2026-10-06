/*
HALO_PRODUCT.H

The program's name, as a player reads it (the window's title, dialogs, the
start-up log), and the upstream releases it is based on. The version is
the build's own (HALO_VERSION: tools/version.py, VERSION).

Only what a player reads: save folders, config files, network identifiers,
the Android package and the like keep the names existing installs and other
builds rely on.
*/

#ifndef __HALO_PRODUCT_H
#define __HALO_PRODUCT_H

#define HALO_PRODUCT_NAME "Halo CE: Arena Evolved"
/* where space is tight */
#define HALO_PRODUCT_SHORT_NAME "Arena Evolved"
/* what this version is built on (README, "How it relates to OpenCE and
ChupathingyCE"); kept in step with tools/version.py's UPSTREAM_BASE */
#define HALO_UPSTREAM_BASE "ChupathingyCE 0.6.8b, OpenCE build-138"

#endif
