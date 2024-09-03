**Observation**: 
During a recent working group meeting, a discrepancy was 
identified in the log of the `min_prog_trace` traced application. 
The function `chacha_block_generic` was observed, which is unexpected given 
its association with the `CRYPTO API` subsystem.

**Investigation**: 
To understand the root cause of this unexpected occurrence, 
a preliminary investigation was initiated. The focus of this investigation 
was to determine how `chacha_block_generic` is being invoked and why it's 
appearing in the context of the `min_prog_trace` traced application.

**Findings**: 
The log reading suggests that `chacha_block_generic` is invoked 
through the following call chain:
```
load_elf_binary -> setup_arg_pages -> arch_align_stack -> get_random_u16 -> _get_random_bytes -> crng_make_state -> crng_fast_key_erasure -> chacha_block_generic 
```
Reading the source code seem not to confirm this call trace,
in particular, the `arch_align_stack` function appear to not call
`get_random_u16`.
```
unsigned long arch_align_stack(unsigned long sp)
{
	if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
		sp -= get_random_u32_below(PAGE_SIZE);
	return sp & ~0xf;
}
```
But it is probably a compiler magic working under the hood, since:
```
┌ 84: sym.arch_align_stack (int64_t arg1);
│           ; arg int64_t arg1 @ x0
│           ; var int64_t var_10h @ sp+0x10
│           0xffffffc080018b20      1f2003d5       nop                               ; process.c:595 {
│           0xffffffc080018b24      1f2003d5       nop
│           0xffffffc080018b28      3f2303d5       paciasp                           ; process.c:596  if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
│           0xffffffc080018b2c      fd7bbea9       stp x29, x30, [sp, -0x20]!
│           0xffffffc080018b30      fd030091       mov x29, sp
│           0xffffffc080018b34      f30b00f9       str x19, [sp, 0x10]
│           0xffffffc080018b38      f30300aa       mov x19, x0                       ; process.c:595 { ; arg1
│           0xffffffc080018b3c      004138d5       mrs x0, sp_el0                    ; current.h:19  asm ("mrs %0, sp_el0" : "=r" (sp_el0));
│           0xffffffc080018b40      008043b9       ldr w0, [x0, 0x380]               ; current.h:21  return (struct task_struct *)sp_el0; ; 0xda ; 218
│       ┌─< 0xffffffc080018b44      e0009037       tbnz w0, 0x12, 0xffffffc080018b60 ; process.c:596  if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
│       │   0xffffffc080018b48      003a00b0       adrp x0, 0xffffffc080759000
│       │   0xffffffc080018b4c      00f046b9       ldr w0, [x0, 0x6f0]               ; 0xda ; 218
│      ┌──< 0xffffffc080018b50      80000034       cbz w0, 0xffffffc080018b60
│      ││   0xffffffc080018b54      77690894       bl sym.get_random_u16             ; process.c:597   sp -= get_random_u32_below(PAGE_SIZE);
│      ││   0xffffffc080018b58      003c44d3       ubfx x0, x0, 4, 0xc               ; random.h:83    if (likely(is_power_of_2(ceil) || (u16)mult >= (1U << 16) % ceil))
│      ││   0xffffffc080018b5c      730200cb       sub x19, x19, x0                  ; process.c:597   sp -= get_random_u32_below(PAGE_SIZE);
│      └└─> 0xffffffc080018b60      60ee7c92       and x0, x19, 0xfffffffffffffff0   ; process.c:598  return sp & ~0xf;
│           0xffffffc080018b64      f30b40f9       ldr x19, [var_10h]                ; 5
│           0xffffffc080018b68      fd7bc2a8       ldp x29, x30, [sp], 0x20          ; process.c:599 }
│           0xffffffc080018b6c      bf2303d5       autiasp
└           0xffffffc080018b70      c0035fd6       ret
```
Binary analysis **confirms** the trace.

**Additional notes**:
In the `lib/crypto/Makefile` you can read the following:
```
# chacha is used by the /dev/random driver which is always builtin
obj-y                                           += chacha.o
```
And in parent `lib/Makefile`:
```
obj-y += math/ crypto/
```

**Significance**: 
Surprisingly, the `CRYPTO API`, at least as it pertains to 
the **chacha** functions, appears to be a minimal feature that Linux must 
provide to offer even a very basic level of functionality. 
This suggests that the invocation of `chacha_block_generic` in this context 
is a necessary part of the operating system's core operations.
