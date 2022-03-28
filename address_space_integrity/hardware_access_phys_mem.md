
<p style="color: red; font-weight: bold">>>>>>  gd2md-html alert:  ERRORs: 0; WARNINGs: 1; ALERTS: 4.</p>
<ul style="color: red; font-weight: bold"><li>See top comment block for details on ERRORs and WARNINGs. <li>In the converted Markdown or HTML, search for inline alerts that start with >>>>>  gd2md-html alert:  for specific instances that need correction.</ul>

<p style="color: red; font-weight: bold">Links to alert messages:</p><a href="#gdcalert1">alert1</a>
<a href="#gdcalert2">alert2</a>
<a href="#gdcalert3">alert3</a>
<a href="#gdcalert4">alert4</a>

<p style="color: red; font-weight: bold">>>>>> PLEASE check and correct alert issues and delete this message and the inline alerts.<hr></p>



# Hardware access to physical memory


# Problem summary (to review)

Heterogeneous systems that integrate a multicore CPU, GPUs, DMAs and other devices on the same die. On these systems, all these devices can access and share the same physical memory, thus unexpected bad interactions between them are possible.

Here is a non exhaustive list of devices that can access physical memory without any CPU assistance:



* System wide DMA channels (e.g., Intel 8237)
* Peripherals with integrated DMA controllers (e.g., PCI peripherals)
* GPUs with unified address space (e.g., Integrated graphics processing unit IGPU)


# Device accessing memory (to review)

Having many heterogeneous devices interacting with the same physical memory leads to, at least, data synchronization issues. At the low level, cache coherence protocols[1] are in place to guarantee data integrity, but still, having heterogeneous systems using the same physical memory amplifies software error consequences. Through DMA requests, peripherals can access other peripheral memory and the main RAM, even in the kernel memory regions, without any control of the processor. GPUs running on a unified address space system can do the same. This aspect is crucial for a security prospect, but it can also affect software safety. A poorly coded driver can instruct peripherals or GPUs to access or overwrite part of the memory they are not supposed to, leading, as a consequence, to corruption of significant data structures.


## DMA (to review)

Direct Memory Access is the technique that allows CPUs to instruct the hardware to perform data transfers independently. By DMA, we can also refer to all devices able to become memory bus masters (able to access the physical memory). This kind of device relieves the host processor from performing certain data copies and operations. DMA cycles have been added to these external buses, allowing peripherals to perform read/write accesses to other peripherals and RAM segments. Traditionally, systems had DMA controllers that allowed dumb peripherals to be managed by these. PCI peripherals dropped the need to use centralized DMA controllers since they own all the hardware needed to access the memory on their own.


## GPU (to review)

Traditional GPU architectures do not permit the CPU and GPU to access each other's memory. To enable data communication, data transfers between their distinct memories need to be explicitly programmed at the application level. Nvida introduced the Unified memory[2] (not to be confused with Shared graphics memory[3]) starting from its Kepler GK110 GPU[4] 2012. ARM's first GPU adopting this technology is the Mali-G31 (2018 Q1). Shared memory between CPUs and GPUs is the foundation of what is known as **Heterogeneous System Architecture** (HSA)[5]. By this technology, memory may be simultaneously accessed by the system's computational units (CPUs or GPUs) with the intent to provide communication or avoid redundant data structure copies. Despite the Heterogeneous System Architecture being beneficial for performance-related matters, it can raise questions on data integrity. 


## Programmable application specific coprocessors (to review)

Another category of devices that can share the memory with the Linux core CPUs is application-specific programmable coprocessors. Examples of this kind of device are **TI PRUs** and **STM32MP1 embedded STM32M4**, to name a few. If these examples look particularly exotic, we may consider something more familiar, such as the **Intel management engine**, a fully-featured processor running inside Intel's chipsets (northbridge or PCH). Programmable application-specific coprocessors are mainly unstandardized, which makes the task of investigating the topic reasonably tricky. The general message here is that these non-standardized features may or may not access the full memory or be brooked by an IOMMU; still, code running on these CAN corrupt the memory. To understand the complexity of this topic, you may refer a few emblematic scenarios:



* Intel Management Engine version 7 or older: Features the main processor **x86_64**, and ME is an **ARC core**. The ARC could access all the memory on such systems, and no IOMMU was out yet to control it. On recent systems, the ME, now ironically called "Converged Security Management Engine," controls the IOMMU memory directly[6].
* A less common but emblematic example is Allwinner A64 SoC: It features a quad-core ARMv8 as the main processor, and as a power management unit, there is a programmable custom implementation of Open Risc 1k (OR1k) that shares its memory with the main core[3].

In most cases, these programmable units are very constrained since they cannot address all the host RAM but only a small fixed area (a few Kb); thus, making them unharmful is a trivial task. But due to the significant implementation difference these systems have, it is impossible to state any general direction on the topic. The suggestion is to start a targeted investigation as a system with such features is chosen as RHIVOS target.


# IOMMU/SMMU (to review)

All modern Operating Systems implement the concept of virtual memory, which is having each process running in a separate address space. This enables memory isolation, so that multiple processes running on the same machine can’t see each other's address space. The Memory Management Unit (MMU) is a device responsible for making the address translation from virtual memory to physical memory.

Devices connected to the bus usually don’t have memory virtualization. Instead, they all share the same address space and access physical memory using Direct Memory Access (DMA).  DMA enables I/O controllers to transfer data directly to or from the main memory.

In order to increase security and enable device address space isolation, a special MMU for devices was created, called I/O MMU.

IOMMUs are relatively new hardware, and they are still far from being standardized. They not only have their name not fixed yet, but they are also not well determined when it comes to listing their features[8][9][10]. The Linux kernel abstracts these differences pretty well, but as a matter of facts, they do not support the same features. [11][12][13][14][15][16][17][18][19][20][21][22][23][24][25]


![alt_text](images/iommu1.png "iommu")


## Architectural differences (to review)

The CPU uses MMU to translate a virtual address to a physical address. The IOMMU, in contrast, is used by devices to translate another virtual address called **IOVA**(IO virtual address) to a **PA**(physical address). 


![alt_text](images/iommu2.png "iommu")


Although IOMMUs are primarily used on virtual guests for device virtualization, they can also be used for system security and safety. The IOMMU's primary task is to translate IOVA on a **per device** basis. This means that similar to MMUs that provide a VA to any process, IOMMUs provide an IOVA to any device served. One key aspect of the IOMMU behavior is identifying a device on the buses they serve. Typically, IOMMUs use the bus provided ids; for example, **BDFids** (Bus:Device:Function) are used for PCI devices. As a consequence, IOMMU must be specialized for the bus they serve. 


### Intel’s IOMMUs (to review)

This section focuses on the mechanisms that enable a DMA remapping unit to translate a memory address and to decide either to reject or to forward the corresponding access request to the memory controller. When a DMA Remapping Hardware Units(DRHU) intercepts a memory access, it has to identify first the I/O controller that has requested the memory access and determine its associated memory regions.


![alt_text](images/interconnect1.png "interconnects")


On current x86 architectures, all PCI and PCI Express controllers are identified by a unique triplet, referred to as its BDF-Id



* bus number: it corresponds to a number assigned by the BIOS to the bus which the I/O controller is physically connected to.
* device number: for a given bus number, it indicates the slot to which the I/O controller is connected to on that bus.
* function number: it identifies a subsystem in the I/O controller. For instance, on multiple ports USB cards, this number refers to the controller associated to a port.

All DMA requests arriving at a DRHU contain a BDF-Id reported in their requester-id field (or source-id, according to Intel VT-d terminology). A DRHU uses this meta-data to identify the I/O controller that requests the memory access and to determine the associated memory regions.


### ARM’s SMMUs (to review)

A System Memory Management Unit (SMMU) performs a task that is analogous to that of an MMU in a CPU, translating addresses for DMA requests from system I/O devices before the requests are passed into the system interconnect.  In order to associate device traffic with translations and to differentiate devices behind an SMMU, requests have an extra property, alongside address, read/write, permissions, to identify a stream. 

A number of SMMUs might exist within a system. An SMMU might translate traffic from just one device or a set of devices.


![alt_text](images/interconnect2.png "interconnects")



## IOMMU initialization (to review)

The IOMMU initialization is another hot topic. The effectiveness of the IOMMU's service depends on how it gets configured. IOMMU assigns a particular IOVA to target devices, but the information is needed to make a working configuration. For a few devices, it can be trivial to get them; for other, general-purpose peripherals configuration can depend on a particular context where the peripheral is operating. Moreover, the OS expects the configuration to be firmware provided for many peripherals[26][27][28]. (e.g., ACPI table or Device Tree blob). Another important aspect is what a couple of papers[26][29] suggested about the default behavior for an IOMMU on receiving requests from an unknown device. They suggest the IOMMU default action for an unknown source DMA request is to **allow** the request. 


## Subpage items access (to review)

DMA makes the system vulnerable to DMA attacks and memory corruption in its basic form. These issues are supposed to be mitigated by the Input-Output Memory Management Unit (IOMMU), which adds a layer of virtual memory to devices. The IOMMU processes all I/O requests, translating their target I/O virtual addresses (IOVA) to physical addresses. In the process, the IOMMU provides address space isolation, allowing a device to access only permitted pages and preventing all other memory from being accessed. Unlike processes that operate at page granularity, I/O buffers can be significantly smaller than a page. I/O buffers and other kernel buffers can co-reside on the same physical pages, exposing these kernel buffers to the device.

Due to historical reasons, having a DMA buffer near other kernel structures is not the first thing that comes to mind. The Intel 8237[30], the DMA controller used in the old x86 machine, poses a lot of restrictions on how DMA could be used. It was bound to use lower memory, and so the Linux kernel used special pools of memory in the lower physical to satisfy these controller requirements. One consequence of this requirement was that it was unusual to have these lower physical memory pages used for kernel structures.

PCI DMA-capable devices do not have such requirements (in fact, they have a very similar one concerning 64bit machine and 32bit PAE extension), but inside the 32bit world, their DMA buffer can be on any memory page. These devices' DMA buffers can be anywhere in the 32bit address space, so driver developers, to reduce the memory usage, can allocate them using the standard **kmalloc()**.

For making a DMA transfer in one shot, it is required that the hardware can access memory space, that it is not cached, and that it is physically contiguous. To allocate the memory for the buffer, you basically may follow a couple of strategies:



* Use specialized allocator: **dma_alloc_coherent()**[31]. This allocator guarantees you you have your buffer in a hardware reachable address space and that it won't suffer from cache-related problems. The **dma_alloc_coherent()** allocates at least a full-page, so if you have many buffers to allocate, you may also use **dma_pool_create()**[32] and **dma_pool_alloc()**[33] to make smart use of it. But if you need just a single buffer smaller than the page size (typically 4k, but can be wider), you may consider another way.
* Go with one-direction streaming DMA mapping and use the standard **kmalloc()** and then **dma_map_single()**[34] or **dma_map_sg()**[35] depending if your buffer is physically fragmented or not.

In the latter case, unless bounce buffers are used by the DMA_api framework (there may be architectures that do this automatically, x86 does not), you may have the situation I pictured above. For this reason, known as the sub-page vulnerability, the IOMMU cannot fully protect the kernel from unprivileged access or the kernel's data structures to be overwritten.

To address this particular subpage issue, a couple of tools have been developed.



* Sub-Page Analysis for DMA Exposure (SPADE)[36]: this tool is a source code static analyzer targeted to find data structures containing function pointers. It is primarily oriented to security and its objective is not to prevent casual corruption but corruption that may lead to control flow hijacking. Still, its reports are also points of casual data corruption.
* DMA-KASAN[37]: dma-kasan patches the kernel by introducing the production of messages exposed by the kernel tracing framework (ftrace). It aims to report DMA violations.


# Conclusions (to review)

The document has listed and analyzed the entities accessing the system's physical memory. This work shows that independent entities can access system RAM and modify it. Fusa requires the address space integrity, but any of these subjects can potentially corrupt it. IOMMUs are designed to provide a virtual address space to devices that can access the host physical memory. Although this can prevent almost all DMA attacks, it can not guarantee protection against casual corruption. IOMMUs have memory page granularity when it comes to virtual address space definition, which means that, at best, they can not prevent devices from accessing any single byte within the allowed memory pages. Furthermore, IOMMUs as an actual device can not provide a dedicated Virtual address space for each device within the system. Concepts such as Domain and IOMMU groups are needed to associate a devices group to a particular DMA Remapping Hardware Unit. Because not all devices have the same needs in terms of accessing the host's physical memory, IOMMU must provide an IOVA, which is the union of all the addresses needed by every single device within the group, exposing more process data to the corruption of a malfunctioning peripheral/driver. In order to indicate which kind of device can be more dangerous: poorly programmed GPUs with a unified memory are more likely to cause problems. Unified memory for GPUs allows them to access data structures directly in place. Frameworks such as OpenMP use their own memory allocators, but in the end, these have no built-in memory corruption prevention mechanism, and structures are likely to be laid out one after another. DMA peripherals are the second most likely source of problems. Still, these are mitigated because the DMA buffer may be allocated in DMA reserved memory, and there, overflows might cause more minor issues. Evaluating the memory corruption risk for the microcontrollers embedded into the SoC is not a trivial task since they have not been standardized yet. Still, all implementations I've seen so far require this devices' operation memory to be well defined and mapped, which makes corruptions outside this memory range unlikely. 


# References



1. [https://en.wikipedia.org/wiki/Cache_coherence](https://en.wikipedia.org/wiki/Cache_coherence) Cache coherence
2. [https://doi.org/10.1145/3458817.3480855](https://doi.org/10.1145/3458817.3480855) In-Depth Analyses of Unified Virtual Memory System for GPU Accelerated Computing
3. [https://en.wikipedia.org/wiki/Shared_graphics_memory](https://en.wikipedia.org/wiki/Shared_graphics_memory) Shared Graphic Memory
4. [https://www.nvidia.com/content/dam/en-zz/Solutions/Data-Center/tesla-product-literature/NVIDIA-Kepler-GK110-GK210-Architecture-Whitepaper.pdf](https://www.nvidia.com/content/dam/en-zz/Solutions/Data-Center/tesla-product-literature/NVIDIA-Kepler-GK110-GK210-Architecture-Whitepaper.pdf) Nvidia KeplerTM GK110/210
5. [https://en.wikipedia.org/wiki/Heterogeneous_System_Architecture](https://en.wikipedia.org/wiki/Heterogeneous_System_Architecture) Heterogeneous System Architecture
6. [https://www.intel.com/content/dam/www/public/us/en/security-advisory/documents/cve-2019-0090-whitepaper.pdf](https://www.intel.com/content/dam/www/public/us/en/security-advisory/documents/cve-2019-0090-whitepaper.pdf) Intel’s virtualization technology The Intel Converged Security and Management Engine IOMMU Hardware Issue – CVE-2019-0090 
7. [https://linux-sunxi.org/AR100](https://linux-sunxi.org/AR100) Sunxi AR100
8. [https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) Intel® 64 and IA-32 Architectures Software Developer Manuals 
9. [https://documentation-service.arm.com/static/60804fe25e70d934bc69f12d](https://documentation-service.arm.com/static/60804fe25e70d934bc69f12d) Arm System Memory Management Unit Architecture Specification, SMMU architecture version 3 
10. [https://docs.google.com/document/d/1ytBZ6eDk1pAeBlZjDvm6_qqJbKQ0fMYKedyx0uoAGB0/edit#heading=h.aaaorcg4gat8](https://docs.google.com/document/d/1ytBZ6eDk1pAeBlZjDvm6_qqJbKQ0fMYKedyx0uoAGB0/edit#heading=h.aaaorcg4gat8) riscv iommu proposition 
11. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/intel/iommu.c#L5561](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/intel/iommu.c#L5561) Intel's VT-d iommu  
12. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu/arm-smmu.c#L1583](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu/arm-smmu.c#L1583) arm smmuv1 struct definition 
13. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c#L2834](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c#L2834) arm smmuv3 struct definition 
14. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/amd/iommu.c#L2247](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/amd/iommu.c#L2247) AMD I/O Virtualization Technology 
15. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu/qcom_iommu.c#L590](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/arm/arm-smmu/qcom_iommu.c#L590) Qualcomm iommu 
16. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/msm_iommu.c#L674](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/msm_iommu.c#L674) Qualcomm MSM iommu 
17. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/exynos-iommu.c#L1310](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/exynos-iommu.c#L1310) Samsung exynos iommu 
18. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/mtk_iommu.c#L659](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/mtk_iommu.c#L659) Mediatek's iommu 
19. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/omap-iommu.c#L1735](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/omap-iommu.c#L1735) Texas Instruments' OMAP iommu 
20. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/rockchip-iommu.c#L1188](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/rockchip-iommu.c#L1188) Rockchip's iommu 
21. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/sprd-iommu.c#L417](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/sprd-iommu.c#L417) Unisoc's iommu 
22. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/sun50i-iommu.c#L761](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/sun50i-iommu.c#L761) Allwinner's sun50i iommu 
23. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/tegra-gart.c#L278](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/tegra-gart.c#L278) Nvidia’s iommu
24. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/tegra-smmu.c#L969](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/tegra-smmu.c#L969) Nvidia’s smmu
25. [https://elixir.bootlin.com/linux/latest/source/drivers/iommu/s390-iommu.c#L363](https://elixir.bootlin.com/linux/latest/source/drivers/iommu/s390-iommu.c#L363) IBM S390 iommu 
26. [https://doi.org/10.1109/MALWARE.2010.5665798](https://doi.org/10.1109/MALWARE.2010.5665798) Exploiting an I/OMMU vulnerability 
27. [https://doi.org/10.5220/0006577202590266](https://doi.org/10.5220/0006577202590266) Hardware-based Cyber Threats 
28. [https://doi.org/10.1145/3447786.3456249](https://doi.org/10.1145/3447786.3456249) Characterizing, Exploiting, and Detecting DMA Code Injection Vulnerabilities in the Presence of an IOMMU 
29. [https://doi.org/10.1007/978-3-642-37300-8_2](https://doi.org/10.1007/978-3-642-37300-8_2) Understanding DMA Malware 
30. [https://en.wikipedia.org/wiki/Intel_8237](https://en.wikipedia.org/wiki/Intel_8237) Intel 8237 DMA controller 
31. [https://elixir.bootlin.com/linux/latest/source/tools/virtio/linux/dma-mapping.h#L16](https://elixir.bootlin.com/linux/latest/source/tools/virtio/linux/dma-mapping.h#L16) dma_alloc_coherent()
32. [https://elixir.bootlin.com/linux/latest/source/mm/dmapool.c#L130](https://elixir.bootlin.com/linux/latest/source/mm/dmapool.c#L130) dma_pool_create()
33. [https://elixir.bootlin.com/linux/latest/source/mm/dmapool.c#L314](https://elixir.bootlin.com/linux/latest/source/mm/dmapool.c#L314) dma_pool_alloc()
34. [https://elixir.bootlin.com/linux/latest/source/include/linux/dma-mapping.h#L406](https://elixir.bootlin.com/linux/latest/source/include/linux/dma-mapping.h#L406) dma_map_single()
35. [https://elixir.bootlin.com/linux/latest/source/include/linux/dma-mapping.h#L408](https://elixir.bootlin.com/linux/latest/source/include/linux/dma-mapping.h#L408) dma_map_sg()
36. [https://github.com/Markuze/mmo-static](https://github.com/Markuze/mmo-static) Sub-Page Analysis for DMA Exposure (SPADE) 
37. [https://github.com/Markuze/dma-kasan](https://github.com/Markuze/dma-kasan) DMA Kernel Address SANitizer (DMA-KASAN)
38. [https://gitlab.com/acarmina/dma-find/-/tree/master](https://gitlab.com/acarmina/dma-find/-/tree/master) 
