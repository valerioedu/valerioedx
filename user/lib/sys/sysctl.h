#ifndef SYSCTL_H
#define SYSCTL_H

#include <sys/types.h>

// Sysctl name identifiers
#define CTL_HW      1
#define CTL_KERN    2
#define CTL_VM      3

// hw.* identifiers
#define HW_MEMSIZE      1
#define HW_NCPU         2
#define HW_PAGESIZE     3
#define HW_CPUFREQ      4
#define HW_TBFREQ       5
#define HW_CACHELINE    6
#define HW_MODEL        7
#define HW_MACHINE      8

// kern.* identifiers
#define KERN_OSTYPE     1
#define KERN_OSRELEASE  2
#define KERN_VERSION    3
#define KERN_HOSTNAME   4
#define KERN_BOOTTIME   5
#define KERN_MAXPROC    6
#define KERN_MAXFILES   7
#define KERN_NPROCS     8

// vm.* identifiers
#define VM_TOTAL        1
#define VM_FREE         2
#define VM_USED         3

int sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);

#endif