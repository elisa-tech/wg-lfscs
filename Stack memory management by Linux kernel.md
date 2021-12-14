# Background notes on stack memory management by Linux kernel
## References:
    *  [Diagrams from Chapters 6] (https://static.packt-cdn.com/downloads/9781789953435_ColorImages.pdf)
    *  [in_task() on line 121] (https://github.com/torvalds/linux/blob/master/include/linux/preempt.h)
    *  [struct task_struct from line 723] (https://github.com/torvalds/linux/blob/master/include/linux/sched.h)
    *  [Use of current pointer] (https://www.kernel.org/doc/html/latest/kernel-hacking/hacking.html#current)  For example, current --> pid    
    *  Code samples   git clone
         [current_affairs] (https://github.com/PacktPublishing/Linux-Kernel-Programming/tree/master/ch6/current_affairs)
         [thrd_showall] (https://github.com/PacktPublishing/Linux-Kernel-Programming/tree/master/ch6/foreach/thrd_showall)

## Kernel execution contexts
1. A user application normally runs in unprivileged mode, in user space.  Contexts for execution of kernel code by user applications:
    * Process context - kernel code is executed from a system call or a processor exception (e.g., page fault).  Synchronous
    * Interrupt context - kernel code (e.g., device driver's interrupt handler) is executed from a peripheral chip's HW interrupt.  Asynchronous.
2. Kernel mode threads are application which execute exclusively in kernel space, in process context.
3. Each process in user space has dedicated virtual memory containing the following segments:
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

Reference diagram on page 81 of https://static.packt-cdn.com/downloads/9781789953435_ColorImages.pdf

