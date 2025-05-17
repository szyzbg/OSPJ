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
    // 初始化所有信号的处理动作为SIG_DFL
    for (int signo = 1; signo <= SIGMAX; signo++) {
        p->signal.sa[signo].sa_sigaction = SIG_DFL;
        // 显式初始化sa_mask，确保没有阻塞信号
        p->signal.sa[signo].sa_mask = 0;
    }
    // 清空信号掩码和待处理信号集
    p->signal.sigmask = 0;
    p->signal.sigpending = 0;
    return 0;
}

int siginit_fork(struct proc *parent, struct proc *child) {
    // copy parent's sigactions and signal mask
    // but clear all pending signals
    // 1. 复制父进程的信号处理动作（sa数组）
    for (int signo = 1; signo <= SIGMAX; signo++) {
        child->signal.sa[signo] = parent->signal.sa[signo];
    }
    // 2. 复制父进程的信号掩码
    child->signal.sigmask = parent->signal.sigmask;
    // 3. 清空子进程的待处理信号集
    child->signal.sigpending = 0;
    
    return 0;
}

int siginit_exec(struct proc *p) {
    // inherit signal mask and pending signals.
    // but reset all sigactions (except ignored) to default.
    // 遍历所有有效信号（1~SIGMAX）
    for (int signo = 1; signo <= SIGMAX; signo++) {
        // 如果当前信号处理方式不是SIG_IGN，则重置为SIG_DFL
        if (p->signal.sa[signo].sa_sigaction != SIG_IGN) {
            p->signal.sa[signo].sa_sigaction = SIG_DFL;
            p->signal.sa[signo].sa_mask = 0;  // 清空信号掩码
        }
    }
    return 0;
}

int do_signal(void) {
    assert(!intr_get());

    struct proc *p = curr_proc();
    acquire(&p->mm->lock);
    sigset_t pending = p->signal.sigpending;
    sigset_t blocked = p->signal.sigmask;

    // 按信号编号从小到大查找优先级最高的可处理信号
    for (int signo = 1; signo <= SIGMAX; signo++) {
        if (sigismember(&pending, signo) && !sigismember(&blocked, signo)) {
            // 1. 处理 SIGKILL 和 SIGSTOP（不可阻塞或忽略）
            if (signo == SIGKILL || signo == SIGSTOP) {
                setkilled(p, -10 - signo);
                release(&p->mm->lock);
                return 0;
            }

            // 2. 清除该信号的 pending 状态
            sigdelset(&p->signal.sigpending, signo);

            // 3. 获取信号处理动作
            sigaction_t *act = &p->signal.sa[signo];

            // 4. 忽略信号
            if (act->sa_sigaction == SIG_IGN) {
                continue;
            }

            // 5. 默认处理
            if (act->sa_sigaction == SIG_DFL) {
        if (signo != SIGCHLD) {
            setkilled(p, -10 - signo);
        }
        release(&p->mm->lock);
        return 0;
}

            // 6. 自定义处理函数
if (act->sa_sigaction != SIG_IGN) {
    struct trapframe *tf = p->trapframe;
    uint64 user_sp = tf->sp;

    // 分配空间：siginfo_t + ucontext，16字节对齐
    user_sp -= sizeof(siginfo_t) + sizeof(struct ucontext);
    user_sp &= ~0xf;

    uint64 info_user = user_sp;
    uint64 uc_user = info_user + sizeof(siginfo_t);

    // 构造内核中的结构体
    siginfo_t kinfo = {0};
    kinfo.si_signo = signo;

    struct ucontext kuc = {0};
    kuc.uc_sigmask = blocked;
    kuc.uc_mcontext.epc = tf->epc;

    // 复制寄存器（假设 RISC-V 有 31 个通用寄存器）
    uint64 *regs = &tf->ra;
    for (int i = 0; i < 31; i++) {
        kuc.uc_mcontext.regs[i] = regs[i];
    }

    // 使用 copy_to_user 安全复制到用户空间
    if (copy_to_user(p->mm, info_user, (char *)&kinfo, sizeof(kinfo)) < 0 ||
        copy_to_user(p->mm, uc_user, (char *)&kuc, sizeof(kuc)) < 0) {
        setkilled(p, -2);  // 用户栈写入失败
        release(&p->mm->lock);
        return -1;
    }

    // 更新信号掩码
    p->signal.sigmask |= act->sa_mask | sigmask(signo);

    // 设置陷阱帧参数
    tf->a0 = signo;
    tf->a1 = info_user;
    tf->a2 = uc_user;
    tf->epc = (uint64)act->sa_sigaction;
    tf->ra = (uint64)act->sa_restorer;
    tf->sp = user_sp;
    release(&p->mm->lock);
    return 0;
}
        }
    }
    release(&p->mm->lock);
    return 0;
}
// syscall handlers:
//  sys_* functions are called by syscall.c

int sys_sigaction(int signo, const sigaction_t __user *act, sigaction_t __user *oldact) {
    // 1. 验证信号编号有效性（1~10）
    if (signo < SIGMIN || signo > SIGMAX) {
        return -1;
    }

    // 2. 验证特殊信号（SIGKILL/SIGSTOP不可修改）
    if (signo == SIGKILL || signo == SIGSTOP) {
        if (act) {
            // 检查用户是否尝试修改默认行为
            sigaction_t tmp_act;
            if (copy_from_user(curr_proc()->mm, (char*)&tmp_act, (uint64)act, sizeof(sigaction_t))) {
                return -1;  // 用户空间拷贝失败
            }
            if (tmp_act.sa_sigaction != SIG_DFL) {
                return -1;  // 禁止修改SIGKILL/SIGSTOP的行为
            }
        }
    }

    struct proc *p = curr_proc();
    sigaction_t *k_act = &p->signal.sa[signo];

    // 3. 保存旧的信号动作（如果oldact非空）
    if (oldact) {
        acquire(&p->mm->lock);
        if (copy_to_user(p->mm, (uint64)oldact, (char*)k_act, sizeof(sigaction_t))) {
            return -1;  // 用户空间拷贝失败
        }
        release(&p->mm->lock);
    }

    // 4. 设置新的信号动作（如果act非空）
    if (act) {
        acquire(&p->mm->lock);
        sigaction_t user_act;
        if (copy_from_user(p->mm, (char*)&user_act, (uint64)act, sizeof(sigaction_t))) {
            return -1;  // 用户空间拷贝失败
        }

        // 更新信号处理函数、掩码和恢复函数
        k_act->sa_sigaction = user_act.sa_sigaction;
        k_act->sa_mask      = user_act.sa_mask;
        k_act->sa_restorer  = user_act.sa_restorer;
    }
    release(&p->mm->lock);

    return 0;
}

int sys_sigreturn(void) {
    struct proc *p = curr_proc();
    acquire(&p->mm->lock);
    struct trapframe *tf = p->trapframe;
    uint64 user_sp = tf->sp;

    // 计算保存的 siginfo_t 和 ucontext 的地址
    uint64 info_user = user_sp;
    uint64 uc_user = info_user + sizeof(siginfo_t);

    // 从用户空间读取 ucontext
    struct ucontext kuc;
    if (copy_from_user(p->mm, (char *)&kuc, uc_user, sizeof(kuc)) < 0) {
        // 用户空间访问失败，终止进程
        setkilled(p, -2);
        release(&p->mm->lock);
        return -1;
    }

    // 恢复信号掩码
    p->signal.sigmask = kuc.uc_sigmask;

    // 恢复程序计数器
    tf->epc = kuc.uc_mcontext.epc;

    // 恢复通用寄存器（x1 到 x31）
    uint64 *regs = &tf->ra;
    for (int i = 0; i < 31; i++) {
        regs[i] = kuc.uc_mcontext.regs[i];
    }

    // 此时，sp 已经被恢复（regs[1] 是 x2/sp）

    release(&p->mm->lock);
    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    struct proc *p = curr_proc();
    acquire(&p->mm->lock);
    sigset_t new_mask = 0;
    sigset_t *current_mask = &p->signal.sigmask;

    // 1. 保存旧的信号掩码（如果oldset非空）
    if (oldset) {
        if (copy_to_user(p->mm, (uint64)oldset, (char*)current_mask, sizeof(sigset_t))) {
            release(&p->mm->lock);
            return -1;  // 用户空间拷贝失败
        }
    }

    // 2. 处理 how 参数
    if (set) {
        // 从用户空间读取新掩码
        if (copy_from_user(p->mm, (char*)&new_mask, (uint64)set, sizeof(sigset_t))) {
            release(&p->mm->lock);
            return -1;  // 用户空间拷贝失败
        }

        // 确保 SIGKILL 和 SIGSTOP 不可被阻塞
        new_mask &= ~(sigmask(SIGKILL) | sigmask(SIGSTOP));

        // 根据 how 参数更新信号掩码
        switch (how) {
            case SIG_BLOCK:
                *current_mask |= new_mask;
                break;
            case SIG_UNBLOCK:
                *current_mask &= ~new_mask;
                break;
            case SIG_SETMASK:
                *current_mask = new_mask;
                break;
            default:
                release(&p->mm->lock);
                return -1;  // 无效的 how 参数
        }
    }
    release(&p->mm->lock);
    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    struct proc *p = curr_proc();
    acquire(&p->mm->lock);

    // 如果用户提供了 set 指针
    if (set) {
        // 将当前进程的 sigpending 信号集复制到用户空间
        if (copy_to_user(p->mm, (uint64)set, (char*)&p->signal.sigpending, sizeof(sigset_t))) {
            release(&p->mm->lock);
            return -1;  // 用户空间拷贝失败
        }
    }
    release(&p->mm->lock);
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

            // //5. 检查信号是否被忽略
            // if (p->signal.handlers[signo].sa_sigaction == SIG_IGN) {
            //     return 0;
            // }

            // 6. 添加到pending信号集
            sigaddset(&p->signal.sigpending, signo);

            // 7. 唤醒睡眠中的进程（模拟wakeup行为）
            if (p->state == SLEEPING) {
                wakeup(p);
                p->state = RUNNABLE;
            }

            return 0;
        }
    }

    // 8. 未找到目标进程
    return -1;
}
