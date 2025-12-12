#ifndef SYNC_H
#define SYNC_H

#include <lib.h>
#include <spinlock.h>
#include <sched.h>

typedef struct {
    spinlock_t lock;        // Protects the count and wait_list
    int count;              // Resource count
    wait_queue_t wait_list; // List of blocked tasks
} semaphore_t;

void sem_init(semaphore_t* sem, int count);
void sem_wait(semaphore_t* sem);
void sem_signal(semaphore_t* sem);

typedef struct {
    spinlock_t lock;        // Protects the locked state and wait_list
    bool locked;            // 1 if locked, 0 if free
    wait_queue_t wait_list; // List of blocked tasks
} mutex_t;

void mutex_init(mutex_t* mutex);
void mutex_acquire(mutex_t* mutex);
void mutex_release(mutex_t* mutex);

#endif