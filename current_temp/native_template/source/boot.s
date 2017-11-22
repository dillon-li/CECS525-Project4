/* boot.s - boot for the CECS 525 Raspberry PI kernel.img for Berry OS
   by Eugene Rockey Copyright 2013 All Rights Reserved
   debug everything that needs debugging
   Add, remove, modify, preserve in order to fulfill project 4 requirements. */

.set	Usr32md,	0x10		/* ARM modes */
.set	FIQ32md,	0x11
.set	IRQ32md,	0x12
.set	Svc32md,	0x13
.set	Abt32md,	0x17
.set	Und32md,	0x1b
.set	Sys32md,	0x1f

.set	IrqFiq,		0xC0        /* IRQ and FIQ bits in the CPSR */

.set SYS_Stack, 0x5000          /* Stack addresses for the different modes employed */
.set IRQ_Stack, 0x6000
.set SVC_Stack, 0x7000

.section ".data"                /* variables that can be used between assembly and C */
.global invar, outvar
.set invar, 0
.set outvar, 0

.section ".text"
.global vectors
/***********************************Vectors***************************************/
vectors: 	B	reset
		B	undef
		B	swi
		B	pabt
		B	dabt
		B	na
		B	irq
		B	fiq
/**********************************************************************************/
.org 0x8000
.global reset
/************************************Reset*************************************/
reset:		MOV 	r2, #Svc32md | IrqFiq 	/*Initialize supervisor stack pointer*/
		MSR 	cpsr_c, r2
        	LDR	r13, =SVC_Stack

		MOV 	r2, #IRQ32md	 	/*Initialize irq stack pointer*/
		MSR 	cpsr_c, r2
		LDR	r13, =IRQ_Stack

		MOV	r2, #Sys32md		/*Initialize system stack pointer*/
		MSR	cpsr_c, r2
		LDR	r13, =SYS_Stack

		MRC	p15, 0, r0, c1, c0,2	/*initialize VFP11 math coprocessor*/
		ORR	r0, r0, #0x300000
		ORR	r0, r0, #0xC00000
		MCR	p15, 0, r0, c1, c0, 2
		MOV	r0, #0x40000000
        	FMXR	fpexc, r0		/*enable the VFP11 math coprocessor */

		B	kernel_main		/* run Tiny OS in system mode */
/***********************************Undefined****************************/		
undef:		B	reset
		STMFD	r13!,{r0-r12, r14}
		NOP                         /*How would you handle this exception?*/
        LDMFD	r13!,{r0-r12, pc}^
/**************************************Software Interrupt******************/		
swi:		B	reset
		STMFD	r13!,{r0-r12, r14}
		NOP                         /*How would you handle this exception?*/
        LDMFD	r13!,{r0-r12, pc}^
/************************************Prefetch Abort**************************/
pabt:		B	reset
		SUB	r14, r14, #4
		STMFD	r13!,{r0-r12, r14}
		NOP                         /*How would you handle this exception?*/
        LDMFD	r13!,{r0-r12, pc}^
/*************************************Data Abort*******************************/	
dabt:		B	reset
		SUB	r14, r14, #8
		STMFD	r13!,{r0-r12, r14}
		NOP                         /*How would you handle this exception?*/
        LDMFD	r13!,{r0-r12, pc}^
/**************************************Reserved**********************************/
na:		B	reset                   /*How would you handle this exception?*/
/**************************************Interrupt Request**************************/
irq:		SUB	r14, r14, #4
		STMFD	r13!,{r0-r12, r14}	/*IRQ Handler, first save registers and return address to stack*/
		BL	irq_handler             /*This one is handled*/
		LDMFD	r13!,{r0-r12, pc}^	/*Restore registers from stack and return to application*/
/**************************************Fast Interrupt Request**********************/
fiq:		B	reset
		SUB	r14, r14, #4
		STMFD	r13!,{r0-r7, r14}			
		NOP                         /*How would you handle this exception?*/
		LDMFD	r13!,{r0-r7, pc}^	
/**********************************************************************************/

.global reboot
reboot:
		B	reset

.global enable_arm_irq
enable_arm_irq:
		MRS	r1, cpsr
		BIC	r1, r1, #0x80
		MSR	cpsr_c, r1
		MOV	pc, lr
		
.global disable_arm_irq
disable_arm_irq:
		MRS	r1,cpsr
		ORR	r1, r1, #0x80
		MSR	cpsr_c, r1
		MOV	pc, lr

.global enable_arm_fiq
enable_arm_fiq:
		MRS	r1, cpsr
		BIC	r1, r1, #0x40
		MSR	cpsr_c, r1
		MOV	pc, lr
		
.global disable_arm_fiq
disable_arm_fiq:
		MRS	r1, cpsr
		ORR	r1, r1, #0x40
		MSR	cpsr_c, r1
		MOV	pc, lr

.global vfp11_add
vfp11_add:
		FMSRR	{s1, s2}, r0, r1
		FADDS	s2, s2, s1	
		FMRS	r0, s2
		MOV	pc, lr

		


