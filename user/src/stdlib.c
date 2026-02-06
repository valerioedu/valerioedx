#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define ALIGNMENT 16
#define PAGE_SIZE 4096

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLOCK_HEADER_SIZE sizeof(block_meta_t)

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    struct block_meta *prev;
    int free;
    int magic;
} block_meta_t;

#define MAGIC 0xDEADBEEF

static block_meta_t *global_base = NULL;

static block_meta_t *request_space(block_meta_t *last, size_t size) {
    block_meta_t *block;
    size_t alloc_size = size + BLOCK_HEADER_SIZE;
    
    // Round up to nearest page multiple
    size_t num_pages = (alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
    alloc_size = num_pages * PAGE_SIZE;

    block = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, 
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (block == MAP_FAILED)
        return NULL;

    if (last) last->next = block;

    block->size = alloc_size - BLOCK_HEADER_SIZE;
    block->next = NULL;
    block->prev = last;
    block->free = 1;
    block->magic = MAGIC;

    return block;
}

static block_meta_t *find_free_block(block_meta_t **last, size_t size) {
    block_meta_t *current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }

    return current;
}

static void split_block(block_meta_t *block, size_t size) {
    // Only split if the remaining space fits a header + some data (alignment size)
    if (block->size >= size + BLOCK_HEADER_SIZE + ALIGNMENT) {
        block_meta_t *new_block = (block_meta_t *)((char *)block + BLOCK_HEADER_SIZE + size);
        
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->free = 1;
        new_block->magic = MAGIC;
        
        if (new_block->next)
            new_block->next->prev = new_block;
        
        block->size = size;
        block->next = new_block;
    }
}

void *malloc(size_t size) {
    if (size <= 0) return NULL;

    size_t aligned_size = ALIGN(size);
    block_meta_t *block;

    if (!global_base) {
        block = request_space(NULL, aligned_size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block_meta_t *last = global_base;
        block = find_free_block(&last, aligned_size);
        
        if (!block) {
            block = request_space(last, aligned_size);
            if (!block) return NULL;
        } else {
            split_block(block, aligned_size);
            block->free = 0;
        }
    }
    
    if (block->free) {
        split_block(block, aligned_size);
        block->free = 0;
    }

    return (void *)(block + 1);
}

void free(void *ptr) {
    if (!ptr) return;

    block_meta_t *block = (block_meta_t *)ptr - 1;
    
    // Pointer invalid or corruption detected
    if (block->magic != MAGIC) return;

    block->free = 1;

    // Coalesce with next block if it is free and physically adjacent
    if (block->next && block->next->free) {
        if ((char *)block + BLOCK_HEADER_SIZE + block->size == (char *)block->next) {
            block->size += BLOCK_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            if (block->next)
                block->next->prev = block;
        }
    }

    // Coalesce with previous block if it is free and physically adjacent
    if (block->prev && block->prev->free) {
        if ((char *)block->prev + BLOCK_HEADER_SIZE + block->prev->size == (char *)block) {
            block->prev->size += BLOCK_HEADER_SIZE + block->size;
            block->prev->next = block->next;
            if (block->next)
                block->next->prev = block->prev;
        }
    }
}

void *calloc(size_t nelem, size_t elsize) {
    size_t size = nelem * elsize;
    void *ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_meta_t *block = (block_meta_t *)ptr - 1;
    if (block->size >= size)
        return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

int atoi(const char *str) {
    if (!str) return 0;
    
    while (*str == ' ' || *str == '\t' ||
        *str == '\n' || *str == '\v' || 
        *str == '\f' || *str == '\r')
        str++;

    int sign = 1;
    if (*str == '+' || *str == '-') {
        if (*str == '-') sign = -1;
        str++;
    }

    long result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return (int)(result * sign);
}

void exit(int status) {
    fflush(NULL);
    _exit(status);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc;
    int c;
    unsigned long cutoff;
    int neg = 0, any, cutlim;

    if (base < 0 || base == 1 || base > 36) {
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }

    while (*s == ' ' || *s == '\t' ||
           *s == '\n' || *s == '\v' || 
           *s == '\f' || *s == '\r')
        s++;

    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') s++;

    if ((base == 0 || base == 16) &&
        *s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        c = s[2];
        if ((c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F')) {
            base = 16;
            s += 2;
        }
    }

    if (base == 0) {
        if (*s == '0') base = 8;
        else base = 10;
    }

    cutoff = (unsigned long)~0 / (unsigned long)base;
    cutlim = (unsigned long)~0 % (unsigned long)base;

    for (acc = 0, any = 0;; s++) {
        c = *s;
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'A' && c <= 'Z') c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z') c -= 'a' - 10;
        else break;
        
        if (c >= base) break;

        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1; // Overflow
        
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }

    if (any < 0) acc = (unsigned long)~0;
    else if (neg) acc = -acc;

    if (endptr != 0)
        *endptr = (char *)(any ? s : nptr);

    return acc;
}

#define BITMAP_INDEX(i) ((i) >> 3)
#define BITMAP_OFFSET(i) ((i) & 0x07)
#define GET_BIT(map, i)  ((map[BITMAP_INDEX(i)] >> BITMAP_OFFSET(i)) & 1)
#define SET_BIT(map, i)  (map[BITMAP_INDEX(i)] |= (1 << BITMAP_OFFSET(i)))
#define CLR_BIT(map, i)  (map[BITMAP_INDEX(i)] &= ~(1 << BITMAP_OFFSET(i)))

char **environ = NULL;

static uint8_t *env_alloc_map = NULL;
static int __environ_malloced = 0;

static char *create_entry(const char *name, const char *value) {
    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    char *entry = malloc(nlen + vlen + 2);
    if (entry) {
        memcpy(entry, name, nlen);
        entry[nlen] = '=';
        strcpy(entry + nlen + 1, value);
    }

    return entry;
}

static int ensure_environ_capacity(size_t required_count) {
    size_t current_count = 0;
    if (environ) {
        while (environ[current_count]) current_count++;
    }

    if (!__environ_malloced) {
        size_t new_size = (current_count + 2) * sizeof(char *);
        char **new_environ = malloc(new_size);
        if (!new_environ) return -1;

        size_t map_bytes = (current_count + 8) / 8;
        uint8_t *new_map = calloc(map_bytes, 1);
        if (!new_map) {
            free(new_environ);
            return -1;
        }

        for (size_t i = 0; i < current_count; i++)
            new_environ[i] = environ[i];

        new_environ[current_count] = NULL;

        environ = new_environ;
        env_alloc_map = new_map;
        __environ_malloced = 1;

    } else if (required_count > current_count) {
        size_t new_size = (required_count + 2) * sizeof(char *);
        char **new_env = realloc(environ, new_size);
        if (!new_env) return -1;

        environ = new_env;

        size_t old_map_bytes = (current_count + 7) / 8;
        size_t new_map_bytes = (required_count + 8) / 8;

        if (new_map_bytes > old_map_bytes) {
            uint8_t *new_map = realloc(env_alloc_map, new_map_bytes);
            if (!new_map) return -1;

            memset(new_map + old_map_bytes, 0, new_map_bytes - old_map_bytes);
            env_alloc_map = new_map;
        }
    }

    return 0;
}

char *getenv(const char *name) {
    if (!environ || !name) return NULL;
    size_t len = strlen(name);
    for (char **env = environ; *env != NULL; env++) {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=')
            return *env + len + 1;

    }

    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !value) return -1;
    if (strchr(name, '=')) return -1;

    if (ensure_environ_capacity(0) != 0) return -1;

    size_t len = strlen(name);
    size_t i = 0;

    for (; environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            if (!overwrite) return 0;

            if (GET_BIT(env_alloc_map, i))
                free(environ[i]);

            environ[i] = create_entry(name, value);
            if (!environ[i]) return -1;
            
            SET_BIT(env_alloc_map, i);
            return 0;
        }
    }

    if (ensure_environ_capacity(i + 1) != 0) return -1;

    environ[i] = create_entry(name, value);
    if (!environ[i]) return -1;

    SET_BIT(env_alloc_map, i);
    environ[i + 1] = NULL;

    return 0;
}

int unsetenv(const char *name) {
    if (!name || !environ) return -1;
    if (strchr(name, '=')) return -1;

    if (ensure_environ_capacity(0) != 0) return -1;

    size_t len = strlen(name);
    size_t i = 0;

    while (environ[i]) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            if (GET_BIT(env_alloc_map, i))
                free(environ[i]);

            size_t j = i;
            while (environ[j]) {
                environ[j] = environ[j+1];
                
                if (environ[j]) {
                    if (GET_BIT(env_alloc_map, j + 1))
                        SET_BIT(env_alloc_map, j);

                    else CLR_BIT(env_alloc_map, j);
                }

                j++;
            }
            
            continue; 
        }
        
        i++;
    }

    return 0;
}