/* Arena Evolved's hooks in the MCC loader files (ae_mcc_boot.h, ae_mcc_platform.c) for the tests that compile
 * those files alone: MCC's own maps are not in the tests' world (weak: a test may define its own). */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#define WEAK __attribute__((weak))
WEAK int ae_mcc_boot_ui(char const *name) { (void)name; return 0; }
WEAK int ae_mcc_install_level(char const *name) { (void)name; return 0; }
WEAK char const *ae_mcc_install_level_identity(char const *name) { (void)name; return ""; }
WEAK int ae_mcc_install_level_path(char const *name, char *path, unsigned long size) { (void)name; (void)path; (void)size; return 0; }
WEAK void ae_mcc_install_level_problem(char const *name, int missing, char *reason, unsigned long size)
{ (void)name; (void)missing; (void)reason; (void)size; }
WEAK int ae_mcc_install_resource_candidates(int type, char (*paths)[1024], int maximum)
{ (void)type; (void)paths; (void)maximum; return 0; }
WEAK int ae_mcc_workshop_resource_candidates(const char *level, int type, char (*paths)[1024], int maximum)
{ (void)level; (void)type; (void)paths; (void)maximum; return 0; }
WEAK FILE *ae_mcc_install_open(char const *path) { (void)path; return NULL; }
WEAK int ae_mcc_sound_name_split(char const *name, char *tag, unsigned long size, unsigned long *range, unsigned long *permutation)
{ (void)name; (void)tag; (void)size; (void)range; (void)permutation; return 0; }
WEAK int ae_mcc_sound_permutation(unsigned char const *item, unsigned long item_size, unsigned long range, unsigned long permutation,
    unsigned long *offset, unsigned long *size)
{ (void)item; (void)item_size; (void)range; (void)permutation; (void)offset; (void)size; return 0; }
WEAK unsigned long mcc_identity_client_version(void) { return 0; }
/* (ae_mcc_crc_reuse.h: no CRC kept, so each load reads its resource files as without it) */
WEAK void ae_mcc_crc_reuse_begin(void const *job, char const *name, uint32_t seed, char const paths[2][128])
{ (void)job; (void)name; (void)seed; (void)paths; }
WEAK void ae_mcc_crc_reuse_end(void const *job, uint32_t crc) { (void)job; (void)crc; }
WEAK void ae_mcc_crc_reuse_forget(void const *job) { (void)job; }
WEAK int ae_mcc_crc_reuse_take(char const *name, uint32_t seed, unsigned int mask, char const *bitmaps, char const *sounds,
    uint32_t *checksum)
{ (void)name; (void)seed; (void)mask; (void)bitmaps; (void)sounds; (void)checksum; return 0; }
