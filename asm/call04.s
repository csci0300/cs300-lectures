	.file	"call04.c"
	.text
	.globl	_Z1fllllll
	.type	_Z1fllllll, @function
_Z1fllllll:
.LFB0:
	movl	%edi, a(%rip)
	movl	%esi, 4+a(%rip)
	movl	%edx, 8+a(%rip)
	movl	%ecx, 12+a(%rip)
	movl	%r8d, 16+a(%rip)
	movl	%r9d, 20+a(%rip)
	ret
.LFE0:
	.size	_Z1fllllll, .-_Z1fllllll
	.ident	"GCC: (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0"
	.section	.note.GNU-stack,"",@progbits
