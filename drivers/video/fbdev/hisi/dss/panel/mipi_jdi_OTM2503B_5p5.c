/* Copyright (c) 2008-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#include "hisi_fb.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
#include <linux/hisi/hisi_adc.h>
#else
#include <linux/huawei/hisi_adc.h>
#endif
#include <huawei_platform/touthscreen/huawei_touchscreen.h>
#include "mipi_jdi_OTM2503B_5p5.h"
#include <huawei_platform/log/log_jank.h>
#include <linux/hisi/hw_cmdline_parse.h>

#define OTM2503B_FPS 0
#define DTS_COMP_JDI_OTM2503B_5p5 "hisilicon,mipi_jdi_OTM2503B_5p5"
#define CABC_OFF	(0)
#define CABC_UI_MODE	(1)
#define CABC_STILL_MODE	(2)
#define CABC_MOVING_MODE	(3)
#define PACKET_PIXELS_SIZE	(216)
#define PACKET_SIZE	(PACKET_PIXELS_SIZE*3+1)
#define PATTERN_PIXELS_X_SIZE	1080
#define PATTERN_PIXELS_Y_SIZE	1920
#define GPIO_TE0 23

#define BACKLIGHT_PRINT_TIMES	10
#define CHECKSUM_SIZE	(8)
#define CHECKSUM_PIC_NUM  (10)
#define CHECKSUM_PIC_N  (4)


#define   VR_MODE_ENABLE	1
#define   VR_MODE_DISABLE	0

static struct hisi_fb_panel_data g_panel_data;

//static int hkadc_buf = 0;
static bool checksum_enable_ctl = false;
static bool otm2503b_send_panel_display_on = false;
static int g_debug_enable = 0;
static int g_cabc_mode = 2;

extern bool gesture_func;
extern bool g_lcd_control_tp_power;
extern unsigned int g_led_rg_para1;
extern unsigned int g_led_rg_para2;
extern u8 color_temp_cal_buf[32];
#define SCALING_UP_FUNC (0)

struct jdi_otm2503b_device {
	uint32_t bl_type;
	uint32_t gpio_n5v5_enable;
	uint32_t gpio_p5v5_enable;
	uint32_t gpio_reset;
	uint32_t gpio_vddio_ctrl;
	uint32_t gpio_id0;
	char bl_ic_name_buf[LCD_BL_IC_NAME_MAX];
};

#define JDI_NT35696_ERR -1;
#define JDI_OTM2503B_OK 0

static struct jdi_otm2503b_device g_dev_info;

//static int mipi_jdi_panel_set_display_region(struct platform_device *pdev, struct dss_rect *dirty);

extern void ts_power_gpio_enable(void);
extern void ts_power_gpio_disable(void);
extern void ts_check_bootup_upgrade(void);
extern struct ts_data g_ts_data;

/*******************************************************************************
** Scaling up function
*/
#if SCALING_UP_FUNC
static char ena_scale[] = {
	0x50,
	0x01,
};
#endif

/*******************************************************************************
** Partial update setting
*/
static char partial_setting_1[] = {
	0xFF,
	0x10,
};

static char partial_setting_2[] = {
	0xFB,
	0x01,
};

static char partial_setting_3[] = {
	0xC9,
	0x4B,0x04,0x21,0x00,0x0F,0x03,0x19,0x01,0x97,0x10,0xF0,
};

/*******************************************************************************
** Power ON Sequence(sleep mode to Normal mode)
*/
static char bl_val[] = {
	0x51,
	0xFF,
};

static char te_enable[] = {
	0x35,
	0x00,
};

static char bl_enable[] = {
	0x53,
	0x2C,
};

static char exit_sleep[] = {
	0x11,
};

static char display_on[] = {
	0x29,
};

static char vesa_dsc0[] = {
	0xFF,
	0x10,
};

//0x03:vesa enable, 0x00: disable
static char vesa_dsc1[] = {
	0xC0,
	0x03,
};

static char vesa_dsc2[] = {
	0xC1,
	0x09,0x20,0x00,0x10,0x02,
	0x00,0x03,0x1C,0x02,0x05,
	0x00,0x0F,0x06,0x67,0x03,
	0x2E,
};

static char vesa_dsc3[] = {
	0xC2,
	0x10,0xF0,
};

static char vesa_dsc4[] = {
	0xFB,
	0x01,
};

/*******************************************************************************
** Power OFF Sequence(Normal to power off)
*/
static char display_off[] = {
	0x28,
};

static char enter_sleep[] = {
	0x10,
};

static char otm_cmd1[] = {0x00, 0x00,};
static char otm_cmd2[] = {0xff, 0x25, 0x03, 0x01,};
static char otm_cmd3[] = {0x00,0x80,};
static char otm_cmd4[] = {0xff,0x25, 0x03,};

static char otm_cmd5[] = {0x00,0x00,};
static char otm_cmd6[] = {0x1c,0x01,};
static char otm_cmd7[] = {0x00,0xb0,};
static char otm_cmd8[] = {0xb4,0x00,0x20,0x02,0x00,0x03,0x87,0x00,0x0a,0x03,0x19,0x02,0x63,0x10,0xf0,0x05};

static char otm_cmd9[] = {0x00, 0xb1,};
static char otm_cmd10[] = {0xb0, 0x01,};

static char otm_cmd11[] = {0x00, 0x00,};
static char otm_cmd12[] = {0xff, 0x00, 0x00, 0x00,};

#if OTM2503B_FPS
static char otm_cmd13[] = {0x00, 0x00,};
static char otm_cmd14[] = {0xfb, 0x01,};

static char otm_fps30_1[] = {0x00, 0xF0,};
static char otm_fps30_2[] = {0xC0, 0x40, 0x00,};

static char otm_fps60_1[] = {0x00, 0xF0,};
static char otm_fps60_2[] = {0xC0, 0x80, 0x02,};

static char otm_fpsauto_1[] = {0x00, 0xF0,};
static char otm_fpsauto_2[] = {0xC0, 0x80, 0x02,};//open dfr
static char otm_fpsauto_3[] = {0xC0, 0x80, 0x00,};//close dfr

static char otm_reload_1[] = {0x00, 0x00};
static char otm_reload_2[] = {0xfb, 0x01};
#endif

static struct dsi_cmd_desc lcd_display_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(te_enable), te_enable},
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_val), bl_val},

	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd1), otm_cmd1},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd2), otm_cmd2},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd3), otm_cmd3},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd4), otm_cmd4},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd5), otm_cmd5},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd6), otm_cmd6},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd7), otm_cmd7},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd8), otm_cmd8},
#if OTM2503B_FPS
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_fpsauto_1), otm_fpsauto_1},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_fpsauto_2), otm_fpsauto_2},
#endif
#if 0
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd9), otm_cmd9},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd10), otm_cmd10},
#endif
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd11), otm_cmd11},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd12), otm_cmd12},

	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},
	//{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		//sizeof(display_on), display_on},
};

static struct dsi_cmd_desc lcd_display_on_cmd_in_backlight[] = {
	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_US,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc lcd_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 60, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static struct dsi_cmd_desc lcd_partial_updt_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(partial_setting_1), partial_setting_1},
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(partial_setting_2), partial_setting_2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(partial_setting_3), partial_setting_3},
};

static struct dsi_cmd_desc lcd_vesa3x_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(vesa_dsc0), vesa_dsc0},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(vesa_dsc1), vesa_dsc1},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(vesa_dsc2), vesa_dsc2},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(vesa_dsc3), vesa_dsc3},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(vesa_dsc4), vesa_dsc4},
};

#if OTM2503B_FPS
static struct dsi_cmd_desc otm2503_fps_to_60[] = {
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd1), otm_cmd1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd2), otm_cmd2},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd3),otm_cmd3},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd4), otm_cmd4},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fps60_1),otm_fps60_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fps60_2), otm_fps60_2},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd13), otm_cmd13},
	{DTYPE_DCS_WRITE1, 0, 50, WAIT_TYPE_MS,
		sizeof(otm_cmd14), otm_cmd14},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd11), otm_cmd11},
	{DTYPE_DCS_LWRITE, 0, 50, WAIT_TYPE_MS,
		sizeof(otm_cmd12), otm_cmd12},
};

static struct dsi_cmd_desc otm2503_fps_to_30[] = {
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd1), otm_cmd1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd2), otm_cmd2},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd3),otm_cmd3},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd4), otm_cmd4},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fps30_1),otm_fps30_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fps30_2), otm_fps30_2},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd13), otm_cmd13},
	{DTYPE_DCS_WRITE1, 0, 50, WAIT_TYPE_MS,
		sizeof(otm_cmd14), otm_cmd14},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_cmd11), otm_cmd11},
	{DTYPE_DCS_LWRITE, 0, 50, WAIT_TYPE_MS,
		sizeof(otm_cmd12), otm_cmd12},
};

static struct dsi_cmd_desc otm2503b_disable_dfr[] = {
	/* switch orise cmd2 */
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd1), otm_cmd1},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd2), otm_cmd2},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd3), otm_cmd3},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd4), otm_cmd4},
	/* close dfr */
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fpsauto_1), otm_fpsauto_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fpsauto_3), otm_fpsauto_3},
	/* reload */
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_reload_1), otm_reload_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_reload_2), otm_reload_2},
	/* switch cmd1 */
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd11), otm_cmd11},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd12), otm_cmd12},
};

static struct dsi_cmd_desc otm2503b_enable_dfr[] = {
	/* switch orise cmd2 */
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd1), otm_cmd1},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd2), otm_cmd2},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd3), otm_cmd3},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd4), otm_cmd4},
	/* open dfr */
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fpsauto_1), otm_fpsauto_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_fpsauto_2), otm_fpsauto_2},
	/* reload */
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(otm_reload_1), otm_reload_1},
	{DTYPE_DCS_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(otm_reload_2), otm_reload_2},
	/* switch cmd1 */
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd11), otm_cmd11},
	{DTYPE_GEN_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(otm_cmd12), otm_cmd12},
};
#endif

/******************************************************************************
*
** Display Effect Sequence(smart color, edge enhancement, smart contrast, cabc)
*/

static char PWM_OUT_0x51[] = {
	0x51,
	0xFE,
};

static struct dsi_cmd_desc pwm_out_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(PWM_OUT_0x51), PWM_OUT_0x51},
};

static char cabc_off[] = {
	0x55,
	0x90,
};

static char cabc_set_mode_UI[] = {
	0x55,
	0x91,
};

static char cabc_set_mode_STILL[] = {
	0x55,
	0x92,
};

static char cabc_set_mode_MOVING[] = {
	0x55,
	0x93,
};

static struct dsi_cmd_desc lg_cabc_off_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_off), cabc_off},
};

static struct dsi_cmd_desc lg_cabc_ui_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_UI), cabc_set_mode_UI},
};

static struct dsi_cmd_desc lg_cabc_still_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_STILL), cabc_set_mode_STILL},
};

static struct dsi_cmd_desc lg_cabc_moving_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_MOVING), cabc_set_mode_MOVING},
};

/*******************************************************************************
** LCD VCC
*/
#define VCC_LCDANALOG_NAME	"lcdanalog-vcc"

static struct regulator *vcc_lcdanalog;//3.3

static struct vcc_desc lcd_vcc_lcdanalog_init_cmds[] = {
	{DTYPE_VCC_GET, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0, WAIT_TYPE_MS, 0},
	{DTYPE_VCC_SET_VOLTAGE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 3300000, 3300000, WAIT_TYPE_MS, 0},
};

static struct vcc_desc lcd_vcc_lcdanalog_finit_cmds[] = {
	{DTYPE_VCC_PUT, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0, WAIT_TYPE_MS, 0},
};

static struct vcc_desc lcd_vcc_lcdanalog_enable_cmds[] = {
	{DTYPE_VCC_ENABLE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0, WAIT_TYPE_MS, 10},
};

static struct vcc_desc lcd_vcc_lcdanalog_disable_cmds[] = {
	{DTYPE_VCC_DISABLE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0, WAIT_TYPE_MS, 3},
};

/*******************************************************************************
** LCD IOMUX
*/
static struct pinctrl_data pctrl;

static struct pinctrl_cmd_desc lcd_pinctrl_init_cmds[] = {
	{DTYPE_PINCTRL_GET, &pctrl, 0},
	{DTYPE_PINCTRL_STATE_GET, &pctrl, DTYPE_PINCTRL_STATE_DEFAULT},
	{DTYPE_PINCTRL_STATE_GET, &pctrl, DTYPE_PINCTRL_STATE_IDLE},
};

static struct pinctrl_cmd_desc lcd_pinctrl_normal_cmds[] = {
	{DTYPE_PINCTRL_SET, &pctrl, DTYPE_PINCTRL_STATE_DEFAULT},
};

static struct pinctrl_cmd_desc lcd_pinctrl_lowpower_cmds[] = {
	{DTYPE_PINCTRL_SET, &pctrl, DTYPE_PINCTRL_STATE_IDLE},
};

static struct pinctrl_cmd_desc lcd_pinctrl_finit_cmds[] = {
	{DTYPE_PINCTRL_PUT, &pctrl, 0},
};

/*******************************************************************************
** LCD GPIO
*/
#define GPIO_LCD_P5V5_ENABLE_NAME	"gpio_lcd_p5v5_enable"
#define GPIO_LCD_N5V5_ENABLE_NAME "gpio_lcd_n5v5_enable"
#define GPIO_LCD_RESET_NAME	"gpio_lcd_reset"
#define GPIO_LCD_ID0_NAME	"gpio_lcd_id0"

#define GPIO_LCD_VDDIO_NAME "gpio_lcd_vddio_switch"

static struct gpio_desc lcd_gpio_vddio_switch_on_cmds[] = {
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_VDDIO_NAME, &g_dev_info.gpio_vddio_ctrl, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_VDDIO_NAME, &g_dev_info.gpio_vddio_ctrl, 1},
};

static struct gpio_desc lcd_gpio_vddio_switch_off_cmds[] = {
	/* lcd vddio gpio down */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_VDDIO_NAME, &g_dev_info.gpio_vddio_ctrl, 0},
	/* set vddio gpio input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_VDDIO_NAME, &g_dev_info.gpio_vddio_ctrl, 0},
	/* free vddio gpio */
	{DTYPE_GPIO_FREE, WAIT_TYPE_US, 0,
		GPIO_LCD_VDDIO_NAME, &g_dev_info.gpio_vddio_ctrl, 0},
};

static struct gpio_desc lcd_gpio_vsp_n_enable_cmds[] = {
	/* AVDD_5.5V */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_P5V5_ENABLE_NAME, &g_dev_info.gpio_p5v5_enable, 0},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_N5V5_ENABLE_NAME, &g_dev_info.gpio_n5v5_enable, 0},
	/* AVDD_5.5V */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_P5V5_ENABLE_NAME, &g_dev_info.gpio_p5v5_enable, 1},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_N5V5_ENABLE_NAME, &g_dev_info.gpio_n5v5_enable, 1},
};

static struct gpio_desc lcd_gpio_vsp_n_disable_cmds[] = {
	/* AVEE_-5.5V gpio disable */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 15,
		GPIO_LCD_N5V5_ENABLE_NAME, &g_dev_info.gpio_n5v5_enable, 0},
	/* AVDD_5.5V gpio disable */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 15,
		GPIO_LCD_P5V5_ENABLE_NAME, &g_dev_info.gpio_p5v5_enable, 0},
	/* AVEE_-5.5V gpio input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 0,
		GPIO_LCD_N5V5_ENABLE_NAME, &g_dev_info.gpio_n5v5_enable, 0},
	/* AVDD_5.5V gpio input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 0,
		GPIO_LCD_P5V5_ENABLE_NAME, &g_dev_info.gpio_p5v5_enable, 0},
	/* AVEE_-5.5V gpio free */
	{DTYPE_GPIO_FREE, WAIT_TYPE_US, 100,
		GPIO_LCD_N5V5_ENABLE_NAME, &g_dev_info.gpio_n5v5_enable, 0},
	/* AVDD_5.5V gpio free */
	{DTYPE_GPIO_FREE, WAIT_TYPE_US, 100,
		GPIO_LCD_P5V5_ENABLE_NAME, &g_dev_info.gpio_p5v5_enable, 0},
};

static struct gpio_desc lcd_gpio_reset_cmds[] = {
	/* reset */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},

	/* reset */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 12,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 12,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 1},
};

static struct gpio_desc lcd_gpio_reset_down_cmds[] = {
	/* lcd reset gpio down */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 8,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	/* lcd reset gpio input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	/* lcd reset free */
	{DTYPE_GPIO_FREE, WAIT_TYPE_US, 100,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
};

static struct gpio_desc lcd_gpio_sleep_request_cmds[] = {
	/* reset */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	/* id0 */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID0_NAME, &g_dev_info.gpio_id0, 0},
};

static struct gpio_desc lcd_gpio_sleep_free_cmds[] = {
	/* reset */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	/* lcd id */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID0_NAME, &g_dev_info.gpio_id0, 0},
};

static struct gpio_desc lcd_gpio_sleep_normal_cmds[] = {
	/* reset */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 12,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_US, 20,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 12,
		GPIO_LCD_RESET_NAME, &g_dev_info.gpio_reset, 1},
	/* lcd id */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID0_NAME, &g_dev_info.gpio_id0, 0},
};

/*******************************************************************************
** ACM
*/
static u32 acm_lut_hue_table[] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
	0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
	0x0070, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007d, 0x007e, 0x007f, 0x0080, 0x0081,
	0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008e, 0x008f, 0x0090, 0x0091, 0x0092,
	0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f, 0x00a0, 0x00a1, 0x00a2,
	0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b0,
	0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bc, 0x00bd, 0x00be,
	0x00bf, 0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd,
	0x00ce, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d6, 0x00d7, 0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc,
	0x00dd, 0x00de, 0x00df, 0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9, 0x00ea, 0x00eb,
	0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa,
	0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff, 0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0107, 0x0108, 0x0109, 0x010a, 0x010b,
	0x010c, 0x010d, 0x010e, 0x010f, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116, 0x0117, 0x0118, 0x0119, 0x011a, 0x011b,
	0x011c, 0x011e, 0x011f, 0x0120, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127, 0x0128, 0x0129, 0x012a, 0x012b, 0x012c,
	0x012d, 0x012e, 0x012f, 0x0130, 0x0131, 0x0132, 0x0134, 0x0135, 0x0136, 0x0137, 0x0138, 0x0139, 0x013a, 0x013b, 0x013c, 0x013d,
	0x013e, 0x013f, 0x0140, 0x0141, 0x0142, 0x0143, 0x0144, 0x0145, 0x0146, 0x0147, 0x0148, 0x0149, 0x014a, 0x014b, 0x014c, 0x014d,
	0x014e, 0x0150, 0x0151, 0x0152, 0x0153, 0x0154, 0x0155, 0x0156, 0x0157, 0x0158, 0x0159, 0x015a, 0x015b, 0x015c, 0x015d, 0x015e,
	0x015f, 0x0160, 0x0161, 0x0162, 0x0163, 0x0164, 0x0165, 0x0166, 0x0167, 0x0168, 0x0169, 0x016a, 0x016b, 0x016c, 0x016d, 0x016e,
	0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x0178, 0x0179, 0x017a, 0x017b, 0x017c, 0x017d, 0x017e, 0x017f,
	0x0180, 0x0181, 0x0182, 0x0183, 0x0184, 0x0185, 0x0186, 0x0187, 0x0188, 0x0189, 0x018a, 0x018b, 0x018c, 0x018d, 0x018e, 0x018f,
	0x0190, 0x0191, 0x0192, 0x0193, 0x0194, 0x0195, 0x0196, 0x0197, 0x0198, 0x0199, 0x019a, 0x019b, 0x019c, 0x019d, 0x019e, 0x019f,
	0x01a0, 0x01a1, 0x01a2, 0x01a3, 0x01a4, 0x01a5, 0x01a6, 0x01a7, 0x01a8, 0x01a9, 0x01aa, 0x01ab, 0x01ac, 0x01ad, 0x01ae, 0x01af,
	0x01b0, 0x01b1, 0x01b2, 0x01b3, 0x01b4, 0x01b5, 0x01b6, 0x01b7, 0x01b8, 0x01b9, 0x01ba, 0x01bb, 0x01bc, 0x01bd, 0x01be, 0x01bf,
	0x01c0, 0x01c1, 0x01c2, 0x01c3, 0x01c4, 0x01c5, 0x01c6, 0x01c7, 0x01c8, 0x01c9, 0x01ca, 0x01cb, 0x01cc, 0x01cd, 0x01ce, 0x01cf,
	0x01d0, 0x01d1, 0x01d2, 0x01d3, 0x01d4, 0x01d5, 0x01d6, 0x01d7, 0x01d8, 0x01d9, 0x01da, 0x01db, 0x01dc, 0x01dd, 0x01de, 0x01df,
	0x01e0, 0x01e1, 0x01e2, 0x01e3, 0x01e4, 0x01e5, 0x01e6, 0x01e7, 0x01e8, 0x01e9, 0x01ea, 0x01eb, 0x01ec, 0x01ed, 0x01ee, 0x01ef,
	0x01f0, 0x01f1, 0x01f2, 0x01f3, 0x01f4, 0x01f5, 0x01f6, 0x01f7, 0x01f8, 0x01f9, 0x01fa, 0x01fb, 0x01fc, 0x01fd, 0x01fe, 0x01ff,
	0x0200, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207, 0x0208, 0x0209, 0x020a, 0x020b, 0x020c, 0x020d, 0x020e, 0x020f,
	0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215, 0x0216, 0x0217, 0x0218, 0x0219, 0x021a, 0x021b, 0x021c, 0x021d, 0x021e, 0x021f,
	0x0220, 0x0221, 0x0222, 0x0223, 0x0224, 0x0225, 0x0226, 0x0227, 0x0228, 0x0229, 0x022a, 0x022b, 0x022c, 0x022d, 0x022e, 0x022f,
	0x0230, 0x0231, 0x0232, 0x0233, 0x0234, 0x0235, 0x0236, 0x0237, 0x0238, 0x0239, 0x023a, 0x023b, 0x023c, 0x023d, 0x023e, 0x023f,
	0x0240, 0x0241, 0x0242, 0x0243, 0x0244, 0x0245, 0x0246, 0x0247, 0x0248, 0x0249, 0x024a, 0x024b, 0x024c, 0x024d, 0x024e, 0x024f,
	0x0250, 0x0251, 0x0252, 0x0253, 0x0254, 0x0255, 0x0256, 0x0257, 0x0258, 0x0259, 0x025a, 0x025b, 0x025c, 0x025d, 0x025e, 0x025f,
	0x0260, 0x0261, 0x0262, 0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268, 0x0269, 0x026a, 0x026b, 0x026c, 0x026d, 0x026e, 0x026f,
	0x0270, 0x0271, 0x0272, 0x0273, 0x0274, 0x0275, 0x0276, 0x0277, 0x0278, 0x0279, 0x027a, 0x027b, 0x027c, 0x027d, 0x027e, 0x027f,
	0x0280, 0x0281, 0x0282, 0x0283, 0x0284, 0x0285, 0x0286, 0x0287, 0x0288, 0x0289, 0x028a, 0x028b, 0x028c, 0x028d, 0x028e, 0x028f,
	0x0290, 0x0290, 0x0291, 0x0292, 0x0293, 0x0294, 0x0295, 0x0296, 0x0297, 0x0298, 0x0299, 0x029a, 0x029b, 0x029c, 0x029d, 0x029e,
	0x029f, 0x02a0, 0x02a1, 0x02a2, 0x02a3, 0x02a4, 0x02a5, 0x02a6, 0x02a7, 0x02a8, 0x02a9, 0x02aa, 0x02ab, 0x02ac, 0x02ad, 0x02ae,
	0x02ae, 0x02af, 0x02b0, 0x02b1, 0x02b2, 0x02b3, 0x02b4, 0x02b5, 0x02b6, 0x02b7, 0x02b8, 0x02b9, 0x02ba, 0x02bb, 0x02bc, 0x02bd,
	0x02be, 0x02bf, 0x02c0, 0x02c1, 0x02c2, 0x02c3, 0x02c4, 0x02c5, 0x02c6, 0x02c7, 0x02c8, 0x02c9, 0x02ca, 0x02cb, 0x02cc, 0x02cd,
	0x02ce, 0x02ce, 0x02cf, 0x02d0, 0x02d1, 0x02d2, 0x02d3, 0x02d4, 0x02d5, 0x02d6, 0x02d7, 0x02d8, 0x02d9, 0x02da, 0x02db, 0x02dc,
	0x02dd, 0x02de, 0x02df, 0x02e0, 0x02e1, 0x02e2, 0x02e3, 0x02e4, 0x02e5, 0x02e6, 0x02e7, 0x02e8, 0x02e9, 0x02ea, 0x02eb, 0x02ec,
	0x02ec, 0x02ed, 0x02ee, 0x02ef, 0x02f0, 0x02f1, 0x02f2, 0x02f3, 0x02f4, 0x02f5, 0x02f6, 0x02f7, 0x02f8, 0x02f9, 0x02fa, 0x02fb,
	0x02fc, 0x02fd, 0x02fe, 0x02ff, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307, 0x0308, 0x0309, 0x030a, 0x030b,
	0x030c, 0x030d, 0x030e, 0x030f, 0x0310, 0x0311, 0x0312, 0x0313, 0x0314, 0x0315, 0x0316, 0x0317, 0x0318, 0x0319, 0x031a, 0x031b,
	0x031c, 0x031e, 0x031f, 0x0320, 0x0321, 0x0322, 0x0323, 0x0324, 0x0325, 0x0326, 0x0327, 0x0328, 0x0329, 0x032a, 0x032b, 0x032c,
	0x032d, 0x032e, 0x032f, 0x0330, 0x0331, 0x0332, 0x0333, 0x0334, 0x0335, 0x0336, 0x0337, 0x0338, 0x0339, 0x033a, 0x033b, 0x033c,
	0x033d, 0x033e, 0x033f, 0x0340, 0x0341, 0x0342, 0x0343, 0x0344, 0x0345, 0x0346, 0x0347, 0x0349, 0x034a, 0x034b, 0x034c, 0x034d,
	0x034e, 0x034f, 0x0350, 0x0351, 0x0352, 0x0353, 0x0354, 0x0355, 0x0356, 0x0357, 0x0358, 0x0359, 0x035a, 0x035b, 0x035c, 0x035d,
	0x035e, 0x0360, 0x0361, 0x0362, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369, 0x036a, 0x036b, 0x036c, 0x036d, 0x036e,
	0x036f, 0x0370, 0x0371, 0x0372, 0x0373, 0x0374, 0x0376, 0x0377, 0x0378, 0x0379, 0x037a, 0x037b, 0x037c, 0x037d, 0x037e, 0x037f,
	0x0380, 0x0381, 0x0382, 0x0383, 0x0384, 0x0385, 0x0386, 0x0387, 0x0388, 0x0389, 0x038a, 0x038b, 0x038c, 0x038d, 0x038e, 0x038f,
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f,
	0x03a0, 0x03a1, 0x03a2, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7, 0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x03ac, 0x03ad, 0x03ae, 0x03af,
	0x03b0, 0x03b1, 0x03b2, 0x03b3, 0x03b4, 0x03b5, 0x03b6, 0x03b7, 0x03b8, 0x03b9, 0x03ba, 0x03bb, 0x03bc, 0x03bd, 0x03be, 0x03bf,
	0x03c0, 0x03c1, 0x03c2, 0x03c3, 0x03c4, 0x03c5, 0x03c6, 0x03c7, 0x03c8, 0x03c9, 0x03ca, 0x03cb, 0x03cc, 0x03cd, 0x03ce, 0x03cf,
	0x03d0, 0x03d1, 0x03d2, 0x03d3, 0x03d4, 0x03d5, 0x03d6, 0x03d7, 0x03d8, 0x03d9, 0x03da, 0x03db, 0x03dc, 0x03dd, 0x03de, 0x03df,
	0x03e0, 0x03e1, 0x03e2, 0x03e3, 0x03e4, 0x03e5, 0x03e6, 0x03e7, 0x03e8, 0x03e9, 0x03ea, 0x03eb, 0x03ec, 0x03ed, 0x03ee, 0x03ef,
	0x03f0, 0x03f1, 0x03f2, 0x03f3, 0x03f4, 0x03f5, 0x03f6, 0x03f7, 0x03f8, 0x03f9, 0x03fa, 0x03fb, 0x03fc, 0x03fd, 0x03fe, 0x03ff,
};

static u32 acm_lut_sata_table[] = {
	0x002b, 0x0030, 0x0034, 0x0038, 0x003d, 0x0042, 0x0046, 0x004a, 0x004f, 0x0050, 0x0052, 0x0053, 0x0054, 0x0056, 0x0057, 0x0059,
	0x005a, 0x0058, 0x0056, 0x0054, 0x0052, 0x0051, 0x004f, 0x004d, 0x004b, 0x0046, 0x0040, 0x003b, 0x0036, 0x0030, 0x002b, 0x0025,
	0x0020, 0x0026, 0x002c, 0x0032, 0x0038, 0x003d, 0x0043, 0x0049, 0x004f, 0x0051, 0x0053, 0x0055, 0x0058, 0x005a, 0x005c, 0x005e,
	0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0061, 0x0061, 0x0061, 0x0061, 0x0062, 0x0062, 0x0062, 0x0063, 0x0064, 0x0064, 0x0064,
	0x0065, 0x0065, 0x0064, 0x0064, 0x0064, 0x0064, 0x0064, 0x0063, 0x0063, 0x0062, 0x0062, 0x0061, 0x0060, 0x0060, 0x005f, 0x005f,
	0x005e, 0x005e, 0x005d, 0x005d, 0x005c, 0x005c, 0x005c, 0x005b, 0x005b, 0x005a, 0x0059, 0x0058, 0x0058, 0x0057, 0x0056, 0x0055,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0058, 0x0059, 0x005a, 0x005b, 0x005b, 0x005b, 0x005b, 0x005c, 0x005c, 0x005c, 0x005c,
	0x005c, 0x005c, 0x005c, 0x005d, 0x005d, 0x005d, 0x005e, 0x005e, 0x005e, 0x005e, 0x005f, 0x005f, 0x0060, 0x0060, 0x0060, 0x0061,
	0x0061, 0x0061, 0x0060, 0x0060, 0x0060, 0x005f, 0x005f, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e,
	0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005e, 0x005d, 0x005d, 0x005d,
	0x005d, 0x005c, 0x005c, 0x005c, 0x005b, 0x005a, 0x005a, 0x005a, 0x0059, 0x0058, 0x0058, 0x0057, 0x0056, 0x0056, 0x0055, 0x0055,
	0x0054, 0x0054, 0x0054, 0x0054, 0x0054, 0x0054, 0x0054, 0x0054, 0x0054, 0x0053, 0x0052, 0x0052, 0x0051, 0x0050, 0x0050, 0x004f,
	0x004e, 0x004d, 0x004c, 0x004b, 0x004a, 0x0048, 0x0047, 0x0046, 0x0045, 0x0043, 0x0042, 0x0040, 0x003e, 0x003d, 0x003b, 0x003a,
	0x0038, 0x0036, 0x0033, 0x0030, 0x002e, 0x002c, 0x0029, 0x0026, 0x0024, 0x0020, 0x001b, 0x0016, 0x0012, 0x000e, 0x0009, 0x0004,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000b, 0x000d, 0x000f,
	0x0011, 0x0013, 0x0015, 0x0017, 0x0018, 0x001a, 0x001c, 0x001e, 0x0020, 0x0021, 0x0023, 0x0024, 0x0026, 0x0027, 0x0028, 0x002a,
};

static u32 acm_lut_satr_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00fd, 0x00fc, 0x00fa, 0x00f9,
	0x00f7, 0x00f7, 0x00f8, 0x00f8, 0x00f8, 0x00f9, 0x00f9, 0x00fa, 0x00fa, 0x00fa, 0x00fb, 0x00fb, 0x00fb, 0x00fc, 0x00fc, 0x00fc,
	0x00fd, 0x00fd, 0x00fe, 0x00fe, 0x00fe, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff,
	0x00ff, 0x00ff, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0003, 0x0006, 0x0009, 0x000c,
	0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e,
	0x000e, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000c, 0x000c, 0x000b, 0x000b, 0x000a, 0x000a, 0x0009, 0x0009, 0x0008,
	0x0008, 0x0007, 0x0007, 0x0006, 0x0006, 0x0005, 0x0005, 0x0004, 0x0004, 0x0003, 0x0003, 0x0002, 0x0002, 0x0001, 0x0001, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0003, 0x0006, 0x0009, 0x000c,
	0x000f, 0x000f, 0x000f, 0x000f, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011,
	0x0011, 0x0011, 0x0011, 0x0012, 0x0012, 0x0012, 0x0012, 0x0011, 0x0011, 0x0010, 0x000f, 0x000e, 0x000e, 0x000d, 0x000c, 0x000c,
	0x000b, 0x000a, 0x0009, 0x0009, 0x0008, 0x0007, 0x0006, 0x0006, 0x0005, 0x0004, 0x0004, 0x0003, 0x0002, 0x0001, 0x0001, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0003, 0x0005, 0x0006,
	0x0008, 0x0008, 0x0008, 0x0008, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000a, 0x000a, 0x000a, 0x000a, 0x000a,
	0x000a, 0x000a, 0x000a, 0x000b, 0x000b, 0x000b, 0x000b, 0x000b, 0x000a, 0x000a, 0x0009, 0x0009, 0x0008, 0x0008, 0x0007, 0x0007,
	0x0007, 0x0006, 0x0006, 0x0005, 0x0005, 0x0004, 0x0004, 0x0004, 0x0003, 0x0003, 0x0002, 0x0002, 0x0001, 0x0001, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0003, 0x0005, 0x0008, 0x000a,
	0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e,
	0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000e, 0x000d, 0x000d, 0x000c, 0x000c, 0x000b, 0x000b, 0x000a, 0x000a, 0x0009,
	0x0008, 0x0008, 0x0007, 0x0007, 0x0006, 0x0006, 0x0005, 0x0004, 0x0004, 0x0003, 0x0003, 0x0002, 0x0002, 0x0001, 0x0001, 0x0000,
	0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003, 0x0005, 0x0007, 0x0009, 0x000b,
	0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000d, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c, 0x000c,
	0x000c, 0x000b, 0x000b, 0x000b, 0x000b, 0x000b, 0x000b, 0x000b, 0x000a, 0x000a, 0x0009, 0x0009, 0x0008, 0x0008, 0x0007, 0x0007,
	0x0007, 0x0006, 0x0006, 0x0005, 0x0005, 0x0004, 0x0004, 0x0004, 0x0003, 0x0003, 0x0002, 0x0002, 0x0001, 0x0001, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0002, 0x0004, 0x0005, 0x0007,
	0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000a, 0x000a, 0x000a, 0x000a,
	0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x0009, 0x0009, 0x0008, 0x0008, 0x0008, 0x0007, 0x0007, 0x0006,
	0x0006, 0x0006, 0x0005, 0x0005, 0x0004, 0x0004, 0x0004, 0x0003, 0x0003, 0x0002, 0x0002, 0x0002, 0x0001, 0x0001, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff,
	0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

/*******************************************************************************
** 2d sharpness
*/
static sharp2d_t sharp2d_table = {
	.sharp_en = 1, .sharp_mode = 0,
	.flt0_c0 = 0xf0, .flt0_c1 = 0xf0, .flt0_c2 = 0xf0,
	.flt1_c0 = 0x1c, .flt1_c1 = 0x1c, .flt1_c2 = 0xf0,
	.flt2_c0 = 0x20, .flt2_c1 = 0x1c, .flt2_c2 = 0xf0,
	.ungain = 0x64, .ovgain = 0x64,
	.lineamt1 = 0xff, .linedeten = 0x1, .linethd2 = 0x0, .linethd1 = 0x1f,
	.sharpthd1 = 0x10, .sharpthd1mul = 0x100, .sharpamt1 = 0x40,
	.edgethd1 = 0xff, .edgethd1mul = 0xfff, .edgeamt1 = 0xfff,
};

/*******************************************************************************
**
*/
static int mipi_jdi_panel_set_fastboot(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	// lcd pinctrl normal
	pinctrl_cmds_tx(pdev, lcd_pinctrl_normal_cmds,
		ARRAY_SIZE(lcd_pinctrl_normal_cmds));

	// backlight on
	hisi_lcd_backlight_on(pdev);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return 0;
}

static u8 esd_recovery_times = 0;
static int mipi_jdi_panel_on(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	char __iomem *mipi_dsi0_base = NULL;
	int error = 0;
#if defined (CONFIG_HUAWEI_DSM)
	static struct lcd_reg_read_t lcd_status_reg[] = {
		{0x0A, 0x98, 0xFF, "lcd power state"},
	};
#endif

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_INFO("fb%d, +!\n", hisifd->index);

	pinfo = &(hisifd->panel_info);
	mipi_dsi0_base = hisifd->mipi_dsi0_base;

	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		g_debug_enable = BACKLIGHT_PRINT_TIMES;
		LOG_JANK_D(JLID_KERNEL_LCD_POWER_ON, "%s", "JL_KERNEL_LCD_POWER_ON");
		if (false == gesture_func && !g_debug_enable_lcd_sleep_in) {
			/* vout17 3.3v on */
			vcc_cmds_tx(pdev, lcd_vcc_lcdanalog_enable_cmds,
				ARRAY_SIZE(lcd_vcc_lcdanalog_enable_cmds));

			/* 1.8v on */
			gpio_cmds_tx(lcd_gpio_vddio_switch_on_cmds, \
				ARRAY_SIZE(lcd_gpio_vddio_switch_on_cmds));

			/* vsp/vsn on */
			gpio_cmds_tx(lcd_gpio_vsp_n_enable_cmds, \
				ARRAY_SIZE(lcd_gpio_vsp_n_enable_cmds));
		} else {
			HISI_FB_INFO("power on (gesture_func:%d)\n", gesture_func);

			gpio_cmds_tx(lcd_gpio_sleep_request_cmds, \
					ARRAY_SIZE(lcd_gpio_sleep_request_cmds));

			gpio_cmds_tx(lcd_gpio_sleep_normal_cmds, \
					ARRAY_SIZE(lcd_gpio_sleep_normal_cmds));
		}

		pinfo->lcd_init_step = LCD_INIT_MIPI_LP_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_LP_SEND_SEQUENCE) {
#ifdef CONFIG_HUAWEI_TS
		if ((g_lcd_control_tp_power || pinfo->esd_recover_step == LCD_ESD_RECOVER_POWER_ON)
			&& !g_debug_enable_lcd_sleep_in) {
			error = ts_power_control_notify(TS_RESUME_DEVICE, NO_SYNC_TIMEOUT);
			if (error)
				HISI_FB_ERR("ts resume device err\n");
		}
#endif
		if (false == gesture_func && !g_debug_enable_lcd_sleep_in) {
			/* time of mipi on to lcd reset on shoud be larger than 10ms */
			msleep(12);

			/* lcd reset */
			gpio_cmds_tx(lcd_gpio_reset_cmds, \
				ARRAY_SIZE(lcd_gpio_reset_cmds));
		} else {
			msleep(50);
			HISI_FB_INFO("lp send sequence (gesture_func:%d)\n", gesture_func);
		}

		if ((pinfo->bl_set_type & BL_SET_BY_BLPWM) || (pinfo->bl_set_type & BL_SET_BY_SH_BLPWM)) {
			mipi_dsi_cmds_tx(pwm_out_on_cmds, \
				ARRAY_SIZE(pwm_out_on_cmds), mipi_dsi0_base);
		}

		if (pinfo->dirty_region_updt_support) {
			// NT35695 partial update sequence
			mipi_dsi_cmds_tx(lcd_partial_updt_cmds, \
				ARRAY_SIZE(lcd_partial_updt_cmds), mipi_dsi0_base);
		}

		// lcd display on sequence
		mipi_dsi_cmds_tx(lcd_display_on_cmds, \
			ARRAY_SIZE(lcd_display_on_cmds), mipi_dsi0_base);

		g_cabc_mode = 2;

#if defined (CONFIG_HUAWEI_DSM)
		if (ESD_RECOVER_STATE_START != hisifd->esd_recover_state){
			panel_check_status_and_report_by_dsm(lcd_status_reg, \
				ARRAY_SIZE(lcd_status_reg), mipi_dsi0_base);

			HISI_FB_INFO("recovery:%u, esd_recovery_times:%u\n", lcd_status_reg[0].recovery, esd_recovery_times);
			if (lcd_status_reg[0].recovery && esd_recovery_times < 3){
				HISI_FB_ERR("start recovery!!!\n");
				hisifd->esd_recover_state = ESD_RECOVER_STATE_START;
				if (hisifd->esd_ctrl.esd_check_wq) {
					queue_work(hisifd->esd_ctrl.esd_check_wq, &(hisifd->esd_ctrl.esd_check_work));
				}
				esd_recovery_times++;
			}
		}
#endif
		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE) {
#ifdef CONFIG_HUAWEI_TS
		if ((g_lcd_control_tp_power || pinfo->esd_recover_step == LCD_ESD_RECOVER_POWER_ON)
			&& !g_debug_enable_lcd_sleep_in) {
			error = ts_power_control_notify(TS_AFTER_RESUME, NO_SYNC_TIMEOUT);
			if (error)
				HISI_FB_ERR("ts after resume err\n");
		}
#endif
		otm2503b_send_panel_display_on = true;
	} else {
		HISI_FB_ERR("failed to init lcd!\n");
	}

	/* backlight on */
	hisi_lcd_backlight_on(pdev);

	HISI_FB_INFO("fb%d, -!\n", hisifd->index);

	return 0;
}

static int mipi_jdi_panel_off(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	int error = 0;
#if defined (CONFIG_HUAWEI_DSM)
	static struct lcd_reg_read_t lcd_status_reg[] = {
		{0x05, 0x00, 0xFF, "dsi error number"},
	};
#endif

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_INFO("fb%d, +!\n", hisifd->index);

	pinfo = &(hisifd->panel_info);

	if (pinfo->lcd_uninit_step == LCD_UNINIT_MIPI_HS_SEND_SEQUENCE) {
		LOG_JANK_D(JLID_KERNEL_LCD_POWER_OFF, "%s", "JL_KERNEL_LCD_POWER_OFF");
		otm2503b_send_panel_display_on = false;
#if defined (CONFIG_HUAWEI_DSM)
		//panel_check_status_and_report_by_dsm(lcd_status_reg, \
			//ARRAY_SIZE(lcd_status_reg), hisifd->mipi_dsi0_base);
#endif

		/* backlight off */
		hisi_lcd_backlight_off(pdev);

		// lcd display off sequence
		mipi_dsi_cmds_tx(lcd_display_off_cmds, \
			ARRAY_SIZE(lcd_display_off_cmds), hisifd->mipi_dsi0_base);

#ifdef CONFIG_HUAWEI_TS
		if ((g_lcd_control_tp_power || pinfo->esd_recover_step == LCD_ESD_RECOVER_POWER_OFF)
			&& !hisifd->fb_shutdown && !g_debug_enable_lcd_sleep_in) {
			error = ts_power_control_notify(TS_BEFORE_SUSPEND, SHORT_SYNC_TIMEOUT);
			if (error)
				HISI_FB_ERR("ts before suspend err\n");

			error = ts_power_control_notify(TS_SUSPEND_DEVICE, NO_SYNC_TIMEOUT);
			if (error)
				HISI_FB_ERR("ts suspend device err\n");
		}
#endif
		if (((false == gesture_func) || hisifd->fb_shutdown) && !g_debug_enable_lcd_sleep_in) {
		}
		pinfo->lcd_uninit_step = LCD_UNINIT_MIPI_LP_SEND_SEQUENCE;
	} else if (pinfo->lcd_uninit_step == LCD_UNINIT_MIPI_LP_SEND_SEQUENCE) {
		pinfo->lcd_uninit_step = LCD_UNINIT_POWER_OFF;
	} else if (pinfo->lcd_uninit_step == LCD_UNINIT_POWER_OFF) {
		if (((false == gesture_func) || hisifd->fb_shutdown) && !g_debug_enable_lcd_sleep_in) {
			/* need 5ms between lp11 and reset */
			msleep(5);
			/* Wait for TP FW Upgrade Success*/
			if (g_ts_data.chip_data->is_parade_solution)
			{
				ts_check_bootup_upgrade();
			}
			/* lcd pinctrl lowpower */
			pinctrl_cmds_tx(pdev, lcd_pinctrl_lowpower_cmds,
				ARRAY_SIZE(lcd_pinctrl_lowpower_cmds));

			/* lcd reset down */
			gpio_cmds_tx(lcd_gpio_reset_down_cmds, \
				ARRAY_SIZE(lcd_gpio_reset_down_cmds));

			/* vsp/vsn off */
			gpio_cmds_tx(lcd_gpio_vsp_n_disable_cmds, \
				ARRAY_SIZE(lcd_gpio_vsp_n_disable_cmds));

			/* 1.8v off */
			gpio_cmds_tx(lcd_gpio_vddio_switch_off_cmds, \
				ARRAY_SIZE(lcd_gpio_vddio_switch_off_cmds));

			/* vout17 3.3v off */
			vcc_cmds_tx(pdev, lcd_vcc_lcdanalog_disable_cmds,
				ARRAY_SIZE(lcd_vcc_lcdanalog_disable_cmds));
		} else {
			HISI_FB_INFO("display_off (gesture_func:%d)\n", gesture_func);

			/* lcd gpio free */
			gpio_cmds_tx(lcd_gpio_sleep_free_cmds, \
				ARRAY_SIZE(lcd_gpio_sleep_free_cmds));
		}

#ifdef CONFIG_HUAWEI_TS
		if (hisifd->fb_shutdown) {
			ts_thread_stop_notify();
		}
#endif

		checksum_enable_ctl = false;
		esd_recovery_times = 0;
	} else {
		HISI_FB_ERR("failed to uninit lcd!\n");
	}

	HISI_FB_INFO("fb%d, -!\n", hisifd->index);

	return 0;
}

static int mipi_jdi_panel_remove(struct platform_device *pdev)
{
	struct hisi_fb_data_type *hisifd = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return 0;
	}

	hisifd = platform_get_drvdata(pdev);
	if (!hisifd) {
		return 0;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	vcc_cmds_tx(pdev, lcd_vcc_lcdanalog_finit_cmds,
		ARRAY_SIZE(lcd_vcc_lcdanalog_finit_cmds));

	// lcd pinctrl finit
	pinctrl_cmds_tx(pdev, lcd_pinctrl_finit_cmds,
		ARRAY_SIZE(lcd_pinctrl_finit_cmds));

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);
	return 0;
}

static int mipi_jdi_panel_set_backlight(struct platform_device *pdev, uint32_t bl_level)
{
	int ret = 0;
	char __iomem *mipi_dsi0_base = NULL;
	struct hisi_fb_data_type *hisifd = NULL;
	static char last_bl_level=0;
	char bl_level_adjust[2] = {
		0x51,
		0x00,
	};

	struct dsi_cmd_desc lcd_bl_level_adjust[] = {
		{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
			sizeof(bl_level_adjust), bl_level_adjust},
	};

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	HISI_FB_DEBUG("fb%d, bl_level=%d.\n", hisifd->index, bl_level);

	if (unlikely(g_debug_enable)) {
		HISI_FB_INFO("Set backlight to %d. (remain times of backlight print: %d)\n", hisifd->bl_level, g_debug_enable);
		if (g_debug_enable == BACKLIGHT_PRINT_TIMES)
			LOG_JANK_D(JLID_KERNEL_LCD_BACKLIGHT_ON, "JL_KERNEL_LCD_BACKLIGHT_ON,%u", hisifd->bl_level);

		g_debug_enable = (g_debug_enable > 0) ? (g_debug_enable - 1) : 0;
	}

	if (true == otm2503b_send_panel_display_on){
		HISI_FB_INFO("Set display on before backlight set\n");
		hisifb_set_vsync_activate_state(hisifd, true);
		hisifb_activate_vsync(hisifd);
		// lcd display on sequence
		mipi_dsi_cmds_tx(lcd_display_on_cmd_in_backlight, \
			ARRAY_SIZE(lcd_display_on_cmd_in_backlight), hisifd->mipi_dsi0_base);
		hisifb_set_vsync_activate_state(hisifd, false);
		hisifb_deactivate_vsync(hisifd);
		otm2503b_send_panel_display_on = false;
	}

	if (!bl_level) {
		HISI_FB_INFO("Set backlight to 0 !!!\n");
	}

	if (hisifd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		ret = hisi_pwm_set_backlight(hisifd, bl_level);
	} else if (hisifd->panel_info.bl_set_type & BL_SET_BY_BLPWM) {
		ret = hisi_blpwm_set_backlight(hisifd, bl_level);
	} else if (hisifd->panel_info.bl_set_type & BL_SET_BY_SH_BLPWM) {
		ret = hisi_sh_blpwm_set_backlight(hisifd, bl_level);
	} else if (hisifd->panel_info.bl_set_type & BL_SET_BY_MIPI) {
		mipi_dsi0_base = hisifd->mipi_dsi0_base;

		bl_level_adjust[1] = bl_level  * 255 / hisifd->panel_info.bl_max;
		if (last_bl_level != bl_level_adjust[1]){
			last_bl_level = bl_level_adjust[1];
			mipi_dsi_cmds_tx(lcd_bl_level_adjust, \
				ARRAY_SIZE(lcd_bl_level_adjust), mipi_dsi0_base);
		}
	} else {
		HISI_FB_ERR("fb%d, not support this bl_set_type(%d)!\n",
			hisifd->index, hisifd->panel_info.bl_set_type);
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}


/******************************************************************************/
static ssize_t mipi_jdi_panel_lcd_model_show(struct platform_device *pdev,
	char *buf)
{
	struct hisi_fb_data_type *hisifd = NULL;
	ssize_t ret = 0;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = snprintf(buf, PAGE_SIZE, "JDI_OTM2503B 5.5' CMD TFT\n");

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static ssize_t mipi_jdi_panel_lcd_cabc_mode_show(struct platform_device *pdev,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_cabc_mode);
}

static ssize_t mipi_jdi_panel_lcd_cabc_mode_store(struct platform_device *pdev,
	const char *buf, size_t count)
{
	int ret = 0;
	unsigned long val = 0;
	int flag=-1;
	struct hisi_fb_data_type *hisifd = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	mipi_dsi0_base =hisifd->mipi_dsi0_base;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
               return ret;

	flag=(int)val;
	if (flag==CABC_OFF ){
              g_cabc_mode=0;
              mipi_dsi_cmds_tx(lg_cabc_off_cmds, \
                               ARRAY_SIZE(lg_cabc_off_cmds),\
                               mipi_dsi0_base);
	}else  if (flag==CABC_UI_MODE) {
              g_cabc_mode=1;
              mipi_dsi_cmds_tx(lg_cabc_ui_on_cmds, \
                               ARRAY_SIZE(lg_cabc_ui_on_cmds),\
                               mipi_dsi0_base);
	} else if (flag==CABC_STILL_MODE ){
              g_cabc_mode=2;
              mipi_dsi_cmds_tx(lg_cabc_still_on_cmds, \
                               ARRAY_SIZE(lg_cabc_still_on_cmds),\
                               mipi_dsi0_base);
	}else if (flag==CABC_MOVING_MODE ){
              g_cabc_mode=3;
              mipi_dsi_cmds_tx(lg_cabc_moving_on_cmds, \
                               ARRAY_SIZE(lg_cabc_moving_on_cmds),\
                               mipi_dsi0_base);
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return snprintf((char *)buf, PAGE_SIZE, "%d\n", g_cabc_mode);
}

static ssize_t mipi_jdi_panel_lcd_sleep_ctrl_show(struct platform_device *pdev, char *buf)
{
	ssize_t ret = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	ret = snprintf(buf, PAGE_SIZE, "enable_lcd_sleep_in=%d, pinfo->lcd_adjust_support=%d\n",
		g_debug_enable_lcd_sleep_in, hisifd->panel_info.lcd_adjust_support);
	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static ssize_t mipi_jdi_panel_lcd_sleep_ctrl_store(struct platform_device *pdev, /*char *buf,*/
	const char *buf, size_t count)
{
	ssize_t ret = 0;
	unsigned long val = 0;
	struct hisi_fb_data_type *hisifd = NULL;

	ret = strict_strtoul(buf, 0, &val);
	if (ret) {
		HISI_FB_ERR("strict_strtoul error, buf=%s", buf);
		return ret;
	}

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}


	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	if (hisifd->panel_info.lcd_adjust_support) {
		g_debug_enable_lcd_sleep_in = val;
	}

	if (g_debug_enable_lcd_sleep_in == 2) {
		HISI_FB_INFO("LCD power off and Touch goto sleep\n");
		g_tp_power_ctrl = 1;	//used for pt  current test, tp sleep
	} else {
		HISI_FB_INFO("g_debug_enable_lcd_sleep_in is %d\n", g_debug_enable_lcd_sleep_in);
		g_tp_power_ctrl = 0;	//used for pt  current test, tp power off
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

/*for esd check*/
static int mipi_jdi_panel_check_esd(struct platform_device* pdev)
{
	int ret = 0, dsm_ret = 0;
	struct hisi_fb_data_type* hisifd = NULL;
	uint32_t read_value[1] = {0};
	uint32_t expected_value[1] = {0x9c};
	uint32_t read_mask[1] = {0xff};
	char* reg_name[1] = {"power mode"};
	char lcd_reg_0a[] = {0x0a};

	struct dsi_cmd_desc lcd_check_reg[] = {
		{DTYPE_DCS_READ, 0, 10, WAIT_TYPE_US,
			sizeof(lcd_reg_0a), lcd_reg_0a},
	};

	struct mipi_dsi_read_compare_data data = {
		.read_value = read_value,
		.expected_value = expected_value,
		.read_mask = read_mask,
		.reg_name = reg_name,
		.log_on = 0,
		.cmds = lcd_check_reg,
		.cnt = ARRAY_SIZE(lcd_check_reg),
	};

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = (struct hisi_fb_data_type*)platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	if (NULL == hisifd->mipi_dsi0_base) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	ret = mipi_dsi_read_compare(&data, hisifd->mipi_dsi0_base);
#if defined (CONFIG_HUAWEI_DSM)
	if (ret) {
		HISI_FB_ERR("ESD ERROR:ret = %d\n", ret);
		dsm_ret = dsm_client_ocuppy(lcd_dclient);
		if ( !dsm_ret ) {
			dsm_client_record(lcd_dclient, "ESD ERROR:ret = %d\n", ret);
			dsm_client_notify(lcd_dclient, DSM_LCD_ESD_RECOVERY_NO);
		}else{
			HISI_FB_ERR("dsm_client_ocuppy ERROR:retVal = %d\n", ret);
		}
	}
#endif
	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

#define LCD_CMD_NAME_MAX 100
static char lcd_cmd_now[LCD_CMD_NAME_MAX] = {0};
static ssize_t mipi_jdi_panel_lcd_test_config_show(struct platform_device *pdev,
	char *buf)
{
	/* backlight self test */
	if (!strncmp(lcd_cmd_now, "RUNNINGTEST10", strlen("RUNNINGTEST10"))) {
		return snprintf(buf, PAGE_SIZE, "BL_OPEN_SHORT");
	} else if (!strncmp(lcd_cmd_now, "BL_OPEN_SHORT", strlen("BL_OPEN_SHORT"))) {
		return snprintf(buf, PAGE_SIZE, "/sys/class/lm36923/lm36923/self_test");
	/* input invaild */
	} else {
		return snprintf(buf, PAGE_SIZE, "INVALID");
	}
}

static ssize_t mipi_jdi_panel_lcd_test_config_store(struct platform_device *pdev,
	const char *buf, size_t count)
{
	struct hisi_fb_data_type *hisifd = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	if (NULL == hisifd->mipi_dsi0_base) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	mipi_dsi0_base =hisifd->mipi_dsi0_base;

	if (strlen(buf) < LCD_CMD_NAME_MAX) {
		memcpy(lcd_cmd_now, buf, strlen(buf) + 1);
		HISI_FB_INFO("current test cmd:%s\n", lcd_cmd_now);
	} else {
		memcpy(lcd_cmd_now, "INVALID", strlen("INVALID") + 1);
		HISI_FB_INFO("invalid test cmd:%s\n", lcd_cmd_now);
	}

	return count;
}

static int g_support_mode = 0;
static ssize_t mipi_jdi_panel_lcd_support_mode_show(struct platform_device *pdev,
	char *buf)
{
	struct hisi_fb_data_type *hisifd = NULL;
	ssize_t ret = 0;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", g_support_mode);

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static ssize_t mipi_jdi_panel_lcd_support_mode_store(struct platform_device *pdev,
	const char *buf, size_t count)
{
	int ret = 0;
	unsigned long val = 0;
	int flag = -1;
	struct hisi_fb_data_type *hisifd = NULL;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
               return ret;

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	flag = (int)val;

	g_support_mode = flag;

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return snprintf((char *)buf, PAGE_SIZE, "%d\n", g_support_mode);
}

static ssize_t mipi_jdi_panel_lcd_support_checkmode_show(struct platform_device *pdev,
	char *buf)
{
	struct hisi_fb_data_type *hisifd = NULL;
	ssize_t ret = 0;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("NULL Pointer\n");
		return -EINVAL;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	ret = snprintf(buf, PAGE_SIZE, "bl_open_short:1\n");

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

static ssize_t mipi_lcd_panel_info_show(struct platform_device *pdev,
	char *buf)
{
	struct hisi_fb_data_type *hisifd = NULL;
	ssize_t ret = 0;

	if (NULL == pdev) {
		HISI_FB_ERR("pdev NULL pointer\n");
		return 0;
	};
	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd) {
		HISI_FB_ERR("hisifd NULL pointer\n");
		return 0;
	}

	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);
	if (buf) {
		ret = snprintf(buf, PAGE_SIZE, "blmax:%u,blmin:%u,lcdtype:%s,\n",
				hisifd->panel_info.bl_max, hisifd->panel_info.bl_min, "LCD");
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);

	return ret;
}

#if OTM2503B_FPS
static int mipi_jdi_panel_lcd_fps_scence_handle(struct platform_device *pdev, uint32_t scence)
{
	struct hisi_fb_data_type *hisifd = NULL;
	struct hisi_panel_info *pinfo = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	hisifd = platform_get_drvdata(pdev);
	BUG_ON(hisifd == NULL);
	pinfo = &(hisifd->panel_info);
	mipi_dsi0_base = hisifd->mipi_dsi0_base;

	switch(scence){
		case LCD_FPS_SCENCE_NORMAL:
			pinfo->fps_updt = LCD_FPS_60;
			HISI_FB_DEBUG("scence:%u is LCD_FPS_SCENCE_NORMAL, framerate is 60fps\n", scence);
			break;
		case LCD_FPS_SCENCE_IDLE:
			pinfo->fps_updt = LCD_FPS_30;
			HISI_FB_DEBUG("scence:%u is LCD_FPS_SCENCE_IDLE, framerate is 30fps\n", scence);
			break;
		case LCD_FPS_SCENCE_VIDEO:
			HISI_FB_INFO("disable drf\n");
			hisifb_set_vsync_activate_state(hisifd, true);
			hisifb_activate_vsync(hisifd);
			/* set mipi in lp mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);
			mipi_dsi_cmds_tx(otm2503b_disable_dfr,
					ARRAY_SIZE(otm2503b_disable_dfr), mipi_dsi0_base);
			/* set mipi in hs mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 1, 24);
			hisifb_set_vsync_activate_state(hisifd, false);
			hisifb_deactivate_vsync(hisifd);
			break;
		case LCD_FPS_SCENCE_GAME:
			HISI_FB_INFO("enable drf\n");
			hisifb_set_vsync_activate_state(hisifd, true);
			hisifb_activate_vsync(hisifd);
			/* set mipi in lp mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);
			mipi_dsi_cmds_tx(otm2503b_enable_dfr,
					ARRAY_SIZE(otm2503b_enable_dfr), mipi_dsi0_base);
			/* set mipi in hs mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 1, 24);
			hisifb_set_vsync_activate_state(hisifd, false);
			hisifb_deactivate_vsync(hisifd);
			break;
		case LCD_FPS_SCENCE_IDLE_FORCE_ENABLE:
			pinfo->fps_updt = LCD_FPS_30;
			pinfo->fps_updt_panel_only = 1;
			HISI_FB_INFO("scence is LCD_FPS_SCENCE_IDLE_FORCE_ENABLE, framerate is 30fps\n");
			break;
		case LCD_FPS_SCENCE_IDLE_FORCE_DISABLE:
			pinfo->fps_updt = LCD_FPS_60;
			pinfo->fps_updt_panel_only = 0;
			HISI_FB_INFO("scence is LCD_FPS_SCENCE_IDLE_FORCE_DISABLE, framerate is 60fps\n");
			break;
		default:
			pinfo->fps_updt = LCD_FPS_60;
			HISI_FB_DEBUG("scence:%u is LCD_FPS_SCENCE_NORMAL, framerate is 60fps\n", scence);
			break;
	}

	return 0;
}

static int mipi_jdi_panel_lcd_fps_updt_handle(struct platform_device *pdev){
	struct hisi_fb_data_type *hisifd = NULL;
	char __iomem *mipi_dsi0_base = NULL;
	struct hisi_panel_info *pinfo = NULL;
	int switch_mode = 0;

	if (NULL == pdev) {
		HISI_FB_ERR("NULL Pointer!\n");
		return 0;
	}

	hisifd = platform_get_drvdata(pdev);
	if (NULL == hisifd){
		HISI_FB_ERR("NULL Pointer!\n");
		return 0;
	}
	mipi_dsi0_base = hisifd->mipi_dsi0_base;

	pinfo = &(hisifd->panel_info);
	if (NULL == pinfo){
		HISI_FB_ERR("NULL Pointer!\n");
		return 0;
	}
	HISI_FB_DEBUG("fb%d, +.\n", hisifd->index);

	switch (pinfo->fps_updt){
		case LCD_FPS_30:
			HISI_FB_INFO("lcd switch %d fps\n", LCD_FPS_30);
			hisifb_set_vsync_activate_state(hisifd, true);
			hisifb_activate_vsync(hisifd);
			/* set mipi in lp mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);
			mipi_dsi_cmds_tx(otm2503_fps_to_30,
					ARRAY_SIZE(otm2503_fps_to_30), mipi_dsi0_base);
			/* set mipi in hs mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 1, 24);
			hisifb_set_vsync_activate_state(hisifd, false);
			hisifb_deactivate_vsync(hisifd);
			break;
		case LCD_FPS_60:
			HISI_FB_INFO("lcd switch %d fps\n", LCD_FPS_60);
			hisifb_set_vsync_activate_state(hisifd, true);
			hisifb_activate_vsync(hisifd);
			/* set mipi in lp mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x7f, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0xf, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x1, 1, 24);
			mipi_dsi_cmds_tx(otm2503_fps_to_60,
					ARRAY_SIZE(otm2503_fps_to_60), mipi_dsi0_base);
			/* set mipi in hs mode */
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 7, 8);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 4, 16);
			set_reg(mipi_dsi0_base + MIPIDSI_CMD_MODE_CFG_OFFSET, 0x0, 1, 24);
			hisifb_set_vsync_activate_state(hisifd, false);
			hisifb_deactivate_vsync(hisifd);
			break;
		default:
			HISI_FB_ERR("lcd unknown attr\n");
			break;
	}

	HISI_FB_DEBUG("fb%d, -.\n", hisifd->index);
	return 0;
}
#endif

static struct hisi_panel_info g_panel_info = {0};
static struct hisi_fb_panel_data g_panel_data = {
	.panel_info = &g_panel_info,
	.set_fastboot = mipi_jdi_panel_set_fastboot,
	.on = mipi_jdi_panel_on,
	.off = mipi_jdi_panel_off,
	.remove = mipi_jdi_panel_remove,
	.set_backlight = mipi_jdi_panel_set_backlight,
	.lcd_model_show = mipi_jdi_panel_lcd_model_show,
	.lcd_sleep_ctrl_show = mipi_jdi_panel_lcd_sleep_ctrl_show,
	.lcd_sleep_ctrl_store = mipi_jdi_panel_lcd_sleep_ctrl_store,
	.panel_info_show = mipi_lcd_panel_info_show,
	.lcd_support_checkmode_show = mipi_jdi_panel_lcd_support_checkmode_show,
	.lcd_test_config_show = mipi_jdi_panel_lcd_test_config_show,
	.lcd_test_config_store = mipi_jdi_panel_lcd_test_config_store,
	.lcd_support_mode_show = mipi_jdi_panel_lcd_support_mode_show,
	.lcd_support_mode_store = mipi_jdi_panel_lcd_support_mode_store,
	.esd_handle = mipi_jdi_panel_check_esd,
#if OTM2503B_FPS
	.lcd_fps_scence_handle = mipi_jdi_panel_lcd_fps_scence_handle,
	.lcd_fps_updt_handle = mipi_jdi_panel_lcd_fps_updt_handle,
#endif
	.lcd_cabc_mode_show = mipi_jdi_panel_lcd_cabc_mode_show,
	.lcd_cabc_mode_store = mipi_jdi_panel_lcd_cabc_mode_store,
#if 0
	.lcd_check_reg = mipi_jdi_panel_lcd_check_reg_show,
	.lcd_mipi_detect = mipi_jdi_panel_lcd_mipi_detect_show,
	.lcd_gram_check_show = mipi_jdi_panel_lcd_gram_check_show,
	.lcd_gram_check_store = mipi_jdi_panel_lcd_gram_check_store,
	.set_display_region = mipi_jdi_panel_set_display_region,
	.set_display_resolution = NULL,
	.sharpness2d_table_store = mipi_jdi_panel_sharpness2d_table_store,
	.sharpness2d_table_show = mipi_jdi_panel_sharpness2d_table_show,
#endif
};
static int mipi_jdi_get_dts_config(void){
	struct device_node *np = NULL;
	int ret = 0;
	const char *bl_ic_name;

	HISI_FB_INFO("%s +\n", __func__);
	memset(&g_dev_info, 0, sizeof(struct jdi_otm2503b_device));

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_JDI_OTM2503B_5p5);
	if (!np) {
		HISI_FB_ERR("not found device node %s!\n", DTS_COMP_JDI_OTM2503B_5p5);
		return JDI_NT35696_ERR;
	}

	//"lcd-bl-type"
	ret = of_property_read_u32(np, LCD_BL_TYPE_NAME, &g_dev_info.bl_type);
	if (ret) {
		HISI_FB_ERR("get lcd_bl_type failed!\n");
		g_dev_info.bl_type = BL_SET_BY_MIPI;
	}

	//"lcd-bl-ic-name"
	ret = of_property_read_string_index(np, "lcd-bl-ic-name", 0, &bl_ic_name);
	if (ret) {
		snprintf(g_dev_info.bl_ic_name_buf, sizeof(g_dev_info.bl_ic_name_buf), "INVALID");
	} else {
		snprintf(g_dev_info.bl_ic_name_buf, sizeof(g_dev_info.bl_ic_name_buf), "%s", bl_ic_name);
	}

	//gpio 037
	g_dev_info.gpio_n5v5_enable = of_get_named_gpio(np, "gpios", 0);
	// gpio 125
	g_dev_info.gpio_p5v5_enable = of_get_named_gpio(np, "gpios", 2);
	//gpio 40
	g_dev_info.gpio_reset = of_get_named_gpio(np, "gpios", 3);
	//gpio 46
	g_dev_info.gpio_id0 = of_get_named_gpio(np, "gpios", 4);
	/* vddio switch gpio 67 */
	g_dev_info.gpio_vddio_ctrl = of_get_named_gpio(np, "gpios", 6);

	HISI_FB_INFO("%s: g_dev_info.bl_ic_name_buf:%s\n", __func__, g_dev_info.bl_ic_name_buf);
	HISI_FB_INFO("%s: g_dev_info.bl_type:%u\n", __func__, g_dev_info.bl_type);

	HISI_FB_INFO("%s: g_dev_info.gpio_id0:%u\n", __func__, g_dev_info.gpio_id0);
	HISI_FB_INFO("%s: g_dev_info.gpio_vddio_ctrl:%u\n", __func__, g_dev_info.gpio_vddio_ctrl);
	HISI_FB_INFO("%s: g_dev_info.gpio_n5v5_enable:%u\n", __func__, g_dev_info.gpio_n5v5_enable);
	HISI_FB_INFO("%s: g_dev_info.gpio_p5v5_enable:%u\n", __func__, g_dev_info.gpio_p5v5_enable);
	HISI_FB_INFO("%s: g_dev_info.gpio_reset:%u\n", __func__, g_dev_info.gpio_reset);

	HISI_FB_INFO("%s -\n", __func__);
	return JDI_OTM2503B_OK;
}

/*******************************************************************************
**
*/
static int mipi_jdi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct hisi_panel_info *pinfo = NULL;

	HISI_FB_DEBUG("+.\n");

	g_lcd_control_tp_power = true;

	if (mipi_jdi_get_dts_config())
		goto err_return;

	if (hisi_fb_device_probe_defer(PANEL_MIPI_CMD, g_dev_info.bl_type))
		goto err_probe_defer;

	HISI_FB_INFO("%s\n", DTS_COMP_JDI_OTM2503B_5p5);
	pdev->id = 1;
	// init lcd panel info
	pinfo = g_panel_data.panel_info;
	memset(pinfo, 0, sizeof(struct hisi_panel_info));
	pinfo->xres = 1440;
	pinfo->yres = 2560;
	pinfo->width = 63;
	pinfo->height = 113;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = LCD_RGB888;
	pinfo->bgr_fmt = LCD_RGB;
	pinfo->bl_set_type = g_dev_info.bl_type;

#if OTM2503B_FPS
	/* open fps switch func */
	pinfo->fps = 60;
	pinfo->fps_updt = 60;
	pinfo->fps_updt_support = 1;
	pinfo->fps_updt_panel_only = 0;
#endif

	if (pinfo->bl_set_type == BL_SET_BY_BLPWM)
		pinfo->blpwm_input_ena = 0;

	if (!strncmp(g_dev_info.bl_ic_name_buf, "LM36923YFFR", strlen("LM36923YFFR"))) {
		pinfo->bl_min = 33;
		/* 10000stage 8710,2048stage 1973 for 450nit */
		pinfo->bl_max = 8820;
		pinfo->bl_default = 4000;
		pinfo->blpwm_precision_type = BLPWM_PRECISION_2048_TYPE;
		pinfo->bl_ic_ctrl_mode = REG_ONLY_MODE;
	} else {
#ifdef CONFIG_BACKLIGHT_2048
		pinfo->bl_min = 33;
		/* 10000stage 8710,2048stage 1973 for 450nit */
		pinfo->bl_max = 8820;
		pinfo->bl_default = 4000;
		pinfo->blpwm_precision_type = BLPWM_PRECISION_2048_TYPE;
		pinfo->bl_ic_ctrl_mode = REG_ONLY_MODE;
#else
		pinfo->bl_min = 4;
		pinfo->bl_max = 255;
		pinfo->bl_default = 102;
#endif
	}
	pinfo->type = PANEL_MIPI_CMD;
	pinfo->ifbc_type = IFBC_TYPE_VESA3X_SINGLE;

	pinfo->frc_enable = 0;
	pinfo->esd_enable = 1;
	pinfo->esd_skip_mipi_check = 1;
	pinfo->lcd_uninit_step_support = 1;
	pinfo->lcd_adjust_support = 1;

	pinfo->color_temperature_support = 1;
	if(color_temp_cal_buf[0] == 1){
		pinfo->color_temp_rectify_support = 0;
	}else{
		pinfo->color_temp_rectify_support =0;
	}
	pinfo->color_temp_rectify_R = 32768; /*100 percent*/
	pinfo->color_temp_rectify_G = 32768; /*100 percent*/
	pinfo->color_temp_rectify_B = 32768; /*100 percent*/

	pinfo->comform_mode_support = 1;
	if(pinfo->comform_mode_support == 1){
		g_support_mode |= COMFORM_MODE;
	}

	g_led_rg_para1 = 7;
	g_led_rg_para2 = 30983;
	g_support_mode |= LED_RG_COLOR_TEMP_MODE
#if OTM2503B_FPS
		| FPS_30_60_SENCE_MODE
#endif
		;

	//prefix ce & sharpness
	pinfo->prefix_ce_support = 0;
	pinfo->prefix_sharpness1D_support = 0;
	pinfo->arsr1p_sharpness_support = 1;
	pinfo->prefix_sharpness2D_support = 1;
	pinfo->sharp2d_table = &sharp2d_table;

	//sbl
	pinfo->sbl_support = 0;
	pinfo->smart_bl.strength_limit = 128;
	pinfo->smart_bl.calibration_a = 25;
	pinfo->smart_bl.calibration_b = 95;
	pinfo->smart_bl.calibration_c = 3;
	pinfo->smart_bl.calibration_d = 0;
	pinfo->smart_bl.t_filter_control = 5;
	pinfo->smart_bl.backlight_min = 480;
	pinfo->smart_bl.backlight_max = 4096;
	pinfo->smart_bl.backlight_scale = 0xff;
	pinfo->smart_bl.ambient_light_min = 14;
	pinfo->smart_bl.filter_a = 1738;
	pinfo->smart_bl.filter_b = 6;
	pinfo->smart_bl.logo_left = 0;
	pinfo->smart_bl.logo_top = 0;
	pinfo->smart_bl.variance_intensity_space = 145;
	pinfo->smart_bl.slope_max = 54;
	pinfo->smart_bl.slope_min = 160;

	//ACM
	pinfo->acm_support = 0;
	if (pinfo->acm_support == 1) {
		pinfo->acm_lut_hue_table = acm_lut_hue_table;
		pinfo->acm_lut_hue_table_len = ARRAY_SIZE(acm_lut_hue_table);
		pinfo->acm_lut_sata_table = acm_lut_sata_table;
		pinfo->acm_lut_sata_table_len = ARRAY_SIZE(acm_lut_sata_table);
		pinfo->acm_lut_satr_table = acm_lut_satr_table;
		pinfo->acm_lut_satr_table_len = ARRAY_SIZE(acm_lut_satr_table);
		pinfo->acm_valid_num = 7;
		pinfo->r0_hh = 0x7f;
		pinfo->r0_lh = 0x0;
		pinfo->r1_hh = 0xff;
		pinfo->r1_lh = 0x80;
		pinfo->r2_hh = 0x17f;
		pinfo->r2_lh = 0x100;
		pinfo->r3_hh = 0x1ff;
		pinfo->r3_lh = 0x180;
		pinfo->r4_hh = 0x27f;
		pinfo->r4_lh = 0x200;
		pinfo->r5_hh = 0x2ff;
		pinfo->r5_lh = 0x280;
		pinfo->r6_hh = 0x37f;
		pinfo->r6_lh = 0x300;

		//ACM_CE
		pinfo->acm_ce_support = 1;
	}

	// Contrast Algorithm
	if (pinfo->prefix_ce_support == 1 || pinfo->acm_ce_support == 1) {
		pinfo->ce_alg_param.iDiffMaxTH = 900;
		pinfo->ce_alg_param.iDiffMinTH = 100;
		pinfo->ce_alg_param.iFlatDiffTH = 500;
		pinfo->ce_alg_param.iAlphaMinTH = 16;
		pinfo->ce_alg_param.iBinDiffMaxTH = 40000;

		pinfo->ce_alg_param.iDarkPixelMinTH = 16;
		pinfo->ce_alg_param.iDarkPixelMaxTH = 24;
		pinfo->ce_alg_param.iDarkAvePixelMinTH = 40;
		pinfo->ce_alg_param.iDarkAvePixelMaxTH = 80;
		pinfo->ce_alg_param.iWhitePixelTH = 236;
		pinfo->ce_alg_param.fweight = 42;
		pinfo->ce_alg_param.fDarkRatio = 51;
		pinfo->ce_alg_param.fWhiteRatio = 51;

		pinfo->ce_alg_param.iDarkPixelTH = 64;
		pinfo->ce_alg_param.fDarkSlopeMinTH = 149;
		pinfo->ce_alg_param.fDarkSlopeMaxTH = 161;
		pinfo->ce_alg_param.fDarkRatioMinTH = 18;
		pinfo->ce_alg_param.fDarkRatioMaxTH = 38;

		pinfo->ce_alg_param.iBrightPixelTH = 192;
		pinfo->ce_alg_param.fBrightSlopeMinTH = 149;
		pinfo->ce_alg_param.fBrightSlopeMaxTH = 174;
		pinfo->ce_alg_param.fBrightRatioMinTH = 20;
		pinfo->ce_alg_param.fBrightRatioMaxTH = 36;

		pinfo->ce_alg_param.iZeroPos0MaxTH = 120;
		pinfo->ce_alg_param.iZeroPos1MaxTH = 128;

		pinfo->ce_alg_param.iDarkFMaxTH = 16;
		pinfo->ce_alg_param.iDarkFMinTH = 12;
		pinfo->ce_alg_param.iPos0MaxTH = 120;
		pinfo->ce_alg_param.iPos0MinTH = 96;

		pinfo->ce_alg_param.fKeepRatio = 61;
	}

	//Gama LCP
	pinfo->gamma_support = 1;
	if (pinfo->gamma_support == 1) {
		pinfo->igm_lut_table_R = igm_lut_table_R;
		pinfo->igm_lut_table_G = igm_lut_table_G;
		pinfo->igm_lut_table_B = igm_lut_table_B;
		pinfo->igm_lut_table_len = ARRAY_SIZE(igm_lut_table_R);

		pinfo->gamma_lut_table_R = gamma_lut_table_R;
		pinfo->gamma_lut_table_G = gamma_lut_table_G;
		pinfo->gamma_lut_table_B = gamma_lut_table_B;
		pinfo->gamma_lut_table_len = ARRAY_SIZE(gamma_lut_table_R);

		pinfo->xcc_support = 1;
		pinfo->xcc_table = xcc_table;
		pinfo->xcc_table_len = ARRAY_SIZE(xcc_table);

		pinfo->gmp_support = 1;
		pinfo->gmp_lut_table_low32bit = &gmp_lut_table_low32bit[0][0][0];
		pinfo->gmp_lut_table_high4bit = &gmp_lut_table_high4bit[0][0][0];
		pinfo->gmp_lut_table_len = ARRAY_SIZE(gmp_lut_table_low32bit);
	}

	// hiace
	pinfo->hiace_support = 1;
	if (pinfo->hiace_support == 1) {
		pinfo->hiace_param.iGlobalHistBlackPos = 16;
		pinfo->hiace_param.iGlobalHistWhitePos = 240;
		pinfo->hiace_param.iGlobalHistBlackWeight = 51;
		pinfo->hiace_param.iGlobalHistWhiteWeight = 51;
		pinfo->hiace_param.iGlobalHistZeroCutRatio = 486;
		pinfo->hiace_param.iGlobalHistSlopeCutRatio = 410;
		pinfo->hiace_param.iMaxLcdLuminance = 500;
		pinfo->hiace_param.iMinLcdLuminance = 3;
		strncpy(pinfo->hiace_param.chCfgName, "/hwprdct/etc/display/effect/algorithm/hdr_engine.xml", sizeof(pinfo->hiace_param.chCfgName) - 1);
	}

	//ldi
	pinfo->ldi.h_back_porch = 66;
	pinfo->ldi.h_front_porch = 75;
	pinfo->ldi.h_pulse_width = 30;
	pinfo->ldi.v_back_porch = 35;
	pinfo->ldi.v_front_porch = 14;
	pinfo->ldi.v_pulse_width = 8;

	//mipi
	pinfo->mipi.dsi_bit_clk = 490;
	pinfo->mipi.dsi_bit_clk_val1 = 471;
	pinfo->mipi.dsi_bit_clk_val2 = 500;
	pinfo->mipi.dsi_bit_clk_val3 = 490;
	pinfo->mipi.dsi_bit_clk_val4 = 500;
	//pinfo->mipi.dsi_bit_clk_val5 = ;
	pinfo->dsi_bit_clk_upt_support = 1;
	pinfo->mipi.dsi_bit_clk_upt = pinfo->mipi.dsi_bit_clk;

	//non_continue adjust : measured in UI
	//LG requires clk_post >= 60ns + 252ui, so need a clk_post_adjust more than 200ui. Here 215 is used.
	pinfo->mipi.clk_post_adjust = 215;
	pinfo->mipi.clk_pre_adjust= 0;
	pinfo->mipi.clk_t_hs_prepare_adjust= 0;
	pinfo->mipi.clk_t_lpx_adjust= 0;
	pinfo->mipi.clk_t_hs_trial_adjust= 0;
	pinfo->mipi.clk_t_hs_exit_adjust= 0;
	pinfo->mipi.clk_t_hs_zero_adjust= 0;
	pinfo->mipi.non_continue_en = 1;

	pinfo->pxl_clk_rate = 288 * 1000000UL;

	//mipi
	pinfo->mipi.lane_nums = DSI_4_LANES;
	//pinfo->mipi.lane_nums_select_support = DSI_1_LANES_SUPPORT | DSI_2_LANES_SUPPORT |
	//	DSI_3_LANES_SUPPORT | DSI_4_LANES_SUPPORT;
	pinfo->mipi.color_mode = DSI_24BITS_1;
	pinfo->mipi.vc = 0;
	pinfo->mipi.max_tx_esc_clk = 10 * 1000000;
	pinfo->mipi.burst_mode = 0;
	pinfo->mipi.non_continue_en = 1;

	pinfo->pxl_clk_rate_div = 1;

	pinfo->vsync_ctrl_type = VSYNC_CTRL_ISR_OFF | VSYNC_CTRL_MIPI_ULPS | VSYNC_CTRL_CLK_OFF;
	pinfo->dirty_region_updt_support = 0;
	pinfo->dirty_region_info.left_align = -1;
	pinfo->dirty_region_info.right_align = -1;
	pinfo->dirty_region_info.top_align = 32;
	pinfo->dirty_region_info.bottom_align = 32;
	pinfo->dirty_region_info.w_align = -1;
	pinfo->dirty_region_info.h_align = -1;
	pinfo->dirty_region_info.w_min = 1080;
	pinfo->dirty_region_info.h_min = -1;
	pinfo->dirty_region_info.top_start = -1;
	pinfo->dirty_region_info.bottom_start = -1;

	if(runmode_is_factory()) {
		HISI_FB_INFO("Factory mode, disable features: dirty update etc.\n");
		pinfo->dirty_region_updt_support = 0;
		pinfo->prefix_ce_support = 0;
		pinfo->prefix_sharpness1D_support = 0;
		pinfo->arsr1p_sharpness_support = 0;
		pinfo->prefix_sharpness2D_support = 0;
		pinfo->sbl_support = 0;
		pinfo->acm_support = 0;
		pinfo->acm_ce_support = 0;
		pinfo->hiace_support = 0;
		pinfo->esd_enable = 0;
		pinfo->ifbc_type = IFBC_TYPE_VESA3X_SINGLE;
		pinfo->blpwm_input_ena = 0;
		pinfo->blpwm_precision_type = BLPWM_PRECISION_DEFAULT_TYPE;
		pinfo->comform_mode_support = 0;
		g_support_mode = 0;
		pinfo->color_temp_rectify_support = 0;
	}

	if (pinfo->ifbc_type == IFBC_TYPE_VESA3X_SINGLE) {
		pinfo->pxl_clk_rate_div = 3;

		//ldi
		pinfo->ldi.h_back_porch /= pinfo->pxl_clk_rate_div;
		pinfo->ldi.h_front_porch /= pinfo->pxl_clk_rate_div;
		pinfo->ldi.h_pulse_width /= pinfo->pxl_clk_rate_div;

		//dsc parameter info
		pinfo->vesa_dsc.bits_per_component = 8;
		pinfo->vesa_dsc.bits_per_pixel = 8;
		pinfo->vesa_dsc.slice_width = 719;
		pinfo->vesa_dsc.slice_height = 31;

		pinfo->vesa_dsc.initial_xmit_delay = 512;
		pinfo->vesa_dsc.first_line_bpg_offset = 12;
		pinfo->vesa_dsc.mux_word_size = 48;

		// DSC_CTRL
		pinfo->vesa_dsc.block_pred_enable = 1;
		pinfo->vesa_dsc.linebuf_depth = 9;

		//RC_PARAM3
		pinfo->vesa_dsc.initial_offset = 6144;

		//FLATNESS_QP_TH
		pinfo->vesa_dsc.flatness_min_qp = 3;
		pinfo->vesa_dsc.flatness_max_qp = 12;

		//DSC_PARAM4
		pinfo->vesa_dsc.rc_edge_factor= 0x6;
		pinfo->vesa_dsc.rc_model_size = 8192;

		//DSC_RC_PARAM5: 0x330b0b
		pinfo->vesa_dsc.rc_tgt_offset_lo = (0x330b0b >> 20) & 0xF;
		pinfo->vesa_dsc.rc_tgt_offset_hi = (0x330b0b >> 16) & 0xF;
		pinfo->vesa_dsc.rc_quant_incr_limit1 = (0x330b0b >> 8) & 0x1F;
		pinfo->vesa_dsc.rc_quant_incr_limit0 = (0x330b0b >> 0) & 0x1F;

		//DSC_RC_BUF_THRESH0: 0xe1c2a38
		pinfo->vesa_dsc.rc_buf_thresh0 = (0xe1c2a38 >> 24) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh1 = (0xe1c2a38 >> 16) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh2 = (0xe1c2a38 >> 8) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh3 = (0xe1c2a38 >> 0) & 0xFF;

		//DSC_RC_BUF_THRESH1: 0x46546269
		pinfo->vesa_dsc.rc_buf_thresh4 = (0x46546269 >> 24) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh5 = (0x46546269 >> 16) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh6 = (0x46546269 >> 8) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh7 = (0x46546269 >> 0) & 0xFF;

		//DSC_RC_BUF_THRESH2: 0x7077797b
		pinfo->vesa_dsc.rc_buf_thresh8 = (0x7077797b >> 24) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh9 = (0x7077797b >> 16) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh10 = (0x7077797b >> 8) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh11 = (0x7077797b >> 0) & 0xFF;

		//DSC_RC_BUF_THRESH3: 0x7d7e0000
		pinfo->vesa_dsc.rc_buf_thresh12 = (0x7d7e0000 >> 24) & 0xFF;
		pinfo->vesa_dsc.rc_buf_thresh13 = (0x7d7e0000 >> 16) & 0xFF;

		//DSC_RC_RANGE_PARAM0: 0x1020100
		pinfo->vesa_dsc.range_min_qp0 = (0x1020100 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp0 = (0x1020100 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset0 = (0x1020100 >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp1 = (0x1020100 >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp1 = (0x1020100 >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset1 = (0x1020100 >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM1: 0x94009be,
		pinfo->vesa_dsc.range_min_qp2 = (0x94009be >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp2 = (0x94009be >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset2 = (0x94009be >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp3 = (0x94009be >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp3 = (0x94009be >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset3 = (0x94009be >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM2, 0x19fc19fa,
		pinfo->vesa_dsc.range_min_qp4 = (0x19fc19fa >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp4 = (0x19fc19fa >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset4 = (0x19fc19fa >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp5 = (0x19fc19fa >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp5 = (0x19fc19fa >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset5 = (0x19fc19fa >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM3, 0x19f81a38,
		pinfo->vesa_dsc.range_min_qp6 = (0x19f81a38 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp6 = (0x19f81a38 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset6 = (0x19f81a38 >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp7 = (0x19f81a38 >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp7 = (0x19f81a38 >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset7 = (0x19f81a38 >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM4, 0x1a781ab6,
		pinfo->vesa_dsc.range_min_qp8 = (0x1a781ab6 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp8 = (0x1a781ab6 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset8 = (0x1a781ab6 >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp9 = (0x1a781ab6 >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp9 = (0x1a781ab6 >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset9 = (0x1a781ab6 >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM5, 0x2af62b34,
		pinfo->vesa_dsc.range_min_qp10 = (0x2af62b34 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp10 = (0x2af62b34 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset10 = (0x2af62b34 >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp11 = (0x2af62b34 >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp11 = (0x2af62b34 >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset11 = (0x2af62b34 >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM6, 0x2b743b74,
		pinfo->vesa_dsc.range_min_qp12 = (0x2b743b74 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp12 = (0x2b743b74 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset12 = (0x2b743b74 >> 16) & 0x3F;
		pinfo->vesa_dsc.range_min_qp13 = (0x2b743b74 >> 11) & 0x1F;
		pinfo->vesa_dsc.range_max_qp13 = (0x2b743b74 >> 6) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset13 = (0x2b743b74 >> 0) & 0x3F;

		//DSC_RC_RANGE_PARAM7, 0x6bf40000,
		pinfo->vesa_dsc.range_min_qp14 = (0x6bf40000 >> 27) & 0x1F;
		pinfo->vesa_dsc.range_max_qp14 = (0x6bf40000 >> 22) & 0x1F;
		pinfo->vesa_dsc.range_bpg_offset14 = (0x6bf40000 >> 16) & 0x3F;
	};

	ret = vcc_cmds_tx(pdev, lcd_vcc_lcdanalog_init_cmds,
		ARRAY_SIZE(lcd_vcc_lcdanalog_init_cmds));
	if (ret != 0) {
		HISI_FB_ERR("LCD vcc lcdanalog init failed!\n");
		goto err_return;
	}

	// lcd pinctrl init
	ret = pinctrl_cmds_tx(pdev, lcd_pinctrl_init_cmds,
		ARRAY_SIZE(lcd_pinctrl_init_cmds));
	if (ret != 0) {
		HISI_FB_ERR("Init pinctrl failed, defer\n");
		goto err_return;
	}

	// lcd vcc enable
	if (is_fastboot_display_enable()) {
		vcc_cmds_tx(pdev, lcd_vcc_lcdanalog_enable_cmds,
			ARRAY_SIZE(lcd_vcc_lcdanalog_enable_cmds));
	}

	// alloc panel device data
	ret = platform_device_add_data(pdev, &g_panel_data,
		sizeof(struct hisi_fb_panel_data));
	if (ret) {
		HISI_FB_ERR("platform_device_add_data failed!\n");
		goto err_device_put;
	}

	hisi_fb_add_device(pdev);

	HISI_FB_DEBUG("-.\n");

	return 0;

err_device_put:
	platform_device_put(pdev);
err_return:
	return ret;
err_probe_defer:
	return -EPROBE_DEFER;
}

static const struct of_device_id hisi_panel_match_table[] = {
	{
		.compatible = DTS_COMP_JDI_OTM2503B_5p5,
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, hisi_panel_match_table);

static struct platform_driver this_driver = {
	.probe = mipi_jdi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "mipi_jdi_OTM2503B_5p5",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hisi_panel_match_table),
	},
};

static int __init mipi_jdi_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		HISI_FB_ERR("platform_driver_register failed, error=%d!panel:%s\n", ret, "mipi_jdi_OTM2503B_5p5");
		return ret;
	}

	return ret;
}

module_init(mipi_jdi_panel_init);
