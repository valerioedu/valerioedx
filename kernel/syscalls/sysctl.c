#include <lib.h>
#include <string.h>
#include <dtb.h>
#include <timer.h>
#include <syscalls.h>
#include <sched.h>

#define CTL_HW      1
#define CTL_KERN    2
#define CTL_VM      3

#define HW_MEMSIZE      1
#define HW_NCPU         2
#define HW_PAGESIZE     3
#define HW_CPUFREQ      4
#define HW_TBFREQ       5
#define HW_CACHELINE    6
#define HW_MODEL        7
#define HW_MACHINE      8

#define KERN_OSTYPE     1
#define KERN_OSRELEASE  2
#define KERN_VERSION    3
#define KERN_HOSTNAME   4
#define KERN_BOOTTIME   5
#define KERN_MAXPROC    6
#define KERN_MAXFILES   7
#define KERN_NPROCS     8

#define VM_TOTAL        1
#define VM_FREE         2
#define VM_USED         3

typedef struct {
    u64 memsize;          // hw.memsize - Physical memory in bytes
    u64 physmem;          // hw.physmem - Same as memsize
    u32 ncpu;             // hw.ncpu - Number of CPUs
    u32 pagesize;         // hw.pagesize - Page size (4096)
    u64 cpufrequency;     // hw.cpufrequency - CPU frequency in Hz
    u64 tbfrequency;      // hw.tbfrequency - Timebase/timer frequency
    u32 cachelinesize;    // hw.cachelinesize - Cache line size
    u32 l1icachesize;     // hw.l1icachesize - L1 instruction cache
    u32 l1dcachesize;     // hw.l1dcachesize - L1 data cache
    u32 l2cachesize;      // hw.l2cachesize - L2 cache size
    char model[64];       // hw.model - Machine model string
    char machine[64];     // hw.machine - Machine type (compatible)
    char serial[32];      // hw.serial - Serial number (if available)
} sysctl_hw_t;

typedef struct {
    char ostype[32];      // kern.ostype - "ValerioEDX"
    char osrelease[32];   // kern.osrelease - Kernel version "1.0.0"
    char version[128];    // kern.version - Full version string
    char hostname[64];    // kern.hostname - System hostname
    u64 boottime;         // kern.boottime - Boot timestamp (ticks)
    u32 maxproc;          // kern.maxproc - Max processes
    u32 maxfiles;         // kern.maxfiles - Max open files
    u32 nprocs;           // kern.nprocs - Current process count
    char bootargs[256];   // kern.bootargs - Kernel command line
    char consdev[64];     // kern.consdev - Console device path
} sysctl_kern_t;

typedef struct {
    u64 total;            // vm.total - Total memory
    u64 free;             // vm.free - Free memory
    u64 used;             // vm.used - Used memory
    u64 wired;            // vm.wired - Wired/pinned memory
    u64 swaptotal;        // vm.swaptotal - Total swap
    u64 swapused;         // vm.swapused - Used swap
    u32 pageins;          // vm.pageins - Pages paged in
    u32 pageouts;         // vm.pageouts - Pages paged out
} sysctl_vm_t;

sysctl_hw_t hw = { 0 };
sysctl_kern_t kern = { 0 };
sysctl_vm_t vm = { 0 };

void sysctl_init() {
    hw.memsize = hw.physmem = dtb_get_memory_size();
    hw.ncpu = dtb_get_cpu_count();
    hw.pagesize = 4096;
    hw.cachelinesize = 64;
    hw.tbfrequency = timer_get_frq();
    hw.cpufrequency = dtb_get_cpu_freq();
    dtb_get_model(hw.model, 64);
    dtb_get_compatible(hw.machine, 64);

    strcpy(kern.ostype, "valerioedx");
    strcpy(kern.osrelease, "0.0.0");
    strcpy(kern.version, "valerioedx 0.0.0");
    strcpy(kern.hostname, "valerioedx");
    strcpy(kern.consdev, "/dev/tty0");
    kern.maxfiles = MAX_FD;
    extern u64 boot_time;
    kern.boottime = boot_time;
}

//TODO: Implement vm
i64 sys_sysctl(int *name, u32 namelen, void *oldp, u64 *oldlenp, void *newp, u64 newlen) {
    if (namelen < 2) return -1;

    int category = name[0];
    int item = name[1];

    switch (category) {
        case CTL_HW:
            switch (item) {
                case HW_MEMSIZE:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u64)) {
                        *(u64*)oldp = hw.memsize;
                        *oldlenp = sizeof(u64);
                    }

                    return 0;

                case HW_NCPU:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = hw.ncpu;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;

                case HW_PAGESIZE:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = hw.pagesize;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;

                case HW_CPUFREQ:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u64)) {
                        *(u64*)oldp = hw.cpufrequency;
                        *oldlenp = sizeof(u64);
                    }

                    return 0;

                case HW_TBFREQ:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u64)) {
                        *(u64*)oldp = hw.tbfrequency;
                        *oldlenp = sizeof(u64);
                    }

                    return 0;

                case HW_CACHELINE:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = hw.cachelinesize;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;

                case HW_MODEL:
                    if (oldp && oldlenp) {
                        u64 len = strlen(hw.model) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, hw.model);

                        *oldlenp = len;
                    }

                    return 0;

                case HW_MACHINE:
                    if (oldp && oldlenp) {
                        u64 len = strlen(hw.machine) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, hw.machine);

                        *oldlenp = len;
                    }

                    return 0;
            }
            break;

        case CTL_KERN:
            switch (item) {
                case KERN_OSTYPE:
                    if (oldp && oldlenp) {
                        u64 len = strlen(kern.ostype) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, kern.ostype);

                        *oldlenp = len;
                    }

                    return 0;

                case KERN_OSRELEASE:
                    if (oldp && oldlenp) {
                        u64 len = strlen(kern.osrelease) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, kern.osrelease);

                        *oldlenp = len;
                    }

                    return 0;

                case KERN_VERSION:
                    if (oldp && oldlenp) {
                        u64 len = strlen(kern.version) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, kern.version);

                        *oldlenp = len;
                    }

                    return 0;

                case KERN_HOSTNAME:
                    // Readable and writable
                    if (oldp && oldlenp) {
                        u64 len = strlen(kern.hostname) + 1;
                        if (*oldlenp >= len)
                            strcpy(oldp, kern.hostname);

                        *oldlenp = len;
                    }

                    if (newp && newlen > 0 && newlen < sizeof(kern.hostname)) {
                        memcpy(kern.hostname, newp, newlen);
                        kern.hostname[newlen] = '\0';
                    }

                    return 0;

                case KERN_BOOTTIME:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u64)) {
                        *(u64*)oldp = kern.boottime;
                        *oldlenp = sizeof(u64);
                    }

                    return 0;

                case KERN_MAXPROC:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = kern.maxproc;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;

                case KERN_MAXFILES:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = kern.maxfiles;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;

                case KERN_NPROCS:
                    if (oldp && oldlenp && *oldlenp >= sizeof(u32)) {
                        *(u32*)oldp = kern.nprocs;
                        *oldlenp = sizeof(u32);
                    }

                    return 0;
            }
            break;
    }

    return -1; // Unknown sysctl
}