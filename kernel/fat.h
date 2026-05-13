/* fat.h -- read-only FAT12 driver.
 *
 * Targets the standard 1.44 MB floppy layout produced by mformat:
 *   sector 0:   boot sector / BIOS Parameter Block
 *   sectors 1..2*spf:                 two copies of the FAT
 *   following 14 sectors:             root directory (224 entries x 32)
 *   following:                        data area (cluster 2 onwards)
 *
 * No subdirectories, no long filenames -- 8.3 names only. The whole
 * point is to be small enough to fit in your head while still being
 * "a real filesystem."
 */
#ifndef NEXUS_FAT_H
#define NEXUS_FAT_H

#include "types.h"

/* Read the BPB and cache layout parameters. Returns false if no FAT
 * filesystem is found (bad signature, unsupported size, etc.). */
bool fat_mount(void);

/* Print the root directory to the console (one entry per line). */
void fat_ls_root(void);

/* Find `name` (in user form, e.g. "HELLO.TXT") in the root and copy up
 * to bufsize bytes of its contents into buf. Returns the actual file
 * size on success, 0 if not found or empty. */
uint32_t fat_read_file(const char *name, void *buf, uint32_t bufsize);

#endif
