/*
AE_DRAWABLE_SIZE.C

(ae_drawable_size.h)
*/

#include "ae_drawable_size.h"

static unsigned long generation = 1, read_at;
static SDL_Window *read_window;
static int read_width, read_height;

void ae_drawable_size_changed(void)
{
	generation++;
}

void ae_drawable_size_get(SDL_Window *window, int *width, int *height)
{
	if (!window)
	{
		/* (as before: SDL's own answer for no window) */
		SDL_GetWindowSizeInPixels(window, width, height);
		return;
	}
	if (read_at != generation || read_window != window)
	{
		read_width = read_height = 0;
		SDL_GetWindowSizeInPixels(window, &read_width, &read_height);
		read_at = generation;
		read_window = window;
	}
	*width = read_width;
	*height = read_height;
}
