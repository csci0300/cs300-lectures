	.file	"controlflow02.c"
	.text
	.globl	_Z1fPKii
	.type	_Z1fPKii, @function
_Z1fPKii:
.LFB0:
	testl	%esi, %esi
	jle	.L4
	movq	%rdi, %rax
	leal	-1(%rsi), %edx
	leaq	4(%rdi,%rdx,4), %rsi
	movl	$0, %edx
.L3:
	movsbq	(%rax), %rcx
	addq	%rcx, %rdx
	addq	$4, %rax
	cmpq	%rsi, %rax
	jne	.L3
.L1:
	movq	%rdx, %rax
	ret
.L4:
	movl	$0, %edx
	jmp	.L1
.LFE0:
	.size	_Z1fPKii, .-_Z1fPKii
	.ident	"GCC: (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0"
	.section	.note.GNU-stack,"",@progbits
