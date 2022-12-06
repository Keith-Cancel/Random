    .global rand64
    .global rand64_maximum
    .text

rand64:
    rdrand %rax
    jnc    rand64
    ret

rand64_maximum:
    mov   %rdi, %rcx
    or    $1,   %rcx # Or by 1 to handle zero case
    mov   $-1,  %rdx
    lzcnt %rcx, %rcx
    shr   %cl,  %rdx # Mask stored in rdx
loop:
    rdrand %rax
    jnc    loop
    and    %rdx, %rax # Mask random number
    cmp    %rdi, %rax # Make sure number is not greater than the limit
    ja     loop
    ret

.section	.note.GNU-stack,"",@progbits
