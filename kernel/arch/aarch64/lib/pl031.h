#ifndef PL031_H
#define PL031_H

#include <lib.h>

#define PL031_DR        0x000   /* Data Register */
#define PL031_MR        0x004   /* Match Register */
#define PL031_LR        0x008   /* Load Register */
#define PL031_CR        0x00C   /* Control Register */
#define PL031_IMSC      0x010   /* Interrupt Mask Set/Clear */
#define PL031_RIS       0x014   /* Raw Interrupt Status */
#define PL031_MIS       0x018   /* Masked Interrupt Status */
#define PL031_ICR       0x01C   /* Interrupt Clear Register */

void pl031_init_time();
void get_monotonic_time(u64 *sec, u64 *nsec);
void get_realtime(u64 *sec, u64 *nsec);
u32 pl031_get_time();
void pl031_set_time(u32 timestamp);

#endif