/* fat.c -- FAT12 read-only.
 *
 * Layout & terminology:
 *   - The BPB at sector 0 tells us how everything's sized.
 *   - "Reserved" sectors (typically 1) come first; the BPB itself is here.
 *   - Then `num_fats` copies of the FAT, each `sectors_per_fat` sectors.
 *   - Then the root directory, fixed at `root_entries * 32` bytes.
 *   - Then the data area, addressed in clusters of `sectors_per_cluster`.
 *
 * FAT12 entry encoding: each 12-bit entry is packed into bytes such that
 * two consecutive entries share a byte. To get entry N:
 *   off = N * 3 / 2
 *   if N is even: entry = fat[off] | ((fat[off+1] & 0x0F) << 8)
 *   if N is odd:  entry = (fat[off] >> 4) | (fat[off+1] << 4)
 *   entry &= 0xFFF
 *
 * Entry values:
 *   0x000           free
 *   0x002..0xFEF    next cluster number
 *   0xFF7           bad cluster
 *   0xFF8..0xFFF    end of chain
 *
 * The first valid data cluster is cluster 2. Cluster N maps to sector
 *   data_start + (N - 2) * sectors_per_cluster
 */

#include "fat.h"
#include "ata.h"
#include "console.h"
#include "string.h"
#include "kmalloc.h"

/* Cached BPB-derived layout. */
static struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t sectors_per_fat;

    /* Computed: */
    uint32_t fat_start_sector;       /* = reserved_sectors */
    uint32_t root_start_sector;      /* = fat_start + num_fats * sectors_per_fat */
    uint32_t data_start_sector;      /* = root_start + ceil(root_entries*32 / bps) */
    uint32_t root_sectors;
} fs;

static uint8_t *fat_cache = NULL;    /* whole FAT, in memory after mount     */
static bool     mounted   = false;

/* On-disk directory entry, 32 bytes. */
typedef struct PACKED {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved[10];
    uint16_t mtime;
    uint16_t mdate;
    uint16_t first_cluster;
    uint32_t size;
} dirent_t;

/* "HELLO   TXT" form used on disk -> "HELLO.TXT" form for display. */
static void format_name(const dirent_t *e, char out[13]) {
    int o = 0;
    for (int i = 0; i < 8 && e->name[i] != ' '; i++) out[o++] = e->name[i];
    if (e->ext[0] != ' ') {
        out[o++] = '.';
        for (int i = 0; i < 3 && e->ext[i] != ' '; i++) out[o++] = e->ext[i];
    }
    out[o] = '\0';
}

/* Compare a user-supplied name ("hello.txt") to a directory entry's 8.3
 * fields, case-insensitively. Returns true on match. */
static bool name_matches(const char *user, const dirent_t *e) {
    char fmt[13];
    format_name(e, fmt);

    for (int i = 0; i < 13; i++) {
        char a = user[i], b = fmt[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - 32);
        if (b >= 'a' && b <= 'z') b = (char)(b - 32);
        if (a != b) return false;
        if (a == '\0') return true;
    }
    return false;
}

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Walk the FAT chain starting at first_cluster. */
static uint16_t fat12_next(uint16_t cluster) {
    uint32_t off = (uint32_t)cluster * 3 / 2;
    uint16_t v;
    if (cluster & 1) {
        v = (uint16_t)((fat_cache[off] >> 4) | ((uint16_t)fat_cache[off + 1] << 4));
    } else {
        v = (uint16_t)(fat_cache[off] | ((uint16_t)(fat_cache[off + 1] & 0x0F) << 8));
    }
    return v & 0xFFF;
}

bool fat_mount(void) {
    uint8_t boot[512];
    if (!ata_read_sector(0, boot)) {
        console_puts("[fat] failed to read boot sector\n");
        return false;
    }

    /* Signature check at offset 510 ("55 AA"). */
    if (boot[510] != 0x55 || boot[511] != 0xAA) {
        console_puts("[fat] bad boot signature\n");
        return false;
    }

    fs.bytes_per_sector     = read_u16(boot + 11);
    fs.sectors_per_cluster  = boot[13];
    fs.reserved_sectors     = read_u16(boot + 14);
    fs.num_fats             = boot[16];
    fs.root_entries         = read_u16(boot + 17);
    fs.sectors_per_fat      = read_u16(boot + 22);

    if (fs.bytes_per_sector != 512) {
        console_puts("[fat] unsupported sector size\n");
        return false;
    }

    fs.fat_start_sector  = fs.reserved_sectors;
    fs.root_start_sector = fs.fat_start_sector + (uint32_t)fs.num_fats * fs.sectors_per_fat;
    fs.root_sectors      = ((uint32_t)fs.root_entries * 32 + fs.bytes_per_sector - 1)
                           / fs.bytes_per_sector;
    fs.data_start_sector = fs.root_start_sector + fs.root_sectors;

    /* Cache the entire FAT in memory. For a 1.44MB floppy that's 9 sectors
     * = 4.5 KiB -- a non-issue. */
    uint32_t fat_bytes = (uint32_t)fs.sectors_per_fat * fs.bytes_per_sector;
    fat_cache = (uint8_t *)kmalloc(fat_bytes);
    if (!fat_cache) {
        console_puts("[fat] out of memory caching FAT\n");
        return false;
    }
    for (uint32_t i = 0; i < fs.sectors_per_fat; i++) {
        if (!ata_read_sector(fs.fat_start_sector + i,
                             fat_cache + i * fs.bytes_per_sector)) {
            console_puts("[fat] FAT read failed\n");
            return false;
        }
    }

    mounted = true;
    console_puts("[fat] mounted (");
    console_put_dec(fs.root_entries); console_puts(" root entries, root at sector ");
    console_put_dec(fs.root_start_sector); console_puts(", data at sector ");
    console_put_dec(fs.data_start_sector); console_puts(")\n");
    return true;
}

/* Iterate over all in-use root directory entries, calling fn for each. */
static void walk_root(void (*fn)(const dirent_t *e, void *ctx), void *ctx) {
    uint8_t sector[512];
    for (uint32_t s = 0; s < fs.root_sectors; s++) {
        if (!ata_read_sector(fs.root_start_sector + s, sector)) return;
        const dirent_t *entries = (const dirent_t *)sector;
        uint32_t per_sector = fs.bytes_per_sector / sizeof(dirent_t);
        for (uint32_t i = 0; i < per_sector; i++) {
            const dirent_t *e = &entries[i];
            if ((uint8_t)e->name[0] == 0x00) return;        /* end of dir   */
            if ((uint8_t)e->name[0] == 0xE5) continue;      /* deleted      */
            if (e->attr & 0x0F) continue;                   /* LFN/volume   */
            fn(e, ctx);
        }
    }
}

static void print_entry(const dirent_t *e, void *ctx) {
    (void)ctx;
    char name[13];
    format_name(e, name);
    console_puts("  "); console_puts(name);
    /* Pad to a 14-column name field so sizes line up. */
    for (int i = (int)strlen(name); i < 14; i++) console_putc(' ');
    console_put_dec(e->size); console_puts(" bytes\n");
}

void fat_ls_root(void) {
    if (!mounted) { console_puts("(no filesystem)\n"); return; }
    walk_root(print_entry, NULL);
}

typedef struct {
    const char *want;
    dirent_t    found;
    bool        ok;
} find_ctx_t;

static void find_cb(const dirent_t *e, void *ctx) {
    find_ctx_t *fc = (find_ctx_t *)ctx;
    if (fc->ok) return;
    if (name_matches(fc->want, e)) { fc->found = *e; fc->ok = true; }
}

uint32_t fat_read_file(const char *name, void *buf, uint32_t bufsize) {
    if (!mounted) return 0;

    find_ctx_t fc = { .want = name, .ok = false };
    walk_root(find_cb, &fc);
    if (!fc.ok) return 0;

    uint32_t to_copy   = fc.found.size;
    if (to_copy > bufsize) to_copy = bufsize;

    uint8_t  sector[512];
    uint16_t cluster   = fc.found.first_cluster;
    uint32_t bytes_per_cluster = (uint32_t)fs.sectors_per_cluster * fs.bytes_per_sector;
    uint32_t copied    = 0;

    while (cluster >= 2 && cluster < 0xFF8 && copied < to_copy) {
        uint32_t first_sector = fs.data_start_sector
                              + (uint32_t)(cluster - 2) * fs.sectors_per_cluster;
        for (uint32_t s = 0; s < fs.sectors_per_cluster && copied < to_copy; s++) {
            if (!ata_read_sector(first_sector + s, sector)) return copied;
            uint32_t take = fs.bytes_per_sector;
            if (copied + take > to_copy) take = to_copy - copied;
            memcpy((uint8_t *)buf + copied, sector, take);
            copied += take;
        }
        cluster = fat12_next(cluster);
        (void)bytes_per_cluster;
    }
    return copied;
}
