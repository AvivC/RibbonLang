	.file	"value.c"
	.text
	.globl	_value_array_init
	.def	_value_array_init;	.scl	2;	.type	32;	.endef
_value_array_init:
LFB17:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	movl	8(%ebp), %eax
	movl	$0, 8(%eax)
	movl	8(%ebp), %eax
	movl	$0, (%eax)
	movl	8(%ebp), %eax
	movl	$0, 4(%eax)
	nop
	popl	%ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE17:
	.section .rdata,"dr"
LC0:
	.ascii "Dynamic array buffer\0"
	.text
	.globl	_value_array_write
	.def	_value_array_write;	.scl	2;	.type	32;	.endef
_value_array_write:
LFB18:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	pushl	%ebx
	subl	$52, %esp
	.cfi_offset 3, -12
	movl	12(%ebp), %eax
	movl	%eax, -40(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, -36(%ebp)
	movl	20(%ebp), %eax
	movl	%eax, -32(%ebp)
	movl	24(%ebp), %eax
	movl	%eax, -28(%ebp)
	movl	8(%ebp), %eax
	movl	(%eax), %edx
	movl	8(%ebp), %eax
	movl	4(%eax), %eax
	cmpl	%eax, %edx
	jne	L3
	movl	8(%ebp), %eax
	movl	4(%eax), %eax
	movl	%eax, -12(%ebp)
	cmpl	$7, -12(%ebp)
	jle	L4
	movl	-12(%ebp), %eax
	leal	(%eax,%eax), %edx
	jmp	L5
L4:
	movl	$8, %edx
L5:
	movl	8(%ebp), %eax
	movl	%edx, 4(%eax)
	movl	8(%ebp), %eax
	movl	4(%eax), %eax
	sall	$4, %eax
	movl	%eax, %ecx
	movl	-12(%ebp), %eax
	sall	$4, %eax
	movl	%eax, %edx
	movl	8(%ebp), %eax
	movl	8(%eax), %eax
	movl	$LC0, 12(%esp)
	movl	%ecx, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	_reallocate
	movl	%eax, %edx
	movl	8(%ebp), %eax
	movl	%edx, 8(%eax)
L3:
	movl	8(%ebp), %eax
	movl	8(%eax), %ebx
	movl	8(%ebp), %eax
	movl	(%eax), %eax
	leal	1(%eax), %ecx
	movl	8(%ebp), %edx
	movl	%ecx, (%edx)
	sall	$4, %eax
	addl	%ebx, %eax
	movl	-40(%ebp), %edx
	movl	%edx, (%eax)
	movl	-36(%ebp), %edx
	movl	%edx, 4(%eax)
	movl	-32(%ebp), %edx
	movl	%edx, 8(%eax)
	movl	-28(%ebp), %edx
	movl	%edx, 12(%eax)
	nop
	addl	$52, %esp
	popl	%ebx
	.cfi_restore 3
	popl	%ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE18:
	.globl	_value_array_free
	.def	_value_array_free;	.scl	2;	.type	32;	.endef
_value_array_free:
LFB19:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$24, %esp
	movl	8(%ebp), %eax
	movl	4(%eax), %eax
	sall	$4, %eax
	movl	%eax, %edx
	movl	8(%ebp), %eax
	movl	8(%eax), %eax
	movl	$LC0, 8(%esp)
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	_deallocate
	movl	8(%ebp), %eax
	movl	%eax, (%esp)
	call	_value_array_init
	nop
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE19:
	.section .rdata,"dr"
LC1:
	.ascii "%g\0"
LC2:
	.ascii "true\0"
LC3:
	.ascii "false\0"
LC4:
	.ascii "nil\0"
LC5:
	.ascii "Unknown value type: %d.\0"
	.text
	.globl	_printValue
	.def	_printValue;	.scl	2;	.type	32;	.endef
_printValue:
LFB20:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$40, %esp
	movl	8(%ebp), %eax
	movl	%eax, -24(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -20(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, -16(%ebp)
	movl	20(%ebp), %eax
	movl	%eax, -12(%ebp)
	movl	-24(%ebp), %eax
	cmpl	$1, %eax
	je	L8
	testl	%eax, %eax
	je	L9
	cmpl	$2, %eax
	je	L10
	cmpl	$3, %eax
	je	L11
	jmp	L16
L9:
	fldl	-16(%ebp)
	fstpl	4(%esp)
	movl	$LC1, (%esp)
	call	_printf
	jmp	L13
L10:
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	_printObject
	jmp	L13
L8:
	movzbl	-16(%ebp), %eax
	testb	%al, %al
	je	L14
	movl	$LC2, %eax
	jmp	L15
L14:
	movl	$LC3, %eax
L15:
	movl	%eax, (%esp)
	call	_printf
	jmp	L13
L11:
	movl	$LC4, (%esp)
	call	_printf
	jmp	L13
L16:
	movl	-24(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	$LC5, (%esp)
	call	_printf
	nop
L13:
	nop
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE20:
	.section .rdata,"dr"
LC7:
	.ascii "FAILING! Reason:'\0"
LC8:
	.ascii "Couldn't compare values.\0"
LC9:
	.ascii "'\12\0"
	.text
	.globl	_compareValues
	.def	_compareValues;	.scl	2;	.type	32;	.endef
_compareValues:
LFB21:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$88, %esp
	movl	8(%ebp), %eax
	movl	%eax, -56(%ebp)
	movl	12(%ebp), %eax
	movl	%eax, -52(%ebp)
	movl	16(%ebp), %eax
	movl	%eax, -48(%ebp)
	movl	20(%ebp), %eax
	movl	%eax, -44(%ebp)
	movl	24(%ebp), %eax
	movl	%eax, -72(%ebp)
	movl	28(%ebp), %eax
	movl	%eax, -68(%ebp)
	movl	32(%ebp), %eax
	movl	%eax, -64(%ebp)
	movl	36(%ebp), %eax
	movl	%eax, -60(%ebp)
	movl	-56(%ebp), %edx
	movl	-72(%ebp), %eax
	cmpl	%eax, %edx
	je	L18
	movl	$0, %eax
	jmp	L19
L18:
	movl	-56(%ebp), %eax
	testl	%eax, %eax
	jne	L20
	fldl	-48(%ebp)
	fstpl	-16(%ebp)
	fldl	-64(%ebp)
	fstpl	-24(%ebp)
	fldl	-16(%ebp)
	fldl	-24(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	jp	L21
	fldl	-16(%ebp)
	fldl	-24(%ebp)
	fucompp
	fnstsw	%ax
	sahf
	jne	L21
	movl	40(%ebp), %eax
	movl	$0, (%eax)
	jmp	L23
L21:
	fldl	-16(%ebp)
	fcompl	-24(%ebp)
	fnstsw	%ax
	sahf
	jbe	L34
	movl	40(%ebp), %eax
	movl	$1, (%eax)
	jmp	L23
L34:
	movl	40(%ebp), %eax
	movl	$-1, (%eax)
L23:
	movl	$1, %eax
	jmp	L19
L20:
	movl	-56(%ebp), %eax
	cmpl	$1, %eax
	jne	L26
	movzbl	-48(%ebp), %eax
	movb	%al, -25(%ebp)
	movzbl	-64(%ebp), %eax
	movb	%al, -26(%ebp)
	movzbl	-25(%ebp), %eax
	cmpb	-26(%ebp), %al
	jne	L27
	movl	40(%ebp), %eax
	movl	$0, (%eax)
	jmp	L28
L27:
	movl	40(%ebp), %eax
	movl	$-1, (%eax)
L28:
	movl	$1, %eax
	jmp	L19
L26:
	movl	-56(%ebp), %eax
	cmpl	$2, %eax
	jne	L29
	movl	-64(%ebp), %edx
	movl	-48(%ebp), %eax
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	_compareObjects
	movb	%al, -27(%ebp)
	cmpb	$0, -27(%ebp)
	je	L30
	movl	40(%ebp), %eax
	movl	$0, (%eax)
	jmp	L31
L30:
	movl	40(%ebp), %eax
	movl	$-1, (%eax)
L31:
	movl	$1, %eax
	jmp	L19
L29:
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, 12(%esp)
	movl	$17, 8(%esp)
	movl	$1, 4(%esp)
	movl	$LC7, (%esp)
	call	_fwrite
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, 12(%esp)
	movl	$24, 8(%esp)
	movl	$1, 4(%esp)
	movl	$LC8, (%esp)
	call	_fwrite
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, 12(%esp)
	movl	$2, 8(%esp)
	movl	$1, 4(%esp)
	movl	$LC9, (%esp)
	call	_fwrite
	movl	$1, (%esp)
	call	_exit
L19:
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE21:
	.ident	"GCC: (MinGW.org GCC-8.2.0-3) 8.2.0"
	.def	_reallocate;	.scl	2;	.type	32;	.endef
	.def	_deallocate;	.scl	2;	.type	32;	.endef
	.def	_printf;	.scl	2;	.type	32;	.endef
	.def	_printObject;	.scl	2;	.type	32;	.endef
	.def	_compareObjects;	.scl	2;	.type	32;	.endef
	.def	_fwrite;	.scl	2;	.type	32;	.endef
	.def	_exit;	.scl	2;	.type	32;	.endef
