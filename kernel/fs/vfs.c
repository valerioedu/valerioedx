#include "vfs.h"
#include <kio.h>
#include <heap.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <vmm.h>

#define MAX_MOUNTS 16
#define MAX_SYMLINK_DEPTH 8
#define CACHE_BUCKETS 256
#define MAX_CACHED_PAGES 1024

extern task_t *current_task;

struct mount_entry {
    u64 host_id;
    inode_t *target;
} mount_table[MAX_MOUNTS];

inode_t *vfs_root = NULL;

typedef struct page_cache_entry {
    inode_t *node;
    u64 page_offset;
    uintptr_t phys_addr;
    bool dirty;
    struct page_cache_entry *next;
    struct page_cache_entry *lru_next;
    struct page_cache_entry *lru_prev;
} page_cache_entry_t;

static page_cache_entry_t *page_cache[CACHE_BUCKETS];
static page_cache_entry_t *lru_head = NULL;
static page_cache_entry_t *lru_tail = NULL;
static u32 current_cached_pages = 0;

static void lru_remove(page_cache_entry_t *entry) {
    if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
    else lru_head = entry->lru_next;
    
    if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
    else lru_tail = entry->lru_prev;
}

static void lru_add_front(page_cache_entry_t *entry) {
    entry->lru_next = lru_head;
    entry->lru_prev = NULL;
    if (lru_head) lru_head->lru_prev = entry;

    lru_head = entry;
    if (!lru_tail) lru_tail = entry;
}

static void lru_touch(page_cache_entry_t *entry) {
    if (entry == lru_head) return;
    lru_remove(entry);
    lru_add_front(entry);
}

static int hash_cache(inode_t *node, u64 offset) {
    return (((uintptr_t)node >> 4) ^ (offset >> PAGE_SHIFT)) % CACHE_BUCKETS;
}

static void cache_evict_one() {
    if (!lru_tail) return;
    
    page_cache_entry_t *evict = lru_tail;
    
    // WRITE-BACK: Flush to disk if the page was modified
    if (evict->dirty && evict->node && evict->node->ops && evict->node->ops->write) {
        u64 write_size = PAGE_SIZE;
        if (evict->page_offset + PAGE_SIZE > evict->node->size)
            write_size = evict->node->size - evict->page_offset;
        
        evict->node->ops->write(evict->node, evict->page_offset, write_size, (u8*)P2V(evict->phys_addr));
    }

    int bucket = hash_cache(evict->node, evict->page_offset);
    if (page_cache[bucket] == evict)
        page_cache[bucket] = evict->next;
    
    else {
        page_cache_entry_t *curr = page_cache[bucket];
        while (curr && curr->next != evict)
            curr = curr->next;
        
        if (curr) curr->next = evict->next;
    }

    lru_remove(evict);
    pmm_free_frame(evict->phys_addr);
    kfree(evict);
    current_cached_pages--;
}

static void cache_evict_inode(inode_t *node, bool flush_dirty) {
    page_cache_entry_t *curr = lru_head;
    
    while (curr) {
        page_cache_entry_t *next = curr->lru_next;
        
        if (curr->node == node) {
            if (flush_dirty && curr->dirty && node->ops && node->ops->write) {
                u64 write_size = PAGE_SIZE;
                if (curr->page_offset + PAGE_SIZE > node->size)
                    write_size = node->size - curr->page_offset;
                
                node->ops->write(node, curr->page_offset, write_size, (u8*)P2V(curr->phys_addr));
            }

            int bucket = hash_cache(node, curr->page_offset);
            if (page_cache[bucket] == curr)
                page_cache[bucket] = curr->next;
            
            else {
                page_cache_entry_t *hc = page_cache[bucket];
                while (hc && hc->next != curr)
                    hc = hc->next;
                
                if (hc) hc->next = curr->next;
            }

            lru_remove(curr);
            pmm_free_frame(curr->phys_addr);
            kfree(curr);
            current_cached_pages--;
        }

        curr = next;
    }
}

static page_cache_entry_t *cache_lookup(inode_t *node, u64 offset) {
    int bucket = hash_cache(node, offset);
    page_cache_entry_t *entry = page_cache[bucket];
    while (entry) {
        if (entry->node == node && entry->page_offset == offset)
            return entry;

        entry = entry->next;
    }

    return NULL;
}

static page_cache_entry_t *cache_add(inode_t *node, u64 offset, uintptr_t phys_addr) {
    if (current_cached_pages >= MAX_CACHED_PAGES)
        cache_evict_one();

    int bucket = hash_cache(node, offset);
    page_cache_entry_t *entry = (page_cache_entry_t *)kmalloc(sizeof(page_cache_entry_t));
    if (!entry) return NULL;
    
    entry->node = node;
    entry->page_offset = offset;
    entry->phys_addr = phys_addr;
    entry->dirty = false;
    
    entry->next = page_cache[bucket];
    page_cache[bucket] = entry;

    lru_add_front(entry);
    current_cached_pages++;

    return entry;
}

void vfs_init() {
    vfs_root = NULL;
    memset(mount_table, 0, sizeof(mount_table));
    kprintf("[ [CVFS [W] Virtual File System Initialized\n");
}

int vfs_check_permission(inode_t *node, int mask) {
    if (!node) return -1;
    if (!current_task || !current_task->proc)
        return 0;

    process_t *p = current_task->proc;

    if (p->euid == 0) return 0;

    int mode = node->mode;

    if (p->euid == node->uid)
        mode >>= 6; 

    else if (p->egid == node->gid)
        mode >>= 3;

    if ((mode & mask) == mask) return 0;

    return -1;
}

void vfs_retain(inode_t *node) {
    if (node) node->ref_count++;
}

void vfs_mount_root(inode_t *node) {
    vfs_root = node;
    vfs_root->parent = NULL;
    vfs_retain(vfs_root);
    kprintf("[ [CVFS[W ] Root mounted: %s\n", node->name);
}

int vfs_mount(inode_t* mountpoint, inode_t* fs_root) {
    if (!mountpoint || !fs_root) return 0;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].target == NULL) {
            mount_table[i].host_id = mountpoint->id; // Bind by ID, not pointer
            mount_table[i].target = fs_root;
            return 1;
        }
    }

    return 0;
}

u64 vfs_read(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (vfs_check_permission(node, 4) != 0)
        return (u64)-1;

    if (!node || !node->ops || !node->ops->read)
        return 0;

    // Devices and directories bypass the cache
    if (!S_ISREG(node->mode))
        return node->ops->read(node, offset, size, buffer);

    u64 bytes_read = 0;
    while (bytes_read < size) {
        u64 current_offset = offset + bytes_read;
        u64 page_offset = current_offset & ~(PAGE_SIZE - 1);
        u64 offset_in_page = current_offset & (PAGE_SIZE - 1);
        u64 bytes_to_read = PAGE_SIZE - offset_in_page;

        if (bytes_read + bytes_to_read > size)
            bytes_to_read = size - bytes_read;
        
        if (current_offset >= node->size)
            break;

        if (current_offset + bytes_to_read > node->size)
            bytes_to_read = node->size - current_offset;

        page_cache_entry_t *entry = cache_lookup(node, page_offset);
        if (entry) lru_touch(entry); // Cache hit, move to front of LRU
        else {
            uintptr_t frame = pmm_alloc_frame();
            if (!frame) break;

            u64 disk_read_size = PAGE_SIZE;
            if (page_offset + PAGE_SIZE > node->size)
                disk_read_size = node->size - page_offset;

            memset((u8*)P2V(frame), 0, PAGE_SIZE);
            node->ops->read(node, page_offset, disk_read_size, (u8*)P2V(frame));

            entry = cache_add(node, page_offset, frame);
            if (!entry) {
                pmm_free_frame(frame);
                break;
            }
        }

        // Copy from cached page to user/kernel buffer
        memcpy(buffer + bytes_read, (u8*)P2V(entry->phys_addr) + offset_in_page, bytes_to_read);
        bytes_read += bytes_to_read;
    }

    return bytes_read;
}

u64 vfs_write(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (vfs_check_permission(node, 2) != 0)
        return (u64)-1;
    
    if (!node || !node->ops || !node->ops->write)
        return 0;

    if (!S_ISREG(node->mode))
        return node->ops->write(node, offset, size, buffer);

    u64 bytes_written = 0;
    while (bytes_written < size) {
        u64 current_offset = offset + bytes_written;
        u64 page_offset = current_offset & ~(PAGE_SIZE - 1);
        u64 offset_in_page = current_offset & (PAGE_SIZE - 1);
        u64 bytes_to_write = PAGE_SIZE - offset_in_page;

        if (bytes_written + bytes_to_write > size)
            bytes_to_write = size - bytes_written;

        page_cache_entry_t *entry = cache_lookup(node, page_offset);
        if (entry) lru_touch(entry);
        else {
            uintptr_t frame = pmm_alloc_frame();
            if (!frame) break;

            // If doing a partial write, read the existing data first
            if (bytes_to_write < PAGE_SIZE && current_offset < node->size) {
                u64 disk_read_size = PAGE_SIZE;
                if (page_offset + PAGE_SIZE > node->size)
                    disk_read_size = node->size - page_offset;
                    
                node->ops->read(node, page_offset, disk_read_size, (u8*)P2V(frame));
            } else memset((u8*)P2V(frame), 0, PAGE_SIZE);

            entry = cache_add(node, page_offset, frame);
            if (!entry) {
                pmm_free_frame(frame);
                break;
            }
        }

        memcpy((u8*)P2V(entry->phys_addr) + offset_in_page, buffer + bytes_written, bytes_to_write);
        entry->dirty = true; 
        
        bytes_written += bytes_to_write;
    }

    if (offset + bytes_written > node->size)
        node->size = offset + bytes_written;

    return bytes_written;
}

void vfs_open(inode_t* node) {
    if (!node) return;

    node->ref_count++;
    
    if (node && node->ops && node->ops->open)
        node->ops->open(node);
}

void vfs_close(inode_t* node) {
    if (!node) return;

    node->ref_count--;
    if (node->ref_count <= 0) {
        cache_evict_inode(node, true);

        if (node->flags & FS_TEMPORARY && node->ops->close) {
            if (node->ops && node->ops->close)
                node->ops->close(node); // Free private driver data (fat32_file_t)
            
            if (node->flags & FS_TEMPORARY)
                kfree(node); // Free the VFS node itself
        }
    }
}

inode_t *vfs_create(inode_t *node, const char *name) {
    if (!node) return NULL;

    if (node && node->ops->create)
        return node->ops->create(node, name);

    return NULL;
}

int vfs_unlink(struct vfs_node *parent, const char *name) {
    if (!parent || !name)
        return -1;

    if (parent && parent->ops->unlink)
        return parent->ops->unlink(parent, name);

    return -1;
}

inode_t *vfs_symlink(inode_t *parent, const char *name, const char *target) {
    if (!parent || !name || !target)
        return NULL;

    if (vfs_check_permission(parent, 2) != 0)
        return NULL;

    if (parent->ops && parent->ops->symlink)
        return parent->ops->symlink(parent, name, target);

    return NULL;
}

inode_t *namei(const char *path) {
    if (!vfs_root || !path) return NULL;

    int symlink_depth = 0;
    char *resolved_path = (char*)kmalloc(strlen(path) + 1);
    strcpy(resolved_path, path);

restart:
    if (symlink_depth >= MAX_SYMLINK_DEPTH) {
        kfree(resolved_path);
        return NULL;
    }

    inode_t *current;
    
    if (resolved_path[0] == '/') current = vfs_root;
    else {
        if (current_task && current_task->proc && current_task->proc->cwd) {
            current = current_task->proc->cwd;
        } else {
            current = vfs_root; 
        }
    }

    vfs_retain(current);

    if (current->mount_point) {
        inode_t *next = current->mount_point;
        vfs_retain(next);
        vfs_close(current);
        current = next;
    }

    char *copy = (char*)kmalloc(strlen(resolved_path) + 1);
    strcpy(copy, resolved_path);

    char *saveptr;
    char *token = strtok_r(copy, "/", &saveptr);

    while (token) {
        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        if (strcmp(token, "..") == 0) {
            if (current->parent) {
                inode_t *parent = current->parent;
                vfs_retain(parent);
                vfs_close(current);
                current = parent;
            }

            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        inode_t* next = NULL;
        if (current == vfs_root && strcmp(token, "dev") == 0) {
            next = devfs_get_root();
            vfs_retain(next); 
        } else {
            next = current->ops->lookup(current, token);
        }

        if (!next) {
            vfs_close(current);
            kfree(copy);
            kfree(resolved_path);
            return NULL;
        }

        next->parent = current;
        
        vfs_retain(current);
        vfs_close(current);

        current = next;

        // Symlink resolution
        if (current->flags == FS_SYMLINK) {
            char link_buf[256];
            memset(link_buf, 0, sizeof(link_buf));

            u64 len = vfs_read(current, 0, sizeof(link_buf) - 1, (u8*)link_buf);
            if (len == 0 || len == (u64)-1) {
                vfs_close(current);
                kfree(copy);
                kfree(resolved_path);
                return NULL;
            }
            link_buf[len] = '\0';

            // Build new path: link_target + "/" + remaining components
            char *remaining = saveptr;
            u64 total = strlen(link_buf) + 1 + (remaining ? strlen(remaining) : 0) + 1;
            char *new_path = (char*)kmalloc(total);
            strcpy(new_path, link_buf);
            if (remaining && *remaining) {
                strcat(new_path, "/");
                strcat(new_path, remaining);
            }

            vfs_close(current);
            kfree(copy);
            kfree(resolved_path);

            resolved_path = new_path;
            symlink_depth++;
            goto restart;
        }

        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (mount_table[i].target != NULL && \
                mount_table[i].host_id == current->id) {
                inode_t *mounted_root = mount_table[i].target;

                vfs_retain(mounted_root);

                vfs_close(current);

                current = mounted_root;
                break;
            }
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    kfree(copy);
    kfree(resolved_path);
    return current;
}

int vfs_link(inode_t *parent, const char *name, inode_t *target) {
    if (!parent || !name || !target) return -1;

    if (vfs_check_permission(parent, 2) != 0)
        return -1;

    if (parent->ops && parent->ops->link)
        return parent->ops->link(parent, name, target);

    return -1;
}

inode_t *vfs_mknod(inode_t *parent, const char *name, int mode, int dev) {
    if (!parent || !name) return NULL;

    if (vfs_check_permission(parent, 2) != 0)
        return NULL;

    if (parent->ops && parent->ops->mknod)
        return parent->ops->mknod(parent, name, mode, dev);

    return NULL;
}