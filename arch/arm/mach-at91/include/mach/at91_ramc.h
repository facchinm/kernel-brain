/*
 * Header file for the Atmel RAM Controller
 *
 * Copyright (C) 2011 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Under GPLv2 only
 */

#ifndef __AT91_RAMC_H__
#define __AT91_RAMC_H__

#ifndef __ASSEMBLY__
extern void __iomem *at91_ramc_base[];

extern void __iomem *at91_get_ramc0_base(void);
extern void __iomem *at91_get_ramc1_base(void);

#define at91_ramc_read(id, field) \
	__raw_readl(at91_ramc_base[id] + field)

#define at91_ramc_write(id, field, value) \
	__raw_writel(value, at91_ramc_base[id] + field)
#else
.extern at91_ramc_base
#endif

#define	AT91_MEMCTRL_MASK	0x0f

#define	AT91_MEMCTRL_PID_MASK	0xff
#define	AT91_MEMCTRL_PID_OFFSET	8
#define	AT91_MEMCTRL_PID(x)	(((x) & AT91_MEMCTRL_PID_MASK) << AT91_MEMCTRL_PID_OFFSET)

#define	AT91_MEMCTRL_SAMA5D4_MASK	0x01
#define	AT91_MEMCTRL_SAMA5D4_OFFSET	24
#define	AT91_MEMCTRL_IS_SAMA5D4(x)	(((x) & AT91_MEMCTRL_SAMA5D4_MASK) << AT91_MEMCTRL_SAMA5D4_OFFSET)
#define	AT91_MEMCTRL_SAMA5D4_BIT	0x01

#define AT91_MEMCTRL_MC		0
#define AT91_MEMCTRL_SDRAMC	1
#define AT91_MEMCTRL_DDRSDR	2

#include <mach/at91rm9200_sdramc.h>
#include <mach/at91sam9_ddrsdr.h>
#include <mach/at91sam9_sdramc.h>

#endif /* __AT91_RAMC_H__ */
