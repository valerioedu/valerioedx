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
#define FAT_ATTR_LFN        0x0F

#define FAT_EOC             0x0FFFFFF8  // End of cluster chain

// BIOS Parameter Block (BPB)
typedef struct {
    u8  jmp[3];
    u8  oem[8];
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
    u8  name[11];
    u8  attr;
    u8  nt_reserved;
    u8  create_time_tenths;
    u16 create_time;
    u16 create_date;
    u16 access_date;
    u16 fst_clus_hi;
    u16 write_time;
    u16 write_date;
    u16 fst_clus_lo;
    u32 file_size;
} __attribute__((packed)) fat_dir_entry_t;

// Long File Name entry
typedef struct {
    u8  order;          // Sequence number (1-20), 0x40 = last
    u16 name1[5];       // First 5 UTF-16 chars
    u8  attr;           // Always 0x0F
    u8  type;           // Always 0
    u8  checksum;       // Checksum of short name
    u16 name2[6];       // Next 6 UTF-16 chars
    u16 first_cluster;  // Always 0
    u16 name3[2];       // Final 2 UTF-16 chars
} __attribute__((packed)) fat_lfn_entry_t;

typedef struct {
    inode_t* device;
    u8  sectors_per_cluster;
    u32 bytes_per_cluster;
    u32 fat_begin_lba;
    u32 cluster_begin_lba;
    u32 root_cluster;
    u32 total_clusters;
    u8* fat_cache;          // Cached FAT table
    u32 fat_cache_sector;   // Which FAT sector is cached
} fat32_fs_t;

typedef struct {
    fat32_fs_t* fs;
    u32 first_cluster;
    u32 parent_cluster;     // Parent directory cluster
    u32 dir_entry_cluster;  // Cluster containing this entry
    u32 dir_entry_offset;   // Offset within that cluster
} fat32_file_t;

// File handle for open files
typedef struct {
    inode_t* inode;
    u64 position;           // Current read/write position
    u32 flags;              // Open flags (read/write mode)
} fat32_handle_t;

#define O_RDONLY  0x0001
#define O_WRONLY  0x0002
#define O_RDWR    0x0003
#define O_CREAT   0x0100
#define O_TRUNC   0x0200
#define O_APPEND  0x0400

inode_t* fat32_mount(inode_t* device);

#endif