/*
AE_RESULT.H

(M2, preflight P22) What AE's glue calls return (ae_glue_lobby.h, the profile
glue): ok, or the reason it failed in words a dialog shows as they are (an
error dialog keeps the glue's reason text). Pure: plain types, no engine
includes.
*/

#ifndef __AE_RESULT_H
#define __AE_RESULT_H

struct ae_result { int ok; char reason[128]; };

#endif
