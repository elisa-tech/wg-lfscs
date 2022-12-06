## Work done during mentorship
<ol>
<li> Accepted patches in the Linux Kernel</li>

During this mentorship, few patches were accepted and merges into the Linux Kernel.  As my profile can show [here](https://linuxlists.cc/profile/52567/Jules_Irenge), I submitted more than 30 patches.

Some of the patches have been merged to the Linux Kernel [1] (https://lore.kernel.org/all/Yxi3pJaK6UDjVJSy@playground/)

All accepted patches can be found on [2] (https://lore.kernel.org/all/?q=Jules+Irenge)

Practice makes better, working in Kernel is no exception. The more I sent patches the better I became. One patch from which I learnt a lot is one that I made a change that appear simple in the BPF subsystem  but ended up being a bug fix [3](https://git.kernel.org/pub/scm/linux/kernel/git/bpf/bpf-next.git/commit/?id=9fad7fe5b298).<br/>The interaction I had with the BPF community in the mailing list was particularly welcoming, fun and learning experience.

<li> Attempted but not accepted </li>
On the request of Daniel Borkmann, I attempted to migrating the sample code [0]
from BPF over to the BPF selftests [1]:
   [0] https://git.kernel.org/pub/scm/linux/kernel/git/bpf/bpf-next.git/tree/samples/bpf
   [1] https://git.kernel.org/pub/scm/linux/kernel/git/bpf/bpf-next.git/tree/tools/testing/selftests/bpf

The objective was to reduce the mess as some few tests that are in the samples directory are not run on BPF CI
Combining the file into one will increase the test on BPF CI and increase reliability on BPF subsystem.

  - https://git.kernel.org/pub/scm/linux/kernel/git/bpf/bpf-next.git/tree/tools/testing/selftests/bpf/prog_tests
  - https://git.kernel.org/pub/scm/linux/kernel/git/bpf/bpf-next.git/tree/tools/testing/selftests/bpf/progs

But the long patch was not accepted, I suspect it was because it features a lot of changes.

<li> Ongoing </li>
<ul>
<li> xdp tools </li>

I spent time reading about xdp and solving the exercise on the below:

 - https://github.com/xdp-project/xdp-tutorial

I ended up building two xdp tool  and a bcc tool to exercise the skills learned.

1. A tool that accepts all TCP packets, reject UDP and retransmit/bounce other type of  packets back to the NIC and count how many packets have been transmitted, passed and dropped and present a percentage of dropped packet.

Source code can be downloaded here - https://github.com/irenge/bcc-tools-devel/tree/main/count_packets

<pre> 
$ sudo python countp.py -dev wlp5s0

Packets count: UDP packets - drooped  TCP - accepted -  Other type of  Packet - bounced to NIC, hit CTRL+C to stop
XDP_TX 1 pkt/s
Total 1870 pkts/s
XDP_PASS 1588 pkt/s
Percentage dropped 0 % 
XDP_DROP 4 pkt/s
XDP_TX 1 pkt/s
Total 1777 pkts/s
XDP_TX 0 pkt/s
Total 1776 pkts/s
</pre>
 
2. A tool that display how many packets passed, dropped, bounced, 
   how many TCP packets, UDP packets and ping the machine received using XDP technology.

<pre>
Packet counts, hit CTRL+C to stop
XDP_PASS 2 pkt/s
Percentage dropped 0 % 
UDP  2 pkt/s
XDP_PASS 1165 pkt/s
Percentage dropped 0 %  
TCP  0 pkt/s
UDP  736 pkt/s
XDP_PASS 758 pkt/s
Percentage dropped 0 % 
</pre> 
 
 <li> bcc tool issue assigned </li>
- I contributed in updating a bcc tool named killsnoop to also use tracepoints to be able to run on newer and older Kernel. https://github.com/iovisor/bcc/pull/4381
- In the process built a tool madvsnoop that help in tuning memory performance of application/system program program  : madvsnoop: https://github.com/iovisor/bcc/pull/4380
</ul>
<li> Future task </li>
- Continue to contribute in the core Linux Kernel Open Source community
- Read more about Linux Kernel development. 
- Enhance eBPF features
- Create more bcc tools that uses both Tracepoints and kprobe for backward compatibility
- Create an IT organisation that focus on program performance and safety.

<li> Thanks </li>

I would like to thank my mentor Elanna Coperman for guiding me and being patient with me and the Linux Foundation for offering me this opportunity to learn skills and develop passions.

</ol>

