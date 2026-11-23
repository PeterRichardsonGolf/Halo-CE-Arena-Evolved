/*
TAG_FIELD_BITS.H

The pointer fields of tag data (a block's and a data's address, a
reference's name) as numbers: a tag bundle keeps offsets in them, and a
donor map's tags keep the donor's own addresses, neither of which is a
pointer of this build. A 64-bit build's fields are 32-bit Xbox addresses
(cseries/xbox_address.h); a 32-bit build's are pointers.
*/

#ifndef __TAG_FIELD_BITS_H
#define __TAG_FIELD_BITS_H
#pragma once

/* what a pointer field holds, as a number */
#define FIELD_BITS(field) ((unsigned long)(field))

/* a number made what a pointer field of `type` holds */
#ifdef HALO_64BIT
#define FIELD_FROM_BITS(type, bits) ((unsigned int)(bits))
#else
#define FIELD_FROM_BITS(type, bits) ((type)(unsigned long)(bits))
#endif

#endif /* __TAG_FIELD_BITS_H */
