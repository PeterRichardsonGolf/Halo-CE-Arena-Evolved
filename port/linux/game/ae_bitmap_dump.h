#ifndef AE_BITMAP_DUMP_H
#define AE_BITMAP_DUMP_H

struct bitmap_data;

/* debug.dump_bitmap: writes a bitmap's decoded pixels once, for a test that compares two ways of reading it */
void ae_bitmap_dump(struct bitmap_data const *bitmap, void const *pixels, unsigned long bytes, char const *source);

#endif
