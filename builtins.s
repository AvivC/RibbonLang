	.file	"builtins.c"
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
	.globl	_builtin_print
	.def	_builtin_print;	.scl	2;	.type	32;	.endef
_builtin_print:
LFB20:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$24, %esp
	movl	16(%ebp), %eax
	movl	(%eax), %edx
	movl	%edx, (%esp)
	movl	4(%eax), %edx
	movl	%edx, 4(%esp)
	movl	8(%eax), %edx
	movl	%edx, 8(%esp)
	movl	12(%eax), %eax
	movl	%eax, 12(%esp)
	call	_printValue
	movl	$10, (%esp)
	call	_putchar
	movl	__imp___iob, %eax
	addl	$32, %eax
	movl	%eax, (%esp)
	call	_fflush
	movl	20(%ebp), %eax
	movl	$3, (%eax)
	movl	20(%ebp), %eax
	fld1
	fchs
	fstpl	8(%eax)
	movl	$1, %eax
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE20:
	.section .rdata,"dr"
LC3:
	.ascii "Object string buffer\0"
	.text
	.globl	_builtin_input
	.def	_builtin_input;	.scl	2;	.type	32;	.endef
_builtin_input:
LFB21:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$56, %esp
	movl	$0, -24(%ebp)
	movl	$0, -12(%ebp)
	movl	$10, -16(%ebp)
	movl	$LC3, -28(%ebp)
	movl	-16(%ebp), %eax
	movl	-28(%ebp), %edx
	movl	%edx, 4(%esp)
	movl	%eax, (%esp)
	call	_allocate
	movl	%eax, -20(%ebp)
	cmpl	$0, -20(%ebp)
	jne	L12
	movl	$0, %eax
	jmp	L11
L15:
	movl	-12(%ebp), %eax
	leal	1(%eax), %edx
	movl	%edx, -12(%ebp)
	movl	%eax, %edx
	movl	-20(%ebp), %eax
	addl	%edx, %eax
	movl	-24(%ebp), %edx
	movb	%dl, (%eax)
	movl	-16(%ebp), %eax
	subl	$1, %eax
	cmpl	%eax, -12(%ebp)
	jne	L12
	movl	-16(%ebp), %eax
	movl	%eax, -32(%ebp)
	cmpl	$7, -32(%ebp)
	jle	L13
	movl	-32(%ebp), %eax
	addl	%eax, %eax
	jmp	L14
L13:
	movl	$8, %eax
L14:
	movl	%eax, -16(%ebp)
	movl	-16(%ebp), %edx
	movl	-32(%ebp), %eax
	movl	-28(%ebp), %ecx
	movl	%ecx, 12(%esp)
	movl	%edx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	-20(%ebp), %eax
	movl	%eax, (%esp)
	call	_reallocate
	movl	%eax, -20(%ebp)
L12:
	call	_getchar
	movl	%eax, -24(%ebp)
	cmpl	$10, -24(%ebp)
	jne	L15
	movl	-12(%ebp), %edx
	movl	-20(%ebp), %eax
	addl	%edx, %eax
	movb	$0, (%eax)
	movl	-12(%ebp), %eax
	leal	1(%eax), %ecx
	movl	-16(%ebp), %eax
	movl	-28(%ebp), %edx
	movl	%edx, 12(%esp)
	movl	%ecx, 8(%esp)
	movl	%eax, 4(%esp)
	movl	-20(%ebp), %eax
	movl	%eax, (%esp)
	call	_reallocate
	movl	%eax, -20(%ebp)
	movl	-12(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	-20(%ebp), %eax
	movl	%eax, (%esp)
	call	_takeString
	movl	%eax, -36(%ebp)
	movl	20(%ebp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	movl	20(%ebp), %eax
	movl	$2, (%eax)
	movl	20(%ebp), %eax
	movl	-36(%ebp), %edx
	movl	%edx, 8(%eax)
	movl	$1, %eax
L11:
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE21:
	.section .rdata,"dr"
LC4:
	.ascii "rb\0"
	.align 4
LC5:
	.ascii "Couldn't open file '%s'. Error:\12'\0"
LC6:
	.ascii "fopen\0"
LC7:
	.ascii "'\12\0"
	.align 4
LC8:
	.ascii "Couldn't read entire file. File size: %d. Bytes read: %d.\12\0"
LC9:
	.ascii "Couldn't close file.\12\0"
	.text
	.globl	_builtin_read_file
	.def	_builtin_read_file;	.scl	2;	.type	32;	.endef
_builtin_read_file:
LFB22:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	movl	%esp, %ebp
	.cfi_def_cfa_register 5
	subl	$72, %esp
	movl	16(%ebp), %eax
	movl	$0, 16(%esp)
	movl	(%eax), %edx
	movl	%edx, (%esp)
	movl	4(%eax), %edx
	movl	%edx, 4(%esp)
	movl	8(%eax), %edx
	movl	%edx, 8(%esp)
	movl	12(%eax), %eax
	movl	%eax, 12(%esp)
	call	_is_value_object_of_type
	xorl	$1, %eax
	testb	%al, %al
	je	L17
	movl	$0, %eax
	jmp	L18
L17:
	movl	16(%ebp), %eax
	movl	8(%eax), %eax
	movl	%eax, (%esp)
	call	_objectAsString
	movl	%eax, -12(%ebp)
	movl	-12(%ebp), %eax
	movl	28(%eax), %eax
	movl	$LC4, 4(%esp)
	movl	%eax, (%esp)
	call	_fopen
	movl	%eax, -16(%ebp)
	cmpl	$0, -16(%ebp)
	jne	L19
	movl	-12(%ebp), %eax
	movl	28(%eax), %eax
	movl	%eax, 8(%esp)
	movl	$LC5, 4(%esp)
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, (%esp)
	call	_fprintf
	movl	$LC6, (%esp)
	call	_perror
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, 12(%esp)
	movl	$2, 8(%esp)
	movl	$1, 4(%esp)
	movl	$LC7, (%esp)
	call	_fwrite
	movl	$0, %eax
	jmp	L18
L19:
	movl	$2, 8(%esp)
	movl	$0, 4(%esp)
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	_fseek
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	_ftell
	movl	%eax, -20(%ebp)
	movl	$0, 8(%esp)
	movl	$0, 4(%esp)
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	_fseek
	movl	-20(%ebp), %eax
	addl	$1, %eax
	movl	$LC3, 4(%esp)
	movl	%eax, (%esp)
	call	_allocate
	movl	%eax, -24(%ebp)
	movl	-16(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	-20(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	$1, 4(%esp)
	movl	-24(%ebp), %eax
	movl	%eax, (%esp)
	call	_fread
	movl	%eax, -28(%ebp)
	movl	-28(%ebp), %eax
	cmpl	-20(%ebp), %eax
	je	L20
	movl	-28(%ebp), %eax
	movl	%eax, 12(%esp)
	movl	-20(%ebp), %eax
	movl	%eax, 8(%esp)
	movl	$LC8, 4(%esp)
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, (%esp)
	call	_fprintf
	movl	$0, %eax
	jmp	L18
L20:
	movl	-16(%ebp), %eax
	movl	%eax, (%esp)
	call	_fclose
	testl	%eax, %eax
	je	L21
	movl	__imp___iob, %eax
	addl	$64, %eax
	movl	%eax, 12(%esp)
	movl	$21, 8(%esp)
	movl	$1, 4(%esp)
	movl	$LC9, (%esp)
	call	_fwrite
	movl	$0, %eax
	jmp	L18
L21:
	movl	-24(%ebp), %edx
	movl	-20(%ebp), %eax
	addl	%edx, %eax
	movb	$0, (%eax)
	movl	-20(%ebp), %eax
	movl	%eax, 4(%esp)
	movl	-24(%ebp), %eax
	movl	%eax, (%esp)
	call	_takeString
	movl	%eax, %edx
	movl	20(%ebp), %eax
	movl	$0, (%eax)
	movl	$0, 4(%eax)
	movl	$0, 8(%eax)
	movl	$0, 12(%eax)
	movl	20(%ebp), %eax
	movl	$2, (%eax)
	movl	20(%ebp), %eax
	movl	%edx, 8(%eax)
	movl	$1, %eax
L18:
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
LFE22:
	.ident	"GCC: (MinGW.org GCC-8.2.0-3) 8.2.0"
	.def	_reallocate;	.scl	2;	.type	32;	.endef
	.def	_deallocate;	.scl	2;	.type	32;	.endef
	.def	_printValue;	.scl	2;	.type	32;	.endef
	.def	_putchar;	.scl	2;	.type	32;	.endef
	.def	_fflush;	.scl	2;	.type	32;	.endef
	.def	_allocate;	.scl	2;	.type	32;	.endef
	.def	_getchar;	.scl	2;	.type	32;	.endef
	.def	_takeString;	.scl	2;	.type	32;	.endef
	.def	_is_value_object_of_type;	.scl	2;	.type	32;	.endef
	.def	_objectAsString;	.scl	2;	.type	32;	.endef
	.def	_fopen;	.scl	2;	.type	32;	.endef
	.def	_fprintf;	.scl	2;	.type	32;	.endef
	.def	_perror;	.scl	2;	.type	32;	.endef
	.def	_fwrite;	.scl	2;	.type	32;	.endef
	.def	_fseek;	.scl	2;	.type	32;	.endef
	.def	_ftell;	.scl	2;	.type	32;	.endef
	.def	_fread;	.scl	2;	.type	32;	.endef
	.def	_fclose;	.scl	2;	.type	32;	.endef
