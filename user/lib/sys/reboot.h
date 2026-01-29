#ifndef REBOOT_H
#define REBOOT_H

#define RB_AUTOBOOT     0
#define RB_HALT         0x08
#define RB_POWEROFF     0x4000

int reboot(int howto);

#endif