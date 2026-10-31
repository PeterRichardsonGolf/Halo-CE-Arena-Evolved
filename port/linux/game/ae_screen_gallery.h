/*
AE_SCREEN_GALLERY.H

The AE menus' widget gallery (ae_screen_gallery.c): a debug screen showing
every M2 widget with the mockups' sample content, for the contact sheets and
the owner's design gate (tools/test_ae_gallery.py). Removed in M8 with the
test screen.
*/

#ifndef __AE_SCREEN_GALLERY_H
#define __AE_SCREEN_GALLERY_H

/* debug.ae_test_screen 11-15: one view, page 1-5; 21 / 23: two views, pages 1-2 / 3-4; 41: four views, pages 1-4.
Closes upstream's menus (an AE screen that replaces them); 0 for another value or a full stack */
int ae_screen_gallery_open(int value);

#endif
