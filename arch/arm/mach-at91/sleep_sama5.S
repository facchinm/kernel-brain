/*
 * Copyright (C) 2014 Atmel,
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <mach/at91_pmc.h>
#include <mach/at91sam9_ddrsdr.h>

	.arm
/*
 * void at91_cortexa5_standby(void)
 *
 * The function is to make the core perform suspend to standby mode.
 */
ENTRY(at91_cortexa5_standby)
	stmfd	sp!, {r4 - r12, lr}

	/* Save LPR register */
	ldr     r1, .ddrc_base
	ldr	r0, [r1, #AT91_DDRSDRC_LPR]
	str	r0, .saved_lpr

	/* Active DDR device enter the self-refresh mode */
	bic	r0, #AT91_DDRSDRC_LPCB
	orr	r0, #AT91_DDRSDRC_LPCB_SELF_REFRESH
	str	r0, [r1, #AT91_DDRSDRC_LPR]

	/* Disable the processor's clock */
	ldr     r1, .pmc_base
	mov	r0, #AT91_PMC_PCK
	str	r0, [r1, #AT91_PMC_SCDR]

	dsb

	wfi	@ Wait For Interrupt

	/* Restore LPR register to exit the self-refresh mode */
	ldr     r1, .ddrc_base
	ldr	r0, .saved_lpr
	str	r0, [r1, #AT91_DDRSDRC_LPR]

	ldmfd	sp!, {r4 - r12, pc}
ENDPROC(at91_cortexa5_standby)

.ddrc_base:
	.word   at91_ramc_base

.pmc_base:
	.word	at91_pmc_base

.saved_lpr:
	.word	0
