	.file	"asm15.c"
	.text
	.globl	_Z3sumii
	.type	_Z3sumii, @function
_Z3sumii:
.LFB0:
	movl	%edi, -4(%rsp)
	movl	%esi, -8(%rsp)
	movl	-4(%rsp), %edx
	movl	-8(%rsp), %eax
	addl	%edx, %eax
	ret
.LFE0:
	.size	_Z3sumii, .-_Z3sumii
	.ident	"GCC: (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0"
	.section	.note.GNU-stack,"",@progbits
