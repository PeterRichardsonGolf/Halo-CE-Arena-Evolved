/*
AE_DEV_WHEEL_RULES.H

The DEV MODE weapon wheel's rules (0.7.4; ae_dev_wheel.c draws and reads it): which page a weapon is on, how many pages
a list takes, which slice the pointer picks (a pick that sticks), and how far a point is inside a slice. Pure: no engine
includes (unit test port/linux/tests/ae_dev_wheel_rules_test.c).

The look (mockups wheel-pause, sizes in pixels of a 1080 line view, times the view's k): a backing disc, up to 8
slices from a ring's inner edge to its outer, a dead zone in the middle that takes nothing.
*/

#ifndef AE_DEV_WHEEL_RULES_H
#define AE_DEV_WHEEL_RULES_H

enum { AE_DEV_WHEEL_HUMAN, AE_DEV_WHEEL_COVENANT, AE_DEV_WHEEL_OTHER, AE_DEV_WHEEL_CATEGORIES };
enum { AE_DEV_WHEEL_SLICES = 8 };

#define AE_DEV_WHEEL_BACK 326.0f     /* the backing disc's radius */
#define AE_DEV_WHEEL_OUT 310.0f      /* the slices' outer radius */
#define AE_DEV_WHEEL_IN 104.0f       /* and inner */
#define AE_DEV_WHEEL_DEAD 96.0f      /* the dead zone's radius: the pointer in it takes nothing */
#define AE_DEV_WHEEL_GAP 8.0f        /* between slices, across */
#define AE_DEV_WHEEL_MARGIN 10.0f    /* a name's text keeps this far from its slice's edges */
/* the share of a slice, past its edge, the pointer must go to pick the next one */
#define AE_DEV_WHEEL_STICKY 0.20f

/* the page group of a weapon tag's name (its path, any case): human weapons, covenant weapons, the rest (the packs') */
int ae_dev_wheel_category(char const *tag_name);
/* the label of a group */
char const *ae_dev_wheel_category_label(int category);
/* the pages count weapons take (at most 8 a page) and how many each has (the first pages one more when they do not
divide) */
short ae_dev_wheel_pages(short count);
short ae_dev_wheel_page_size(short count, short page);
/* the slice of n the pointer (x right, y up, in the wheel's disc: 1 at its edge) picks, -1 for none (inside the dead
zone, no slices). previous: the pick so far (-1 none): it stays until the pointer is past the next slice's edge by
AE_DEV_WHEEL_STICKY of a slice, and the dead zone clears it */
short ae_dev_wheel_pick(short n, float x, float y, short previous);
/* the distance a point (dx, dy from the centre, y down, pixels) lies inside slice index of n at view factor k: to its two
radial edges (the gap counted), its inner and outer arcs; negative outside */
float ae_dev_wheel_margin(float k, short n, short index, float dx, float dy);
/* the view factor of a view of height pixels: 1 from 900 up, 0.5 least */
float ae_dev_wheel_scale(float height);

#endif
