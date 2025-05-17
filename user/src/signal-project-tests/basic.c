#include "../../os/ktest/ktest.h"
#include "../lib/user.h"

/*
 * 信号处理基础测试
 * 测试点1: sigaction, sigkill和sigreturn系统调用
 */

/*
 * 测试1: 向子进程发送SIGUSR0信号
 * 默认情况下SIGUSR0会终止进程
 * @param s 测试名称(未使用)
 */
void basic1(char* s) {
    int pid = fork();  // 创建子进程
    if (pid == 0) {
        // 子进程代码
        sleep(10);     // 休眠10秒
        exit(1);       // 正常情况下退出码为1
    } else {
        // 父进程代码
        sigkill(pid, SIGUSR0, 0);  // 向子进程发送SIGUSR0信号
        int ret;                   // 用于接收子进程退出状态
        printf("ret=%d",ret);
        wait(0, &ret);             // 等待子进程结束
        printf("ret=%d",ret);
        assert(ret == -10 - SIGUSR0);  // 验证子进程被信号终止
    }
}

/*
 * 测试2: 测试忽略SIGUSR0信号
 * 子进程设置SIG_IGN忽略SIGUSR0信号
 * @param s 测试名称(未使用)
 */
void basic2(char* s) {
    int pid = fork();  // 创建子进程
    if (pid == 0) {
        // 子进程代码
        sigaction_t sa = {  // 信号处理结构体
            .sa_sigaction = SIG_IGN,  // 设置为忽略信号
            .sa_mask      = 0,        // 不阻塞任何信号
            .sa_restorer  = NULL,     // 不需要恢复函数
        };
        sigaction(SIGUSR0, &sa, 0);  // 设置SIGUSR0的处理方式
        sleep(10);
        sleep(10);
        sleep(10);
        exit(1);  // 正常退出
    } else {
        // 父进程代码
        sleep(5);  // 等待子进程设置好信号处理
        sigkill(pid, SIGUSR0, 0);  // 发送SIGUSR0信号
        int ret;                   // 接收子进程退出状态
        wait(0, &ret);             // 等待子进程结束
        assert(ret == 1);          // 验证子进程正常退出
    }
}

/*
 * SIGUSR0信号处理函数3
 * @param signo 信号编号(应为SIGUSR0)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler3(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR0);  // 验证信号类型
    getpid();                  // 获取当前进程ID
    sleep(1);                  // 休眠1秒
    exit(103);                 // 以特定退出码终止进程
}

/*
 * 测试3: 测试不返回的信号处理函数
 * 设置SIGUSR0的处理函数，该函数直接退出进程而不返回
 * @param s 测试名称(未使用)
 */
void basic3(char* s) {
    int pid = fork();  // 创建子进程
    if (pid == 0) {
        // 子进程代码
        sigaction_t sa = {  // 信号处理结构体
            .sa_sigaction = handler3,  // 设置处理函数
            .sa_restorer  = sigreturn, // 恢复函数(虽然不会用到)
        };
        sigemptyset(&sa.sa_mask);  // 清空信号掩码
        sigaction(SIGUSR0, &sa, 0);  // 设置SIGUSR0处理方式
        while (1);  // 无限循环等待信号
        exit(1);    // 正常情况下不会执行到这里
    } else {
        // 父进程代码
        sleep(10);  // 等待子进程设置好信号处理
        sigkill(pid, SIGUSR0, 0);  // 发送SIGUSR0信号
        int ret;                   // 接收子进程退出状态
        wait(0, &ret);             // 等待子进程结束
        assert_eq(ret, 103);       // 验证子进程以特定退出码终止
    }
}

volatile int handler4_flag = 0;  // 信号处理完成标志

/*
 * SIGUSR0信号处理函数4
 * 处理完成后设置标志位并返回
 * @param signo 信号编号(应为SIGUSR0)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler4(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR0);  // 验证信号类型
    sleep(1);                  // 休眠1秒
    sleep(1);                  // 再次休眠1秒
    fprintf(1, "handler4 triggered\n");  // 打印处理信息
    handler4_flag = 1;         // 设置处理完成标志
}

/*
 * 测试4: 测试正常返回的信号处理函数
 * 设置SIGUSR0的处理函数，该函数处理完成后返回
 * @param s 测试名称(未使用)
 */
void basic4(char* s) {
    int pid = fork();  // 创建子进程
    if (pid == 0) {
        // 子进程代码
        sigaction_t sa = {  // 信号处理结构体
            .sa_sigaction = handler4,  // 设置处理函数
            .sa_restorer  = sigreturn, // 恢复函数
        };
        sigemptyset(&sa.sa_mask);  // 清空信号掩码
        sigaction(SIGUSR0, &sa, 0);  // 设置SIGUSR0处理方式
        while (handler4_flag == 0);  // 等待信号处理完成
        exit(104);                  // 以特定退出码终止进程
    } else {
        // 父进程代码
        sleep(10);  // 等待子进程设置好信号处理
        sigkill(pid, SIGUSR0, 0);  // 发送SIGUSR0信号
        int ret;                   // 接收子进程退出状态
        wait(0, &ret);             // 等待子进程结束
        assert_eq(ret, 104);       // 验证子进程以特定退出码终止
    }
}

/*
 * 信号处理函数5
 * 测试信号处理函数的不可重入性
 * 当处理SIGUSR0信号时，内核应自动阻塞SIGUSR0信号
 * @param signo 信号编号(应为SIGUSR0)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
static volatile int handler5_cnt = 0; // 信号处理计数器，记录处理次数
void handler5(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR0);  // 验证信号类型
    static volatile int nonreentrace = 0; // 重入检查标志
    assert(!nonreentrace);    // 确保不会重入
    nonreentrace = 1;         // 设置重入标志
    sleep(5);
    sleep(5);
    if (handler5_cnt < 5)      // 如果处理次数不足5次
        sigkill(getpid(), SIGUSR0, 0); // 再次发送SIGUSR0信号
    sleep(5);
    sleep(5);
    fprintf(1, "handler5 triggered\n"); // 打印处理信息
    nonreentrace = 0;         // 清除重入标志
    handler5_cnt++;            // 增加处理计数器
}

// signal handler itself should not be reentrant.
//  when the signal handler is called for SIGUSR0, it should block all SIGUSR0.
//  after the signal handler returns, the signal should be unblocked.
//   then, the signal handler should be called again. (5 times)
// set handler for SIGUSR0, kernel should block it from re-entrance.
void basic5(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigaction_t sa = {
            .sa_sigaction = handler5,
            .sa_restorer  = sigreturn,
        };
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR0, &sa, 0);
        while (handler5_cnt < 5);
        exit(105);
    } else {
        // parent
        sleep(10);
        sigkill(pid, SIGUSR0, 0);
        int ret;
        wait(0, &ret);
        assert_eq(ret, 105);
    }
}

volatile int handler6_flag = 0; // 信号处理状态标志
/*
 * SIGUSR0信号处理函数6
 * 测试信号处理函数的嵌套性
 * 在处理SIGUSR0期间可以被SIGUSR1中断
 * @param signo 信号编号(应为SIGUSR0)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler6(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR0);  // 验证信号类型
    handler6_flag = 1;         // 设置状态标志1
    fprintf(1, "handler6 triggered due to %d\n", signo);
    sleep(30);                 // 长时间休眠允许被中断
    assert(handler6_flag == 2); // 验证被SIGUSR1中断过
    handler6_flag = 3;         // 设置最终状态标志
}

/*
 * SIGUSR1信号处理函数6_2
 * 嵌套在SIGUSR0处理函数中执行
 * @param signo 信号编号(应为SIGUSR1)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler6_2(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR1);  // 验证信号类型
    assert(handler6_flag == 1); // 验证当前状态
    handler6_flag = 2;         // 设置状态标志2
    fprintf(1, "handler6_2 triggered due to %d\n", signo);
}

// signal handler can be nested.
void basic6(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigaction_t sa = {
            .sa_sigaction = handler6,
            .sa_restorer  = sigreturn,
        };
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR0, &sa, 0);
        sigaction_t sa2 = {
            .sa_sigaction = handler6_2,
            .sa_restorer  = sigreturn,
        };
        sigemptyset(&sa2.sa_mask);
        sigaction(SIGUSR1, &sa2, 0);
        while (handler6_flag != 3);
        exit(106);
    } else {
        // parent
        sleep(10);
        sigkill(pid, SIGUSR0, 0);
        sleep(5);
        sigkill(pid, SIGUSR1, 0);
        sleep(5);
        int ret;
        wait(0, &ret);
        assert_eq(ret, 106);
    }
}

volatile int handler7_flag = 0; // 信号处理状态标志
/*
 * SIGUSR0信号处理函数7
 * 测试信号掩码功能
 * 在处理SIGUSR0时阻塞SIGUSR1信号
 * @param signo 信号编号(应为SIGUSR0)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler7(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR0);  // 验证信号类型
    handler7_flag = 1;         // 设置状态标志1
    fprintf(1, "handler7 triggered due to %d\n", signo);
    sleep(30);                 // 长时间休眠
    sigset_t pending;          // 待处理信号集
    sigpending(&pending);      // 获取待处理信号
    assert_eq(pending, sigmask(SIGUSR1)); // 验证SIGUSR1被阻塞
    assert(handler7_flag == 1); // 验证未被SIGUSR1中断
    handler7_flag = 2;         // 设置状态标志2
}

/*
 * SIGUSR1信号处理函数7_2
 * 在SIGUSR0处理完成后执行
 * @param signo 信号编号(应为SIGUSR1)
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler7_2(int signo, siginfo_t* info, void* ctx2) {
    assert(signo == SIGUSR1);  // 验证信号类型
    assert(handler7_flag == 2); // 验证当前状态
    handler7_flag = 3;         // 设置最终状态标志
    fprintf(1, "handler7_2 triggered due to %d\n", signo);
}

// signal handler can be nested.
void basic7(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigaction_t sa = {
            .sa_sigaction = handler7,
            .sa_restorer  = sigreturn,
        };
        sigemptyset(&sa.sa_mask);
        sigaddset(&sa.sa_mask, SIGUSR1); // block SIGUSR1 when handling SIGUSR0
        sigaction(SIGUSR0, &sa, 0);

        sigaction_t sa2 = {
            .sa_sigaction = handler7_2,
            .sa_restorer  = sigreturn,
        };
        sigemptyset(&sa2.sa_mask);
        sigaction(SIGUSR1, &sa2, 0);

        while (handler7_flag != 3);
        exit(107);
    } else {
        // parent
        sleep(10);
        sigkill(pid, SIGUSR0, 0);
        sleep(5);
        sigkill(pid, SIGUSR1, 0);
        sleep(5);
        int ret;
        wait(0, &ret);
        assert_eq(ret, 107);
    }
}

// SIG_IGN and SIG_DFL
void basic8(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigaction_t sa = {
            .sa_sigaction = SIG_IGN,
            .sa_restorer  = NULL,
        };
        sigaction(SIGUSR0, &sa, 0);
        sigkill(getpid(), SIGUSR0, 0); // should have no effect

        sigaction_t sa2 = {
            .sa_sigaction = SIG_DFL,
            .sa_restorer  = NULL,
        };
        sigaction(SIGUSR1, &sa2, 0);
        sigkill(getpid(), SIGUSR1, 0); // should terminate the process

        exit(1);
    } else {
        // parent
        sigkill(pid, SIGUSR0, 0);
        int ret;
        wait(0, &ret);
        assert(ret == -10 - SIGUSR1); // child terminated by SIGUSR1
    }
}


/*
 * SIGKILL信号处理函数10
 * 尝试处理SIGKILL信号（实际上不应该被执行）
 * @param signo 信号编号
 * @param info 信号信息结构体
 * @param ctx2 上下文信息
 */
void handler10(int signo, siginfo_t* info, void* ctx2) {
    exit(2);  // 尝试退出，但实际上SIGKILL不能被捕获
}

/*
 * 测试10: 测试SIGKILL信号的不可处理性
 * 验证SIGKILL信号不能被捕获、忽略或阻塞
 * 子进程尝试设置SIGKILL处理函数，但发送SIGKILL仍会立即终止进程
 * @param s 测试名称(未使用)
 */
void basic10(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigaction_t sa = {
            .sa_sigaction = handler10,
            .sa_restorer  = NULL,
        };
        sigaction(SIGKILL, &sa, 0); 
        // set handler for SIGKILL, which should not be called
        while (1);
        exit(1);
    } else {
        // parent
        sleep(20);
        sigkill(pid, SIGKILL, 0);
        int ret;
        wait(0, &ret);
        assert(ret == -10 - SIGKILL);
    }
}

/*
 * 测试11: 测试SIGKILL信号的不可阻塞性
 * 验证即使尝试阻塞SIGKILL信号，发送SIGKILL仍会立即终止进程
 * @param s 测试名称(未使用)
 */
void basic11(char* s) {
    int pid = fork();
    if (pid == 0) {
        // child
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGKILL);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        // set handler for SIGKILL, which should not be called
        while (1);
        exit(1);
    } else {
        // parent
        sleep(20);
        sigkill(pid, SIGKILL, 0);
        int ret;
        wait(0, &ret);
        assert(ret == -10 - SIGKILL);
    }
}

/*
 * 测试20: 测试fork后信号处理继承
 * 验证父进程的信号处理设置会被子进程继承
 * 父进程忽略SIGUSR0信号，子进程继承此设置
 * @param s 测试名称(未使用)
 */
void basic20(char *s) {
    // our modification does not affect our parent process.
    // because `run` method in the testsuite will do fork for us.

    sigaction_t sa = {
        .sa_sigaction = SIG_IGN,
        .sa_restorer  = NULL,
    };
    sigaction(SIGUSR0, &sa, 0);
    // ignore SIGUSR0.

    int pid = fork();
    if (pid == 0) {
        // child
        sigkill(getpid(), SIGUSR0, 0); 
        // should have no effect, because parent ignores it.
        exit(1);
    } else {
        // parent
        int ret;
        wait(0, &ret);
        assert(ret == 1); // child should not be terminated by SIGUSR0
    }
}