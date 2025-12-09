#ifndef FAT32_H
#define FAT32_H

#include <lib.h>
#include <vfs.h>

// Attribute Flags
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

// BIOS Parameter Block (BPB)
typedef struct {
    u8  jmp_boot[3];
    u8  oem_name[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  num_fats;
    u16 root_entry_count;
    u16 total_sectors_16;
    u8  media_type;
    u16 sectors_per_fat_16;
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
    
    // FAT32 Extended Fields
    u32 sectors_per_fat_32;
    u16 ext_flags;
    u16 fs_version;
    u32 root_cluster;
    u16 fs_info;
    u16 backup_boot_sector;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  boot_signature;
    u32 volume_id;
    u8  volume_label[11];
    u8  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

// Directory Entry (32 bytes)
typedef struct {
    u8  name[11];      // 8.3 format
    u8  attr;
    u8  nt_res;
    u8  crt_time_tenth;
    u16 crt_time;
    u16 crt_date;
    u16 lst_acc_date;
    u16 fst_clus_hi;   // High word of first cluster
    u16 wrt_time;
    u16 wrt_date;
    u16 fst_clus_lo;   // Low word of first cluster
    u32 file_size;
} __attribute__((packed)) fat_dir_entry_t;

// Internal structure to store FS state in inode->ptr
typedef struct {
    inode_t* device;       // Underlying block device (virtio-blk)
    u32 cluster_begin_lba;
    u32 fat_begin_lba;
    u32 sectors_per_cluster;
    u32 bytes_per_cluster;
    u32 root_cluster;
} fat32_fs_t;

// Internal structure for specific file data in inode->ptr
typedef struct {
    fat32_fs_t* fs;
    u32 first_cluster;
    u32 current_cluster;
    u32 current_offset;
} fat32_file_t;

inode_t* fat32_mount(inode_t* device);

#endif