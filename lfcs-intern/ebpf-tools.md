## Berkeley Packet Filter (BPF) and work plan
#### Agenda
<ol> <li>BPF </li>
     <ol> <li> Introduction </li>
          <li> Practical use of BPF </li>
              <ol> <li>Observability</li>
                      <ul><li> BPF front-end tool </li>
                          <li> BCC tools and programming</li>
                          <li> BPFtrace </li> 
                      </ul>
                    <li>Security
                        <ul> <li>BPF verifier</li></ul>
                    <li> Networking </li>
                         <ul> <li>XDP</li></ul>
                </ol> 
      <li> Workplan</li>
      <ol> <li> BPF current issue proposed by Daniel Borkmann </li>
           <li>  Using XDP to create a novel BCC tool for safety </li>
           <li> Improve on the current eBPF verifier</li>
           </ol>
</ol>
________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________

### Introduction

The Berkeley Packet Filter(BPF/eBPF) is one of the subsystems in the core Linux Kernel. BPF is the industry standard for packet filtering. It works as an in kernel execution engine that processes virtual instruction set. 

BPF is actually composed of an instruction set, storage objects and helper functions.

The technology started as a simple language for writing packet-filtering code for utilities like tcpdump: [McCanne 92](https://www.tcpdump.org/papers/bpf-usenix93.pdf) and grew to a general purpose execution engine; that can be used for a variety of things including creation of advanced performance analysis tools.

With BPF, we can run mini programs on wide variety of kernel and application events.

> An eBPF program is attached to a designated code path in the Kernel. When a code path is traversed, any attached eBPF programs are executed.

The main use of BPF are networking, observability and security.

In this introduction, we will focus on the main use of the BPF subsystem.

### Practical Use 

<ol><li> Observability</li>
    BPF is an event driven programming that provides observability or tracing.
    System administrator tools that give extra informations that are not provided by common system administrator tools. 
	<ul> <li> BCC </li>
	BPF compiler collection (BCC) is the higher level tracing framework developed for BPF.
	The framework provides a C programming environment for writing Kernel BPF code and other languages(python, Lua, C++) 
	for user-level interface.<br/>
		<ol><li>bcc tools </li>
		BCC repository has more than 70 BPF tools for performance and analysis. We will go through 12 BCC tools.
			<ol><li>execsnoop</li>
<pre># execsnoop
PCOMM            PID    PPID   RET ARGS
dhcpcd-run-hook  29407  2642     0 /lib/dhcpcd/dhcpcd-run-hooks
sed              29410  29409    0 /bin/sed -n s/^domain //p wlan0.dhcp
cmp              29417  29407    0 /usr/bin/cmp -s /etc/resolv.conf ../resolv.conf.wlan0.ra
         qemu-system-x86  29422  27546    0 /usr/bin/qemu-system-x86_64 -m 4096 -smp 8 ... -snapshot 
</pre>
execsnoop works by tracing the execve(2) system call and reveal processes that may be shortlived that they are invisible 
to other tools like ps. 
        <li>opensnoop</li>
	      <pre># opensnoop -T
	      TIME(s)       PID    COMM               FD ERR PATH
	      0.000000000   11552  baloo_file_extr    20   0 /home/jules/../linux/../unistd_32.h
	      0.000433000   11552  baloo_file_extr    20   0 /home/jules/../linux/../unistd_64.h
	      0.000764000   11552  baloo_file_extr    20   0 /home/jules/../linux/../unistd_x32.h
	      0.001084000   11552  baloo_file_extr    20   0 /home/jules/../linux/../syscalls_32.h
	      0.001391000   11552  baloo_file_extr    20   0 /home/jules/../linux/../unistd_32_ia32.h
	      0.001685000   11552  baloo_file_extr    20   0 /home/jules/../linux/../unistd_64_x32.h
	      0.079771000   3486   qemu-system-x86    23   0 /etc/resolv.conf
	      0.422395000   11858  Chrome_IOThread   389   0 /dev/shm/.com.google.Chrome.ct746O
	      </pre>
This debugging tools prints a line of the output per each open() system call and its variants. 
opensnoop can be used to troubleshoot failing software which may be attempting to open files from a wrong path
as well as determine where the config and log files are kept.
       <li>ext4slower</li>
       <pre># ext4slower
       Tracing ext4 operations slower than 10 ms
       TIME     COMM           PID    T BYTES   OFF_KB   LAT(ms) FILENAME
       22:16:08 baloo_file_ext 4458   S 0       0         125.20 index
       22:16:12 baloo_file_ext 4458   S 0       0         134.65 index
       22:16:16 baloo_file_ext 4458   S 0       0         151.65 index
       22:16:20 baloo_file_ext 4458   S 0       0         172.81 index
	     22:16:25 baloo_file_ext 4458   W 60678144 5098540    11.48 index
	     </pre>
	     This tool trace common operation of ext4 file system(reads, write, open, syncs)
	     and prints those that exceed a time threshold
	   <li> biolatency </li>
	      <pre># biolatency 
	      Tracing block device I/O... Hit Ctrl-C to end.
	      usecs               : count     distribution
	      0 -> 1          : 0        |                                        |
	      2 -> 3          : 0        |                                        |
	      4 -> 7          : 3        |                                        |
	      8 -> 15         : 115      |**************                          |
	      16 -> 31         : 49       |******                                  |
	      32 -> 63         : 36       |****                                    |
	      64 -> 127        : 1        |                                        |
	      128 -> 255        : 286      |************************************    |
        256 -> 511        : 160      |********************                    |
        512 -> 1023       : 315      |****************************************|
        1024 -> 2047       : 21       |**                                      |
	      2048 -> 4095       : 1        |                                        |
	      </pre>
	     biolatency traces disk I/O latency and  shows result as an histogram.
	     Latency here refers to the time taken from device issue to completion.
	     The tool gives better performance information than iostat(1) 
	   <li> biosnoop</li>
	      <pre># biosnoop
	      TIME(s)     COMM           PID    DISK    T SECTOR     BYTES  LAT(ms)
	      0.000000    kworker/23:1   9126           R 18446744073709551615 0         0.61
	      1.774198    ThreadPoolFore 5270   nvme0n1 W 520198144  225280    0.48
	      1.774381    jbd2/nvme0n1p3 686    nvme0n1 W 490161296  65536     0.03
	      1.774609    ?              0              R 0          0         0.21
	      1.774809    jbd2/nvme0n1p3 686    nvme0n1 W 490161424  4096      0.19
	      2.069546    kworker/23:1   9126           R 18446744073709551615 0         0.17
	      2.159061    ?              0              R 0          0         0.24
	      2.159129    ThreadPoolFore 5270   nvme0n1 W 777702184  4096      0.01
	      2.159341    ?              0              R 0          0         0.20
	      2.159387    ThreadPoolFore 5270   nvme0n1 W 15221256   8192      0.01
	      2.159598    ?              0              R 0          0         0.20
	      2.159713    jbd2/nvme0n1p3 686    nvme0n1 W 490161432  53248     0.02</pre>
	      The tool prints a line of output for each disk I/O with details include latency
	   <li>tcpconnect</li>
	      <pre># tcpconnect
	      Tracing connect ... Hit Ctrl-C to end
	      PID    COMM         IP SADDR            DADDR            DPORT
	      4909   Chrome_Child 4  192.168.1.245    40.74.98.194     443
	      4909   Chrome_Child 4  192.168.1.245    40.74.98.194     443
	      5564   Chrome_Child 4  192.168.1.245    172.217.16.238   443
	      4909   Chrome_Child 4  192.168.1.245    52.97.208.18     443
	      5564   Chrome_Child 4  192.168.1.245    142.250.200.14   443
	      5564   Chrome_Child 4  192.168.1.245    35.206.151.171   443
	      4909   Chrome_Child 4  192.168.1.245    52.113.205.5     443
	      5564   Chrome_Child 4  192.168.1.245    34.131.36.146    443
	      4909   Chrome_Child 4  192.168.1.245    13.89.179.10     443
	      5564   Chrome_Child 4  192.168.1.245    142.250.179.229  443
	      </pre>
	      tcpconnect display a one line of output  of every active TCP connection(connect()) 
	   <li>tcpretrans</li>
	      <pre># tcpretrans
Tracing retransmits ... Hit Ctrl-C to end
TIME     PID    IP LADDR:LPORT          T> RADDR:RPORT          STATE
 22:36:32 0  4  192.168.1.245:42072  R> 13.33.52.19:443  ESTABLISHED
 22:39:50 0  4  192.168.1.245:59090  R> 142.250.179.229:443  ESTABLISHED
22:39:50 0   4  192.168.1.245:59070  R> 142.250.179.229:443  ESTABLISHED
22:39:51 1372 4  192.168.1.245:59090  R> 142.250.179.229:443  ESTABLISHED
22:39:51 1372 4  192.168.1.245:59092  R> 142.250.179.229:443  ESTABLISHED
 </pre>
This tool uses dynamic tracing of the kernel 
tcp_retransmit_skb() and tcp_send_loss_probe() functions
to only traces TCP retransmits, showing address, port, 
PID and information state. 
<li>dcsnoop</li>
 <pre># dcsnoop
  TIME(s)     PID    COMM             T FILE
  8.893741    29295  sadc             M dev
  8.893782    29295  sadc             M dev
  8.893813    29295  sadc             M dev
  8.894006    29295  sadc             M nfs
  8.894028    29295  sadc             M nfsd
  8.894041    29295  sadc             M sockstat
  8.894053    29295  sadc             M softnet_stat
  13.240580   3743   ThreadPoolForeg  M todelete_e3e954c5761fd557_0_1
  13.240988   3557   Chrome_IOThread  M .org.chromium.Chromium.PPeyk5
  13.243443   3743   ThreadPoolForeg  M e3e954c5761fd557_0
  21.747442   29303  dhcpcd-run-hook  M resolv.conf.wlan0.ra
  21.747854   29313  cmp              M maps
  21.748626   29315  rm               M resolv.conf.wlan0.ra
  21.750007   2470   dhcpcd           M if_inet6 </pre>
The tool traces every dcache lookup, and shows the process performing the lookup and the filename requested.<br/>
 <li>cachestat</li>
 <pre># cachestat
  HITS   MISSES  DIRTIES HITRATIO   BUFFERS_MB  CACHED_MB
   16        1        1   94.12%         1312       3249
    0        0        0    0.00%         1312       3249
   34        3       15   91.89%         1312       3249
    0        0        0    0.00%         1312       3249
   14        3        5   82.35%         1312       3249
  407        0       80  100.00%         1312       3249
	0        0        0    0.00%         1312       3249
	0        0        0    0.00%         1312       3249
	0        0       19    0.00%         1312       3249
	0        0        0    0.00%         1312       3249
	9743        0      136  100.00%         1312       3249
	0        0        3    0.00%         1312       3249
	0        0        0    0.00%         1312       3249
	5        0        0  100.00%         1312       3249
   0        0        0    0.00%         1312       3249</pre>
   cachestat prints a one line summary every second 
   (or every custom interval) 
   showing statistics from the file system cache.
    <li> trace</li>
<pre> # trace 'do_nanosleep(struct hrtimer_sleeper *t) "task: %x", t->task'
 PID     TID     COMM            FUNC             -
 3437    3489    teams           do_nanosleep  task: d4588000
 2511    2815    pool-gsd-smartc do_nanosleep  task: 6b248000
 18685   18693   nautilus        do_nanosleep  task: 21b55200
 112985  113009  vqueue:src      do_nanosleep  task: 328b0000
 3437    3489    teams           do_nanosleep  task: d4588000
 </pre>
trace is a multi-tool per event tracing 
from many different sources: kprobes, uprobes,
 tracepoints and USDT probes. It is used when looking 
 for the arguments when a kernel or user-level function 
 is called, the return value of a function, finding out  
 whether a function is failing nad how a function is called  
 or what the user or kernel level stack trace.
 The tool is suited for infrequently called events. 
 If used for frequently occuring events, trace would 
 produce so much output that would cost significant
 overhead to instrument.
 To reduce overhead , it is advised 
 to use a filter expressiopn to print only events of interest.
 <li> funccount</li>
 <pre>
<<<<<<< HEAD
       # funccount 'tcp_send*'
       Tracing 16 functions for "b'tcp_send*'"... Hit Ctrl-C to end.
       ^C        
=======
       # funccount 'vfs_*'
       Tracing 70 functions for "b'vfs_*'"...
       Hit Ctrl-C to end. ^C
>>>>>>> d88860d83a4d33fdd69b6b3bf76019d86e9a54e0
       FUNC                                    COUNT
       b'tcp_send_probe0'                          5
       b'tcp_send_active_reset'                    8
       b'tcp_send_loss_probe'                      9
       b'tcp_send_dupack'                         18
       b'tcp_send_fin'                            58
       b'tcp_send_mss'                          2594
       b'tcp_sendmsg_locked'                    2595
       b'tcp_sendmsg'                           2595
       b'tcp_send_delayed_ack'                  2778
       b'tcp_send_ack'                          3723
       Detaching...
       </pre>
funccount counts events, 
specifically function calls and
respond to queries on 
whether a specific kernel or user-level function is being called
 the rate at which the function is being called(per second)
<li> stackcount </li> 
       The tool counts stack traces that led to an event. 
       The event can be  a kernel- or user -level function,
       tracepoint or USDT probe.
       stackcount respond to queries:
       <ol> <li>Why is this event called? What is the code path?</li>
       <li>What are all the different code paths that call
        this event, and what are their frequencies?</li> </ol>
       stackcount performs a summary entirely in kernel context 
       </ol>
       <pre> # sudo stackcount t:sched:sched_switch
        b'__sched_text_start'
  b'__sched_text_start'
  b'schedule'
  b'__down_write_common'
  b'down_write_killable'
  b'mmap_write_lock_killable'
  b'__vm_munmap'
  b'__x64_sys_munmap'
  b'do_syscall_64'
  b'entry_SYSCALL_64_after_hwframe'
    1</pre>
  <li> bcc programming </li>
  <li>bpftrace</li></ul>
  <li> XDP </li>
  <li> eBPF Verifer</li>
  </ol>


