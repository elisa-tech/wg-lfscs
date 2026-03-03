# Linux VMA Investigation: Findings Document (Draft)

## Scope and interpretation of findings

This document consolidates the findings emerging from the Virtual Memory
Area (VMA) investigation.
Within this context, _findings_ refer to structurally plausible fault
scenarios evidenced through the analysis of:

-   VMA lifecycle mechanisms
-   Mapping construction and evolution
-   Permission enforcement paths
-   Allocator dependencies
-   Kernel memory topology

The objective is not to enumerate implementation defects, but to characterize
conditions under which isolation, containment, or integrity guarantees may be
stressed.

# Analytical perimeter

The investigation progressively expanded beyond VMA structures themselves to
include interacting subsystems influencing mapping guarantees, including:

-   Page allocation and reclamation
-   Slab and object allocators
-   Page fault handling
-   Mapping inheritance mechanisms
-   Kernel linear mapping behavior

This systemic perspective informs the findings described below.

# Finding domains

Findings are grouped into five domains reflecting where fault conditions may
emerge.

## Mapping topology faults

### Observational basis

VMAs define the structural representation of process address spaces,
governing:

-   Mapping ranges
-   Permission attributes
-   Backing associations

Mappings evolve through operations such as:

-   `mmap`
-   `munmap`
-   `mremap`
-   `fork`
-   `mprotect`

### Evidenced fault scenarios

-   Inconsistent mapping splits or merges
-   Incorrect inheritance during fork
-   Permission transition timing gaps
-   Stale mapping attributes

### Isolation impact

Such conditions may result in:

-   Transient containment boundary misalignment
-   Spatial containment boundary convergence
-   Latency in containment state enforcement

## Backing memory provenance faults

### Observational basis

VMAs rely on backing memory supplied by the page allocator and related
subsystems.

Backing pages may undergo:

-   Reuse after reclamation
-   Migration
-   Compaction
-   Swapping

### Evidenced fault scenarios

-   Reallocation of pages with residual state
-   Provenance ambiguity across reuse cycles
-   Inconsistent metadata association

### Isolation impact

These dynamics may influence:

-   Cross-context mapping separation
-   Mapping confidentiality
-   Temporal stability of containment boundaries

## Spatial adjacency faults

### Observational basis

The kernel linear mapping establishes a contiguous virtual projection of
physical memory.

This projection includes:

-   Kernel objects
-   Allocator metadata
-   Page structures
-   User-backed pages

### Evidenced fault scenarios

-   Physical adjacency between unrelated contexts
-   Kernel object proximity to user memory
-   Metadata co-location with mapped pages

### Isolation impact

While page permissions remain enforced, spatial proximity creates conditions
where corruption may propagate if kernel state integrity is disrupted.

Due to the monolithic execution model, a misbehaving kernel component can
issue legitimate writes within its domain that overlap or influence memory
regions associated with other contexts.

## Kernel co-tenancy faults

### Observational basis

Kernel space hosts heterogeneous actors, including:

-   Core subsystems
-   Drivers
-   Allocators
-   Memory managers

All operate within a shared execution and memory domain.

### Evidenced fault scenarios

-   Object overwrite across subsystems
-   Allocator metadata corruption
-   Shared structure interference
-   Execution-context memory misuse

### Isolation impact

Containment boundaries between kernel actors are functionally narrow, making
interference propagation structurally possible.

## Mapping-mediated cross-domain faults

### Observational basis

VMAs mediate shared memory constructs, including:

-   File-backed mappings
-   Shared anonymous memory
-   Inter-process shared regions

### Evidenced fault scenarios

-   Misconfigured sharing permissions
-   Delayed copy-on-write enforcement
-   Shared backing misuse
-   Cross-process write exposure

### Isolation impact

Such conditions may affect:

-   Process isolation guarantees
-   Data integrity across domains
-   Containment of faults originating in peer contexts

# Propagation dynamics

Beyond individual fault sites, the investigation identified propagation
pathways where faults may extend across subsystems.

Examples include:

-   Corruption of page tables influencing mapping enforcement
-   Allocator metadata faults affecting future mappings
-   Reclaim operations relocating compromised pages
-   Fault handler misdirection due to mapping inconsistency

These propagation chains illustrate how localized integrity violations may
scale systemically.

# Amplification conditions

Certain system properties may amplify fault impact:

### Memory pressure

Under reclaim or OOM conditions:

-   Mapping persistence changes
-   Allocation locality shifts
-   Reuse frequency increases

### Dynamic mapping evolution

Frequent operations such as:

-   Remapping
-   Permission changes
-   Process cloning

Increase structural churn within address spaces.

### Kernel extensibility

Loadable modules and drivers introduce:

-   Additional allocators
-   DMA interactions
-   Direct memory manipulation

Expanding the interference surface.

# Consolidated findings summary

| Finding domain       | Structural origin   | Potential impact        |
|----------------------|---------------------|-------------------------|
| Mapping topology     | VMA lifecycle       |Permission gaps, overlap |
| Backing provenance   | Allocator reuse     |Data integrity loss      |
| Spatial adjacency    | Linear mapping      |Corruption propagation   |
| Kernel co-tenancy    | Shared kernel space |Subsystem interference   |
| Cross-domain mapping | Shared VMAs         |Isolation breaches       |
| Propagation chains   | Subsystem coupling  |Cascaded faults          |

# Positioning statement

The investigation evidences that VMA isolation guarantees emerge from the
coordinated behavior of multiple subsystems rather than from mapping
structures alone.

Fault conditions therefore arise not only from mapping misconfiguration, but
from interactions involving allocators, kernel memory topology, and execution
co-tenancy.

Understanding these relationships is essential to characterizing containment
boundaries within Linux memory management.
