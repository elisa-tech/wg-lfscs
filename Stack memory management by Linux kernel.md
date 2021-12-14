# Background notes on stack memory management by Linux kernel
## References:
* [Linux Kernel Programming by Kaiwan N Billimoria, Packt Publishing, March 2021, Ch 6](https://static.packt-cdn.com/downloads/9781789953435_ColorImages.pdf)
* [in_task()](https://github.com/torvalds/linux/blob/master/include/linux/preempt.h)  See line 121
* [struct task_struct](https://github.com/torvalds/linux/blob/master/include/linux/sched.h)  From line 723
* [current pointer](https://www.kernel.org/doc/html/latest/kernel-hacking/hacking.html#current)  For example, current --> pid    

## Kernel execution contexts
1. A user application normally runs in unprivileged mode, in user space.  Contexts for execution of kernel code by user applications:
    * Process context - kernel code is executed from a system call or a processor exception (e.g., page fault).  Synchronous
    * Interrupt context - kernel code (e.g., device driver's interrupt handler) is executed from a peripheral chip's HW interrupt.  Asynchronous.
2. Kernel mode threads are application which execute exclusively in kernel space, in process context.

## Kernel and user space stacks
1. Each process in user space has dedicated virtual memory containing the following segments:
    * Text segment - machine code
    * Data setments - global and static data:
      * Initialized data segment
      * Uninitialized data segment (bss), may be auto-initialized to 0 at run-time
      * Heap segment - for dynamic allocation, grows up from lower addresses
      * Libraries (text data) - shared libraries which a process may dynamically link into
      * Stack - Captures high-level lanaguage function calling flow, including call/return sequence, parameter passing, local variable instantiation, return value.
          Stack allocation is architecture dependent, but normally grows down towards lower addresses.  
          A stack frame is allocated and appropriately initialized on every funciton call.
          Details of stack frame layout are architecture dependent, a SP (Stack Pointer) register normally will be used to point to current frame at top of stack
2. Each thread (user as well as kernel) in kernel space has a dedicated task_struct which contains all attributes related to the thread.
3. Each user space thread has 2 stacks, one in user space and a second in kernel space, each of which monitors thread execution when the thread executes in user or kernel space respectively.
4. Kernel space threads have only 1 stack, in kernel space.  The kernel mode stacks are normally fixed size and relatively small.
5. [getrlimit / setrlimit](https://man7.org/linux/man-pages/man2/setrlimit.2.html) may be used to get/set resource limits, in particular to set max size of user space stack.
6. A dedicated stack per CPU may be allocated by architecture for interrupt handling, to avoid overloading the kernel mode stack of the thread which was interrupted.
  
Reference diagram on page 81 of [Linux Kernel Programming diagrams](https://static.packt-cdn.com/downloads/9781789953435_ColorImages.pdf)

## Seeing is believing
* ps aux   
    * Print list (including pids) of all running processes
* cat /proc/< pid >/stack   
    * Dumps kernel space stack (call frame per line)
* [gstack](https://linux.die.net/man/1/gstack)
    * Outputs user space stack
* [strace()](https://man7.org/linux/man-pages/man1/strace.1.html)  
    * trace system calls
* [ltrace()](https://man7.org/linux/man-pages/man1/ltrace.1.html)
    * trace library calls
 * Code samples:   git clone
   * [current_affairs](https://github.com/PacktPublishing/Linux-Kernel-Programming/tree/master/ch6/current_affairs)
   * [thrd_showall](https://github.com/PacktPublishing/Linux-Kernel-Programming/tree/master/ch6/foreach/thrd_showall)
