/*
AE_WEB_VORBIS.C

The Ogg Vorbis decoder (stb_vorbis, port/third_party/stb) for the web build,
which has no Custom Edition support (ce_vorbis.c is empty there) yet links
the Custom Edition copy of the sound reader (cev_custom_edition_sounds.c),
which calls it. The other builds get it from ce_vorbis.c.
*/

#ifdef HALO_WEB

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include <stdlib.h>
#include "stb_vorbis.c"

#endif
