/*
AE_SOUND.H

The AE menus' sounds (spec 5): which of CE's four UI feedback sounds a frame
plays (ae_sound.c, pure, unit test port/linux/tests/ae_sound_test.c), and
playing it (ae_glue_sound.c, the game's sound). At most one sound a frame: the
strongest asked (failure > back > forward > cursor), so a press that both
moves and fails is heard as the failure; a held key's repeat steps play the
cursor at most every 80 ms.
*/

#ifndef __AE_SOUND_H
#define __AE_SOUND_H

enum { AE_SOUND_NONE, AE_SOUND_CURSOR, AE_SOUND_FORWARD, AE_SOUND_BACK, AE_SOUND_FAILURE };
/* asks for a sound this frame; repeat: a held key's repeat step (cursor at most every 80 ms) */
void ae_sound_request(int sound, int repeat);
/* the frame's one sound, the strongest asked (failure > back > forward > cursor), or NONE; clears the requests */
int ae_sound_take(unsigned long now_ms);
void ae_sound_reset(void);
/* the hooks' frame (ae_hooks.c ae_ui_process, after AE's input), so nothing is silent or stale: up, AE's screens are
up now; was_up, they were last frame. Returns the frame's sound: what was asked, also on the frame that closed the
last screen (a pointer's click or right-click on a screen comes before the frame's input: its BACK must still play,
now); with no screen up and none just closed, nothing, and nothing asked is kept. (A screen opened over none calls
ae_sound_reset before its first input, ae_hooks.c) */
int ae_sound_end_frame(int was_up, int up, unsigned long now_ms);
/* (ae_glue_sound.c) plays CE's sound\sfx\ui\cursor / forward / back / flag_failure through
unspatialized_impulse_sound_new at ae_settings_menu_volume(); volume 0 plays nothing */
void ae_glue_sound_play(int sound);

#endif
