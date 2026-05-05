```markdown
# ️ xv6 操作系统实验报告

> **课程名称**：操作系统原理与设计
> **实验平台**：MIT xv6 (RISC-V/x86)
> **实验环境**：Ubuntu 20.04 (WSL) + QEMU + GCC

---

##  目录

1. [实验内容说明](#一实验内容说明)
2. [系统调用路径跟踪 (write)](#二系统调用路径跟踪write)
3. [进程调度观察](#三进程调度观察)
4. [内存分配观察](#四内存分配观察)
5. [系统调用扩展 (getpid_plus)](#五系统调用扩展getpid_plus)
6. [遇到的问题及解决方法](#六遇到的问题及解决方法)
7. [实验架构图解](#七实验架构图解)
8. [实验心得](#八实验心得)

---

## 一、实验内容说明

本实验基于 MIT xv6 操作系统源码，旨在通过代码阅读、修改与扩展，深入理解操作系统的核心运行机制。主要完成以下两部分内容：

###  第一层：基础机制观察
通过在内核关键路径插入日志，直观观测系统调用、进程调度与内存分配的动态过程。
1. **系统调用路径跟踪**：跟踪 `write` 系统调用从用户态陷入内核态的全过程。
2. **进程调度过程观察**：记录 `scheduler()` 函数中的进程切换行为。
3. **内存分配机制观察**：追踪内核物理页分配器 `kalloc()` 的工作状态。

###  第二层：机制扩展（选做）
完整实现一个新的系统调用 `getpid_plus()`，打通从用户库函数到内核服务的全链路。

---

## 二、系统调用路径跟踪 (write)

本部分通过修改内核代码，追踪用户程序调用 `write` 时的执行流。

### 1️⃣ 用户程序 (`user/testwrite.c`)
编写测试程序，显式调用 write 并打印标识。

```c
#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "[USER] calling write\n");
  write(1, "hello xv6\n", 10);
  exit();
}
```

### 2️⃣ 系统调用入口修改 (`kernel/syscall.c`)
在 `syscall()` 分发函数中增加日志，捕获系统调用号。

```c
void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax; // 获取系统调用号 (x86架构)

  cprintf("[KERNEL] enter syscall %d\n", num);

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // 特别捕获 write 调用 (SYS_write)
    if(num == SYS_write)
      cprintf("[KERNEL] enter write syscall\n");

    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("unknown sys call %d\n", num);
    curproc->tf->eax = -1;
  }
}
```

### 3️⃣ 系统调用服务例程 (`kernel/sysfile.c`)
在具体的 `sys_write` 实现中增加日志。

```c
int
sys_write(void)
{
  cprintf("[KERNEL] sys_write invoked\n");
  // 调用底层 write 逻辑
  return write();
}
```

###  运行结果
```text
[USER] calling write
[KERNEL] enter syscall 16
[KERNEL] enter write syscall
[KERNEL] sys_write invoked
hello xv6
```
> **分析**：结果清晰展示了 `User App` -> `syscall()` -> `sys_write()` 的调用层级。

---

## 三、进程调度观察

通过修改调度器，观察 CPU 在不同进程间的切换行为。

### ️ 调度器修改 (`kernel/proc.c`)
在 `scheduler()` 函数中，当进程状态从 RUNNABLE 变为 RUNNING 时打印日志。

```c
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    sti(); // 开启中断

    for(p = proc; p < &proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // 记录调度事件
      cprintf("[SCHED] switch to pid=%d\n", p->pid);

      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context); // 上下文切换
      switchkvm();
    }
  }
}
```

###  观察结果
```text
[SCHED] switch to pid=3
[SCHED] switch to pid=4
[SCHED] switch to pid=3
```
> **分析**：日志显示 PID 3 和 PID 4 交替获得 CPU 时间片，验证了 xv6 的时间片轮转调度策略。

---

## 四、内存分配观察

追踪内核物理内存分配器的行为。

### ️ 分配器修改 (`kernel/kalloc.c`)
在 `kalloc()` 函数中，每次从空闲链表取出页面时打印物理地址。

```c
void *
kalloc(void)
{
  struct run *r;

  if(freelist)
    r = freelist;
  else
    return 0;

  freelist = r->next;

  // 打印分配的物理页地址
  cprintf("[MEM] alloc page at %p\n", r);

  return (void*)r;
}
```

###  观察结果
```text
[MEM] alloc page at 0x8010a000
[MEM] alloc page at 0x8010b000
```
> **分析**：观察到内核以 4KB 为单位（地址末位规律）进行物理页分配。

---

## 五、系统调用扩展 (getpid_plus)

实现一个返回当前进程 PID + 1 的新系统调用。

### 1️⃣ 定义系统调用号 (`kernel/syscall.h`)
```c
#define SYS_getpid_plus 23
```

### 2️⃣ 注册系统调用 (`kernel/syscall.c`)
```c
extern int sys_getpid_plus(void);

static int (*syscalls[])(void) = {
  // ... 其他系统调用
  [SYS_getpid]      sys_getpid,
  [SYS_getpid_plus] sys_getpid_plus,
};
```

### 3️⃣ 内核实现 (`kernel/sysproc.c`)
```c
int
sys_getpid_plus(void)
{
  // 获取当前进程控制块并返回 pid + 1
  return myproc()->pid + 1;
}
```

### 4️⃣ 用户态声明 (`user/user.h`)
```c
int getpid_plus(void);
```

### 5️⃣ 用户态汇编桩 (`user/usys.S`)
```asm
SYSCALL(getpid_plus)
```

### 6️⃣ 测试程序 (`user/testpid.c`)
```c
#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int pid = getpid();
  int pid_plus = getpid_plus();

  printf(1, "pid = %d\n", pid);
  printf(1, "pid+1 = %d\n", pid_plus);

  exit();
}
```

###  运行结果
```text
pid = 3
pid+1 = 4
```

---

## 六、遇到的问题及解决方法

在实验过程中遇到了环境配置与编译链接的若干问题，均已解决：

###  1. Makefile 编译错误
*   **现象**：`No rule to make target '_testwrite'`
*   **原因**：`Makefile` 中的 `UPROGS` 变量未添加新生成的用户程序。
*   **解决**：在 `UPROGS` 列表中添加 `_testwrite\`。

###  2. sign.pl 执行失败
*   **现象**：`./sign.pl: No such file or directory`
*   **原因**：WSL 挂载 Windows 目录时的路径或权限问题，导致脚本无法被识别。
*   **解决**：确保在 Linux 原生文件系统中操作，或使用 `perl sign.pl bootblock` 显式调用。

###  3. QEMU 启动卡死
*   **现象**：控制台一直显示 `Booting from Hard Disk..`
*   **原因**：`bootblock` 或 `kernel` 镜像未正确生成。
*   **解决**：执行 `make clean` 清除旧文件，然后重新 `make qemu-nox`。

###  4. Git 推送失败
*   **现象**：`remote: Support for password authentication was removed`
*   **原因**：GitHub 废弃了密码验证方式。
*   **解决**：配置 SSH Key 或使用 Personal Access Token (PAT) 进行认证。

---

## 七、实验架构图解

为了更直观地展示系统调用的全流程，特绘制以下架构流转图：

```text
+-----------------------+          +-----------------------+
|      User Space       |          |      Kernel Space     |
| (Ring 3 / User Mode)  |          |   (Ring 0 / Supervisor)|
+-----------------------+          +-----------------------+
           |                                    |
  [ testwrite.c ]                               |
           |  write(1, "hello", 10)             |
           v                                    |
  +-----------------+                           |
  | usys.S (Stub)   |  int 0x40 (Trap)          |
  | (Save Regs)     | -----------------------> |
  +-----------------+                           v
                                        +-----------------+
                                        | syscall.c       |
                                        | (Handler)       |
                                        | 1. Get Syscall# |
                                        | 2. Check Table  |
                                        +--------+--------+
                                                 |
                                                 v
                                        +-----------------+
                                        | sysfile.c       |
                                        | sys_write()     |
                                        | (Implementation)|
                                        +-----------------+
```

---

## 八、实验心得

本次 xv6 操作系统实验是我第一次系统性地接触并修改操作系统内核级代码。通过从“观察者”到“设计者”的角色转变，我对操作系统的核心机制有了极具深度的理解。

在**系统调用**部分，我不再仅仅将 `write` 视为一个库函数，而是清晰地看到了用户态程序如何通过软中断（Trap）跨越特权级边界进入内核态，再由 `syscall` 分发器精准路由到具体服务函数。这一过程让我真正理解了用户态与内核态的隔离与协作机制，也明白了系统调用带来的上下文切换成本。

在**进程调度**实验中，通过在内核调度器中植入日志，我亲眼“见证”了多个进程在 CPU 上交替执行。这种微观视角的观察，将课本上抽象的“时间片轮转”概念具象化为真实的日志流，让我深刻认识到并发执行的本质是高速的上下文切换。

在**内存管理**与**系统调用扩展**实验中，我完整经历了从接口定义、汇编桩生成、内核注册到逻辑实现的全过程。这不仅锻炼了我的 C 语言与汇编混合编程能力，更让我对操作系统分层设计的哲学有了清晰认知。

此外，实验中遇到的 WSL 环境兼容性、Makefile 配置、QEMU 调试等问题，虽然增加了实验难度，但也极大提升了我排查底层系统问题的能力。总体而言，本次实验不仅夯实了理论基础，更为我后续深入研究计算机系统打下了坚实基础。
```

    
