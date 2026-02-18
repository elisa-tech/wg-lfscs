# Linux VMA Investigation: Synthesis Framing

## Investigation perimeter recap

The investigation explored Virtual Memory Areas not as isolated kernel
structures, but as part of a broader memory management ecosystem.

The work progressively expanded from:

-   VMA lifecycle mechanics
-   Address space construction
-   Mapping evolution
-   Permission enforcement

Into surrounding subsystems that influence or depend on them, including:

-   Allocation paths
-   Caching layers
-   Page fault handling
-   Instrumentation mechanisms
-   Pressure and reclaim strategies

This widening scope was necessary to understand not only how VMAs operate,
but under which conditions their guarantees may be stressed.

## Structural complexity of the memory model

What emerges from the investigation is a layered system where behavior is
shaped by the interaction of multiple mechanisms rather than by a single
enforcement point.

Examples include:

-   Copy-on-write inheritance during fork
-   Dynamic remapping via mmap/mremap
-   Permission transitions through mprotect
-   Lazy allocation via demand paging

Each mechanism independently preserves isolation assumptions, yet their
composition introduces corner cases:

-   Timing dependencies
-   State transitions
-   Partial updates
-   Cache coherency windows

The address space is therefore not static, but continuously evolving.

## Influence of runtime conditions

Isolation guarantees are also shaped by system state.

Under memory pressure, the kernel activates recovery strategies such as:

-   Page reclamation
-   Swap migration
-   OOM termination heuristics

These mechanisms alter mapping persistence, residency, and allocation timing.

From a safety analysis perspective, this introduces environmental variability:

-   Mappings may disappear or reappear
-   Allocation locality may shift
-   Shared resources may be reclaimed

The isolation model is therefore influenced by global system dynamics, not
only local process behavior.

## Kernel space as a shared operational arena

While user address spaces are isolated, kernel space operates under a
different model.

Kernel memory:

-   Hosts core metadata
-   Allocator structures
-   Object caches
-   Execution code
-   Driver allocations

All of these coexist within the same operational domain.

This is structurally different from user isolation:

-   There is no per-driver address space
-   No per-subsystem containment boundary
-   No hardware-enforced separation between kernel actors

Kernel components therefore interact through shared memory substrates.

## 5. Linear mapping as an exposure amplifier

The linear map introduces a particularly relevant structural property.

It provides:

-   A contiguous virtual projection of physical RAM
-   Direct dereference capability for kernel code
-   Fast access to allocator outputs

Operationally, this means:

-   Kernel objects reside in linearly mapped space
-   User pages and kernel pages originate from the same physical pool
-   Allocator reuse can relocate data across contexts

Adjacency therefore becomes possible at the physical level.

This does not violate isolation at the page table level, but it narrows
spatial separation inside kernel reach.

## Proximity and metadata sensitivity

Within the linear map reside:

-   Page metadata structures
-   Slab objects
-   Allocator bookkeeping
-   Kernel lists and caches

These structures are both:

-   Operationally critical
-   Spatially co-located

Corruption in this space can propagate through allocator state, mapping
structures, or object graphs.

Hardening mechanisms exist (freelist protections, poisoning, sanitizers),
but they operate as detection or mitigation layers rather than structural
separation mechanisms.

## Driver execution model

Drivers operate inside kernel space with:

-   Direct memory access capabilities
-   Allocator access
-   DMA interactions
-   Interrupt-context execution

A malfunctioning driver therefore:

-   Executes with kernel privileges
-   Operates inside shared memory arenas
-   Can influence allocator state
-   Can affect mapping integrity

This is not an anomalous condition, it is intrinsic to the kernel
extensibility model.

## Narrowed containment boundaries

From the investigation, containment boundaries inside kernel space appear
functionally narrow:

-   Objects share allocator pools
-   Metadata shares mapping regions
-   Execution contexts share privilege domains
-   Hardware interactions bypass some abstractions (e.g., DMA)

Isolation enforcement is therefore concentrated at:

-   Page permissions
-   Mapping structures
-   Runtime checks

Rather than at structural spatial separation.

## Synthesis perspective

The VMA subsystem provides:

-   Process-level isolation
-   Permission enforcement
-   Mapping lifecycle control

However, its guarantees operate within a broader environment characterized by:

-   Dynamic mapping evolution
-   Allocator reuse
-   Shared kernel memory arenas
-   Linear physical projection
-   Driver co-tenancy

Understanding VMA safety therefore requires analyzing these surrounding
interactions, not only the VMA structures themselves.
