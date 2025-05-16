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
    sigset_t pending = p->signal.sigpending;
    sigset_t blocked = p->signal.sigmask;

    // 按信号编号从小到大查找优先级最高的可处理信号
    for (int signo = 1; signo <= SIGMAX; signo++) {
        if (sigismember(&pending, signo) && !sigismember(&blocked, signo)) {
            // 1. 处理 SIGKILL 和 SIGSTOP（不可阻塞或忽略）
            if (signo == SIGKILL || signo == SIGSTOP) {
                setkilled(p, -10 - signo);
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
    return 0;
}

            // 6. 自定义处理函数
            if (act->sa_sigaction != SIG_IGN) {
                // 用户栈地址
                uint64 user_sp = p->trapframe->sp;

                // 分配空间：ucontext + siginfo + 对齐
                user_sp -= sizeof(struct ucontext);
                user_sp -= sizeof(siginfo_t);
                user_sp &= ~0x7;  // 8字节对齐

                // 构造 siginfo
                siginfo_t *si = (siginfo_t *)user_sp;
                memset(si, 0, sizeof(siginfo_t));
                si->si_signo = signo;

                // 构造 ucontext
                struct ucontext *uc = (struct ucontext *)(user_sp + sizeof(siginfo_t));
                struct mcontext *mc = &uc->uc_mcontext;

                // 从 trapframe 复制寄存器状态到 mcontext
                mc->regs[0] = p->trapframe->ra;  // ra = regs[0]
                mc->regs[1] = p->trapframe->sp;  // sp = regs[1]
                mc->regs[2] = p->trapframe->gp;  // gp = regs[2]
                mc->regs[3] = p->trapframe->tp;  // tp = regs[3]
                mc->regs[4] = p->trapframe->t0;  // t0 = regs[4]
                mc->regs[5] = p->trapframe->t1;  // t1 = regs[5]
                mc->regs[6] = p->trapframe->t2;  // t2 = regs[6]
                mc->regs[7] = p->trapframe->s0;  // s0 = regs[7]
                mc->regs[8] = p->trapframe->s1;  // s1 = regs[8]
                mc->regs[9] = p->trapframe->a0;  // a0 = regs[9]
                mc->regs[10] = p->trapframe->a1; // a1 = regs[10]
                mc->regs[11] = p->trapframe->a2; // a2 = regs[11]
                mc->regs[12] = p->trapframe->a3; // a3 = regs[12]
                mc->regs[13] = p->trapframe->a4; // a4 = regs[13]
                mc->regs[14] = p->trapframe->a5; // a5 = regs[14]
                mc->regs[15] = p->trapframe->a6; // a6 = regs[15]
                mc->regs[16] = p->trapframe->a7; // a7 = regs[16]
                mc->regs[17] = p->trapframe->s2; // s2 = regs[17]
                mc->regs[18] = p->trapframe->s3; // s3 = regs[18]
                mc->regs[19] = p->trapframe->s4; // s4 = regs[19]
                mc->regs[20] = p->trapframe->s5; // s5 = regs[20]
                mc->regs[21] = p->trapframe->s6; // s6 = regs[21]
                mc->regs[22] = p->trapframe->s7; // s7 = regs[22]
                mc->regs[23] = p->trapframe->s8; // s8 = regs[23]
                mc->regs[24] = p->trapframe->s9; // s9 = regs[24]
                mc->regs[25] = p->trapframe->s10; // s10 = regs[25]
                mc->regs[26] = p->trapframe->s11; // s11 = regs[26]

                mc->epc = p->trapframe->epc;  // 程序计数器

                // 保存原始信号掩码
                uc->uc_sigmask = blocked;

                // 更新信号掩码（添加 sa_mask 和当前信号）
                p->signal.sigmask |= (act->sa_mask | sigmask(signo));

                // 设置陷阱帧参数
                p->trapframe->a0 = signo;          // 第一个参数：signo
                p->trapframe->a1 = (uint64)si;     // 第二个参数：siginfo
                p->trapframe->a2 = (uint64)uc;     // 第三个参数：ucontext
                p->trapframe->ra = (uint64)act->sa_restorer; // 返回地址设为 sigreturn stub
                p->trapframe->epc = (uint64)act->sa_sigaction; // PC指向用户处理函数
                p->trapframe->sp = user_sp;        // 调整用户栈指针

                return 0;
            }
        }
    }

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
        release(&p->mm->lock);
    }

    return 0;
}

int sys_sigreturn() {
    struct proc *p = curr_proc();
    struct trapframe *tf = p->trapframe;

    // 从 a0 寄存器获取用户传递的 ucontext 指针
    struct ucontext *uc = (struct ucontext *)tf->a0;

    // 从用户空间拷贝 ucontext 结构体到内核栈
    if (copy_from_user(p->mm, (char *)&uc, (uint64)uc, sizeof(struct ucontext))) {
        return -1; // 用户空间拷贝失败
    }

    // 恢复信号掩码
    p->signal.sigmask = uc->uc_sigmask;

    // 恢复寄存器状态（对应 RISC-V 寄存器布局）
    tf->ra = uc->uc_mcontext.regs[0];  // ra = regs[0]
    tf->sp = uc->uc_mcontext.regs[1];  // sp = regs[1]
    tf->gp = uc->uc_mcontext.regs[2];  // gp = regs[2]
    tf->tp = uc->uc_mcontext.regs[3];  // tp = regs[3]
    tf->t0 = uc->uc_mcontext.regs[4];  // t0 = regs[4]
    tf->t1 = uc->uc_mcontext.regs[5];  // t1 = regs[5]
    tf->t2 = uc->uc_mcontext.regs[6];  // t2 = regs[6]
    tf->s0 = uc->uc_mcontext.regs[7];  // s0 = regs[7]
    tf->s1 = uc->uc_mcontext.regs[8];  // s1 = regs[8]
    tf->a0 = uc->uc_mcontext.regs[9];  // a0 = regs[9]
    tf->a1 = uc->uc_mcontext.regs[10]; // a1 = regs[10]
    tf->a2 = uc->uc_mcontext.regs[11]; // a2 = regs[11]
    tf->a3 = uc->uc_mcontext.regs[12]; // a3 = regs[12]
    tf->a4 = uc->uc_mcontext.regs[13]; // a4 = regs[13]
    tf->a5 = uc->uc_mcontext.regs[14]; // a5 = regs[14]
    tf->a6 = uc->uc_mcontext.regs[15]; // a6 = regs[15]
    tf->a7 = uc->uc_mcontext.regs[16]; // a7 = regs[16]
    tf->s2 = uc->uc_mcontext.regs[17]; // s2 = regs[17]
    tf->s3 = uc->uc_mcontext.regs[18]; // s3 = regs[18]
    tf->s4 = uc->uc_mcontext.regs[19]; // s4 = regs[19]
    tf->s5 = uc->uc_mcontext.regs[20]; // s5 = regs[20]
    tf->s6 = uc->uc_mcontext.regs[21]; // s6 = regs[21]
    tf->s7 = uc->uc_mcontext.regs[22]; // s7 = regs[22]
    tf->s8 = uc->uc_mcontext.regs[23]; // s8 = regs[23]
    tf->s9 = uc->uc_mcontext.regs[24]; // s9 = regs[24]
    tf->s10 = uc->uc_mcontext.regs[25]; // s10 = regs[25]
    tf->s11 = uc->uc_mcontext.regs[26]; // s11 = regs[26]
    tf->epc = uc->uc_mcontext.epc;     // 程序计数器

    return 0;
}

int sys_sigprocmask(int how, const sigset_t __user *set, sigset_t __user *oldset) {
    struct proc *p = curr_proc();
    sigset_t new_mask = 0;
    sigset_t *current_mask = &p->signal.sigmask;

    // 1. 保存旧的信号掩码（如果oldset非空）
    if (oldset) {
        if (copy_to_user(p->mm, (uint64)oldset, (char*)current_mask, sizeof(sigset_t))) {
            return -1;  // 用户空间拷贝失败
        }
    }

    // 2. 处理 how 参数
    if (set) {
        // 从用户空间读取新掩码
        if (copy_from_user(p->mm, (char*)&new_mask, (uint64)set, sizeof(sigset_t))) {
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
                return -1;  // 无效的 how 参数
        }
    }

    return 0;
}

int sys_sigpending(sigset_t __user *set) {
    struct proc *p = curr_proc();

    // 如果用户提供了 set 指针
    if (set) {
        // 将当前进程的 sigpending 信号集复制到用户空间
        if (copy_to_user(p->mm, (uint64)set, (char*)&p->signal.sigpending, sizeof(sigset_t))) {
            return -1;  // 用户空间拷贝失败
        }
    }

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
