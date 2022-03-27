# eBPF Verifier Introduction

This documentation is based on Linux 5.17.

The Berkeley Packet Filter (eBPF) enables user space programs to execute in the Linux kernel. 

Before eBPF, kernel code was mainly released as kernel patches and kernel modules. The eBPF enables program to be loaded from user space and run in kernel space. Some insecure programs might be introduced by these eBPF programs during this process. 

Thus, an eBPF verifier is needed to conduct static analysis to reject and disapprove the insecure programs executing in kernel address space. 

eBPF verifier is an event driven framework, where the events are triggered by kernel hooks. 
Hooks monitors:
(1) syscalls, 
(2) function entry and exit, 
(3) network events and 
(4) kprobes and uprobes. 

The current set of eBPF program types supported by the kernel is as following, Linux 5.16 source code:

```
- BPF_PROG_TYPE_SOCKET_FILTER: a network packet filter
- BPF_PROG_TYPE_KPROBE: determine whether a kprobe should fire or not
- BPF_PROG_TYPE_SCHED_CLS: a network traffic-control classifier
- BPF_PROG_TYPE_SCHED_ACT: a network traffic-control action
- BPF_PROG_TYPE_TRACEPOINT: determine whether a tracepoint should fire or not
- BPF_PROG_TYPE_XDP: a network packet filter run from the device-driver receive path
- BPF_PROG_TYPE_PERF_EVENT: determine whether a perf event handler should fire or not
- BPF_PROG_TYPE_CGROUP_SKB: a network packet filter for control groups
- BPF_PROG_TYPE_CGROUP_SOCK: a network packet filter for control groups that is allowed to modify socket options
- BPF_PROG_TYPE_LWT_*: a network packet filter for lightweight tunnels
- BPF_PROG_TYPE_SOCK_OPS: a program for setting socket parameters
- BPF_PROG_TYPE_SK_SKB: a network packet filter for forwarding packets between sockets
- BPF_PROG_CGROUP_DEVICE: determine if a device operation should be permitted or not
```


## eBPF Passes

Currently the eBPF verifier is focusing on verifying/validating "function entry and exit".


There are two passes in eBPF verifier:

-  1. DAG checks, uses depth first search to check if the bytecode of eBPF program could be parsed into a DAG, and check DAG for unreachable instructions and worst execution time. 

- 2. FSM checks, creates finite state machines, explore all execution paths from the entry instruction of eBPF program, verifies and monitors if states present valid/correct behaviors. 

eBPF verifier builds and checks the control flow graph (i.e. a Directed Acyclic Graph) of programs, and verifies/validates the function calls. 

It also simulates instruction execution and monitors state change of registers and stack. 

The calls to unknown functions and unresolved function calls will be rejected.

For example, eBPF confirms program ends with BPF_EXIT, and confirms all branch instructions, except for BPF_EXIT and BPF_CALL, are within program bounds. 

The current eBPF verifier performs:
- checking performance bug: 
  - if a program execution ends (e.g. bounded loops)
  - checking depth of execution path
- checking memory safety: 
  - if memory address addresses are within memorn boundary 
  - invalid accessing uninitilized contents in registers
- type checking, type checking of load/store instructions of registers with valid typed
- ownership checking, eBPF programs may read data if the program wrote it


## Issues with eBPF 

Some Issues with eBPF programs: 
1. cannot perform dynamic allocation
2. cannot access kernel data structures 
3. cannot call in-kernel APIs
4. run in single-threaded mode
5. execution time is bounded
6. do not have indirect jumps . Each jump instruction in eBPF program points to a fixed location in the code
7. Heavily uses pointers and pointer arithemetic
8. reliance on register spilling 
9. manipulates a fixed number for exclusively owned memory regions



## References

1. BPF: the universal in-kernel virtual machine https://lwn.net/Articles/599755/
2. eBPF - extended Berkeley Packet Filter: https://prototype-kernel.readthedocs.io/en/latest/bpf/
3. (PLDI'19) Simple and Precise Static Analysis of Untrusted Linux Kernel extensions  https://vbpf.github.io/assets/prevail-paper.pdf , https://github.com/vbpf/ebpf-verifier
4. eBPF verifier code in Linux v5.14.14, https://github.com/torvalds/linux/blob/master/kernel/bpf/verifier.c
5. Rust for Linux, https://github.com/Rust-for-Linux
6. Bounded loops in BPF for the 5.3 kernel , https://lwn.net/Articles/794934/
7. eBPF config in Linux v5.15.11, https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/bpf_common.h
8. Interactive Map for DataStructure, https://makelinux.github.io/kernel/map/






