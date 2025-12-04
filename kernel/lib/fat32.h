#ifndef FAT32_H
#define FAT32_H

#include <lib.h>
#include <vfs.h>

typedef struct {
    u8  jmp[3];
    char oem[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  fat_count;
    u16 root_dir_entries;
    u16 total_sectors_16;
    u8  media_descriptor;
    u16 sectors_per_fat_16;
    u16 sectors_per_track;
    u16 head_count;
    u32 hidden_sectors;
    u32 total_sectors_32;
    // FAT32 Extended
    u32 sectors_per_fat_32;
    u16 flags;
    u16 fat_version;
    u32 root_cluster;
    u16 fs_info_sector;
    u16 backup_boot_sector;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  boot_signature;
    u32 volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed)) fat_bpb_t;

typedef struct {
    char name[11];
    u8  attr;
    u8  reserved;
    u8  ctime_tenth;
    u16 ctime;
    u16 cdate;
    u16 adate;
    u16 cluster_high;
    u16 wtime;
    u16 wdate;
    u16 cluster_low;
    u32 size;
} __attribute__((packed)) fat_dir_entry_t;

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

inode_t* fat32_init(inode_t* block_device);

#endif