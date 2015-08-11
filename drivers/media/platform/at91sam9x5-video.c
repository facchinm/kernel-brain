/*
 * Copyright (C) 2011 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */

/*
 * XXX:
 * - handle setting of global alpha
 * - handle more formats
 * - complete this list :-)
 */

#include <linux/err.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#define debug(fmt, ...)

#define DRIVER_NAME "at91sam9x5-video"

#define REG_HEOCHER		0x00
#define REG_HEOCHER_CHEN		0x00000001
#define REG_HEOCHER_UPDATEEN		0x00000002
#define REG_HEOCHER_A2QEN		0x00000004

#define REG_HEOCHDR		0x04
#define REG_HEOCHDR_CHDIS		0x00000001
#define REG_HEOCHDR_CHRST		0x00000100

#define REG_HEOCHSR		0x08
#define REG_HEOCHSR_CHSR		0x00000001
#define REG_HEOCHSR_UPDATESR		0x00000002
#define REG_HEOCHSR_A2QSR		0x00000004

#define REG_HEOIER		0x0c
#define REG_HEOIDR		0x10
#define REG_HEOIMR		0x14
#define REG_HEOISR		0x18
#define REG_HEOIxR_DMA			0x00000004
#define REG_HEOIxR_DSCR			0x00000008
#define REG_HEOIxR_ADD			0x00000010
#define REG_HEOIxR_DONE			0x00000020
#define REG_HEOIxR_OVR			0x00000040
#define REG_HEOIxR_UDMA			0x00000400
#define REG_HEOIxR_UDSCR		0x00000800
#define REG_HEOIxR_UADD			0x00001000
#define REG_HEOIxR_UDONE		0x00002000
#define REG_HEOIxR_UOVR			0x00004000
#define REG_HEOIxR_VDMA			0x00040000
#define REG_HEOIxR_VDSCR		0x00080000
#define REG_HEOIxR_VADD			0x00100000
#define REG_HEOIxR_VDONE		0x00200000
#define REG_HEOIxR_VOVR			0x00400000

#define REG_HEOHEAD		0x1c
#define REG_HEOUHEAD		0x2c
#define REG_HEOVHEAD		0x3c

#define REG_HEOADDR		0x20
#define REG_HEOUADDR		0x30
#define REG_HEOVADDR		0x40

#define REG_HEOCTRL		0x24
#define REG_HEOUCTRL		0x34
#define REG_HEOVCTRL		0x44
#define REG_HEOxCTRL_DFETCH		0x00000001
#define REG_HEOCTRL_LFETCH		0x00000002
#define REG_HEOxCTRL_DMAIEN		0x00000004
#define REG_HEOxCTRL_DSCRIEN		0x00000008
#define REG_HEOxCTRL_ADDIEN		0x00000010
#define REG_HEOxCTRL_DONEIEN		0x00000020

#define REG_HEONEXT		0x28
#define REG_HEOUNEXT		0x38
#define REG_HEOVNEXT		0x48

#define REG_HEOCFG0		0x4c
#define REG_HEOCFG0_DLBO		0x00000100
#define REG_HEOCFG0_BLEN		0x00000030
#define REG_HEOCFG0_BLEN_INCR1			0x00000000
#define REG_HEOCFG0_BLEN_INCR4			0x00000010
#define REG_HEOCFG0_BLEN_INCR8			0x00000020
#define REG_HEOCFG0_BLEN_INCR16			0x00000030
#define REG_HEOCFG0_BLENUV		0x000000c0
#define REG_HEOCFG0_BLENUV_INCR1		0x00000000
#define REG_HEOCFG0_BLENUV_INCR4		0x00000040
#define REG_HEOCFG0_BLENUV_INCR8		0x00000080
#define REG_HEOCFG0_BLENUV_INCR16		0x000000c0

#define REG_HEOCFG1		0x50
#define REG_HEOCFG1_CLUTEN		0x00000001
#define REG_HEOCFG1_YUVEN		0x00000002
#define REG_HEOCFG1_YUVMODE_12YCBCRP	0x00008000
#define REG_HEOCFG1_YUVMODE_16YCBCR_0	0x00001000
#define REG_HEOCFG1_YUVMODE_16YCBCR_1	0x00002000

#define REG_HEOCFG2		0x54
#define REG_HEOCFG2_XPOS		0x000007ff
#define REG_HEOCFG2_YPOS		0x07ff0000

#define REG_HEOCFG3		0x58
#define REG_HEOCFG3_XSIZE		0x000007ff
#define REG_HEOCFG3_YSIZE		0x07ff0000

#define REG_HEOCFG4		0x5c
#define REG_HEOCFG4_XMEMSIZE		0x000007ff
#define REG_HEOCFG4_YMEMSIZE		0x07ff0000

#define REG_HEOCFG5		0x60
#define REG_HEOCFG5_XSTRIDE		0xffffffff

#define REG_HEOCFG6		0x64
#define REG_HEOCFG6_PSTRIDE		0xffffffff

#define REG_HEOCFG7		0x68
#define REG_HEOCFG7_UVXSTRIDE		0xffffffff

#define REG_HEOCFG8		0x6c
#define REG_HEOCFG8_UVPSTRIDE		0xffffffff

#define REG_HEOCFG9		0x70
#define REG_HEOCFG10		0x74
#define REG_HEOCFG11		0x78

#define REG_HEOCFG12		0x7c
#define REG_HEOCFG12_CRKEY		0x00000001
#define REG_HEOCFG12_INV		0x00000002
#define REG_HEOCFG12_ITER2BL		0x00000004
#define REG_HEOCFG12_ITER		0x00000008
#define REG_HEOCFG12_REVALPHA		0x00000010
#define REG_HEOCFG12_GAEN		0x00000020
#define REG_HEOCFG12_LAEN		0x00000040
#define REG_HEOCFG12_OVR		0x00000080
#define REG_HEOCFG12_DMA		0x00000100
#define REG_HEOCFG12_REP		0x00000200
#define REG_HEOCFG12_DSTKEY		0x00000400
#define REG_HEOCFG12_VIDPRI		0x00001000
#define REG_HEOCFG12_GA			0x00ff0000

#define REG_HEOCFG13		0x80
#define REG_HEOCFG13_XFACTOR		0x00001fff
#define REG_HEOCFG13_YFACTOR		0x1fff0000
#define REG_HEOCFG13_SCALEN		0x80000000

#define REG_HEOCFG14		0x84
#define REG_HEOCFG15		0x88
#define REG_HEOCFG16		0x8c
#define REG_HEO_COEF_BASE	0x90
#define REG_HEO_COEF_END	0xEC
#define REG_HEOCFG41		0xF0
#define REG_HEOCFG41_XPHIDEF		0x00000007
#define REG_HEOCFG41_YPHIDEF		0x00070000

#define HEOCFG41_XPHIDEF_DEFAULT	4
#define HEOCFG41_YPHIDEF_DEFAULT	4

#define valtomask(val, mask)	(((val) << __ffs((mask))) & (mask))
#define valfrommask(val, mask)	(((val) & (mask)) >> __ffs((mask)))

u32 heo_downscaling_coef[] = {
	0x11343311,
	0x000000f7,
	0x1635300c,
	0x000000f9,
	0x1b362c08,
	0x000000fb,
	0x1f372804,
	0x000000fe,
	0x24382400,
	0x00000000,
	0x28371ffe,
	0x00000004,
	0x2c361bfb,
	0x00000008,
	0x303516f9,
	0x0000000c,
	0x00123737,
	0x00173732,
	0x001b382d,
	0x001f3928,
	0x00243824,
	0x0028391f,
	0x002d381b,
	0x00323717,
};

u32 heo_upscaling_coef[] = {
	0xf74949f7,
	0x00000000,
	0xf55f33fb,
	0x000000fe,
	0xf5701efe,
	0x000000ff,
	0xf87c0dff,
	0x00000000,
	0x00800000,
	0x00000000,
	0x0d7cf800,
	0x000000ff,
	0x1e70f5ff,
	0x000000fe,
	0x335ff5fe,
	0x000000fb,
	0x00004040,
	0x00075920,
	0x00056f0c,
	0x00027b03,
	0x00008000,
	0x00037b02,
	0x000c6f05,
	0x00205907,
};

struct atmel_vout_fmt {
	u32		pfmt;
	char		*desc;
	unsigned char	bpp;
};

/* Further pixel formats can be added */
static struct atmel_vout_fmt vout_fmt[] = {
	{
		.pfmt	= V4L2_PIX_FMT_YUV420,
		.bpp	= 12,
		.desc	= "YVU420 planar",
	},
	{
		.pfmt	= V4L2_PIX_FMT_YUYV,
		.bpp	= 16,
		.desc	= "YVYU 422",
	},
	{
		.pfmt	= V4L2_PIX_FMT_UYVY,
		.bpp	= 16,
		.desc	= "YVYU 422",
	}
};


struct at91sam9x5_video_pdata {
	u16 base_width;
	u16 base_height;
};

struct at91sam9x5_video_bufinfo {
	struct vb2_buffer *vb;
	unsigned u_planeno, v_planeno;
	unsigned long plane_size[3];
};

struct at91sam9x5_video_priv {
	struct platform_device *pdev;

	/* framebuffer stuff */
	struct notifier_block fb_notifier;
	struct fb_info *fbinfo;

	struct video_device *video_dev;

	void __iomem *regbase;
	unsigned int irq;

	struct vb2_queue queue;
	void *alloc_ctx;

	struct at91sam9x5_video_bufinfo cur, next;

	/* protects the members after lock and hardware access */
	spinlock_t lock;

	enum {
		/* DMA not running */
		at91sam9x5_video_HW_IDLE,
		/* DMA running, unless cfgstate is BAD */
		at91sam9x5_video_HW_RUNNING,
	} hwstate;

	enum {
		at91sam9x5_video_CFG_GOOD,
		/* the shadow registers need an update */
		at91sam9x5_video_CFG_GOOD_LATCH,
		at91sam9x5_video_CFG_BAD,
	} cfgstate;

	/* if true the vid_out config in hardware doesn't match sw config */
	int cfgupdate;

	int valid_config;

	struct v4l2_pix_format fmt_vid_out_cur, fmt_vid_out_next;

	int rotation;

	struct v4l2_window fmt_vid_overlay;

	/*
	 * For YUV formats Y data is always in plane 0. U, V are either both in
	 * 0, both in 1, or U in 1 or V in 2. -1 for formats that don't use U
	 * and V.
	 */
	int u_planeno, v_planeno;

	unsigned long plane_size[3];

	/*
	 * These are the offsets into the buffers to start the hardware for.
	 * Depending on rotation and overlay position this is more or less ugly
	 * to calculate. (y_offset is used for rgb data, too.)
	 */
	u32 y_offset, u_offset, v_offset;

	u32 irqstat;
};

static u32 at91sam9x5_video_read32(struct at91sam9x5_video_priv *priv,
		size_t offset)
{
	/* XXX: really use the __raw variants? */
	return __raw_readl(priv->regbase + offset);
}

static void at91sam9x5_video_write32(struct at91sam9x5_video_priv *priv,
		size_t offset, u32 val)
{
	debug("$%x := %08x, $08 == %08x\n", offset, val,
			at91sam9x5_video_read32(priv, REG_HEOCHSR));
	__raw_writel(val, priv->regbase + offset);
	debug("$08 == %08x\n", at91sam9x5_video_read32(priv, REG_HEOCHSR));
}

static int __at91sam9x5_video_buf_in_use(struct at91sam9x5_video_priv *priv,
		struct at91sam9x5_video_bufinfo *bi,
		size_t heoaddr_offset, unsigned planeno)
{
	if (planeno >= 0) {
		u32 heoaddr = at91sam9x5_video_read32(priv, heoaddr_offset);
		dma_addr_t plane_paddr =
			vb2_dma_contig_plane_dma_addr(bi->vb, planeno);

		if (heoaddr - plane_paddr <= bi->plane_size[planeno])
			return 1;
	}

	return 0;
}


static int at91sam9x5_video_buf_in_use(struct at91sam9x5_video_priv *priv,
		struct at91sam9x5_video_bufinfo *bi)
{
	if (__at91sam9x5_video_buf_in_use(priv, bi, REG_HEOADDR, 0))
		return 1;
	if (__at91sam9x5_video_buf_in_use(priv, bi,
				REG_HEOUADDR, bi->u_planeno))
		return 1;
	if (__at91sam9x5_video_buf_in_use(priv, bi,
				REG_HEOVADDR, bi->v_planeno))
		return 1;

	return 0;
}

static u32 at91sam9x5_video_handle_irqstat(struct at91sam9x5_video_priv *priv)
{
	u32 heoisr = at91sam9x5_video_read32(priv, REG_HEOISR);

	debug("cur=%p, next=%p, heoisr=%08x\n", priv->cur.vb,
			priv->next.vb, heoisr);
	debug("cfgupdate=%d hwstate=%d cfgstate=%d\n",
			priv->cfgupdate, priv->hwstate, priv->cfgstate);

	if (!priv->cur.vb) {
		priv->cur = priv->next;
		priv->next.vb = NULL;
	}

	if (priv->hwstate == at91sam9x5_video_HW_IDLE &&
			!(at91sam9x5_video_read32(priv, REG_HEOCHSR) &
				REG_HEOCHSR_CHSR)) {
		if (priv->cur.vb) {
			vb2_buffer_done(priv->cur.vb, VB2_BUF_STATE_DONE);
			priv->cur.vb = NULL;
		}

		if (priv->next.vb) {
			vb2_buffer_done(priv->next.vb, VB2_BUF_STATE_DONE);
			priv->next.vb = NULL;
		}

		at91sam9x5_video_write32(priv, REG_HEOIDR,
				REG_HEOIxR_ADD | REG_HEOIxR_DMA |
				REG_HEOIxR_UADD | REG_HEOIxR_UDMA |
				REG_HEOIxR_VADD | REG_HEOIxR_VDMA);

	} else if (priv->cur.vb && priv->next.vb) {
		int hwrunning = 1;
		if (priv->cfgstate == at91sam9x5_video_CFG_BAD &&
				!(at91sam9x5_video_read32(priv, REG_HEOCHSR) &
					REG_HEOCHSR_CHSR))
			hwrunning = 0;

		if (!hwrunning || !at91sam9x5_video_buf_in_use(priv,
					&priv->cur)) {
			vb2_buffer_done(priv->cur.vb, VB2_BUF_STATE_DONE);
			priv->cur = priv->next;
			priv->next.vb = NULL;
		}
	} else if (priv->next.vb) {
		priv->cur = priv->next;
		priv->next.vb = NULL;
	}

	return heoisr;
}

static irqreturn_t at91sam9x5_video_irq(int irq, void *data)
{
	struct at91sam9x5_video_priv *priv = data;
	unsigned long flags;
	u32 handled, heoimr;

	spin_lock_irqsave(&priv->lock, flags);

	heoimr = at91sam9x5_video_read32(priv, REG_HEOIMR);
	handled = at91sam9x5_video_handle_irqstat(priv);

	debug("HEOIMR = 0x%08x, HEOCHSR = 0x%08x\n", heoimr, handled);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (handled & heoimr)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static inline int sign(int x)
{
	if (x > 0)
		return 1;
	else if (x < 0)
		return -1;
	else
		return 0;
}

static void at91sam9x5_video_show_buf(struct at91sam9x5_video_priv *priv,
		struct vb2_buffer *vb)
{
	dma_addr_t buffer = vb2_dma_contig_plane_dma_addr(vb, 0);
	void *vaddr = vb2_plane_vaddr(vb, 0);
	struct v4l2_pix_format *pix = &priv->fmt_vid_out_cur;
	/* XXX: format dependant */
	size_t offset_dmadesc = ALIGN(pix->width * pix->height +
			ALIGN(pix->width, 2) * ALIGN(pix->height, 2) / 2, 64);
	u32 *dmadesc = vaddr + offset_dmadesc;
	u32 heocher;

	if (priv->cfgstate == at91sam9x5_video_CFG_GOOD_LATCH) {
		heocher = REG_HEOCHER_UPDATEEN;
		priv->cfgstate = at91sam9x5_video_CFG_GOOD;
	} else {
		BUG_ON(priv->cfgstate != at91sam9x5_video_CFG_GOOD);
		heocher = 0;
	}

	debug("vout=%ux%u, heocher=%08x\n", pix->width, pix->height, heocher);
	debug("dmadesc @ 0x%08x\n", dmadesc);
	debug("dmadesc u @ 0x%08x\n", &dmadesc[4]);
	debug("dmadesc v @ 0x%08x\n", &dmadesc[8]);

	dmadesc[0] = buffer + priv->y_offset;
	dmadesc[1] = REG_HEOxCTRL_DFETCH;
	dmadesc[2] = buffer + offset_dmadesc;
	/* dmadesc[3] not used to align U plane descriptor */

	if (priv->u_planeno >= 0) {
		dmadesc[4] = vb2_dma_contig_plane_dma_addr(vb, priv->u_planeno) +
			priv->u_offset;
		dmadesc[5] = REG_HEOxCTRL_DFETCH;
		/* link to physical address of this U descriptor */
		dmadesc[6] = buffer + offset_dmadesc + 4 * 4;
	}
	/* dmadesc[7] not used to align V plane descriptor */

	if (priv->v_planeno >= 0) {
		dmadesc[8] = vb2_dma_contig_plane_dma_addr(vb, priv->v_planeno) +
			priv->v_offset;
		dmadesc[9] = REG_HEOxCTRL_DFETCH;
		/* link to physical address of this V descriptor */
		dmadesc[10] = buffer + offset_dmadesc + 8 * 4;
	}


	debug("HEOCHSR = %08x\n", at91sam9x5_video_read32(priv, REG_HEOCHSR));
	if (likely(priv->hwstate == at91sam9x5_video_HW_RUNNING)) {

		at91sam9x5_video_write32(priv, REG_HEOHEAD, dmadesc[2]);

		if (priv->u_planeno >= 0)
			at91sam9x5_video_write32(priv,
					REG_HEOUHEAD, dmadesc[6]);

		if (priv->v_planeno >= 0)
			at91sam9x5_video_write32(priv,
					REG_HEOVHEAD, dmadesc[10]);

		at91sam9x5_video_write32(priv,
				REG_HEOCHER, heocher | REG_HEOCHER_A2QEN);

	} else {

		at91sam9x5_video_write32(priv, REG_HEOADDR, dmadesc[0]);
		at91sam9x5_video_write32(priv, REG_HEOCTRL, dmadesc[1]);
		at91sam9x5_video_write32(priv, REG_HEONEXT, dmadesc[2]);

		if (priv->u_planeno >= 0) {
			at91sam9x5_video_write32(priv,
					REG_HEOUADDR, dmadesc[4]);
			at91sam9x5_video_write32(priv,
					REG_HEOUCTRL, dmadesc[5]);
			at91sam9x5_video_write32(priv,
					REG_HEOUNEXT, dmadesc[6]);
		}

		if (priv->v_planeno >= 0) {
			at91sam9x5_video_write32(priv,
					REG_HEOVADDR, dmadesc[8]);
			at91sam9x5_video_write32(priv,
					REG_HEOVCTRL, dmadesc[9]);
			at91sam9x5_video_write32(priv,
					REG_HEOVNEXT, dmadesc[10]);
		}

		at91sam9x5_video_write32(priv, REG_HEOCHER,
				heocher | REG_HEOCHER_CHEN);

		priv->hwstate = at91sam9x5_video_HW_RUNNING;
	}

	if (priv->cur.vb && at91sam9x5_video_buf_in_use(priv, &priv->cur)) {
		if (priv->next.vb) {
			/* drop next; XXX: is this an error? */
			debug("drop %p\n", priv->next.vb);
			vb2_buffer_done(priv->next.vb, VB2_BUF_STATE_ERROR);
		}
	} else {
		if (priv->cur.vb)
			vb2_buffer_done(priv->cur.vb, VB2_BUF_STATE_DONE);

		priv->cur = priv->next;
	}
	priv->next.vb = vb;
	priv->next.u_planeno = priv->u_planeno;
	priv->next.v_planeno = priv->v_planeno;
	priv->next.plane_size[0] = priv->plane_size[0];
	priv->next.plane_size[1] = priv->plane_size[1];
	priv->next.plane_size[2] = priv->plane_size[2];
}

static bool experimental;
module_param(experimental, bool, 0644);
MODULE_PARM_DESC(experimental, "enable experimental features");

static void at91sam9x5_video_params(unsigned width, unsigned height,
		int rotation, u32 *xstride, u32 *pstride, u32 *tloffset)
{
/* offset of pixel at (x, y) in the buffer */
#define po(x, y) ((x) + width * (y))

	/* offsets of the edges in counter-clockwise order */
	const unsigned e[] = {
		po(0, 0),
		po(0, height - 1),
		po(width - 1, height - 1),
		po(width - 1, 0),
	};

	/*
	 * offsets of the pixels next to the corresponding edges
	 * If edge[i] goes to the top left corner, edge_neighbour[i] is
	 * located just below of edge[i].
	 */
	const unsigned en[] = {
		po(0, 1),
		po(1, height - 1),
		po(width - 1, height - 2),
		po(width - 2, 0),
	};

#define ro(r) ((rotation + (r)) % 4)

	*xstride = en[ro(0)] - e[ro(3)];
	*pstride = e[ro(3)] - en[ro(3)];
	*tloffset = e[ro(0)];
}

#if defined(CONFIG_SOC_SAMA5)
static void at91sam9x5_video_setup_scaling_coef(
		struct at91sam9x5_video_priv *priv,
		unsigned hwxmem_size, unsigned hwxsize,
		unsigned hwymem_size, unsigned hwysize,
		unsigned *xphidef, unsigned *yphidef)
{
	int i;
	u32 *sc_coef;
	unsigned int scaling_coef_nbr = (REG_HEO_COEF_END - REG_HEO_COEF_BASE) / 4 + 1;

	if (scaling_coef_nbr != ARRAY_SIZE(heo_downscaling_coef)
	    || scaling_coef_nbr != ARRAY_SIZE(heo_upscaling_coef)) {
		dev_err(&priv->pdev->dev,
			"number of scaling coefficients not coherent\n");
		return;
	}

	/* base our up/down scaling decision on X size only */
	if (hwxmem_size >= hwxsize)
		sc_coef = heo_downscaling_coef;
	else
		sc_coef = heo_upscaling_coef;

	/* use coefficient tables */
	for (i = 0 ; i < scaling_coef_nbr ; i++)
		at91sam9x5_video_write32(priv, REG_HEO_COEF_BASE + 4 * i,
								sc_coef[i]);

	*xphidef = HEOCFG41_XPHIDEF_DEFAULT;
	*yphidef = HEOCFG41_YPHIDEF_DEFAULT;

	/* configure filter phase offset */
	at91sam9x5_video_write32(priv, REG_HEOCFG41,
			valtomask(*xphidef, REG_HEOCFG41_XPHIDEF) |
			valtomask(*yphidef, REG_HEOCFG41_YPHIDEF));
}

static void at91sam9x5_video_setup_scaling_factor(
		unsigned hwmem_size, unsigned hwsize,
		unsigned phidef, unsigned *factor)
{
	unsigned mem_max;

	*factor = (8 * 256 * hwmem_size - 256 * phidef) / hwsize + 1;
	mem_max  = (*factor * hwsize + 256 * phidef) / 2048;
	if (mem_max > hwmem_size)
		(*factor)--;
}
#else
static void at91sam9x5_video_setup_scaling_coef(
		struct at91sam9x5_video_priv *priv,
		unsigned hwxmem_size, unsigned hwxsize,
		unsigned hwymem_size, unsigned hwysize,
		unsigned *xphidef, unsigned *yphidef) {}

static void at91sam9x5_video_setup_scaling_factor(
		unsigned hwmem_size, unsigned hwsize,
		unsigned phidef, unsigned *factor)
{
	*factor = 1024 * hwmem_size / hwsize;
}
#endif

static void at91sam9x5_video_update_config_real(
		struct at91sam9x5_video_priv *priv)
{
	struct v4l2_pix_format *pix = &priv->fmt_vid_out_cur;
	struct v4l2_window *win = &priv->fmt_vid_overlay;
	struct v4l2_rect *rect = &win->w;

	/* XXX: check for overflow? */
	s32 right = rect->left + rect->width, bottom = rect->top + rect->height;

	unsigned hwxpos, hwypos, hwxsize, hwysize;
	unsigned hwxmem_size, hwymem_size;
	unsigned xphidef = 0;
	unsigned yphidef = 0;
	unsigned xfactor, yfactor;
	s32 hwxstride, hwpstride;
	s32 hwuvxstride, hwuvpstride;
	s32 rotated_pixwidth, rotated_pixheight;

	debug("vout=%ux%u, ovl=(%d,%d)+(%d,%d)\n", pix->width, pix->height,
			rect->left, rect->top, rect->width, rect->height);

	if (!experimental && priv->rotation) {
		dev_info(&priv->video_dev->dev, "disable rotation\n");
		priv->rotation = 0;
	}

	if (rect->left < 0)
		hwxpos = 0;
	else
		hwxpos = rect->left;

	if (rect->top < 0)
		hwypos = 0;
	else
		hwypos = rect->top;

	if (right > priv->fbinfo->var.xres)
		hwxsize = priv->fbinfo->var.xres - hwxpos;
	else
		hwxsize = right - hwxpos;

	if (bottom > priv->fbinfo->var.yres)
		hwysize = priv->fbinfo->var.yres - hwypos;
	else
		hwysize = bottom - hwypos;

	at91sam9x5_video_write32(priv, REG_HEOCFG2,
			valtomask(hwxpos, REG_HEOCFG2_XPOS) |
			valtomask(hwypos, REG_HEOCFG2_YPOS));

	at91sam9x5_video_write32(priv, REG_HEOCFG3,
			valtomask(hwxsize - 1, REG_HEOCFG3_XSIZE) |
			valtomask(hwysize - 1, REG_HEOCFG3_YSIZE));

	switch(pix->pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			at91sam9x5_video_write32(priv, REG_HEOCFG1,
			REG_HEOCFG1_YUVMODE_16YCBCR_0 |
			REG_HEOCFG1_YUVEN);
			break;
		case V4L2_PIX_FMT_UYVY:
			at91sam9x5_video_write32(priv, REG_HEOCFG1,
			REG_HEOCFG1_YUVMODE_16YCBCR_1 |
			REG_HEOCFG1_YUVEN);
			break;
		case V4L2_PIX_FMT_YUV420:
			at91sam9x5_video_write32(priv, REG_HEOCFG1,
			REG_HEOCFG1_YUVMODE_12YCBCRP |
			REG_HEOCFG1_YUVEN);
		default:
			at91sam9x5_video_write32(priv, REG_HEOCFG1,
			REG_HEOCFG1_YUVMODE_12YCBCRP |
			REG_HEOCFG1_YUVEN);
			break;
	}

	/* XXX:
	 *  - clipping
	 */
	at91sam9x5_video_write32(priv, REG_HEOCFG12,
			REG_HEOCFG12_GAEN |
			REG_HEOCFG12_OVR |
			REG_HEOCFG12_DMA |
			REG_HEOCFG12_REP |
			REG_HEOCFG12_GA);

#define vx(pos) xedge[(priv->rotation + pos) % 4]
#define vy(pos) yedge[(priv->rotation + pos) % 4]

	if (priv->rotation & 1) {
		rotated_pixwidth = pix->height;
		rotated_pixheight = pix->width;
	} else {
		rotated_pixwidth = pix->width;
		rotated_pixheight = pix->height;
	}

	hwxmem_size = rotated_pixwidth * hwxsize / rect->width;
	hwymem_size = rotated_pixheight * hwysize / rect->height;

	at91sam9x5_video_write32(priv, REG_HEOCFG4,
			valtomask(hwxmem_size - 1, REG_HEOCFG4_XMEMSIZE) |
			valtomask(hwymem_size - 1, REG_HEOCFG4_YMEMSIZE));

	at91sam9x5_video_setup_scaling_coef(priv,
					    hwxmem_size, hwxsize,
					    hwymem_size, hwysize,
					    &xphidef, &yphidef);

	at91sam9x5_video_setup_scaling_factor(hwxmem_size - 1, hwxsize - 1,
					      xphidef, &xfactor);

	at91sam9x5_video_setup_scaling_factor(hwymem_size - 1, hwysize - 1,
					      yphidef, &yfactor);

	at91sam9x5_video_write32(priv, REG_HEOCFG13, REG_HEOCFG13_SCALEN
				| valtomask(xfactor, REG_HEOCFG13_XFACTOR)
				| valtomask(yfactor, REG_HEOCFG13_YFACTOR));

	at91sam9x5_video_params(pix->width, pix->height, priv->rotation,
			&hwxstride, &hwpstride, &priv->y_offset);

	/* XXX: format-dependant */
	at91sam9x5_video_params(DIV_ROUND_UP(pix->width, 2),
			DIV_ROUND_UP(pix->height, 2), priv->rotation,
			&hwuvxstride, &hwuvpstride, &priv->u_offset);

	at91sam9x5_video_write32(priv, REG_HEOCFG5,
			valtomask(hwxstride - 1, REG_HEOCFG5_XSTRIDE));
	at91sam9x5_video_write32(priv, REG_HEOCFG6,
			valtomask(hwpstride - 1, REG_HEOCFG6_PSTRIDE));

	at91sam9x5_video_write32(priv, REG_HEOCFG7,
			valtomask(hwuvxstride - 1, REG_HEOCFG7_UVXSTRIDE));
	at91sam9x5_video_write32(priv, REG_HEOCFG8,
			valtomask(hwuvpstride - 1, REG_HEOCFG8_UVPSTRIDE));

	/* XXX: format dependant */
	priv->u_planeno = 0;
	priv->v_planeno = 0;
	priv->u_offset += pix->width * pix->height;
	priv->v_offset = priv->u_offset +
		DIV_ROUND_UP(pix->width, 2) * DIV_ROUND_UP(pix->height, 2);

	/* XXX: evaluate pix->colorspace */
	at91sam9x5_video_write32(priv, REG_HEOCFG14, 0x4c900091);
	at91sam9x5_video_write32(priv, REG_HEOCFG15, 0x7a5f5090);
	at91sam9x5_video_write32(priv, REG_HEOCFG16, 0x40040890);
}

static void at91sam9x5_video_update_config(struct at91sam9x5_video_priv *priv,
		int overlay_only)
{
	debug("cfgupdate=%d overlay_only=%d\n", priv->cfgupdate, overlay_only);

	at91sam9x5_video_handle_irqstat(priv);

	if (priv->cfgupdate || overlay_only) {
		struct v4l2_pix_format *pix = &priv->fmt_vid_out_cur;
		struct v4l2_window *win = &priv->fmt_vid_overlay;
		struct v4l2_rect *rect = &win->w;

		if (!overlay_only) {
			*pix = priv->fmt_vid_out_next;
			priv->cfgupdate = 0;
		}

		/* XXX: handle clipping */
		if (rect->width <= 0 || rect->height <= 0 ||
				/* vid_out is set */
				pix->width <= 0 ||
				pix->height <= 0 ||
				/* window is partly invisible or too small */
				rect->left < 0 ||
				rect->top < 0 ||
				rect->left >= (int)priv->fbinfo->var.xres - 5 ||
				rect->top >= (int)priv->fbinfo->var.yres - 5 ||
				rect->left + rect->width >
					(int)priv->fbinfo->var.xres ||
				rect->top + rect->height >
					(int)priv->fbinfo->var.yres) {

			if (priv->cfgstate == at91sam9x5_video_CFG_GOOD ||
					priv->cfgstate ==
					at91sam9x5_video_CFG_GOOD_LATCH)
				at91sam9x5_video_write32(priv,
						REG_HEOCHDR, REG_HEOCHDR_CHDIS);

			priv->cfgstate = at91sam9x5_video_CFG_BAD;
		} else {
			at91sam9x5_video_update_config_real(priv);

			debug("hwstate=%d cfgstate=%d\n",
					priv->hwstate, priv->cfgstate);
			if (overlay_only && priv->hwstate ==
					at91sam9x5_video_HW_RUNNING) {
				if (priv->cfgstate ==
						at91sam9x5_video_CFG_BAD) {
					priv->cfgstate =
						at91sam9x5_video_CFG_GOOD_LATCH;
					priv->hwstate =
						at91sam9x5_video_HW_IDLE;

					at91sam9x5_video_show_buf(priv,
							priv->cur.vb);
				} else
					at91sam9x5_video_write32(priv,
							REG_HEOCHER,
							REG_HEOCHER_UPDATEEN);
			} else
				priv->cfgstate =
					at91sam9x5_video_CFG_GOOD_LATCH;
		}

	}
}

static int at91sam9x5_video_vb_queue_setup(struct vb2_queue *q,
		const struct v4l2_format *fmt,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned int sizes[], void *alloc_ctxs[])
{
	struct at91sam9x5_video_priv *priv =
		container_of(q, struct at91sam9x5_video_priv, queue);
	struct v4l2_pix_format *pix = &priv->fmt_vid_out_next;

	debug("vout=%ux%u\n", pix->width, pix->height);

	/* XXX */
	*num_planes = 1;

	/*
	 * The last 9 (64 bits aligned) words are used for the 3 dma
	 * descriptors (3 * 32-bit words each).
	 * The additional 64 + 2 * 32 bits are for alignment.
	 * XXX: is that allowed and done right?
	 * XXX: format-dependant
	 */
	switch(pix->pixelformat) {
		case V4L2_PIX_FMT_YUV420:
			sizes[0] = pix->width * pix->height +
				ALIGN(pix->width, 2) * ALIGN(pix->height, 2) / 2 +
				9 * 32 + 128;
			break;
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUYV:
			sizes[0] = pix->width * pix->height +
			ALIGN(pix->width, 2) * ALIGN(pix->height, 2) +
			9 * 32 + 128;
			break;
		default:
			return -EINVAL;
	}

	priv->plane_size[0] = sizes[0];

	alloc_ctxs[0] = priv->alloc_ctx;

	return 0;
}

static void at91sam9x5_video_vb_wait_prepare(struct vb2_queue *q)
{
	struct at91sam9x5_video_priv *priv =
		container_of(q, struct at91sam9x5_video_priv, queue);
	unsigned long flags;

	debug("cfgupdate=%d hwstate=%d cfgstate=%d\n",
			priv->cfgupdate, priv->hwstate, priv->cfgstate);
	debug("bufs=%p,%p\n", priv->cur.vb, priv->next.vb);
	spin_lock_irqsave(&priv->lock, flags);

	at91sam9x5_video_handle_irqstat(priv);

	at91sam9x5_video_write32(priv, REG_HEOIER,
			REG_HEOIxR_ADD | REG_HEOIxR_DMA |
			REG_HEOIxR_UADD | REG_HEOIxR_UDMA |
			REG_HEOIxR_VADD | REG_HEOIxR_VDMA);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void at91sam9x5_video_vb_wait_finish(struct vb2_queue *q)
{
	struct at91sam9x5_video_priv *priv =
		container_of(q, struct at91sam9x5_video_priv, queue);
	unsigned long flags;

	debug("cfgupdate=%d hwstate=%d cfgstate=%d\n",
			priv->cfgupdate, priv->hwstate, priv->cfgstate);
	debug("bufs=%p,%p\n", priv->cur.vb, priv->next.vb);
	spin_lock_irqsave(&priv->lock, flags);

	at91sam9x5_video_write32(priv, REG_HEOIDR,
			REG_HEOIxR_ADD | REG_HEOIxR_DMA |
			REG_HEOIxR_UADD | REG_HEOIxR_UDMA |
			REG_HEOIxR_VADD | REG_HEOIxR_VDMA);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static int at91sam9x5_video_vb_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	struct at91sam9x5_video_priv *priv =
		container_of(q, struct at91sam9x5_video_priv, queue);
	struct v4l2_pix_format *pix = &priv->fmt_vid_out_cur;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (priv->cfgupdate)
		pix = &priv->fmt_vid_out_next;
	spin_unlock_irqrestore(&priv->lock, flags);

	debug("vout=%ux%u\n", pix->width, pix->height);
	debug("buflen=%u\n", vb->v4l2_planes[0].length);

	/* XXX: format-dependant */
	if (vb->v4l2_planes[0].length < pix->width * pix->height +
			ALIGN(pix->width, 2) * ALIGN(pix->height, 2) / 2 +
			9 * 32 + 128)
		return -EINVAL;

	return 0;
}

static void at91sam9x5_video_vb_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *q = vb->vb2_queue;
	struct at91sam9x5_video_priv *priv =
		container_of(q, struct at91sam9x5_video_priv, queue);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	at91sam9x5_video_update_config(priv, 0);

	switch (priv->cfgstate) {
	case at91sam9x5_video_CFG_GOOD:
	case at91sam9x5_video_CFG_GOOD_LATCH:
		/* show_buf takes care of the eventual hwstate update */
		at91sam9x5_video_show_buf(priv, vb);
		break;

	case at91sam9x5_video_CFG_BAD:
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		priv->hwstate = at91sam9x5_video_HW_RUNNING;
		break;
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

const struct vb2_ops at91sam9x5_video_vb_ops = {
	.queue_setup = at91sam9x5_video_vb_queue_setup,

	.wait_prepare = at91sam9x5_video_vb_wait_prepare,
	.wait_finish = at91sam9x5_video_vb_wait_finish,

	.buf_prepare = at91sam9x5_video_vb_buf_prepare,
	.buf_queue = at91sam9x5_video_vb_buf_queue,
};

static int at91sam9x5_video_vidioc_querycap(struct file *filp,
		void *fh, struct v4l2_capability *cap)
{
	strlcpy(cap->driver, DRIVER_NAME, sizeof(cap->driver));
	cap->capabilities = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_OVERLAY;

	cap->version = 0;
	strlcpy(cap->card, "Atmel HEO Layer", sizeof(cap->card));
	cap->bus_info[0] = '\0';

	return 0;
}

static int at91sam9x5_video_vidioc_g_fmt_vid_out(struct file *filp,
		void *fh, struct v4l2_format *f)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	unsigned long flags;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);

	f->fmt.pix = priv->fmt_vid_out_next;

	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static int at91sam9x5_video_vidioc_s_fmt_vid_out(struct file *filp,
		void *fh, struct v4l2_format *f)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	unsigned long flags;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch(pix->pixelformat) {
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YUYV:
			break;
		default:
			return -EINVAL;
	}

	debug("vout=%ux%u\n", pix->width, pix->height);

	spin_lock_irqsave(&priv->lock, flags);

	priv->fmt_vid_out_next = *pix;

	priv->cfgupdate = 1;

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int at91sam9x5_video_vidioc_g_fmt_vid_overlay(struct file *filp,
		void *fh, struct v4l2_format *f)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	unsigned long flags;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);

	f->fmt.win = priv->fmt_vid_overlay;

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int at91sam9x5_video_vidioc_s_fmt_vid_overlay(struct file *filp,
		void *fh, struct v4l2_format *f)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	struct v4l2_window *win = &f->fmt.win;
	unsigned long flags;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	debug("rect=(%d,%d)+(%d,%d)\n",
			win->w.left, win->w.top, win->w.width, win->w.height);

	spin_lock_irqsave(&priv->lock, flags);

	priv->fmt_vid_overlay = *win;

	at91sam9x5_video_update_config(priv, 1);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int at91sam9x5_video_vidioc_enum_fmt_vid_out(struct file *filp,
		void *fh, struct v4l2_fmtdesc *f)
{
	if (f->index >= ARRAY_SIZE(vout_fmt))
		return -EINVAL;

	f->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	strlcpy(f->description, vout_fmt[f->index].desc,
		sizeof(f->description));
	f->pixelformat = vout_fmt[f->index].pfmt;

	return 0;
}

static int at91sam9x5_video_vidioc_reqbufs(struct file *filp,
		void *fh, struct v4l2_requestbuffers *b)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	struct vb2_queue *q = &priv->queue;

	if (b->type != q->type) {
		dev_err(&priv->pdev->dev, "invalid buffer type (%d != %d)\n",
				b->type, q->type);
		return -EINVAL;
	}

	return vb2_reqbufs(q, b);
}

static int at91sam9x5_video_vidioc_querybuf(struct file *filp,
		void *fh, struct v4l2_buffer *b)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);

	return vb2_querybuf(&priv->queue, b);
}

static int at91sam9x5_video_vidioc_qbuf(struct file *filp,
		void *fh, struct v4l2_buffer *b)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);

	return vb2_qbuf(&priv->queue, b);
}

static int at91sam9x5_video_vidioc_dqbuf(struct file *filp,
		void *fh, struct v4l2_buffer *b)
{
	struct video_device *vdev = filp->private_data;
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);

	return vb2_dqbuf(&priv->queue, b, filp->f_flags & O_NONBLOCK);
}

static int at91sam9x5_video_vidioc_streamon(struct file *filp,
		void *fh, enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(filp);
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);

	return vb2_streamon(&priv->queue, type);
}

static int at91sam9x5_video_vidioc_streamoff(struct file *filp,
		void *fh, enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(filp);
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	/* disable channel */
	at91sam9x5_video_write32(priv, REG_HEOCHDR, REG_HEOCHDR_CHDIS);

	at91sam9x5_video_handle_irqstat(priv);

	if (priv->cur.vb)
		at91sam9x5_video_write32(priv, REG_HEOIER,
				REG_HEOIxR_ADD | REG_HEOIxR_DMA |
				REG_HEOIxR_UADD | REG_HEOIxR_UDMA |
				REG_HEOIxR_VADD | REG_HEOIxR_VDMA);

	priv->hwstate = at91sam9x5_video_HW_IDLE;

	spin_unlock_irqrestore(&priv->lock, flags);

	return vb2_streamoff(&priv->queue, type);
}

static int at91sam9x5_video_vidioc_queryctrl(struct file *filp, void *fh,
		struct v4l2_queryctrl *a)
{
	int ret;

	switch (a->id) {
	case V4L2_CID_ROTATE:
		ret = v4l2_ctrl_query_fill(a, 0, 270, 90, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int at91sam9x5_video_vidioc_g_ctrl(struct file *filp, void *fh,
		struct v4l2_control *a)
{
	struct video_device *vdev = video_devdata(filp);
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	int ret = 0;

	switch (a->id) {
	case V4L2_CID_ROTATE:
		a->value = 90 * priv->rotation;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int at91sam9x5_video_vidioc_s_ctrl(struct file *filp, void *fh,
		struct v4l2_control *a)
{
	struct video_device *vdev = video_devdata(filp);
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);
	int ret;
	unsigned long flags;

	switch (a->id) {
	case V4L2_CID_ROTATE:
		if (a->value / 90 * 90 != a->value ||
				(a->value / 90) % 4 != a->value / 90) {
			ret = -EINVAL;
		} else {
			debug("rotation: %d\n", a->value);
			spin_lock_irqsave(&priv->lock, flags);
			priv->rotation = a->value / 90;
			at91sam9x5_video_update_config(priv, 1);
			spin_unlock_irqrestore(&priv->lock, flags);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ioctl_ops at91sam9x5_video_ioctl_ops = {
	.vidioc_querycap = at91sam9x5_video_vidioc_querycap,
	.vidioc_g_fmt_vid_out = at91sam9x5_video_vidioc_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out = at91sam9x5_video_vidioc_s_fmt_vid_out,
	.vidioc_g_fmt_vid_overlay = at91sam9x5_video_vidioc_g_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay = at91sam9x5_video_vidioc_s_fmt_vid_overlay,
	.vidioc_enum_fmt_vid_out = at91sam9x5_video_vidioc_enum_fmt_vid_out,
	.vidioc_reqbufs = at91sam9x5_video_vidioc_reqbufs,
	.vidioc_querybuf = at91sam9x5_video_vidioc_querybuf,
	.vidioc_qbuf = at91sam9x5_video_vidioc_qbuf,
	.vidioc_dqbuf = at91sam9x5_video_vidioc_dqbuf,
	.vidioc_streamon = at91sam9x5_video_vidioc_streamon,
	.vidioc_streamoff = at91sam9x5_video_vidioc_streamoff,
	.vidioc_queryctrl = at91sam9x5_video_vidioc_queryctrl,
	.vidioc_g_ctrl = at91sam9x5_video_vidioc_g_ctrl,
	.vidioc_s_ctrl = at91sam9x5_video_vidioc_s_ctrl,
};

static int at91sam9x5_video_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);

	/*
	 * XXX: allow only one open? Or is that already enforced by the
	 * framework?
	 */
	filp->private_data = vdev;

	return 0;
}

static int at91sam9x5_video_release(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);

	dev_dbg(&vdev->dev, "%s\n", __func__);

	return 0;
}

static int at91sam9x5_video_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct video_device *vdev = video_devdata(filp);
	struct at91sam9x5_video_priv *priv = video_get_drvdata(vdev);

	dev_dbg(&vdev->dev, "%s\n", __func__);

	/* returning -EIO here makes gst-launch segfault */
	return vb2_mmap(&priv->queue, vma);
}

static struct v4l2_file_operations at91sam9x5_video_fops = {
	.owner = THIS_MODULE,
	.open = at91sam9x5_video_open,
	.release = at91sam9x5_video_release,
	.ioctl = video_ioctl2,
	.mmap = at91sam9x5_video_mmap,
};

static int at91sam9x5_video_register(struct at91sam9x5_video_priv *priv,
		struct fb_info *fbinfo)
{
	int ret = -ENOMEM;
	struct platform_device *pdev = priv->pdev;
	struct resource *res;
	/*const struct at91sam9x5_video_pdata *pdata =
		dev_get_platdata(&pdev->dev);*/
	struct vb2_queue *q = &priv->queue;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (priv->fbinfo) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return -EBUSY;
	}
	priv->fbinfo = fbinfo;
	spin_unlock_irqrestore(&priv->lock, flags);

	/* XXX: this doesn't belong here, does it? */
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	/* Not used for now */
#if 0
	if (!pdata) {
		dev_err(&pdev->dev, "failed to get platform data\n");
		goto err_get_pdata;
	}
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get register base\n");
		goto err_get_regbase;
	}

	priv->regbase = ioremap(res->start, resource_size(res));
	if (!priv->regbase) {
		dev_err(&pdev->dev, "failed to remap register base\n");
		goto err_ioremap;
	}

	/*
	 * XXX: video_device_alloc is just a kzalloc, so embedding struct
	 * video_device into struct at91sam9x5_video_priv would work, too.
	 * Is that allowed?
	 */
	priv->video_dev = video_device_alloc();
	if (!priv->video_dev) {
		dev_err(&pdev->dev, "failed to alloc video device for %p\n",
				fbinfo);
		goto err_video_device_alloc;
	}

	priv->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(priv->alloc_ctx)) {
		ret = PTR_ERR(priv->alloc_ctx);
		dev_err(&pdev->dev, "failed to init alloc_ctx (%d)\n", ret);
		goto err_init_ctx;
	}

	q->ops = &at91sam9x5_video_vb_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_WRITE;
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	ret = vb2_queue_init(q);
	if (ret) {
		dev_err(&pdev->dev, "failed to init queue (%d)\n", ret);
		goto err_queue_init;
	}

	strlcpy(priv->video_dev->name, DRIVER_NAME,
			sizeof(priv->video_dev->name));
	priv->video_dev->fops = &at91sam9x5_video_fops;
	priv->video_dev->ioctl_ops = &at91sam9x5_video_ioctl_ops;
	priv->video_dev->release = video_device_release;

	video_set_drvdata(priv->video_dev, priv);

	/* reset channel and clear status */
	at91sam9x5_video_write32(priv, REG_HEOCHDR, REG_HEOCHDR_CHRST);
	(void)at91sam9x5_video_read32(priv, REG_HEOISR);

	/* set maximal bursting */
	at91sam9x5_video_write32(priv, REG_HEOCFG0, 0x1 |
			REG_HEOCFG0_BLEN_INCR16 |
			REG_HEOCFG0_BLENUV_INCR16);

	ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(&pdev->dev, "failed to get irq from resources (%d)\n",
				ret);
		if (!ret)
			ret = -ENXIO;
		goto err_get_irq;
	}
	priv->irq = ret;

	ret = request_irq(priv->irq, at91sam9x5_video_irq, IRQF_SHARED,
			DRIVER_NAME, priv);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq (%d)\n", ret);
		goto err_request_irq;
	}

	ret = video_register_device(priv->video_dev,
			/* XXX: really grabber? */ VFL_TYPE_GRABBER, -1);

	priv->video_dev->vfl_dir = (VFL_DIR_TX | VFL_DIR_RX);

	if (ret) {
		dev_err(&pdev->dev, "failed to register video device (%d)\n",
				ret);

		free_irq(priv->irq, priv);
 err_request_irq:
 err_get_irq:

		vb2_queue_release(q);
err_queue_init:

		vb2_dma_contig_cleanup_ctx(priv->alloc_ctx);
 err_init_ctx:

		video_device_release(priv->video_dev);
 err_video_device_alloc:

		iounmap(priv->regbase);

		priv->fbinfo = NULL;
	} else {
		dev_info(&pdev->dev,
				"video device registered @ 0x%08x, irq = %d\n",
				(unsigned int)priv->regbase, priv->irq);
	}
 err_ioremap:
 err_get_regbase:
/* err_get_pdata:*/

	return ret;
}

static void at91sam9x5_video_unregister(struct at91sam9x5_video_priv *priv)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	if (!priv->fbinfo) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return;
	}
	/* XXX: handle fbinfo being NULL in various callbacks */
	priv->fbinfo = NULL;
	spin_unlock_irqrestore(&priv->lock, flags);

	/* silence DMA */
	at91sam9x5_video_write32(priv, REG_HEOIDR,
			REG_HEOIxR_ADD | REG_HEOIxR_DMA | REG_HEOIxR_UADD |
			REG_HEOIxR_UDMA | REG_HEOIxR_VADD | REG_HEOIxR_VDMA);

	video_unregister_device(priv->video_dev);
	free_irq(priv->irq, priv);
	vb2_queue_release(&priv->queue);
	vb2_dma_contig_cleanup_ctx(priv->alloc_ctx);
	video_device_release(priv->video_dev);
	iounmap(priv->regbase);
}

static int at91sam9x5_video_fb_event_notify(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct at91sam9x5_video_priv *priv =
		container_of(self, struct at91sam9x5_video_priv, fb_notifier);
	struct fb_event *event = data;
	struct fb_info *fbinfo = event->info;

	/* XXX: only do this for atmel_lcdfb devices! */
	switch (action) {
	case FB_EVENT_FB_REGISTERED:
		at91sam9x5_video_register(priv, fbinfo);
		break;

	case FB_EVENT_FB_UNREGISTERED:
		at91sam9x5_video_unregister(priv);
		break;
	}
	return 0;
}

static int at91sam9x5_video_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;
	size_t i;
	struct at91sam9x5_video_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	if (!priv) {
		dev_err(&pdev->dev, "failed to allocate driver private data\n");
		goto err_alloc_priv;
	}

	priv->pdev = pdev;
	priv->fb_notifier.notifier_call = at91sam9x5_video_fb_event_notify;

	platform_set_drvdata(pdev, priv);

	spin_lock_init(&priv->lock);

	ret = fb_register_client(&priv->fb_notifier);
	if (ret) {
		dev_err(&pdev->dev, "failed to register fb client (%d)\n", ret);

		kfree(priv);
err_alloc_priv:

		return ret;
	}

	/* XXX: This is racy. If a new fb is registered then
	 * at91sam9x5_video_register is called twice. This should be solved
	 * somewhere in drivers/fb. priv->fbinfo is used to prevent multiple
	 * registration.
	 */

	for (i = 0; i < ARRAY_SIZE(registered_fb); ++i)
		if (registered_fb[i])
			at91sam9x5_video_register(priv, registered_fb[i]);

	return 0;
}

static int at91sam9x5_video_remove(struct platform_device *pdev)
{
	struct at91sam9x5_video_priv *priv = platform_get_drvdata(pdev);

	fb_unregister_client(&priv->fb_notifier);
	at91sam9x5_video_unregister(priv);
	kfree(priv);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id atmel_heo_dt_ids[] = {
	{
		.compatible = "atmel,at91sam9x5-heo",
		.data = (void *)0,
	}, {
		.compatible = "atmel,sama5d3-heo",
		.data = (void *)1,
	}, {
		/* sentinel */
	}
};

MODULE_DEVICE_TABLE(of, atmel_heo_dt_ids);
#endif

static struct platform_driver at91sam9x5_video_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= of_match_ptr(atmel_heo_dt_ids),
	},
	.probe = at91sam9x5_video_probe,
	.remove = at91sam9x5_video_remove,
};

static int __init at91sam9x5_video_init(void)
{
	return platform_driver_probe(&at91sam9x5_video_driver,
				     &at91sam9x5_video_probe);
}
module_init(at91sam9x5_video_init);

static void __exit at91sam9x5_video_exit(void)
{
	platform_driver_unregister(&at91sam9x5_video_driver);
}
module_exit(at91sam9x5_video_exit);

MODULE_AUTHOR("Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>");
MODULE_LICENSE("GPL v2");
