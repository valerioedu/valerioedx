#include <sync.h>
#include <lib.h>
#include <sched.h> 

void sem_init(semaphore_t* sem, int count) {
    sem->lock = 0;
    sem->count = count;
    sem->wait_list = NULL;
}

void sem_wait(semaphore_t* sem) {
    while (true) {
        u32 flags = spinlock_acquire_irqsave(&sem->lock);

        if (sem->count > 0) {
            sem->count--;
            spinlock_release_irqrestore(&sem->lock, flags);
            return;
        }

        spinlock_release_irqrestore(&sem->lock, flags);

        sleep_on(&sem->wait_list);
    }
}

void sem_signal(semaphore_t* sem) {
    u32 flags = spinlock_acquire_irqsave(&sem->lock);

    sem->count++;

    wake_up(&sem->wait_list);

    spinlock_release_irqrestore(&sem->lock, flags);
}

void mutex_init(mutex_t* mutex) {
    mutex->lock = 0;
    mutex->locked = 0;
    mutex->wait_list = NULL;
}

void mutex_acquire(mutex_t* mutex) {
    while (true) {
        u32 flags = spinlock_acquire_irqsave(&mutex->lock);

        if (!mutex->locked) {
            mutex->locked = true;
            spinlock_release_irqrestore(&mutex->lock, flags);
            return;
        }

        spinlock_release_irqrestore(&mutex->lock, flags);
        
        sleep_on(&mutex->wait_list);
    }
}

void mutex_release(mutex_t* mutex) {
    u32 flags = spinlock_acquire_irqsave(&mutex->lock);

    mutex->locked = false;
    
    wake_up(&mutex->wait_list);

    spinlock_release_irqrestore(&mutex->lock, flags);
}