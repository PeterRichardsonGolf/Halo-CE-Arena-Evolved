/*
AE_PURE_TYPES.H

The game's boolean and real for AE's pure units (no engine includes), so the
same header builds in the game, after cseries.h, and in the unit tests
(port/linux/tests), without it. In the game, include cseries.h first.
*/

#ifndef __AE_PURE_TYPES_H
#define __AE_PURE_TYPES_H

#ifndef __CSERIES_H
typedef unsigned char boolean;
typedef float real;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

#endif /* __AE_PURE_TYPES_H */
