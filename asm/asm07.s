	.file	"asm07.c"
	.text
	.globl	_Z3sumv
	.type	_Z3sumv, @function
_Z3sumv:
.LFB0:
	movl	b(%rip), %eax
	addl	a(%rip), %eax
	ret
.LFE0:
	.size	_Z3sumv, .-_Z3sumv
	.ident	"GCC: (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0"
	.section	.note.GNU-stack,"",@progbits
