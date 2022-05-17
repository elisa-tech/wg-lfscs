**SUMMARY**

Various stack memory protection mechanisms in Linux are provided to support data integrity of both kernel and user space memory (prevention), as well as traceability of stack memory usage during execution (detection).

These mechanisms include the following:

1. User space stack protection – OS mechanisms which provide the infrastructure for a compiler (e.g., gcc) to detect corrupted data:

  - Uninitialized variables (which can yield corrupted data if accessed)

  - Stack overflow (which can override data outside the stack boundaries and therefore provide no guarantees on data integrity)

Note that these features commonly require a combination of kernel configuration to enable the features, as well as compiler configuration to support the runtime stack protection.  In addition, these features have some runtime performance impact which needs to be considered, and often can be configured with increasing levels of protection.  The integrator must clearly understand these implications and configure both the kernel as well as the compiler to provide the appropriate level of support.

Examples of kernel features supporting user space stack memory protection:

  * CONFIG_GCC_PLUGIN_STRUCTLEAK
  * CONFIG_GCC_PLUGIN_STRUCTLEAK_BYREF_ALL
      * GCC plugin to initialize variables sent by reference to zero, leaving no assumptions on the calling function

 2. Kernel space stack protection – Examples of kernel features supporting kernel space stack memory protection:

  * CONFIG_STACKPROTECTOR
  * CONFIG_STACKPROTECTOR_STRONG
      * Stack protection in gcc compiler must be enabled

  * CONFIG_SCHED_STACK_END_CHECK
      * Detects stack corruption on calls to schedule()
      * In case end-of-stack corruption is detected, always panic

  * CONFIG_VMAP_STACK
      * Add guard pages to virtually mapped kernel stacks, allowing earlier detection of kernel stack overflows
      * Unlike CONFIG_SCHED_STACK_END_CHECK, this option prevents writes beyond the allocated stack
      * The offending thread can be terminated without need to panic

  * CONFIG_GCC_PLUGIN_STACKLEAK
      * Poison kernel stack before returning from syscalls; it's mainly intended for security, however in safety context it can help in detecting uninitialized stack variables.

  * CONFIG_TREAD_INFO_IN_TASK
      * Moves thread information off the stack and into the task struct for protection of task info, particularly during context switch.

**ANALYSIS**
  * A process/thread will always have a kernel-siede component and optionally a user-space side part, which is the typical case of applications.
  * Each side has own call stack
  * The user-side call stack is managed accordingly to how the application/library binary has been compiled
  * The kernel-side stack is managed accordingly to how the kernel has been compiled
  * The userspace-side stack is exposed to corruption primarily coming from:
     * application
     * libraries linked in by the application
     * memory interference coming from low level kernel mechanisms (this is relatively less likely to happen undetected)
  * The kernel-space side is exposed to corruption from anything that runs with kernel privilege
  * Adding stackprotection does generate overhead
  * The available kernel options (STACKPROTECTOR and STACKPROTECTOR_STRONG) rely on heuristic to decide which functions should receive canaries hardening and which shouldn't.
  * The compiler, however, supports also a third option that allows adding protection to all of them.
   
 
**CONSIDERATIONS**
  * Assuming that a userspace program has been designed and implemented correctly, and that the same can be said for the libraries it uses, stack protection is less useful as a runtime error detection tool, for an end product, and more as debugging tool during development and testing.
  * Other applications that might not have been designed with the same quality criteria are prevented from interfering by the way userspace memory is mapped: a program can only see (and interfere with) the memory it has been assigned.
  * In kernel space, on the other hand, different modules with differnet level of trust are exposed to each other.
  * Even if a module was developed and implemented correctly, it is exposed to the interference from another module that doesn't meet the same quality criteria.
  * Here monitoring for stack corruption at runtime is more critical and has a higher ROI, in terms of improved confidence vs. overhead introduced.
  * In particular, by enabling VMAP_STACK, it is possible to make the system more resilinet to failures, and increase the confidence that It can be trusted to take some corrective actions (for example terminating the offending task and entering safe mode)
  * Enabling full function coverage for stack canaries reduces further the chances of having undetected stack corruption. This option is not currently available in the kernel, however it can be added easily. When activated, though, it can cause (at least on on ARM64) stack overflow, because of the additional space required. This can be avoided by increasing the stack size (doubled).
  * Whether adding full stack canraries support is feasible or not needs to be investigated [ongoing] and results might differ from platform to platform, however it is normally expected that a userspace program will spend most of its time executing own code, rather than waiting for syscalls to complete. Perhaps the best way to judge the feasibility of the solution is to gauge the increase in execution time and latency for each desired use-case.
