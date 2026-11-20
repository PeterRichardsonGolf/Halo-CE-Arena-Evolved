/* Independent MCC validation context and dispatch for shared schema callbacks. */
#ifndef MCC_TAG_VALIDATE_H
#define MCC_TAG_VALIDATE_H

#include "cseries.h"
#include <stdarg.h>

struct mcc_runtime;
struct tag_validation;

int mcc_tags_validate(struct mcc_runtime *runtime);
/* (port) The tags of an MCC map opened for the Tag Injector to take tags out
 * of (mcc_tag_bundle.c): as mcc_tags_validate, but their models are still
 * gbxmodels ('mod2', tag_schema_custom_edition_groups), their bitmaps Halo
 * PC's (the schemas' own checks), and their pixels and samples in `ranges`
 * of the bundle's offsets, not the converted streams. */
/* (the weapon tag's references, in the tag data's 32-bit layout: an object's
 * model, and a weapon's first-person model and animations; the checks follow
 * them by these offsets on every build) */
#define MCC_OBJECT_MODEL 40
#define MCC_WEAPON_FIRST_PERSON_MODEL 0x45C
#define MCC_WEAPON_FIRST_PERSON_ANIMATIONS 0x46C
#define MCC_WEAPON_FIRST_PERSON_END 0x47C

struct tag_validate_file_range;
int mcc_tags_validate_bundle(struct mcc_runtime *runtime, struct tag_validate_file_range const *ranges,
    short range_count);
int mcc_bsp_validate(struct mcc_runtime *runtime, long handle, void *address, long size);
void mcc_validation_dispose(struct mcc_runtime *runtime);

/* Identity comparisons only: a legacy validator is never read as an MCC one. */
boolean mcc_validation_owns(struct tag_validation *validation);
boolean mcc_validation_callback_active(void);
void mcc_validation_message(struct tag_validation *validation, boolean refuse,
    char const *format, va_list arguments);
boolean mcc_validation_file_contains(struct tag_validation *validation, long offset, long size);
void *mcc_validation_root(struct tag_validation *validation);
boolean mcc_validation_contains(struct tag_validation *validation, void const *address, unsigned long size);
void *mcc_validation_tag_get(struct tag_validation *validation, long handle, unsigned long group);
void *mcc_validation_buffer_data(struct tag_validation *validation, void const *buffer, boolean indices);
boolean mcc_validation_any_claimed(void const *address, unsigned long size);

/* Optional MCC callback routing. The stock standalone validator has no MCC
 * runtime dependency: this pointer is NULL except inside an MCC callback. */
struct mcc_validation_dispatch {
    boolean (*owns)(struct tag_validation *);
    void (*message)(struct tag_validation *, boolean, char const *, va_list);
    boolean (*file_contains)(struct tag_validation *, long, long);
    void *(*root)(struct tag_validation *);
    boolean (*contains)(struct tag_validation *, void const *, unsigned long);
    void *(*tag_get)(struct tag_validation *, long, unsigned long);
    void *(*buffer_data)(struct tag_validation *, void const *, boolean);
    boolean (*any_claimed)(void const *, unsigned long);
    /* (port) whether the tags are laid out as a Custom Edition map's
     * (tag_validate_custom_edition): an MCC map's taken out as bundles */
    boolean (*custom_edition)(struct tag_validation *);
};
extern struct mcc_validation_dispatch const *mcc_validation_callbacks;

#endif
