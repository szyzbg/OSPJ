#include "ksignal.h"

#include <defs.h>
#include <proc.h>
#include <trap.h>

/**
 * @brief init the signal struct inside a PCB.
 * 
 * @param p 
 * @return int 
 */
int siginit(struct proc *p) {
    // init p->signal
    return 0;
}

int siginit_fork(struct proc *parent, struct proc *child) {
    // copy parent's sigactions and signal mask
    // but clear all pending signals
    return 0;
}

int siginit_exec(struct proc *p) {
    // inherit signal mask and pending signals.
    // but reset all sigactions (except ignored) to default.
    return 0;
}

int do_signal(void) {
    assert(!intr_get());

    return 0;
}

// syscall handlers:
//  sys_* functions are called by syscall.c

int sys_sigaction(int signo, const sigaction_t __user *act, sigaction_t __user *oldact) {
    return 0;
}

int sys_sigreturn() {
    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    return 0;
}

int sys_sigkill(int pid, int signo, int code) {
    // 1. 验证信号编号有效性（1~10）
    if (signo <= 0 || signo >= 10) {
        return -1;
    }

    // 2. 遍历进程表查找目标进程
    struct proc *p;
    for (int i = 0; i < NPROC; i++) {
        p = pool[i];
        // 3. 检查进程有效性（非UNUSED状态且PID匹配）
        if (p->state != UNUSED && p->pid == pid) {
            // 4. 特殊处理SIGKILL信号（不可阻塞/忽略）
            if (signo == SIGKILL) {
                setkilled(p, -10 - signo);  // 强制终止进程
                return 0;
            }

            // 5. 检查信号是否被忽略
            // if (p->signal.handlers[signo].sa_sigaction == SIG_IGN) {
            //     return 0;
            // }

            // 6. 添加到pending信号集
            sigaddset(&p->signal.sigpending, signo);

            // 7. 唤醒睡眠中的进程（模拟wakeup行为）
            if (p->state == SLEEPING) {
                p->state = RUNNABLE;
            }

            return 0;
        }
    }

    // 8. 未找到目标进程
    return -1;
}
