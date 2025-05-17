#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include "../types.h"

/* 用户自定义信号 */
#define SIGUSR0 1  // 用户自定义信号0
#define SIGUSR1 2  // 用户自定义信号1
#define SIGUSR2 3  // 用户自定义信号2
#define SIGKILL 4  // 强制终止进程信号

/* 系统标准信号 */
#define SIGTERM 5  // 终止请求信号
#define SIGCHLD 6  // 子进程状态改变信号
#define SIGSTOP 7  // 停止进程执行信号
#define SIGCONT 8  // 继续执行被停止的进程信号
#define SIGSEGV 9  // 无效内存访问信号
#define SIGINT  10 // 终端中断信号

#define SIGMIN SIGUSR0
#define SIGMAX SIGINT

#define sigmask(signo) (1 << (signo))

typedef uint64 sigset_t;
typedef struct sigaction sigaction_t;

/* 信号信息结构体 */
typedef struct siginfo {
    int si_signo;   // 信号编号
    int si_code;    // 信号代码
    int si_pid;     // 发送信号的进程ID
    int si_status;  // 退出状态或信号值
    void* addr;     // 导致错误的地址(如SIGSEGV)
} siginfo_t;

/* 信号处理动作结构体 */
struct sigaction {
    void (*sa_sigaction)(int, siginfo_t*, void *); // 信号处理函数
    sigset_t sa_mask;                             // 执行处理函数时要阻塞的信号集
    void (*sa_restorer)(void);                    // 恢复函数指针
};

/* 用户上下文结构体 */
struct ucontext {
    sigset_t uc_sigmask;          // 被阻塞的信号集
    struct mcontext {
        uint64 epc;              // 程序计数器
        uint64 regs[31];         // 通用寄存器组
    } uc_mcontext;
};

// sigaction:
#define SIG_DFL  ((void *)0)
#define SIG_IGN  ((void *)1)

// used in sigprocmask
#define SIG_BLOCK   1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

// sigset manipulate inline function

/* 初始化信号集为空集 */
static inline void sigemptyset(sigset_t *set) {
    *set = 0;
}

/* 初始化信号集包含所有信号 */
static inline void sigfillset(sigset_t *set) {
    *set = -1;
}

/* 向信号集中添加指定信号 */
static inline void sigaddset(sigset_t *set, int signo) {
    *set |= 1 << signo;
}

/* 从信号集中删除指定信号 */
static inline void sigdelset(sigset_t *set, int signo) {
    *set &= ~(1 << signo);
}

/* 检查信号是否在信号集中 */
static inline int sigismember(const sigset_t *set, int signo) {
    return *set & (1 << signo);
}

#endif  //__SIGNAL_H__