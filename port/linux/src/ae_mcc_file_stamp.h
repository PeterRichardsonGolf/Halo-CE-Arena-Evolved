/*
AE_MCC_FILE_STAMP.H

A file's stamp, to tell whether it is the same file with the same bytes as when the stamp was taken: its size, its
last write time and its change time (which any write, rename or link onto the path moves, and which no program can
put back on Linux or macOS), and on Windows the volume and file index too. Only unsigned int crosses here, so the game's
units and the platform layer agree on the structure in every build. Paths are the host's.
*/

#ifndef AE_MCC_FILE_STAMP_H
#define AE_MCC_FILE_STAMP_H

struct ae_mcc_file_stamp
{
	unsigned int size_low, size_high;
	unsigned int written_seconds, written_fraction;
	unsigned int changed_seconds, changed_fraction;
	unsigned int volume, index_low, index_high;
};

/* the file's stamp: 1 taken, 0 not (no such file, a folder, or macOS, which has no change time here; the stamp is
zeroed) */
int ae_mcc_file_stamp(char const *path, struct ae_mcc_file_stamp *stamp);

#endif
