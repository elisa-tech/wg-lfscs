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

  * CONFIG_STACKPROTECTOR
  * CONFIG_STACKPROTECTOR_STRONG 
      * Stack protection in gcc compiler must be enabled

 2. Kernel space stack protection – Examples of kernel features supporting kernel space stack memory protection:

  * CONFIG_SCHED_STACK_END_CHECK 
      * Detects stack corruption on calls to schedule()

  * CONFIG_VMAP_STACK 
      * Add guard pages to virtually mapped kernel stacks, allowing earlier detection of kernel stack overflows

  * CONFIG_GCC_PLUGIN_STACKLEAK 
      * Poison kernel stack before returning from syscalls

  * CONFIG_TREAD_INFO_IN_STACK 
      * Moves thread information off the stack and into the task struct for protection of task info, particularly during context switch

A tool for checking the related configurations could be found here:  https://github.com/elisa-tech/kconfig-safety-check
