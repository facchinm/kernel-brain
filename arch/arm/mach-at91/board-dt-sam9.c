/*
 *  Setup code for AT91SAM Evaluation Kits with Device Tree support
 *
 *  Copyright (C) 2011 Atmel,
 *                2011 Nicolas Ferre <nicolas.ferre@atmel.com>
 *
 * Licensed under GPLv2 or later.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include "at91_aic.h"
#include "board.h"
#include "generic.h"
#include "clock.h"

/************************************/
/* TEMPORARY NON-DT STUFF LCD       */
/************************************/
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <video/atmel_lcdfb.h>
#include <mach/atmel_hlcdc.h>
#include <mach/atmel_lcdc.h>
#include <media/soc_camera.h>
#include <media/atmel-isi.h>

#define LINK_SENSOR_MODULE_TO_SOC_CAMERA(_sensor_name, _soc_camera_id, _i2c_adapter_id)	\
	static struct soc_camera_desc iclink_##_sensor_name = {		\
		.subdev_desc = {					\
			.power = i2c_camera_power,			\
			.reset = i2c_camera_reset,			\
			.query_bus_param = isi_camera_query_bus_param,	\
		},							\
		.host_desc = {						\
			.bus_id		= -1,				\
			.board_info	= &i2c_##_sensor_name,		\
			.i2c_adapter_id	= _i2c_adapter_id,		\
		},							\
	};								\
	static struct platform_device isi_##_sensor_name = {		\
		.name	= "soc-camera-pdrv",				\
		.id	= _soc_camera_id,				\
		.dev	= {						\
			.platform_data = &iclink_##_sensor_name,	\
		},							\
	};

/*
 * LCD Controller
 */
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name		= "LG",
		.refresh	= 60,
		.xres		= 800,		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),

		.left_margin	= 88,		.right_margin	= 168,
		.upper_margin	= 8,		.lower_margin	= 37,
		.hsync_len	= 128,		.vsync_len	= 2,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

static struct fb_monspecs at91fb_default_monspecs = {
	.manufacturer	= "LG",
	.monitor	= "LB043WQ1",

	.modedb		= at91_tft_vga_modes,
	.modedb_len	= ARRAY_SIZE(at91_tft_vga_modes),
	.hfmin		= 15000,
	.hfmax		= 17640,
	.vfmin		= 57,
	.vfmax		= 67,
};

/* Default output mode is TFT 24 bit */
#define BPP_OUT_DEFAULT_LCDCFG5	(LCDC_LCDCFG5_MODE_OUTPUT_24BPP)

/* Driver datas */
static struct atmel_lcdfb_info __initdata ek_lcdc_data = {
	.lcdcon_is_backlight		= true,
	.alpha_enabled			= false,
	.default_bpp			= 16,
	/* Reserve enough memory for 32bpp */
	.smem_len			= 800 * 480 * 4,
	/* default_lcdcon2 is used for LCDCFG5 */
	.default_lcdcon2		= BPP_OUT_DEFAULT_LCDCFG5,
	.default_monspecs		= &at91fb_default_monspecs,
	.guard_time			= 9,
	.lcd_wiring_mode		= ATMEL_LCDC_WIRING_RGB,
};

/*
 *  ISI
 */
static struct isi_platform_data isi_data = {
	.frate			= ISI_CFG1_FRATE_CAPTURE_ALL,
	/* to use codec and preview path simultaneously */
	.full_mode		= 1,
	.data_width_flags	= ISI_DATAWIDTH_8 | ISI_DATAWIDTH_10,
	/* ISI_MCK is provided by programmable clock or external clock */
	.mck_hz			= 25000000,
};

static struct clk_lookup isi_mck_lookups[] = {
	CLKDEV_CON_DEV_ID("isi_mck", "atmel_isi", NULL),
};

static void __init at91_config_isi(bool use_pck_as_mck, const char *pck_id)
{
	struct clk *pck;
	struct clk *parent;

	if (use_pck_as_mck) {
		pck = clk_get(NULL, pck_id);
		parent = clk_get(NULL, "plla");

		BUG_ON(IS_ERR(pck) || IS_ERR(parent));

		if (clk_set_parent(pck, parent)) {
			pr_err("Failed to set PCK's parent\n");
		} else {
			/* Register PCK as ISI_MCK */
			isi_mck_lookups[0].clk = pck;
			clkdev_add_table(isi_mck_lookups,
				ARRAY_SIZE(isi_mck_lookups));
		}

		clk_put(pck);
		clk_put(parent);
	}
}

static unsigned int camera_reset_pin;
static unsigned int camera_power_pin;
static void camera_set_gpio_pins(uint reset_pin, uint power_pin)
{
	camera_reset_pin = reset_pin;
	camera_power_pin = power_pin;
}

/*
 * soc-camera
 */
static unsigned long isi_camera_query_bus_param(struct soc_camera_subdev_desc *link)
{
	/* ISI board for ek using default 8-bits connection */
	return SOCAM_DATAWIDTH_8;
}

static int i2c_camera_reset(struct device *dev)
{
	int res, ret = 0;

	res = devm_gpio_request(dev, camera_reset_pin, "camera_reset");
	if (res < 0) {
		printk("can't request camera reset pin\n");
		return -1;
	}

	res = gpio_direction_output(camera_reset_pin, 0);
	if (res < 0) {
		printk("can't request output direction for camera reset pin\n");
		ret = -1;
		goto out;
	}
	msleep(20);
	res = gpio_direction_output(camera_reset_pin, 1);
	if (res < 0) {
		printk("can't request output direction for camera reset pin\n");
		ret = -1;
		goto out;
	}
	msleep(100);

out:
	devm_gpio_free(dev, camera_reset_pin);
	return ret;
}

static int i2c_camera_power(struct device *dev, int on)
{
	int ret = 0;

	pr_debug("%s: %s the camera\n", __func__, on ? "ENABLE" : "DISABLE");

	if (devm_gpio_request(dev, camera_power_pin, "camera_power") < 0) {
		printk("can't request camera power pin\n");
		return -1;
	}

	/* enable or disable the camera */
	if (gpio_direction_output(camera_power_pin, !on) < 0) {
		printk("can't request output direction for camera power pin\n");
		ret = -1;
	}

	devm_gpio_free(dev, camera_power_pin);
	return ret;
}

static struct i2c_board_info i2c_ov2640 = {
	I2C_BOARD_INFO("ov2640", 0x30),
};
static struct i2c_board_info i2c_ov5642 = {
	I2C_BOARD_INFO("ov5642", 0x3c),
};
static struct i2c_board_info i2c_ov9740 = {
	I2C_BOARD_INFO("ov9740", 0x10),
};
static struct i2c_board_info i2c_ov7740 = {
	I2C_BOARD_INFO("ov7740", 0x21),
};

LINK_SENSOR_MODULE_TO_SOC_CAMERA(ov2640, 0, 0);
LINK_SENSOR_MODULE_TO_SOC_CAMERA(ov5642, 1, 0);
LINK_SENSOR_MODULE_TO_SOC_CAMERA(ov9740, 2, 0);
LINK_SENSOR_MODULE_TO_SOC_CAMERA(ov7740, 3, 0);

static struct platform_device *sensors[] __initdata = {
	&isi_ov2640,
	&isi_ov5642,	/* compatible for ov5640 */
	&isi_ov9740,
	&isi_ov7740,
};

static struct of_dev_auxdata at91_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("atmel,at91sam9x5-lcd", 0xf8038000, "atmel_hlcdfb_base", &ek_lcdc_data),
	OF_DEV_AUXDATA("atmel,at91sam9x5-lcd", 0xf8038100, "atmel_hlcdfb_ovl1", &ek_lcdc_data),
	OF_DEV_AUXDATA("atmel,at91sam9g45-lcd", 0x00500000, "atmel_lcdfb", &ek_lcdc_data),
	OF_DEV_AUXDATA("atmel,at91sam9g45-isi", 0xfffb4000, "atmel_isi", &isi_data),    /* 9m10g45ek */
	OF_DEV_AUXDATA("atmel,at91sam9g45-isi", 0xf8048000, "atmel_isi", &isi_data),	/* 9x5ek */
	{ /* sentinel */ }
};

static const struct of_device_id irq_of_match[] __initconst = {

	{ .compatible = "atmel,at91rm9200-aic", .data = at91_aic_of_init },
	{ /*sentinel*/ }
};

static void __init at91_dt_init_irq(void)
{
	of_irq_init(irq_of_match);
}

static void __init at91_dt_device_init(void)
{
	if (of_machine_is_compatible("atmel,at91sam9n12ek")) {
		__u8 manufacturer[4] = "QD";
		__u8 monitor[14] = "QD43003C1";

		/* set LCD configuration */
		at91_tft_vga_modes[0].name = "QD";
		at91_tft_vga_modes[0].xres = 480;
		at91_tft_vga_modes[0].yres = 272;
		at91_tft_vga_modes[0].pixclock = KHZ2PICOS(9000),

		at91_tft_vga_modes[0].left_margin = 8;
		at91_tft_vga_modes[0].right_margin = 43;
		at91_tft_vga_modes[0].upper_margin = 4;
		at91_tft_vga_modes[0].lower_margin = 12;
		at91_tft_vga_modes[0].hsync_len = 5;
		at91_tft_vga_modes[0].vsync_len = 10;

		memcpy(at91fb_default_monspecs.manufacturer, manufacturer, 4);
		memcpy(at91fb_default_monspecs.monitor, monitor, 14);

		printk("LCD parameters updated for at91sam9n12ek display module\n");
	}

	if (of_machine_is_compatible("atmel,at91sam9m10g45ek")) {
		#define AT91SAM9G45_DEFAULT_LCDCON2		\
				(ATMEL_LCDC_MEMOR_LITTLE \
				 | ATMEL_LCDC_DISTYPE_TFT \
				 | ATMEL_LCDC_CLKMOD_ALWAYSACTIVE)

		/* set LCD configuration */
		at91_tft_vga_modes[0].xres = 480;
		at91_tft_vga_modes[0].yres = 272;
		at91_tft_vga_modes[0].pixclock = KHZ2PICOS(9000),

		at91_tft_vga_modes[0].left_margin = 1;
		at91_tft_vga_modes[0].right_margin = 1;
		at91_tft_vga_modes[0].upper_margin = 40;
		at91_tft_vga_modes[0].lower_margin = 1;
		at91_tft_vga_modes[0].hsync_len = 45;
		at91_tft_vga_modes[0].vsync_len = 1;

		ek_lcdc_data.default_lcdcon2 = AT91SAM9G45_DEFAULT_LCDCON2;
		ek_lcdc_data.default_dmacon = ATMEL_LCDC_DMAEN;

		printk("LCD parameters updated for at91sam9m10g45ek display module\n");
	}

	if (of_machine_is_compatible("atmel,at91sam9x5ek")) {

		camera_set_gpio_pins(AT91_PIN_PA7, AT91_PIN_PA13);
		at91_config_isi(true, "pck0");

		printk("ISI parameters updated for at91sam9x5ek\n");
	}

	if (of_machine_is_compatible("atmel,at91sam9m10g45ek")) {

		camera_set_gpio_pins(AT91_PIN_PD12, AT91_PIN_PD13);
		at91_config_isi(true, "pck1");

		printk("ISI parameters updated for at91sam9m10g45ek\n");
	}

	of_platform_populate(NULL, of_default_bus_match_table, at91_auxdata_lookup, NULL);
	platform_add_devices(sensors, ARRAY_SIZE(sensors));
}

static const char *at91_dt_board_compat[] __initdata = {
	"atmel,at91sam9",
	NULL
};

DT_MACHINE_START(at91sam_dt, "Atmel AT91SAM (Device Tree)")
	/* Maintainer: Atmel */
	.init_time	= at91sam926x_pit_init,
	.map_io		= at91_map_io,
	.handle_irq	= at91_aic_handle_irq,
	.init_early	= at91_dt_initialize,
	.init_irq	= at91_dt_init_irq,
	.init_machine	= at91_dt_device_init,
	.dt_compat	= at91_dt_board_compat,
MACHINE_END
