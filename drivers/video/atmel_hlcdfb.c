/*
 *  Driver for AT91/AT32 LCD Controller
 *
 *  Copyright (C) 2007 Atmel Corporation
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/platform_data/atmel.h>
#include <mach/cpu.h>
#include <mach/atmel_hlcdc.h>
#include <mach/atmel_hlcdc_ovl.h>

#include <video/atmel_lcdfb.h>

#define	ATMEL_LCDFB_FBINFO_DEFAULT	(FBINFO_DEFAULT \
					 | FBINFO_PARTIAL_PAN_OK \
					 | FBINFO_HWACCEL_YPAN)

#define ATMEL_LCDC_CVAL_DEFAULT         0xc8

/* Wait for the SIPSTS in status register until synchronization is done */
static void wait_sync_SIPSTS(struct atmel_lcdfb_info *sinfo)
{
	while (lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_SIPSTS)
		msleep(1);
}

/*
 * When access LCDC_LCDEN, LCDC_LCDDIS, and LCDC_LCDCCFG[0..6], we need to
 * use this function to wait for the synchronization of clock domain.
 */
static void lcdc_writel_wait_sync(struct atmel_lcdfb_info *sinfo,
		u32 lcdc_reg, u32 val)
{
	wait_sync_SIPSTS(sinfo);
	lcdc_writel(sinfo, lcdc_reg, val);
}

struct atmel_hlcd_dma_desc {
	u32	address;
	u32	control;
	u32	next;
};

static void atmel_hlcdfb_update_dma_base(struct fb_info *info,

			       struct fb_var_screeninfo *var)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	struct fb_fix_screeninfo *fix = &info->fix;
	unsigned long dma_addr;
	struct atmel_hlcd_dma_desc *desc;

	dma_addr = (fix->smem_start + var->yoffset * fix->line_length
		    + var->xoffset * var->bits_per_pixel / 8);

	dma_addr &= ~3UL;

	/* Setup the DMA descriptor, this descriptor will loop to itself */
	desc = sinfo->dma_desc;

	desc->address = dma_addr;
	/* Disable DMA transfer interrupt & descriptor loaded interrupt. */
	desc->control = LCDC_BASECTRL_ADDIEN | LCDC_BASECTRL_DSCRIEN
			| LCDC_BASECTRL_DMAIEN | LCDC_BASECTRL_DFETCH;
	desc->next = sinfo->dma_desc_phys;

	lcdc_writel(sinfo, ATMEL_LCDC_BASEADDR, dma_addr);
	lcdc_writel(sinfo, ATMEL_LCDC_BASECTRL, desc->control);
	lcdc_writel(sinfo, ATMEL_LCDC_BASENEXT, sinfo->dma_desc_phys);
	lcdc_writel(sinfo, ATMEL_LCDC_BASECHER, LCDC_BASECHER_CHEN | LCDC_BASECHER_UPDATEEN);
}

static void atmel_hlcdfb_update_dma_ovl(struct fb_info *info,
			       struct fb_var_screeninfo *var)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	struct fb_fix_screeninfo *fix = &info->fix;
	unsigned long dma_addr;
	struct atmel_hlcd_dma_desc *desc;

	dma_addr = (fix->smem_start + var->yoffset * fix->line_length
		    + var->xoffset * var->bits_per_pixel / 8);

	dma_addr &= ~3UL;

	/* Setup the DMA descriptor, this descriptor will loop to itself */
	desc = sinfo->dma_desc;

	desc->address = dma_addr;
	/* Disable DMA transfer interrupt & descriptor loaded interrupt. */
	desc->control = LCDC_OVRCTRL_ADDIEN | LCDC_OVRCTRL_DSCRIEN
			| LCDC_OVRCTRL_DMAIEN | LCDC_OVRCTRL_DFETCH;
	desc->next = sinfo->dma_desc_phys;

	lcdc_writel(sinfo, ATMEL_LCDC_OVRADDR, dma_addr);
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCTRL, desc->control);
	lcdc_writel(sinfo, ATMEL_LCDC_OVRNEXT, sinfo->dma_desc_phys);
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCHER, LCDC_OVRCHER_CHEN | LCDC_OVRCHER_UPDATEEN);
}

#if defined(CONFIG_BACKLIGHT_ATMEL_LCDC)
/* some bl->props field just changed */
static int atmel_bl_update_status(struct backlight_device *bl)
{
	struct atmel_lcdfb_info *sinfo = bl_get_data(bl);
	int power = sinfo->bl_power;
	int brightness = bl->props.brightness;
	u32 reg;

	/* REVISIT there may be a meaningful difference between
	 * fb_blank and power ... there seem to be some cases
	 * this doesn't handle correctly.
	 */
	if (bl->props.fb_blank != sinfo->bl_power)
		power = bl->props.fb_blank;
	else if (bl->props.power != sinfo->bl_power)
		power = bl->props.power;

	if (brightness < 0 && power == FB_BLANK_UNBLANK)
		brightness = lcdc_readl(sinfo, ATMEL_LCDC_LCDCFG6)
			     >> LCDC_LCDCFG6_PWMCVAL_OFFSET;
	else if (power != FB_BLANK_UNBLANK)
		brightness = 0;

	reg = lcdc_readl(sinfo, ATMEL_LCDC_LCDCFG6) & ~LCDC_LCDCFG6_PWMCVAL;
	reg |= brightness << LCDC_LCDCFG6_PWMCVAL_OFFSET;
	lcdc_writel(sinfo, ATMEL_LCDC_LCDCFG6, reg);

	bl->props.fb_blank = bl->props.power = sinfo->bl_power = power;

	return 0;
}

static int atmel_bl_get_brightness(struct backlight_device *bl)
{
	struct atmel_lcdfb_info *sinfo = bl_get_data(bl);

	return lcdc_readl(sinfo, ATMEL_LCDC_LCDCFG6) >> LCDC_LCDCFG6_PWMCVAL_OFFSET;
}

static void atmel_hlcdfb_init_contrast(struct atmel_lcdfb_info *sinfo)
{
	/* have some default contrast/backlight settings */
	lcdc_writel(sinfo, ATMEL_LCDC_LCDCFG6, LCDC_LCDCFG6_PWMPOL |
		(ATMEL_LCDC_CVAL_DEFAULT << LCDC_LCDCFG6_PWMCVAL_OFFSET) |
		LCDC_LCDCFG6_PWMPS_DIV_2);
}
#else
static int atmel_bl_update_status(struct backlight_device *bl)
{
	return 0;
}

static int atmel_bl_get_brightness(struct backlight_device *bl)
{
	return ATMEL_LCDC_CVAL_DEFAULT;
}

static void atmel_hlcdfb_init_contrast(struct atmel_lcdfb_info *sinfo) {}
#endif

static const struct backlight_ops atmel_hlcdc_bl_ops = {
	.update_status = atmel_bl_update_status,
	.get_brightness = atmel_bl_get_brightness,
};

void atmel_hlcdfb_start(struct atmel_lcdfb_info *sinfo)
{
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDEN, LCDC_LCDEN_CLKEN);
	while (!(lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_CLKSTS))
		msleep(1);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDEN, LCDC_LCDEN_SYNCEN);
	while (!(lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_LCDSTS))
		msleep(1);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDEN, LCDC_LCDEN_DISPEN);
	while (!(lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_DISPSTS))
		msleep(1);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDEN, LCDC_LCDEN_PWMEN);
	while (!(lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_PWMSTS))
		msleep(1);
}

static void atmel_hlcdfb_stop(struct atmel_lcdfb_info *sinfo, u32 flags)
{
	/* Disable DISP signal */
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDDIS, LCDC_LCDDIS_DISPDIS);
	while ((lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_DISPSTS))
		msleep(1);
	/* Disable synchronization */
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDDIS, LCDC_LCDDIS_SYNCDIS);
	while ((lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_LCDSTS))
		msleep(1);
	/* Disable pixel clock */
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDDIS, LCDC_LCDDIS_CLKDIS);
	while ((lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_CLKSTS))
		msleep(1);
	/* Disable PWM */
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDDIS, LCDC_LCDDIS_PWMDIS);
	while ((lcdc_readl(sinfo, ATMEL_LCDC_LCDSR) & LCDC_LCDSR_PWMSTS))
		msleep(1);

	if (!(flags & ATMEL_LCDC_STOP_NOWAIT))
		/* Wait for the end of DMA transfer */
		while (!(lcdc_readl(sinfo, ATMEL_LCDC_BASEISR) & LCDC_BASEISR_DMA))
			msleep(10);
		//FIXME: OVL DMA?
}

static u32 atmel_hlcdfb_get_rgbmode(struct fb_info *info)
{
	u32 value = 0;

	switch (info->var.bits_per_pixel) {
	case 1:
		value = LCDC_BASECFG1_CLUTMODE_1BPP | LCDC_BASECFG1_CLUTEN;
		break;
	case 2:
		value = LCDC_BASECFG1_CLUTMODE_2BPP | LCDC_BASECFG1_CLUTEN;
		break;
	case 4:
		value = LCDC_BASECFG1_CLUTMODE_4BPP | LCDC_BASECFG1_CLUTEN;
		break;
	case 8:
		value = LCDC_BASECFG1_CLUTMODE_8BPP | LCDC_BASECFG1_CLUTEN;
		break;
	case 12:
		value = LCDC_BASECFG1_RGBMODE_12BPP_RGB_444;
		break;
	case 16:
		if (info->var.transp.offset)
			value = LCDC_BASECFG1_RGBMODE_16BPP_ARGB_4444;
		else
			value = LCDC_BASECFG1_RGBMODE_16BPP_RGB_565;
		break;
	case 18:
		value = LCDC_BASECFG1_RGBMODE_18BPP_RGB_666_PACKED;
		break;
	case 24:
		value = LCDC_BASECFG1_RGBMODE_24BPP_RGB_888_PACKED;
		break;
	case 32:
		value = LCDC_BASECFG1_RGBMODE_32BPP_ARGB_8888;
		break;
	default:
		dev_err(info->device, "Cannot set video mode for %dbpp\n",
			info->var.bits_per_pixel);
		break;
	}

	return value;
}

static int atmel_hlcdfb_setup_core_base(struct fb_info *info)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	unsigned long value;
	unsigned long clk_value_khz;
	unsigned long required_pixclk, updated_pixclk;

	dev_dbg(info->device, "%s:\n", __func__);
	atmel_hlcdfb_stop(sinfo, ATMEL_LCDC_STOP_NOWAIT);

	/* Set pixel clock */
	clk_value_khz = clk_get_rate(sinfo->lcdc_clk) / 1000;

	value = DIV_ROUND_CLOSEST(clk_value_khz, PICOS2KHZ(info->var.pixclock));
	required_pixclk = info->var.pixclock;

	if (value < 1) {
		dev_notice(info->device, "using system clock as pixel clock\n");
		value = LCDC_LCDCFG0_CLKPWMSEL | LCDC_LCDCFG0_CGDISBASE;
		lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG0, value);
	} else {
		info->var.pixclock = KHZ2PICOS(clk_value_khz / value);
		dev_dbg(info->device, "  updated pixclk:     %lu KHz\n",
				PICOS2KHZ(info->var.pixclock));
		value = value - 2;
		dev_dbg(info->device, "  * programming CLKDIV = 0x%08lx\n",
					value);
		value = (value << LCDC_LCDCFG0_CLKDIV_OFFSET)
			| LCDC_LCDCFG0_CGDISBASE;
		lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG0, value);
	}

	updated_pixclk = info->var.pixclock;

	if (required_pixclk < updated_pixclk * 95 / 100
	    || required_pixclk > updated_pixclk * 105 / 100)
		dev_warn(info->device, "WARNING: required pixclk is: %lu KHz, real get clock: %lu KHz\n",
			 PICOS2KHZ(required_pixclk), PICOS2KHZ(updated_pixclk));

	/* Initialize control register 5 */
	/* In 9x5, the default_lcdcon2 will use for LCDCFG5 */
	value = sinfo->default_lcdcon2;
	value |= sinfo->guard_time << LCDC_LCDCFG5_GUARDTIME_OFFSET;

	if (!(info->var.sync & FB_SYNC_HOR_HIGH_ACT))
		value |= LCDC_LCDCFG5_HSPOL;
	if (!(info->var.sync & FB_SYNC_VERT_HIGH_ACT))
		value |= LCDC_LCDCFG5_VSPOL;

	if (info->var.sync & LCDC_LCDCFG5_VSPDLYS)
		value |= LCDC_LCDCFG5_VSPDLYS;
	if (info->var.sync & LCDC_LCDCFG5_VSPDLYE)
		value |= LCDC_LCDCFG5_VSPDLYE;
	if (info->var.sync & LCDC_LCDCFG5_DISPPOL)
		value |= LCDC_LCDCFG5_DISPPOL;

	if (info->var.sync & LCDC_LCDCFG5_DITHER)
		value |= LCDC_LCDCFG5_DITHER;

	if (info->var.sync & LCDC_LCDCFG5_DISPDLY)
		value |= LCDC_LCDCFG5_DISPDLY;

	if (info->var.sync & LCDC_LCDCFG5_VSPSU)
		value |= LCDC_LCDCFG5_VSPSU;

	if (info->var.sync & LCDC_LCDCFG5_VSPHO)
		value |= LCDC_LCDCFG5_VSPHO;

	dev_dbg(info->device, "  * LCDC_LCDCFG5 = %08lx\n", value);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG5, value);

	/* Vertical & Horizontal Timing */
	value = (info->var.vsync_len - 1) << LCDC_LCDCFG1_VSPW_OFFSET;
	value |= (info->var.hsync_len - 1) << LCDC_LCDCFG1_HSPW_OFFSET;
	dev_dbg(info->device, "  * LCDC_LCDCFG1 = %08lx\n", value);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG1, value);

	value = (info->var.upper_margin) << LCDC_LCDCFG2_VBPW_OFFSET;
	value |= (info->var.lower_margin - 1) << LCDC_LCDCFG2_VFPW_OFFSET;
	dev_dbg(info->device, "  * LCDC_LCDCFG2 = %08lx\n", value);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG2, value);

	value = (info->var.left_margin - 1) << LCDC_LCDCFG3_HBPW_OFFSET;
	value |= (info->var.right_margin - 1) << LCDC_LCDCFG3_HFPW_OFFSET;
	dev_dbg(info->device, "  * LCDC_LCDCFG3 = %08lx\n", value);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG3, value);

	/* Display size */
	value = (info->var.yres - 1) << LCDC_LCDCFG4_RPF_OFFSET;
	value |= (info->var.xres - 1) << LCDC_LCDCFG4_PPL_OFFSET;
	dev_dbg(info->device, "  * LCDC_LCDCFG4 = %08lx\n", value);
	lcdc_writel_wait_sync(sinfo, ATMEL_LCDC_LCDCFG4, value);

	lcdc_writel(sinfo, ATMEL_LCDC_BASECFG0, LCDC_BASECFG0_BLEN_AHB_INCR16 | LCDC_BASECFG0_DLBO);
	lcdc_writel(sinfo, ATMEL_LCDC_BASECFG1, atmel_hlcdfb_get_rgbmode(info));
	lcdc_writel(sinfo, ATMEL_LCDC_BASECFG2, 0);
	lcdc_writel(sinfo, ATMEL_LCDC_BASECFG3, 0);	/* Default color */
	lcdc_writel(sinfo, ATMEL_LCDC_BASECFG4, LCDC_BASECFG4_DMA);

	/* Disable all interrupts */
	lcdc_writel(sinfo, ATMEL_LCDC_LCDIDR, ~0UL);
	lcdc_writel(sinfo, ATMEL_LCDC_BASEIDR, ~0UL);
	/* Enable BASE LAYER overflow interrupts, if want to enable DMA interrupt, also need set it at LCDC_BASECTRL reg */
	lcdc_writel(sinfo, ATMEL_LCDC_BASEIER, LCDC_BASEIER_OVR);
	//FIXME: Let video-driver register a callback
	lcdc_writel(sinfo, ATMEL_LCDC_LCDIER, LCDC_LCDIER_FIFOERRIE |
				LCDC_LCDIER_BASEIE | LCDC_LCDIER_HEOIE);

	return 0;
}

static int atmel_hlcdfb_setup_core_ovl(struct fb_info *info)
{
	struct atmel_lcdfb_info *sinfo = info->par;
	u32 xpos, ypos, xres, yres, cfg9;

	if (info->var.nonstd >> 31) {
		xpos = (info->var.nonstd >> 10) & 0x3ff;
		ypos = info->var.nonstd & 0x3ff;
		xres = info->var.xres ? info->var.xres - 1 : 0;
		yres = info->var.yres ? info->var.yres - 1 : 0;
		cfg9 =  LCDC_OVRCFG9_DMA | LCDC_OVRCFG9_OVR |
			LCDC_OVRCFG9_ITER | LCDC_OVRCFG9_ITER2BL |
			LCDC_OVRCFG9_REP;
		if (info->var.transp.offset)
			cfg9 |= LCDC_OVRCFG9_LAEN;
		else
			cfg9 |= LCDC_OVRCFG9_GAEN | LCDC_OVRCFG9_GA;
	} else {
		xpos = ypos = yres = xres = cfg9 = 0;
	}

	lcdc_writel(sinfo, ATMEL_LCDC_OVRCFG0,
			LCDC_OVRCFG0_BLEN_AHB_INCR16 | LCDC_OVRCFG0_DLBO);
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCFG1,
			atmel_hlcdfb_get_rgbmode(info));
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCFG2, xpos |
			(ypos << LCDC_OVRCFG2_YOFFSET_OFFSET));
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCFG3, xres |
			(yres << LCDC_OVRCFG3_YSIZE_OFFSET));
	lcdc_writel(sinfo, ATMEL_LCDC_OVRCFG9, cfg9);

	return 0;
}
static void atmelfb_limit_screeninfo(struct fb_var_screeninfo *var)
{
	u32 hbpw, hfpw;

	if (cpu_is_at91sam9x5()) {
		hbpw = LCDC_LCDCFG3_HBPW;
		hfpw = LCDC_LCDCFG3_HFPW;
	} else {
		hbpw = LCDC2_LCDCFG3_HBPW;
		hfpw = LCDC2_LCDCFG3_HFPW;
	}

	/* Saturate vertical and horizontal timings at maximum values */
	var->vsync_len = min_t(u32, var->vsync_len,
			(LCDC_LCDCFG1_VSPW >> LCDC_LCDCFG1_VSPW_OFFSET) + 1);
	var->upper_margin = min_t(u32, var->upper_margin,
			(LCDC_LCDCFG2_VFPW >> LCDC_LCDCFG2_VFPW_OFFSET) + 1);
	var->lower_margin = min_t(u32, var->lower_margin,
			LCDC_LCDCFG2_VBPW >> LCDC_LCDCFG2_VBPW_OFFSET);
	var->right_margin = min_t(u32, var->right_margin,
			(hbpw >> LCDC_LCDCFG3_HBPW_OFFSET) + 1);
	var->hsync_len = min_t(u32, var->hsync_len,
			(LCDC_LCDCFG1_HSPW >> LCDC_LCDCFG1_HSPW_OFFSET) + 1);
	var->left_margin = min_t(u32, var->left_margin,
			(hfpw >> LCDC_LCDCFG3_HFPW_OFFSET) + 1);

}

static irqreturn_t atmel_hlcdfb_interrupt(int irq, void *dev_id)
{
	struct fb_info *info = dev_id;
	struct atmel_lcdfb_info *sinfo = info->par;
	u32 status, baselayer_status;

	/* Check for error status via interrupt.*/
	status = lcdc_readl(sinfo, ATMEL_LCDC_LCDISR);
	if (status & LCDC_LCDISR_HEO)
		return IRQ_NONE;

	if (status & LCDC_LCDISR_FIFOERR)
		dev_warn(info->device, "FIFO underflow %#x\n", status);

	if (status & LCDC_LCDISR_BASE) {
		/* Check base layer's overflow error. */
		baselayer_status = lcdc_readl(sinfo, ATMEL_LCDC_BASEISR);

		if (baselayer_status & LCDC_BASEISR_OVR)
			dev_warn(info->device, "base layer overflow %#x\n",
						baselayer_status);
	}

	return IRQ_HANDLED;
}


#ifdef CONFIG_PM

static int atmel_hlcdfb_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);
	struct fb_info *info = platform_get_drvdata(pdev);
	struct atmel_lcdfb_info *sinfo = info->par;

	if (strcmp(id->name, "atmel_hlcdfb_base"))
		return 0;

	/*
	 * We don't want to handle interrupts while the clock is
	 * stopped. It may take forever.
	 */
	lcdc_writel(sinfo, ATMEL_LCDC_LCDIDR, ~0UL);
	lcdc_writel(sinfo, ATMEL_LCDC_BASEIDR, ~0UL);

	if (sinfo->atmel_lcdfb_power_control)
		sinfo->atmel_lcdfb_power_control(0);

	atmel_hlcdfb_stop(sinfo, ATMEL_LCDC_STOP_NOWAIT);
	atmel_lcdfb_stop_clock(sinfo);

	return 0;
}

static int atmel_hlcdfb_resume(struct platform_device *pdev)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);
	struct fb_info *info = platform_get_drvdata(pdev);
	struct atmel_lcdfb_info *sinfo = info->par;

	if (strcmp(id->name, "atmel_hlcdfb_base"))
		return 0;

	atmel_lcdfb_start_clock(sinfo);
	atmel_hlcdfb_start(sinfo);
	if (sinfo->atmel_lcdfb_power_control)
		sinfo->atmel_lcdfb_power_control(1);

	/* Enable fifo error & BASE LAYER overflow interrupts */
	lcdc_writel(sinfo, ATMEL_LCDC_BASEIER, LCDC_BASEIER_OVR);
	lcdc_writel(sinfo, ATMEL_LCDC_LCDIER, LCDC_LCDIER_FIFOERRIE |
				LCDC_LCDIER_BASEIE | LCDC_LCDIER_HEOIE);

	return 0;
}

#else
#define atmel_hlcdfb_suspend	NULL
#define atmel_hlcdfb_resume	NULL
#endif

static struct atmel_lcdfb_devdata dev_data_base = {
	.setup_core = atmel_hlcdfb_setup_core_base,
	.start = atmel_hlcdfb_start,
	.stop = atmel_hlcdfb_stop,
	.isr = atmel_hlcdfb_interrupt,
	.update_dma = atmel_hlcdfb_update_dma_base,
	.bl_ops = &atmel_hlcdc_bl_ops,
	.init_contrast = atmel_hlcdfb_init_contrast,
	.limit_screeninfo = atmelfb_limit_screeninfo,
	.fbinfo_flags = ATMEL_LCDFB_FBINFO_DEFAULT,
	.dma_desc_size = sizeof(struct atmel_hlcd_dma_desc),
};

static struct atmel_lcdfb_devdata dev_data_ovl = {
	.setup_core = atmel_hlcdfb_setup_core_ovl,
	.update_dma = atmel_hlcdfb_update_dma_ovl,
	.limit_screeninfo = atmelfb_limit_screeninfo,
	.fbinfo_flags = ATMEL_LCDFB_FBINFO_DEFAULT,
	.dma_desc_size = sizeof(struct atmel_hlcd_dma_desc),
};

static const struct platform_device_id atmelfb_dev_table[] = {
	{ "atmel_hlcdfb_base", (kernel_ulong_t)&dev_data_base },
	{ "atmel_hlcdfb_ovl1", (kernel_ulong_t)&dev_data_ovl },
	{ "atmel_hlcdfb_ovl2", (kernel_ulong_t)&dev_data_ovl },
}
MODULE_DEVICE_TABLE(platform, atmelfb_dev_table);

static int __init atmel_hlcdfb_probe(struct platform_device *pdev)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);

	return __atmel_lcdfb_probe(pdev, (struct atmel_lcdfb_devdata *)id->driver_data);
}
static int __exit atmel_hlcdfb_remove(struct platform_device *pdev)
{
	return __atmel_lcdfb_remove(pdev);
}

static struct platform_driver atmel_hlcdfb_driver = {
	.remove		= __exit_p(atmel_hlcdfb_remove),
	.suspend	= atmel_hlcdfb_suspend,
	.resume		= atmel_hlcdfb_resume,

	.driver		= {
		.name	= "atmel_hlcdfb",
		.owner	= THIS_MODULE,
	},
	.id_table	= atmelfb_dev_table,
};

static int __init atmel_hlcdfb_init(void)
{
	return platform_driver_probe(&atmel_hlcdfb_driver, atmel_hlcdfb_probe);
}
module_init(atmel_hlcdfb_init);

static void __exit atmel_hlcdfb_exit(void)
{
	platform_driver_unregister(&atmel_hlcdfb_driver);
}
module_exit(atmel_hlcdfb_exit);

MODULE_DESCRIPTION("AT91 HLCD Controller framebuffer driver");
MODULE_AUTHOR("Nicolas Ferre <nicolas.ferre@atmel.com> "
		"and Wolfram Sang <w.sang@pengutronix.de");
MODULE_LICENSE("GPL");
