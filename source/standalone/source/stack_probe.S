/* Probe Windows stack guard pages while preserving the requested size in RAX.
   The caller subtracts that size from RSP after this helper returns. */
.text
.globl ___chkstk_ms
___chkstk_ms:
    pushq %rcx
    pushq %rax
    leaq 24(%rsp), %rcx
    cmpq $4096, %rax
    jb .Lfinal_probe
.Lprobe_page:
    subq $4096, %rcx
    orb $0, (%rcx)
    subq $4096, %rax
    cmpq $4096, %rax
    ja .Lprobe_page
.Lfinal_probe:
    subq %rax, %rcx
    orb $0, (%rcx)
    popq %rax
    popq %rcx
    retq
