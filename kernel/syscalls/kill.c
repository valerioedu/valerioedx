#include <signal.h>
#include <sched.h>
#include <heap.h>
#include <string.h>
#include <kio.h>
#include <syscalls.h>
#include <vmm.h>
#include <vma.h>
#include <pmm.h>

//TODO: Check for ^C outside of read too

#define offsetof(type, member) ((size_t) &((type *)0)->member)

extern void sys_exit(int code);

extern task_t *current_task;

static const u8 sig_default_action[NSIG + 1] = {
    [0]         = SIG_ACTION_IGN,
    [SIGHUP]    = SIG_ACTION_TERM,
    [SIGINT]    = SIG_ACTION_TERM,
    [SIGQUIT]   = SIG_ACTION_CORE,
    [SIGILL]    = SIG_ACTION_CORE,
    [SIGTRAP]   = SIG_ACTION_CORE,
    [SIGABRT]   = SIG_ACTION_CORE,
    [SIGBUS]    = SIG_ACTION_CORE,
    [SIGFPE]    = SIG_ACTION_CORE,
    [SIGKILL]   = SIG_ACTION_TERM,
    [SIGUSR1]   = SIG_ACTION_TERM,
    [SIGSEGV]   = SIG_ACTION_CORE,
    [SIGUSR2]   = SIG_ACTION_TERM,
    [SIGPIPE]   = SIG_ACTION_TERM,
    [SIGALRM]   = SIG_ACTION_TERM,
    [SIGTERM]   = SIG_ACTION_TERM,
    [SIGSTKFLT] = SIG_ACTION_TERM,
    [SIGCHLD]   = SIG_ACTION_IGN,
    [SIGCONT]   = SIG_ACTION_CONT,
    [SIGSTOP]   = SIG_ACTION_STOP,
    [SIGTSTP]   = SIG_ACTION_STOP,
    [SIGTTIN]   = SIG_ACTION_STOP,
    [SIGTTOU]   = SIG_ACTION_STOP,
    [SIGURG]    = SIG_ACTION_IGN,
    [SIGXCPU]   = SIG_ACTION_CORE,
    [SIGXFSZ]   = SIG_ACTION_CORE,
    [SIGVTALRM] = SIG_ACTION_TERM,
    [SIGPROF]   = SIG_ACTION_TERM,
    [SIGWINCH]  = SIG_ACTION_IGN,
    [SIGIO]     = SIG_ACTION_TERM,
    [SIGPWR]    = SIG_ACTION_TERM,
    [SIGSYS]    = SIG_ACTION_CORE,
};

i64 sys_kill(i64 pid, int sig);

int signal_get_default_action(int sig) {
    if (sig < 1 || sig > NSIG) return SIG_ACTION_IGN;
    return sig_default_action[sig];
}

signal_struct_t* signal_create(void) {
    signal_struct_t* sig = kmalloc(sizeof(signal_struct_t));
    if (!sig) return NULL;
    
    memset(sig, 0, sizeof(signal_struct_t));
    
    for (int i = 0; i < NSIG; i++) {
        sig->actions[i].sa_handler = SIG_DFL;
        sig->actions[i].sa_mask = 0;
        sig->actions[i].sa_flags = 0;
    }
    
    return sig;
}

void signal_destroy(signal_struct_t* sig) {
    if (sig) kfree(sig);
}

signal_struct_t* signal_copy(signal_struct_t* old) {
    if (!old) return signal_create();
    
    signal_struct_t* new = kmalloc(sizeof(signal_struct_t));
    if (!new) return NULL;
    
    memcpy(new, old, sizeof(signal_struct_t));
    new->pending = 0;
    
    return new;
}

int signal_send(process_t* proc, int sig) {
    if (!proc || sig < 1 || sig > NSIG) return -1;
    
    signal_struct_t* siginfo = proc->signals;
    if (!siginfo) return -1;
    
    if (sig == SIGCONT) {
        siginfo->pending &= ~SIG_STOP_MASK;
        proc->state = PROCESS_ACTIVE;
        task_wake_up_process(proc);
    }
    
    if (sigmask(sig) & SIG_STOP_MASK)
        sigdelset(&siginfo->pending, SIGCONT);

    sigaddset(&siginfo->pending, sig);

    return 0;
}

int signal_send_pid(u64 pid, int sig) {
    process_t* proc = find_process_by_pid(pid);
    if (!proc) return -1;
    
    return signal_send(proc, sig);
}

static int signal_dequeue(signal_struct_t* sig) {
    sigset_t pending = sig->pending & ~sig->blocked;
    
    // SIGKILL and SIGSTOP cannot be blocked
    pending |= sig->pending & SIG_KERNEL_ONLY_MASK;
    
    if (pending == 0) return 0;
    
    // Find first set bit (lowest signal number)
    for (int i = 1; i <= NSIG; i++) {
        if (pending & sigmask(i)) {
            sigdelset(&sig->pending, i);
            return i;
        }
    }
    
    return 0;
}

// Setup signal frame on user stack and modify trapframe to run handler
static int setup_signal_frame(trapframe_t* tf, int sig, sigaction_t* act) {
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -1;
    
    mm_struct_t* mm = current_task->proc->mm;
    signal_struct_t* siginfo = current_task->proc->signals;
    
    // Calculate frame location on user stack
    u64 sp = tf->sp_el0;
    sp -= sizeof(sigframe_t);
    sp &= ~15;  // Align to 16 bytes
    
    u64* pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), sp);
    if (!pte) return -1;
    
    if (!(*pte & PT_VALID)) {
        u64 phys = pmm_alloc_frame();
        if (!phys) return -1;
        memset(P2V(phys), 0, PAGE_SIZE);
        u64 entry = phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
        entry |= (MT_NORMAL << 2);
        entry |= PT_AP_RW_EL0;
        entry |= PT_UXN;
        *pte = entry;
    }
    
    sigframe_t frame;
    memset(&frame, 0, sizeof(frame));
    
    memcpy(frame.x, tf->x, sizeof(frame.x));
    frame.sp = tf->sp_el0;
    frame.pc = tf->elr;
    frame.pstate = tf->spsr;
    frame.old_mask = siginfo->blocked;
    frame.sig = sig;
    
    // Setup sigreturn trampoline
    // mov x8, #119    ; SYS_sigreturn
    // svc #0
    frame.retcode[0] = 0xD2800CE8;  // mov x8, #103
    frame.retcode[1] = 0xD4000001;  // svc #0
    
    u64 frame_phys = (*pte & 0x0000FFFFFFFFF000ULL) + (sp & (PAGE_SIZE - 1));
    
    // Handle potential page boundary crossing
    size_t first_page_bytes = PAGE_SIZE - (sp & (PAGE_SIZE - 1));
    if (first_page_bytes >= sizeof(sigframe_t)) {
        memcpy(P2V(frame_phys), &frame, sizeof(sigframe_t));
    } else {
        // Frame spans two pages
        memcpy(P2V(frame_phys), &frame, first_page_bytes);
        
        u64 next_page = (sp & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
        pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), next_page);
        if (!pte || !(*pte & PT_VALID)) return -1;
        
        u64 next_phys = *pte & 0x0000FFFFFFFFF000ULL;
        memcpy(P2V(next_phys), (u8*)&frame + first_page_bytes, 
               sizeof(sigframe_t) - first_page_bytes);
    }
    
    // Block signals during handler execution
    siginfo->blocked |= act->sa_mask;
    if (!(act->sa_flags & SA_NODEFER))
        sigaddset(&siginfo->blocked, sig);
    
    tf->elr = (u64)act->sa_handler;
    tf->sp_el0 = sp;
    tf->x[0] = sig;                                     // First argument: signal number
    tf->x[30] = sp + offsetof(sigframe_t, retcode);     // Return address
    
    // Reset handler if SA_RESETHAND
    if (act->sa_flags & SA_RESETHAND)
        act->sa_handler = SIG_DFL;
    
    return 0;
}

void signal_check_pending(trapframe_t* tf) {
    if (!current_task || !current_task->proc)
        return;
    
    signal_struct_t* siginfo = current_task->proc->signals;
    if (!siginfo) return;
    
    int sig;
    while ((sig = signal_dequeue(siginfo)) != 0) {
        sigaction_t* act = &siginfo->actions[sig - 1];
        
        if (act->sa_handler == SIG_IGN) {
            // Ignored (but SIGKILL/SIGSTOP cannot be ignored)
            if (sig != SIGKILL && sig != SIGSTOP)
                continue;
        }
        
        if (act->sa_handler == SIG_DFL) {
            // Default action
            int action = signal_get_default_action(sig);
            
            switch (action) {
                case SIG_ACTION_TERM:
                case SIG_ACTION_CORE:
                    // Terminate the process
                    sys_exit(128 + sig);
                    return;
                
                case SIG_ACTION_STOP:
                    current_task->proc->state = PROCESS_STOPPED;
                    current_task->state = TASK_STOPPED;
                    
                    if (current_task->proc->parent) {
                        signal_send(current_task->proc->parent, SIGCHLD);
                        wake_up(&current_task->proc->parent->wait_queue);
                    }
                    
                    schedule(); 
                    continue;
                
                case SIG_ACTION_CONT:
                    // Implicitly handled by signal_send.
                    continue;
                
                case SIG_ACTION_IGN:
                default:
                    continue;
            }
        }
        
        if (setup_signal_frame(tf, sig, act) < 0) {
            sys_exit(128 + sig);
            return;
        }
        
        break;
    }
}

i64 sys_kill(i64 pid, int sig) {
    if (sig < 0 || sig > NSIG) return -1;
    
    // sig == 0 is used to check process existence
    if (sig == 0) {
        if (pid > 0)
            return find_process_by_pid(pid) ? 0 : -1;

        return -1;
    }
    
    if (pid > 0) {
        return signal_send_pid(pid, sig);
    } else if (pid == 0) {
        // Send to all processes in caller's process group
        if (!current_task || !current_task->proc) return -1;
        return signal_send_group(current_task->proc->pgid, sig);
    } else if (pid == -1) {
        int ret = 0;
        for (int i = 2; i < 1024; i++) { //Pid hash size
            process_t *proc = find_process_by_pid(i);
            if (!proc) continue;
            if (signal_send_pid(i, sig) == 0)
                ret++;
        }

        return ret > 0 ? 0 : -1;
    } else return signal_send_group((u64)(-pid), sig);
}

i64 sys_sigaction(int sig, const sigaction_t* act, sigaction_t* oldact) {
    if (sig < 1 || sig > NSIG) return -1;
    if (sig == SIGKILL || sig == SIGSTOP) return -1;
    
    if (!current_task || !current_task->proc) return -1;
    
    signal_struct_t* siginfo = current_task->proc->signals;
    if (!siginfo) return -1;
    
    sigaction_t* sa = &siginfo->actions[sig - 1];
    
    // Return old action if requested
    if (oldact) {
        sigaction_t kold;
        kold.sa_handler = sa->sa_handler;
        kold.sa_mask = sa->sa_mask;
        kold.sa_flags = sa->sa_flags;
        
        if (copy_to_user(oldact, &kold, sizeof(sigaction_t)) != 0)
            return -1;
    }
    
    // Set new action if provided
    if (act) {
        sigaction_t knew;
        if (copy_from_user(&knew, act, sizeof(sigaction_t)) != 0)
            return -1;
        
        sa->sa_handler = knew.sa_handler;
        sa->sa_mask = knew.sa_mask & ~SIG_KERNEL_ONLY_MASK;
        sa->sa_flags = knew.sa_flags;
    }
    
    return 0;
}

i64 sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    if (!current_task || !current_task->proc) return -1;
    
    signal_struct_t* siginfo = current_task->proc->signals;
    if (!siginfo) return -1;
    
    // Return old mask if requested
    if (oldset) {
        if (copy_to_user(oldset, &siginfo->blocked, sizeof(sigset_t)) != 0)
            return -1;
    }
    
    // Modify mask if set provided
    if (set) {
        sigset_t kset;
        if (copy_from_user(&kset, set, sizeof(sigset_t)) != 0)
            return -1;
        
        // Cannot block SIGKILL or SIGSTOP
        kset &= ~SIG_KERNEL_ONLY_MASK;
        
        switch (how) {
            case SIG_BLOCK:
                siginfo->blocked |= kset;
                break;
            case SIG_UNBLOCK:
                siginfo->blocked &= ~kset;
                break;
            case SIG_SETMASK:
                siginfo->blocked = kset;
                break;
            default:
                return -1;
        }
    }
    
    return 0;
}

i64 sys_sigpending(sigset_t* set) {
    if (!set) return -1;
    if (!current_task || !current_task->proc) return -1;
    
    signal_struct_t* siginfo = current_task->proc->signals;
    if (!siginfo) return -1;
    
    if (copy_to_user(set, &siginfo->pending, sizeof(sigset_t)) != 0)
        return -1;
    
    return 0;
}

i64 sys_sigreturn(trapframe_t* tf) {
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -1;
    
    mm_struct_t* mm = current_task->proc->mm;
    signal_struct_t* siginfo = current_task->proc->signals;
    if (!siginfo) return -1;
    
    // The stack pointer points to the sigframe
    u64 sp = tf->sp_el0;
    
    // Read signal frame from user stack
    sigframe_t frame;
    
    u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), sp);
    if (!pte || !(*pte & PT_VALID)) return -1;
    
    u64 frame_phys = (*pte & 0x0000FFFFFFFFF000ULL) + (sp & (PAGE_SIZE - 1));
    
    size_t first_page_bytes = PAGE_SIZE - (sp & (PAGE_SIZE - 1));
    if (first_page_bytes >= sizeof(sigframe_t)) {
        memcpy(&frame, P2V(frame_phys), sizeof(sigframe_t));
    } else {
        memcpy(&frame, P2V(frame_phys), first_page_bytes);
        
        u64 next_page = (sp & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
        pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), next_page);
        if (!pte || !(*pte & PT_VALID)) return -1;
        
        u64 next_phys = *pte & 0x0000FFFFFFFFF000ULL;
        memcpy((u8*)&frame + first_page_bytes, P2V(next_phys),
               sizeof(sigframe_t) - first_page_bytes);
    }
    
    memcpy(tf->x, frame.x, sizeof(frame.x));
    tf->sp_el0 = frame.sp;
    tf->elr = frame.pc;
    tf->spsr = frame.pstate;
    
    // Restore signal mask
    siginfo->blocked = frame.old_mask & ~SIG_KERNEL_ONLY_MASK;
    
    return tf->x[0];
}