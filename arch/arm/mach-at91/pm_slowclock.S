/*
 * arch/arm/mach-at91/pm_slow_clock.S
 *
 *  Copyright (C) 2006 Savin Zlobec
 *
 * AT91SAM9 support:
 *  Copyright (C) 2007 Anti Sullin <anti.sullin@artecdesign.ee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/linkage.h>
#include <mach/hardware.h>
#include <mach/at91_pmc.h>
#include <mach/at91_ramc.h>

	.arm

#ifdef CONFIG_SOC_AT91SAM9263
/*
 * FIXME either or both the SDRAM controllers (EB0, EB1) might be in use;
 * handle those cases both here and in the Suspend-To-RAM support.
 */
#warning Assuming EB1 SDRAM controller is *NOT* used
#endif

/*
 * When SLOWDOWN_MASTER_CLOCK is defined we will also slow down the Master
 * clock during suspend by adjusting its prescalar and divisor.
 * NOTE: This hasn't been shown to be stable on SAM9s; and on the RM9200 there
 *       are errata regarding adjusting the prescalar and divisor.
 */
#undef SLOWDOWN_MASTER_CLOCK

#define	SRAMC_SELF_REFRESH_ACTIVE	0x01
#define	SRAMC_SELF_REFRESH_EXIT		0x00

#define	DDR_CLOCK_ENABLE		0x01
#define	DDR_CLOCK_DISABLE		0x00

pmc	.req	r0
tmp1	.req	r4
tmp2	.req	r5

/*
 * Wait until master clock is ready (after switching master clock source)
 */
	.macro wait_mckrdy
1:	ldr	tmp1, [pmc, #AT91_PMC_SR]
	tst	tmp1, #AT91_PMC_MCKRDY
	beq	1b
	.endm

/*
 * Wait until master oscillator has stabilized.
 */
	.macro wait_moscrdy
1:	ldr	tmp1, [pmc, #AT91_PMC_SR]
	tst	tmp1, #AT91_PMC_MOSCS
	beq	1b
	.endm

/*
 * Wait until PLLA has locked.
 */
	.macro wait_pllalock
1:	ldr	tmp1, [pmc, #AT91_PMC_SR]
	tst	tmp1, #AT91_PMC_LOCKA
	beq	1b
	.endm

/*
 * Put the processor to enter into Standby mode, wait for interrupt to wakeup
 */
	.macro _do_wfi

#if defined(CONFIG_CPU_V7)
	mov	tmp1, #AT91_PMC_PCK
	str	tmp1, [pmc, #AT91_PMC_SCDR]

	dsb

	wfi		@ Wait For Interrupt
#else
	mcr	p15, 0, tmp1, c7, c0, 4
#endif

	.endm

	.text

/* void at91_slow_clock(void __iomem *pmc, void __iomem *sdramc,
 *			void __iomem *ramc1, int memctrl)
 */
ENTRY(at91_slow_clock)
	/* Save registers on stack */
	stmfd	sp!, {r4 - r12, lr}

	/*
	 * Register usage:
	 *  R0 = Base address of AT91_PMC
	 *  R1 = Base address of RAM Controller (SDRAM, DDRSDR, or AT91_SYS)
	 *  R2 = Base address of second RAM Controller or 0 if not present
	 *  R3 = Memory controller related info
	 *  R4 = temporary register
	 *  R5 = temporary register
	 */

	str	r0, .pmc_base
	str	r1, .sramc_base
	str	r2, .sramc1_base

	and	r0, r3, #AT91_MEMCTRL_MASK
	str	r0, .memtype

	lsr	r0, r3, #AT91_MEMCTRL_PID_OFFSET
	and	r0, r0, #AT91_MEMCTRL_PID_MASK
	str	r0, .ddrc_pid

	lsr	r0, r3, #AT91_MEMCTRL_SAMA5D4_OFFSET
	and	r0, r0, #AT91_MEMCTRL_SAMA5D4_MASK
	str	r0, .sama5d4_bit

	/* Drain write buffer */
	mov	tmp1, #0
	mcr	p15, 0, tmp1, c7, c10, 4

	/* Active the self-refresh mode */
	mov	r0, #SRAMC_SELF_REFRESH_ACTIVE
	bl	at91_sramc_self_refresh

	/* If the cpu isn't sama5d4, disable the ddr clock here */
	ldr	r0, .sama5d4_bit
	tst	r0, #AT91_MEMCTRL_SAMA5D4_BIT
	bne	skip_ddr_clock

	/* Disable DDR clock */
	mov	r0, #DDR_CLOCK_DISABLE
	bl	at91_ddr_clock
skip_ddr_clock:

	/* Load the the PMC base address */
	ldr	pmc, .pmc_base

	/* Save Master clock setting */
	ldr	tmp1, [pmc, #AT91_PMC_MCKR]
	str	tmp1, .saved_mckr

	/*
	 * Set the Master clock source to slow clock
	 */
	bic	tmp1, tmp1, #AT91_PMC_CSS
	str	tmp1, [pmc, #AT91_PMC_MCKR]

	wait_mckrdy

#ifdef SLOWDOWN_MASTER_CLOCK
	/*
	 * Set the Master Clock PRES and MDIV fields.
	 *
	 * See AT91RM9200 errata #27 and #28 for details.
	 */
	mov	tmp1, #0
	str	tmp1, [pmc, #AT91_PMC_MCKR]

	wait_mckrdy
#endif

	/* Save PLLA setting and disable it */
	ldr	tmp1, [pmc, #AT91_CKGR_PLLAR]
	str	tmp1, .saved_pllar

	mov	tmp1, #AT91_PMC_PLLCOUNT
	orr	tmp1, tmp1, #(1 << 29)		/* bit 29 always set */
	str	tmp1, [pmc, #AT91_CKGR_PLLAR]

	/* Turn off the main oscillator */
	ldr	tmp1, [pmc, #AT91_CKGR_MOR]
	bic	tmp1, tmp1, #AT91_PMC_MOSCEN
	orr	tmp1, tmp1, #AT91_PMC_KEY
	str	tmp1, [pmc, #AT91_CKGR_MOR]

	/* If the cpu is the sama5d4, disable the ddr clock here */
	ldr	r0, .sama5d4_bit
	tst	r0, #AT91_MEMCTRL_SAMA5D4_BIT
	beq	skip_postpone_ddr_clock

	/* Disable DDR clock */
	mov	r0, #DDR_CLOCK_DISABLE
	bl	at91_ddr_clock
skip_postpone_ddr_clock:

	/* Load the the PMC base address */
	ldr	pmc, .pmc_base

	/* Wait for interrupt */
	_do_wfi

	/* Turn on the main oscillator */
	ldr	tmp1, [pmc, #AT91_CKGR_MOR]
	orr	tmp1, tmp1, #AT91_PMC_MOSCEN
	orr	tmp1, tmp1, #AT91_PMC_KEY
	str	tmp1, [pmc, #AT91_CKGR_MOR]

	wait_moscrdy

	/* Restore PLLA setting */
	ldr	tmp1, .saved_pllar
	str	tmp1, [pmc, #AT91_CKGR_PLLAR]

	tst	tmp1, #(AT91_PMC_MUL &  0xff0000)
	bne	3f
	tst	tmp1, #(AT91_PMC_MUL & ~0xff0000)
	beq	4f
3:
	wait_pllalock
4:

#ifdef SLOWDOWN_MASTER_CLOCK
	/*
	 * First set PRES if it was not 0,
	 * than set CSS and MDIV fields.
	 *
	 * See AT91RM9200 errata #27 and #28 for details.
	 */
	ldr	tmp1, .saved_mckr
	tst	tmp1, #AT91_PMC_PRES
	beq	2f
	and	tmp1, tmp1, #AT91_PMC_PRES
	str	tmp1, [pmc, #AT91_PMC_MCKR]

	wait_mckrdy
#endif

	/*
	 * Restore master clock setting
	 */
2:	ldr	tmp1, .saved_mckr
	str	tmp1, [pmc, #AT91_PMC_MCKR]

	wait_mckrdy

	/* Enable DDR clock*/
	mov	r0, #DDR_CLOCK_ENABLE
	bl	at91_ddr_clock

	/* Exit the self-refresh mode */
	mov	r0, #SRAMC_SELF_REFRESH_EXIT
	bl	at91_sramc_self_refresh

	/* Restore registers, and return */
	ldmfd	sp!, {r4 - r12, pc}
ENDPROC(at91_slow_clock)

/*
 * void at91_sramc_self_refresh(unsigned int is_active)
 *
 * @input param:
 *	@r0: 1 - active self-refresh mode
 *	     0 - exit self-refresh mode
 * register usage:
 * 	@r1: memory type
 *	@r2: base address of the sram controller
 */

ENTRY(at91_sramc_self_refresh)
	ldr	r1, .memtype
	ldr	r2, .sramc_base

	cmp	r1, #AT91_MEMCTRL_MC
	bne	ddrc_sf

	/*
	 * at91rm9200 Memory controller
	 */

	 /*
	  * For exiting the self_refresh mode, do nothing,
	  * automatically exit self-refresh mode
	  */
	tst	r0, #SRAMC_SELF_REFRESH_ACTIVE
	beq	exit_sramc_sf

	/* Active SDRAM self-refresh mode */
	mov	r3, #1
	str	r3, [r2, #AT91RM9200_SDRAMC_SRR]
	b	exit_sramc_sf

ddrc_sf:
	cmp	r1, #AT91_MEMCTRL_DDRSDR
	bne	sdramc_sf

	/*
	 * DDR Memory controller
	 */
	tst	r0, #SRAMC_SELF_REFRESH_ACTIVE
	beq	ddrc_exit_sf

	/* LPDDR1 --> force DDR2 mode during self-refresh */
	ldr	r3, [r2, #AT91_DDRSDRC_MDR]
	str	r3, .saved_sam9_mdr
	bic	r3, r3, #~AT91_DDRSDRC_MD
	cmp	r3, #AT91_DDRSDRC_MD_LOW_POWER_DDR
	ldreq	r3, [r2, #AT91_DDRSDRC_MDR]
	biceq	r3, r3, #AT91_DDRSDRC_MD
	orreq	r3, r3, #AT91_DDRSDRC_MD_DDR2
	streq	r3, [r2, #AT91_DDRSDRC_MDR]

	/* Active DDRC self-refresh mode */
	ldr	r3, [r2, #AT91_DDRSDRC_LPR]
	str	r3, .saved_sam9_lpr
	bic	r3, r3, #AT91_DDRSDRC_LPCB
	orr	r3, r3, #AT91_DDRSDRC_LPCB_SELF_REFRESH
	str	r3, [r2, #AT91_DDRSDRC_LPR]

	/* If using the 2nd ddr controller */
	ldr	r2, .sramc1_base
	cmp	r2, #0
	beq	no_2nd_ddrc

	ldr	r3, [r2, #AT91_DDRSDRC_MDR]
	str	r3, .saved_sam9_mdr1
	bic	r3, r3, #~AT91_DDRSDRC_MD
	cmp	r3, #AT91_DDRSDRC_MD_LOW_POWER_DDR
	ldreq	r3, [r2, #AT91_DDRSDRC_MDR]
	biceq	r3, r3, #AT91_DDRSDRC_MD
	orreq	r3, r3, #AT91_DDRSDRC_MD_DDR2
	streq	r3, [r2, #AT91_DDRSDRC_MDR]

	ldr	r3, [r2, #AT91_DDRSDRC_LPR]
	str	r3, .saved_sam9_lpr1
	bic	r3, r3, #AT91_DDRSDRC_LPCB
	orr	r3, r3, #AT91_DDRSDRC_LPCB_SELF_REFRESH

	/* Active DDRC self-refresh mode */
	str	r3, [r2, #AT91_DDRSDRC_LPR]
no_2nd_ddrc:
	b	exit_sramc_sf

ddrc_exit_sf:
	/* Restore MDR in case of LPDDR1 */
	ldr	r3, .saved_sam9_mdr
	str	r3, [r2, #AT91_DDRSDRC_MDR]
	/* Restore LPR on AT91 with DDRAM */
	ldr	r3, .saved_sam9_lpr
	str	r3, [r2, #AT91_DDRSDRC_LPR]

	/* If using the 2nd ddr controller */
	ldr	r2, .sramc1_base
	cmp	r2, #0
	ldrne	r3, .saved_sam9_mdr1
	strne	r3, [r2, #AT91_DDRSDRC_MDR]
	ldrne	r3, .saved_sam9_lpr1
	strne	r3, [r2, #AT91_DDRSDRC_LPR]

	b	exit_sramc_sf

	/*
	 * SDRAMC Memory controller
	 */
sdramc_sf:
	tst	r0, #SRAMC_SELF_REFRESH_ACTIVE
	beq	sdramc_exit_sf

	/* Active SDRAMC self-refresh mode */
	ldr	r3, [r2, #AT91_SDRAMC_LPR]
	str	r3, .saved_sam9_lpr

	bic	r3, r3, #AT91_SDRAMC_LPCB
	orr	r3, r3, #AT91_SDRAMC_LPCB_SELF_REFRESH
	str	r3, [r2, #AT91_SDRAMC_LPR]

sdramc_exit_sf:
	ldr	r3, .saved_sam9_lpr
	str	r3, [r2, #AT91_SDRAMC_LPR]

exit_sramc_sf:
	mov	pc, lr
ENDPROC(at91_sramc_self_refresh)

/*
 * void at91_ddr_clock(unsigned char is_enable)
 *
 * @input param:
 *	@r0: 0x01 - enable DDR clock
 *	     0x00 - disable DDR clock
 * register usage:
 * 	@r1: ddrc peripheral id
 *	@r2: base address of the pmc
 */
ENTRY(at91_ddr_clock)
	ldr	r1, .ddrc_pid
	ldr	r2, .pmc_base

	/* DDRC peripheral clock */
	cmp	r1, #0
	beq	ddr_sys_clk

	and	r1, r1, #AT91_PMC_PCR_PID
	orr	r1, r1, #AT91_PMC_PCR_CMD

	tst	r0, #DDR_CLOCK_ENABLE
	beq	ddrc_clk
	orr	r1, r1, #AT91_PMC_PCR_EN

ddrc_clk:
	str	r1, [r2, #AT91_PMC_PCR]

ddr_sys_clk:
	/* DDR system clock */
	mov 	r3, #AT91_PMC_SYS_DDR

	tst	r0, #DDR_CLOCK_ENABLE
	beq	disable_sys_clk

	str	r3, [r2, #AT91_PMC_SCER]
	b	exit_ddr_clock

disable_sys_clk:
	str	r3, [r2, #AT91_PMC_SCDR]

exit_ddr_clock:
	mov	pc, lr
ENDPROC(at91_ddr_clock)

.pmc_base:
	.word 0
.sramc_base:
	.word 0
.sramc1_base:
	.word 0
.memtype:
	.word 0
.ddrc_pid:
	.word 0
.sama5d4_bit:
	.word 0
.saved_mckr:
	.word 0
.saved_pllar:
	.word 0
.saved_sam9_lpr:
	.word 0
.saved_sam9_lpr1:
	.word 0
.saved_sam9_mdr:
	.word 0
.saved_sam9_mdr1:
	.word 0

ENTRY(at91_slow_clock_sz)
	.word .-at91_slow_clock
