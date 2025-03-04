# Introduction
This project demonstrates the use of the Linux ftrace subsystem to trace the
kernel functions involved in running a minimal C application. 
The objective is to capture all the kernel interactions of a simple user-space
program using ftrace.

# Project Components
The project consists of two main components:

`min.c`: A minimal C application that performs a basic operation and enters an 
infinite loop.
`ftrace_it.c`: An application that sets up ftrace to trace the execution of
another program.

# `min.c`
The `min.c` application is designed to do almost nothing, making it an ideal
candidate for tracing the fundamental kernel interactions. 
The source code for min.c is intentionally simple to avoid introducing 
unnecessary operations.

To compile `min.c` with minimal additional code, the following is used:

```
gcc -o min min.c --static -nostartfiles
```

This command compiles the application as a statically linked binary and omits
the standard startup files, ensuring that no additional code is included.
**NOTES**: the `min.large.c` differs from `min.c` because the text body is larger
of a memory page, so kernel needs to trigger mm functions.

# `ftrace_it.c`
The `ftrace_it.c` application sets up the ftrace subsystem to trace the execution
of a specified program, such as `min.c`. 
It performs the following tasks:

* Forks a child process.
* Routes all routable interrupts to CPU0
* Configures ftrace to trace the child process.
* Makes sure the child process is pinned on CPU1
* Starts the child process to execute the given program.
* Waits for the child process to complete.
* Disables ftrace.

Key Features:
* **Shared Memory** Synchronization: Uses a shared variable in memory to 
  synchronize the parent and child processes without impacting the trace.
* **ftrace Configuration**: Configures ftrace to trace the functions of 
  the child process, capturing all kernel interactions.

Usage:
Included Makefile provides two targets to compile the two artifacts.
After having them compiled run `ftrace_it`
```
sudo ./ftrace_it ./min
```

Note: sudo is required to access and modify ftrace settings.

# Elisa scenario
This repo, although it can run on any Linux system, is intended to be run on
a minimal aarch64 system within the ELISA community for which it was realized.
To recreate the environment where it is built, please use Buildroot and apply
the patch br_package.patch to add the repo to the available Buildroot packages.
Then, use the provided configurations `br.config` (which should be copied to 
`.config`) and `kernel.config` to build the system.

The resulting build output, located at `{br_dir}/output/images/Image`, needs 
to be used with aarch64 QEMU:
```
{br_dir} $ qemu-system-aarch64 -M virt -m 512M -cpu cortex-a72 -nographic -smp 2 -kernel output/images/Image
```
Inside the VM, use the following commands to recreate the log:
```
mount -t tracefs none /sys/kernel/tracing/
sleep 20 && echo start && cat /sys/kernel/tracing/trace_pipe > /tmp/log &

/usr/bin/ftrace_it /usr/bin/min
or
/usr/bin/ftrace_it /usr/bin/min.large
```
# Summary
1. Download and extract Buildroot: [Buildroot 2024.02.4}(https://buildroot.org/downloads/buildroot-2024.02.4.tar.gz)
2. Apply the patch: `br_package.patch`
3. Use provided configurations:
    * Copy `br.config` to `.config`
    * Use `kernel.config` (place it in the BR root dir)
4. Build the system.
5. Run the built image with QEMU:
```
qemu-system-aarch64 -M virt -m 2048M -cpu cortex-a72 -nographic -smp 2 -kernel output/images/Image -append "trace_buf_size=256M"
```
5. Recreate the log inside the VM:
```
mount -t tracefs none /sys/kernel/tracing/
sleep 20 && echo start && cat /sys/kernel/tracing/trace_pipe > /tmp/log &

/usr/bin/ftrace_it /usr/bin/min
or
/usr/bin/ftrace_it /usr/bin/min.large
```
# Notes
1. The suggested sleep 20 seconds can be too much, is better to keep it 
   short. Iuse 5 instead of 20.
2. To capture the log and work on it in local machine, the suggested strategy is to
   use tmux pan capture and fetch it from screen. 
   to capture tmux pane use `pipe-pane`. 
   ```
   C-b : pipe-pane "cat >log"
   ```
3. Lastly, to extract from logs the unique functions I suggest the following
   bash one liner:
   ```
   cat logs | grep min.large | cut -d: -f2 | sed -r 's/ *<-/\n/'| tr -d $'\r'| sed -e 's/^ //' | sort| uniq >funcs
   ```
