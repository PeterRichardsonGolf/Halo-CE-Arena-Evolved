/*
AE_FRAME_TIMES.H

(Arena Evolved) Where each frame's time goes: a measuring build's timers, on with HALO_AE_FRAME_TIMES=1 (else each
call returns at once). Named slots add up the time between ae_ft_enter and ae_ft_leave (main thread), counters add
up calls; once a second one line goes to the log (port/linux/src/ae_frame_times.c).
*/

#ifndef __AE_FRAME_TIMES_H
#define __AE_FRAME_TIMES_H

enum
{
	/* main loop (source/main/main.c) */
	AE_FT_INPUT,
	AE_FT_EVENTS,
	AE_FT_PORT_UPDATES,
	AE_FT_NET_START,
	AE_FT_UI_WIDGETS,
	AE_FT_PLAYER_CONTROL,
	AE_FT_NET_END,
	AE_FT_GAME_TIME,
	AE_FT_NON_DETERMINISTIC,
	AE_FT_GAME_RENDER,
	AE_FT_THROTTLE,
	AE_FT_PRESENT_FRAME,
	/* render (source/render/render.c, source/interface/interface.c) */
	AE_FT_FRAME_BEGIN,
	AE_FT_PLAYER_VIEW,
	AE_FT_WINDOW_BEGIN,
	AE_FT_OBJECTS,
	AE_FT_SCENE,
	AE_FT_TONE_AA,
	AE_FT_INTERFACE,
	AE_FT_HUD,
	AE_FT_ENGINE_HUD,
	AE_FT_UI_RENDER,
	AE_FT_WINDOW_END,
	AE_FT_NON_PLAYER_VIEW,
	AE_FT_FRAME_END,
	/* AE (port/linux/game/ae_hooks.c, port/linux/src/ae_draw.c) */
	AE_FT_AE_UI_PROCESS,
	AE_FT_AE_PLAYER_SETTINGS,
	AE_FT_AE_CE_MAP,
	AE_FT_AE_MAIN_MENU,
	AE_FT_AE_DIALOG,
	AE_FT_AE_LOBBY,
	AE_FT_AE_UI_RENDER,
	AE_FT_AE_DRAW_PRESENT,
	/* present (port/linux/src/d3d8_gl.c, sdl_platform.c) */
	AE_FT_D3D_PRESENT,
	AE_FT_VISIBILITY_SEND,
	AE_FT_CAPTURE,
	AE_FT_TO_WINDOW,
	AE_FT_SWAP,
	AE_FT_SWAP_GL,
	AE_FT_SWAP_SOUND_WAIT,
	AE_FT_LIMITER,
	AE_FT_VISIBILITY_READ,
	AE_FT_FENCE_WAIT,
	/* the Xbox's two frames queued at most, without interpolation (d3d8_gl.c) */
	AE_FT_FLIP_WAIT,
	/* the time inside the counted GL calls below (gl_functions.c) */
	AE_FT_GL_COUNTED,
	AE_FT_SLOTS
};

enum
{
	AE_FC_CONFIG_LOOKUP,
	AE_FC_CONFIG_PLAYER_HIT,
	AE_FC_CONFIG_CHANGES,
	AE_FC_CONFIG_PLAYER_CHANGES,
	AE_FC_CONFIG_STAT,
	AE_FC_LOG_LINE,
	AE_FC_AE_UI_UP,
	AE_FC_AE_QUADS,
	AE_FC_GL_GET_INTEGER,
	AE_FC_GL_IS_ENABLED,
	AE_FC_GL_GET_ERROR,
	AE_FC_GL_READ_PIXELS,
	AE_FC_GL_FINISH,
	AE_FC_GL_FLUSH,
	AE_FC_GL_QUERY_RESULT,
	AE_FC_GL_CLIENT_WAIT,
	AE_FC_GL_MAP_BUFFER,
	AE_FC_GL_GET_TEX_IMAGE,
	AE_FC_GL_FRAMEBUFFER_STATUS,
	AE_FC_GL_GET_OTHER,
	/* config_changes calls made inside the HUD's slot (of cfg_changes) */
	AE_FC_CONFIG_CHANGES_IN_HUD,
	AE_FC_COUNTERS
};

/* HALO_AE_FRAME_TIMES set (read once) */
int ae_ft_on(void);
void ae_ft_enter(int slot);
void ae_ft_leave(int slot);
void ae_ft_count(int counter);
/* player 1's unit alive this frame (the line says how much of the second) */
void ae_ft_alive(int alive);
/* the frame's end, after the swap and the frame limit: the line once a second */
void ae_ft_frame(void);
/* GPU time: a GL_TIME_ELAPSED query from the frame's start to before the swap, read frames later (d3d8_gl.c) */
void ae_ft_gpu_begin(void);
void ae_ft_gpu_end(void);

#endif
