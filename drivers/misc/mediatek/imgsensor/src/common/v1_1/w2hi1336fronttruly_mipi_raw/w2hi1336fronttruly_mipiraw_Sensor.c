/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "w2hi1336fronttruly_mipiraw_Sensor.h"
#define MAX_TIME1 655706 // 6004 * 0xfff7 / PCLK   us  (PCLK @ 600MHz)
#define MAX_TIME2 20000000  // 6004 * 0x1E8480 / PCLK us  (PCLK @ 600MHz)

#define PFX "hi1336_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)

#define ENABLE_OTP 1
#define HI1336_OTP_DUMP 0
//PDAF
#define ENABLE_PDAF 0
#define e2prom 1
#define per_frame 1

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//+bug767771,liangyiyi.wt,MODIFY,2022/09/20,modify for hi1336 Configure correct MIPI_ Data_ Rate and exposure line
//+bug767771,liangyiyi.wt,MODIFY,2022/09/20,modify hi1336 592M mipi clk and 2mA mclk for RF interference
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = W2HI1336FRONTTRULY_SENSOR_ID,
	.checksum_value = 0x4f1b1d5e,       //0x6d01485c // Auto Test Mode ..
	.pre = {
		// frame rate = 30.01fps
		// vblank length = 17711.80us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 3330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 236800000,
		.max_framerate = 300,
	},
	.cap = {
		// frame rate = 30.01fps
		// vblank length = 2101.40us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 3330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000,
		.max_framerate = 300,
	},
	// need to setting
	.cap1 = {
		// frame rate = 30.01fps
		// vblank length = 2101.40us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 3330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 3330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 2368,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000,
		.max_framerate = 300,
	},
	.hs_video = {
		// frame rate = 60.02fps
		// vblank length = 5853.90us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 1665,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 236800000,
		.max_framerate = 600,
	},
	.slim_video = {
		// frame rate = 120.11fps
		// vblank length = 1120.75us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 832,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 571200000,
		.max_framerate = 1200,
	},
	//-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
	//+bug 663265 zhoumin.wt, modify, 2021/09/13, modify for dualcam sync
	.custom1 = {//24.005
		// frame rate = 30.01fps
		// vblank length = 2101.40us
		.pclk = 600000000,
		.linelength = 6004,
		.framelength = 3330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000,
		.max_framerate = 300,
	},
	//-bug 663265 zhoumin.wt, modify, 2021/09/13, modify for dualcam sync
	//-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0x1E8480,

	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.exp_step = 1,
	.gain_step = 4,
	.gain_type = 3,

#if per_frame
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 6,	  //support sensor mode num
	.frame_time_delay_frame = 1,//The delay frame of setting frame length
	.cap_delay_frame = 1,
	.pre_delay_frame = 1,
	.video_delay_frame = 1,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,//bug767771,liangyiyi.wt,MODIFY,2022/11/11,Modify the driving current of hi1336 mclk to 4MA to match the radio frequency interference
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,//bug783855,liangyiyi.wt,MODIFY,2022/07/23,modify for fix HI1336 mirrot flip
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0x42, 0xff},
	.i2c_speed = 1000, //bug 682590, zhanghao2.wt, Modify, 2021/11/05, modify main camera i2c speed 400k to 1M.
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0100,
	.gain = 0xe0,
	.dummy_pixel = 0,
	.dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x42,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {

    { 4224, 3136,   0,   4, 4224, 3128,    2112, 1564,  4,  2, 2104, 1560, 0, 0, 2104, 1560},		// preview (2104 x 1560)//DIFF
	{ 4224, 3136,   0,   6, 4224, 3124,    4224, 3124,  8,  2, 4208, 3120, 0, 0, 4208, 3120},		// capture (4208 x 3120) //bug 767771,liangyiyi.wt, MODIFY, 2022.8.17, modify right winsize about hi1336
	{ 4224, 3136,   0,   382, 4224, 2372,    4224, 2372,  8,  2, 4208, 2368, 0, 0, 4208, 2368},		// VIDEO (4208 x 2368)
	{ 4224, 3136,   0, 484, 4224, 2168,    2112, 1084,  96,  2, 1920, 1080, 0, 0, 1920, 1080},		// hight speed video (1920 x 1080)
 	{ 4224, 3136,   0, 482, 4224, 2172,  1408,  724,  64, 2, 1280,  720, 0, 0, 1280,  720},       // slim video (1280 x 720)
	{ 4224, 3136,   0,   0, 4224, 3124,    4224, 3124,  8,  2, 4208, 3120, 0, 0, 4208, 3120}, // custom1(4208 x 3120)
};

#if ENABLE_PDAF
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX = 32,
    .i4OffsetY = 24,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum =8,
    .i4SubBlkW =16,
    .i4SubBlkH =8,
    .i4PosL = {{37,28},{53,28},{45,32},{61,32},{37,44},{53,44},{45,48},{61,48}},
    .i4PosR = {{37,24},{53,24},{45,36},{61,36},{37,40},{53,40},{45,52},{61,52}},
    .i4BlockNumX = 128,
    .i4BlockNumY = 96,
    /* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
    .iMirrorFlip = 0,
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_4208_2368 =
{
#if 0
    .i4OffsetX = 56,
    .i4OffsetY = 0,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum =8,
    .i4SubBlkW =16,
    .i4SubBlkH =8,
    .i4PosL = {{61,4},{61,20},{69, 8},{69,24},{77,4},{77,20},{85, 8},{85,24}},
    .i4PosR = {{61,0},{61,16},{69,12},{69,28},{77,0},{77,16},{85,12},{85,28}},
    .i4BlockNumX = 128,
    .i4BlockNumY = 74,
    /* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
    .iMirrorFlip = 0,
    .i4Crop = { {0, 0}, {0, 0}, {0, 374}, {0,0}, {0, 0},{0, 0}, {0, 0}, {0, 0},{0, 0}, {0, 0} },
#endif

#if 0
      .i4OffsetX = 56,
      .i4OffsetY = 24,
      .i4PitchX  = 32,
      .i4PitchY  = 32,
      .i4PairNum = 8,
      .i4SubBlkW = 16,
      .i4SubBlkH = 8,
      .i4PosL    ={{61,28},{61,44},{69,32},{69,48},{77,28},{77,44},{85,32},{85,48}},
      .i4PosR    ={{61,24},{61,40},{69,36},{69,52},{77,24},{77,40},{85,36},{85,52}},
      .i4BlockNumX = 128,
      .i4BlockNumY = 73,
      .iMirrorFlip = 0,
      .i4Crop = { {0, 0}, {0, 0}, {0, 374}, {0,0}, {0, 0},{0, 0}, {0, 0}, {0, 0},{0, 0}, {0, 0} },
#endif
};
#endif



#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 255

static kal_uint16 hi1336_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}

		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				4, imgsensor_info.i2c_speed);

			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x020e, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0206, imgsensor.line_length/4);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717));

}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("shutter = %d, imgsensor.frame_length = %d, imgsensor.min_frame_length = %d\n",
		shutter, imgsensor.frame_length, imgsensor.min_frame_length);


	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x020e, imgsensor.frame_length);

	} else{
			write_cmos_sensor(0x020e, imgsensor.frame_length);
	}

	write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16 );
	write_cmos_sensor(0x020A, shutter);

	LOG_INF("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);
}

static void set_shutter_frame_length(kal_uint32 shutter, kal_uint16 frame_length){

	unsigned long flags;
	unsigned short realtime_fps = 0;
	static unsigned short fps_fine = 0;
	LOG_INF("set_shutter_frame_length");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
	/*Change frame time */
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

	LOG_INF("shutter = %d, imgsensor.frame_length = %d, imgsensor.min_frame_length = %d\n",
		shutter, imgsensor.frame_length, imgsensor.min_frame_length);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;

    //shutter = (shutter >> 1) << 1;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x020e, imgsensor.frame_length);

	} else{
			write_cmos_sensor(0x020e, imgsensor.frame_length & 0xFFFF);
	}
	if(imgsensor.frame_length == 7005){
		fps_fine++;
		if (fps_fine != 3){
			imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
			imgsensor.frame_length -= 1;
		}
		else{
			imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
			fps_fine = 0;
		}
	}
	else imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	set_dummy();
	write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16);
	write_cmos_sensor(0x020A, shutter);

	LOG_INF("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

}

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */


/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 gain2reg(kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;
    reg_gain = gain / 4 - 16;

    return (kal_uint16)reg_gain;

}


static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X    */
    /* [4:9] = M meams M X         */
    /* Total gain = M + N /16 X   */

    if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 16 * BASEGAIN)
            gain = 16 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_8(0x0213,reg_gain);
	return gain;

}

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;
		// Extend frame length first
		write_cmos_sensor(0x0006, imgsensor.frame_length);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0000, 0x0000);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0000, 0x0100);

		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0000, 0x0200);

		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0000, 0x0300);

		break;
	default:
		LOG_INF("Error image_mirror setting");
		break;
	}

}
#endif
/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

#if MULTI_WRITE
static kal_uint16 addr_data_pair_init_hi1336[] = {
//0x0b00,0x0000,
0x2000,0x0021,
0x2002,0x04a5,
0x2004,0xb124,
0x2006,0xc09c,
0x2008,0x0064,
0x200a,0x088e,
0x200c,0x01c2,
0x200e,0x00b4,
0x2010,0x4020,
0x2012,0x4292,
0x2014,0xf00a,
0x2016,0x0310,
0x2018,0x12b0,
0x201a,0xc3f2,
0x201c,0x425f,
0x201e,0x0282,
0x2020,0xf35f,
0x2022,0xf37f,
0x2024,0x5f0f,
0x2026,0x4f92,
0x2028,0xf692,
0x202a,0x0402,
0x202c,0x93c2,
0x202e,0x82cc,
0x2030,0x2403,
0x2032,0xf0f2,
0x2034,0xffe7,
0x2036,0x0254,
0x2038,0x4130,
0x203a,0x120b,
0x203c,0x120a,
0x203e,0x1209,
0x2040,0x425f,
0x2042,0x0600,
0x2044,0xf35f,
0x2046,0x4f4b,
0x2048,0x12b0,
0x204a,0xc6ae,
0x204c,0x403d,
0x204e,0x0100,
0x2050,0x403e,
0x2052,0x2bfc,
0x2054,0x403f,
0x2056,0x8020,
0x2058,0x12b0,
0x205a,0xc476,
0x205c,0x930b,
0x205e,0x2009,
0x2060,0x93c2,
0x2062,0x0c0a,
0x2064,0x2403,
0x2066,0x43d2,
0x2068,0x0e1f,
0x206a,0x3c13,
0x206c,0x43c2,
0x206e,0x0e1f,
0x2070,0x3c10,
0x2072,0x4039,
0x2074,0x0e08,
0x2076,0x492a,
0x2078,0x421c,
0x207a,0xf010,
0x207c,0x430b,
0x207e,0x430d,
0x2080,0x12b0,
0x2082,0xdfb6,
0x2084,0x403d,
0x2086,0x000e,
0x2088,0x12b0,
0x208a,0xc62c,
0x208c,0x4e89,
0x208e,0x0000,
0x2090,0x3fe7,
0x2092,0x4139,
0x2094,0x413a,
0x2096,0x413b,
0x2098,0x4130,
0x209a,0xb0b2,
0x209c,0x0020,
0x209e,0xf002,
0x20a0,0x2429,
0x20a2,0x421e,
0x20a4,0x0256,
0x20a6,0x532e,
0x20a8,0x421f,
0x20aa,0xf008,
0x20ac,0x9e0f,
0x20ae,0x2c01,
0x20b0,0x4e0f,
0x20b2,0x4f0c,
0x20b4,0x430d,
0x20b6,0x421e,
0x20b8,0x7300,
0x20ba,0x421f,
0x20bc,0x7302,
0x20be,0x5c0e,
0x20c0,0x6d0f,
0x20c2,0x821e,
0x20c4,0x830c,
0x20c6,0x721f,
0x20c8,0x830e,
0x20ca,0x2c0d,
0x20cc,0x0900,
0x20ce,0x7312,
0x20d0,0x421e,
0x20d2,0x7300,
0x20d4,0x421f,
0x20d6,0x7302,
0x20d8,0x5c0e,
0x20da,0x6d0f,
0x20dc,0x821e,
0x20de,0x830c,
0x20e0,0x721f,
0x20e2,0x830e,
0x20e4,0x2bf3,
0x20e6,0x4292,
0x20e8,0x8248,
0x20ea,0x0a08,
0x20ec,0x0c10,
0x20ee,0x4292,
0x20f0,0x8252,
0x20f2,0x0a12,
0x20f4,0x12b0,
0x20f6,0xdc9c,
0x20f8,0xd0f2,
0x20fa,0x0018,
0x20fc,0x0254,
0x20fe,0x4130,
0x2100,0x120b,
0x2102,0x12b0,
0x2104,0xcfc8,
0x2106,0x4f4b,
0x2108,0x12b0,
0x210a,0xcfc8,
0x210c,0xf37f,
0x210e,0x108f,
0x2110,0xdb0f,
0x2112,0x413b,
0x2114,0x4130,
0x2116,0x120b,
0x2118,0x12b0,
0x211a,0xcfc8,
0x211c,0x4f4b,
0x211e,0x108b,
0x2120,0x12b0,
0x2122,0xcfc8,
0x2124,0xf37f,
0x2126,0xdb0f,
0x2128,0x413b,
0x212a,0x4130,
0x212c,0x120b,
0x212e,0x120a,
0x2130,0x1209,
0x2132,0x1208,
0x2134,0x4338,
0x2136,0x40b2,
0x2138,0x17fb,
0x213a,0x83be,
0x213c,0x12b0,
0x213e,0xcfc8,
0x2140,0xf37f,
0x2142,0x903f,
0x2144,0x0013,
0x2146,0x244c,
0x2148,0x12b0,
0x214a,0xf100,
0x214c,0x4f82,
0x214e,0x82a4,
0x2150,0xb3e2,
0x2152,0x0282,
0x2154,0x240a,
0x2156,0x5f0f,
0x2158,0x5f0f,
0x215a,0x521f,
0x215c,0x83be,
0x215e,0x533f,
0x2160,0x4f82,
0x2162,0x83be,
0x2164,0x43f2,
0x2166,0x83c0,
0x2168,0x4308,
0x216a,0x4309,
0x216c,0x9219,
0x216e,0x82a4,
0x2170,0x2c34,
0x2172,0xb3e2,
0x2174,0x0282,
0x2176,0x242a,
0x2178,0x12b0,
0x217a,0xf116,
0x217c,0x4f0b,
0x217e,0x12b0,
0x2180,0xf116,
0x2182,0x4f0a,
0x2184,0x490f,
0x2186,0x5f0f,
0x2188,0x5f0f,
0x218a,0x4b8f,
0x218c,0x2bfc,
0x218e,0x4a8f,
0x2190,0x2bfe,
0x2192,0x5319,
0x2194,0x9039,
0x2196,0x0100,
0x2198,0x2be9,
0x219a,0x43d2,
0x219c,0x83c0,
0x219e,0x421e,
0x21a0,0x82a4,
0x21a2,0x903e,
0x21a4,0x0080,
0x21a6,0x2810,
0x21a8,0x421f,
0x21aa,0x2d28,
0x21ac,0x503f,
0x21ae,0x0014,
0x21b0,0x4f82,
0x21b2,0x82a0,
0x21b4,0x903e,
0x21b6,0x00c0,
0x21b8,0x2805,
0x21ba,0x421f,
0x21bc,0x2e28,
0x21be,0x503f,
0x21c0,0x0014,
0x21c2,0x3c12,
0x21c4,0x480f,
0x21c6,0x3c10,
0x21c8,0x480f,
0x21ca,0x3ff2,
0x21cc,0x12b0,
0x21ce,0xf100,
0x21d0,0x4f0a,
0x21d2,0x12b0,
0x21d4,0xf100,
0x21d6,0x4f0b,
0x21d8,0x3fd5,
0x21da,0x430a,
0x21dc,0x430b,
0x21de,0x3fd2,
0x21e0,0x40b2,
0x21e2,0x1bfe,
0x21e4,0x83be,
0x21e6,0x3fb0,
0x21e8,0x4f82,
0x21ea,0x82a2,
0x21ec,0x4138,
0x21ee,0x4139,
0x21f0,0x413a,
0x21f2,0x413b,
0x21f4,0x4130,
0x21f6,0x43d2,
0x21f8,0x0300,
0x21fa,0x12b0,
0x21fc,0xcf6a,
0x21fe,0x12b0,
0x2200,0xcf0a,
0x2202,0xb3d2,
0x2204,0x0267,
0x2206,0x2404,
0x2208,0x12b0,
0x220a,0xf12c,
0x220c,0xc3d2,
0x220e,0x0267,
0x2210,0x12b0,
0x2212,0xd0d4,
0x2214,0x0261,
0x2216,0x0000,
0x2218,0x43c2,
0x221a,0x0300,
0x221c,0x4392,
0x221e,0x732a,
0x2220,0x4130,
0x2222,0x90f2,
0x2224,0x0010,
0x2226,0x0260,
0x2228,0x2002,
0x222a,0x12b0,
0x222c,0xd4aa,
0x222e,0x12b0,
0x2230,0xd5fa,
0x2232,0x4392,
0x2234,0x732a,
0x2236,0x12b0,
0x2238,0xf1f6,
0x223a,0x4130,
0x223c,0x120b,
0x223e,0x120a,
0x2240,0x1209,
0x2242,0x1208,
0x2244,0x1207,
0x2246,0x1206,
0x2248,0x1205,
0x224a,0x1204,
0x224c,0x8031,
0x224e,0x000a,
0x2250,0x4291,
0x2252,0x82d8,
0x2254,0x0004,
0x2256,0x411f,
0x2258,0x0004,
0x225a,0x4fa1,
0x225c,0x0006,
0x225e,0x4257,
0x2260,0x82e5,
0x2262,0x4708,
0x2264,0xd038,
0x2266,0xff00,
0x2268,0x4349,
0x226a,0x4346,
0x226c,0x90b2,
0x226e,0x07d1,
0x2270,0x0b94,
0x2272,0x2806,
0x2274,0x40b2,
0x2276,0x0246,
0x2278,0x0228,
0x227a,0x40b2,
0x227c,0x09fb,
0x227e,0x0232,
0x2280,0x4291,
0x2282,0x0422,
0x2284,0x0000,
0x2286,0x421f,
0x2288,0x0424,
0x228a,0x812f,
0x228c,0x4f81,
0x228e,0x0002,
0x2290,0x4291,
0x2292,0x8248,
0x2294,0x0008,
0x2296,0x4214,
0x2298,0x0310,
0x229a,0x421a,
0x229c,0x82a0,
0x229e,0xf80a,
0x22a0,0x421b,
0x22a2,0x82a2,
0x22a4,0xf80b,
0x22a6,0x4382,
0x22a8,0x7334,
0x22aa,0x0f00,
0x22ac,0x7304,
0x22ae,0x4192,
0x22b0,0x0008,
0x22b2,0x0a08,
0x22b4,0x4382,
0x22b6,0x040c,
0x22b8,0x4305,
0x22ba,0x9382,
0x22bc,0x7112,
0x22be,0x2001,
0x22c0,0x4315,
0x22c2,0x421e,
0x22c4,0x7100,
0x22c6,0xb2f2,
0x22c8,0x0261,
0x22ca,0x2406,
0x22cc,0xb3d2,
0x22ce,0x0b02,
0x22d0,0x2403,
0x22d2,0x42d2,
0x22d4,0x0809,
0x22d6,0x0b00,
0x22d8,0x40b2,
0x22da,0x00b6,
0x22dc,0x7334,
0x22de,0x0f00,
0x22e0,0x7304,
0x22e2,0x4482,
0x22e4,0x0a08,
0x22e6,0xb2e2,
0x22e8,0x0b05,
0x22ea,0x2404,
0x22ec,0x4392,
0x22ee,0x7a0e,
0x22f0,0x0800,
0x22f2,0x7a10,
0x22f4,0xf80e,
0x22f6,0x93c2,
0x22f8,0x82de,
0x22fa,0x2468,
0x22fc,0x9e0a,
0x22fe,0x2803,
0x2300,0x9349,
0x2302,0x2001,
0x2304,0x4359,
0x2306,0x9e0b,
0x2308,0x2802,
0x230a,0x9369,
0x230c,0x245c,
0x230e,0x421f,
0x2310,0x731a,
0x2312,0xc312,
0x2314,0x100f,
0x2316,0x4f82,
0x2318,0x7334,
0x231a,0x0f00,
0x231c,0x7304,
0x231e,0x4192,
0x2320,0x0008,
0x2322,0x0a08,
0x2324,0x421e,
0x2326,0x7100,
0x2328,0x812e,
0x232a,0x425c,
0x232c,0x0419,
0x232e,0x537c,
0x2330,0xfe4c,
0x2332,0x9305,
0x2334,0x2003,
0x2336,0x40b2,
0x2338,0x0c78,
0x233a,0x7100,
0x233c,0x421f,
0x233e,0x731a,
0x2340,0xc312,
0x2342,0x100f,
0x2344,0x503f,
0x2346,0x00b6,
0x2348,0x4f82,
0x234a,0x7334,
0x234c,0x0f00,
0x234e,0x7304,
0x2350,0x4482,
0x2352,0x0a08,
0x2354,0x9e81,
0x2356,0x0002,
0x2358,0x2814,
0x235a,0xf74c,
0x235c,0x434d,
0x235e,0x411f,
0x2360,0x0004,
0x2362,0x4f1e,
0x2364,0x0002,
0x2366,0x9381,
0x2368,0x0006,
0x236a,0x240b,
0x236c,0x4e6f,
0x236e,0xf74f,
0x2370,0x9c4f,
0x2372,0x2423,
0x2374,0x535d,
0x2376,0x503e,
0x2378,0x0006,
0x237a,0x4d4f,
0x237c,0x911f,
0x237e,0x0006,
0x2380,0x2bf5,
0x2382,0x9359,
0x2384,0x2403,
0x2386,0x9079,
0x2388,0x0003,
0x238a,0x2028,
0x238c,0x434d,
0x238e,0x464f,
0x2390,0x5f0f,
0x2392,0x5f0f,
0x2394,0x4f9f,
0x2396,0x2dfc,
0x2398,0x8020,
0x239a,0x4f9f,
0x239c,0x2dfe,
0x239e,0x8022,
0x23a0,0x5356,
0x23a2,0x9076,
0x23a4,0x0040,
0x23a6,0x2407,
0x23a8,0x9076,
0x23aa,0xff80,
0x23ac,0x2404,
0x23ae,0x535d,
0x23b0,0x926d,
0x23b2,0x2bed,
0x23b4,0x3c13,
0x23b6,0x5359,
0x23b8,0x3c11,
0x23ba,0x4ea2,
0x23bc,0x040c,
0x23be,0x4e92,
0x23c0,0x0002,
0x23c2,0x040e,
0x23c4,0x3fde,
0x23c6,0x4079,
0x23c8,0x0003,
0x23ca,0x3fa1,
0x23cc,0x9a0e,
0x23ce,0x2803,
0x23d0,0x9349,
0x23d2,0x2001,
0x23d4,0x4359,
0x23d6,0x9b0e,
0x23d8,0x2b9a,
0x23da,0x3f97,
0x23dc,0x9305,
0x23de,0x2363,
0x23e0,0x5031,
0x23e2,0x000a,
0x23e4,0x4134,
0x23e6,0x4135,
0x23e8,0x4136,
0x23ea,0x4137,
0x23ec,0x4138,
0x23ee,0x4139,
0x23f0,0x413a,
0x23f2,0x413b,
0x23f4,0x4130,
0x23f6,0x120b,
0x23f8,0x120a,
0x23fa,0x1209,
0x23fc,0x1208,
0x23fe,0x1207,
0x2400,0x1206,
0x2402,0x1205,
0x2404,0x1204,
0x2406,0x8221,
0x2408,0x425f,
0x240a,0x0600,
0x240c,0xf35f,
0x240e,0x4fc1,
0x2410,0x0002,
0x2412,0x43c1,
0x2414,0x0003,
0x2416,0x403f,
0x2418,0x0603,
0x241a,0x4fe1,
0x241c,0x0000,
0x241e,0xb3ef,
0x2420,0x0000,
0x2422,0x2431,
0x2424,0x4344,
0x2426,0x4445,
0x2428,0x450f,
0x242a,0x5f0f,
0x242c,0x5f0f,
0x242e,0x403d,
0x2430,0x000e,
0x2432,0x4f1e,
0x2434,0x0632,
0x2436,0x4f1f,
0x2438,0x0634,
0x243a,0x12b0,
0x243c,0xc62c,
0x243e,0x4e08,
0x2440,0x4f09,
0x2442,0x421e,
0x2444,0xf00c,
0x2446,0x430f,
0x2448,0x480a,
0x244a,0x490b,
0x244c,0x4e0c,
0x244e,0x4f0d,
0x2450,0x12b0,
0x2452,0xdf96,
0x2454,0x421a,
0x2456,0xf00e,
0x2458,0x430b,
0x245a,0x403d,
0x245c,0x0009,
0x245e,0x12b0,
0x2460,0xc62c,
0x2462,0x4e06,
0x2464,0x4f07,
0x2466,0x5a06,
0x2468,0x6b07,
0x246a,0x425f,
0x246c,0x0668,
0x246e,0xf37f,
0x2470,0x9f08,
0x2472,0x2c6b,
0x2474,0x4216,
0x2476,0x06ca,
0x2478,0x4307,
0x247a,0x5505,
0x247c,0x4685,
0x247e,0x065e,
0x2480,0x5354,
0x2482,0x9264,
0x2484,0x2bd0,
0x2486,0x403b,
0x2488,0x0603,
0x248a,0x416f,
0x248c,0xc36f,
0x248e,0x4fcb,
0x2490,0x0000,
0x2492,0x12b0,
0x2494,0xcd42,
0x2496,0x41eb,
0x2498,0x0000,
0x249a,0x421f,
0x249c,0x0256,
0x249e,0x522f,
0x24a0,0x421b,
0x24a2,0xf008,
0x24a4,0x532b,
0x24a6,0x9f0b,
0x24a8,0x2c01,
0x24aa,0x4f0b,
0x24ac,0x9381,
0x24ae,0x0002,
0x24b0,0x2409,
0x24b2,0x430a,
0x24b4,0x421e,
0x24b6,0x0614,
0x24b8,0x503e,
0x24ba,0x000a,
0x24bc,0x421f,
0x24be,0x0680,
0x24c0,0x9f0e,
0x24c2,0x2801,
0x24c4,0x431a,
0x24c6,0xb0b2,
0x24c8,0x0020,
0x24ca,0xf002,
0x24cc,0x241f,
0x24ce,0x93c2,
0x24d0,0x82cc,
0x24d2,0x201c,
0x24d4,0x4b0e,
0x24d6,0x430f,
0x24d8,0x521e,
0x24da,0x7300,
0x24dc,0x621f,
0x24de,0x7302,
0x24e0,0x421c,
0x24e2,0x7316,
0x24e4,0x421d,
0x24e6,0x7318,
0x24e8,0x8c0e,
0x24ea,0x7d0f,
0x24ec,0x2c0f,
0x24ee,0x930a,
0x24f0,0x240d,
0x24f2,0x421f,
0x24f4,0x8248,
0x24f6,0xf03f,
0x24f8,0xf7ff,
0x24fa,0x4f82,
0x24fc,0x0a08,
0x24fe,0x0c10,
0x2500,0x421f,
0x2502,0x8252,
0x2504,0xd03f,
0x2506,0x00c0,
0x2508,0x4f82,
0x250a,0x0a12,
0x250c,0x4b0a,
0x250e,0x430b,
0x2510,0x421e,
0x2512,0x7300,
0x2514,0x421f,
0x2516,0x7302,
0x2518,0x5a0e,
0x251a,0x6b0f,
0x251c,0x421c,
0x251e,0x7316,
0x2520,0x421d,
0x2522,0x7318,
0x2524,0x8c0e,
0x2526,0x7d0f,
0x2528,0x2c1a,
0x252a,0x0900,
0x252c,0x7312,
0x252e,0x421e,
0x2530,0x7300,
0x2532,0x421f,
0x2534,0x7302,
0x2536,0x5a0e,
0x2538,0x6b0f,
0x253a,0x421c,
0x253c,0x7316,
0x253e,0x421d,
0x2540,0x7318,
0x2542,0x8c0e,
0x2544,0x7d0f,
0x2546,0x2bf1,
0x2548,0x3c0a,
0x254a,0x460e,
0x254c,0x470f,
0x254e,0x803e,
0x2550,0x0800,
0x2552,0x730f,
0x2554,0x2b92,
0x2556,0x4036,
0x2558,0x07ff,
0x255a,0x4307,
0x255c,0x3f8e,
0x255e,0x5221,
0x2560,0x4134,
0x2562,0x4135,
0x2564,0x4136,
0x2566,0x4137,
0x2568,0x4138,
0x256a,0x4139,
0x256c,0x413a,
0x256e,0x413b,
0x2570,0x4130,
0x2572,0x7400,
0x2574,0x2003,
0x2576,0x72a1,
0x2578,0x2f00,
0x257a,0x7020,
0x257c,0x2f21,
0x257e,0x7800,
0x2580,0x0040,
0x2582,0x7400,
0x2584,0x2005,
0x2586,0x72a1,
0x2588,0x2f00,
0x258a,0x7020,
0x258c,0x2f22,
0x258e,0x7800,
0x2590,0x7400,
0x2592,0x2011,
0x2594,0x72a1,
0x2596,0x2f00,
0x2598,0x7020,
0x259a,0x2f21,
0x259c,0x7800,
0x259e,0x7400,
0x25a0,0x2009,
0x25a2,0x72a1,
0x25a4,0x2f1f,
0x25a6,0x7021,
0x25a8,0x3f40,
0x25aa,0x7800,
0x25ac,0x7400,
0x25ae,0x2005,
0x25b0,0x72a1,
0x25b2,0x2f1f,
0x25b4,0x7021,
0x25b6,0x3f40,
0x25b8,0x7800,
0x25ba,0x7400,
0x25bc,0x2009,
0x25be,0x72a1,
0x25c0,0x2f00,
0x25c2,0x7020,
0x25c4,0x2f22,
0x25c6,0x7800,
0x25c8,0x0009,
0x25ca,0xf572,
0x25cc,0x0009,
0x25ce,0xf582,
0x25d0,0x0009,
0x25d2,0xf590,
0x25d4,0x0009,
0x25d6,0xf59e,
0x25d8,0xf580,
0x25da,0x0004,
0x25dc,0x0009,
0x25de,0xf590,
0x25e0,0x0009,
0x25e2,0xf5ba,
0x25e4,0x0009,
0x25e6,0xf572,
0x25e8,0x0009,
0x25ea,0xf5ac,
0x25ec,0xf580,
0x25ee,0x0004,
0x25f0,0x0009,
0x25f2,0xf572,
0x25f4,0x0009,
0x25f6,0xf5ac,
0x25f8,0x0009,
0x25fa,0xf590,
0x25fc,0x0009,
0x25fe,0xf59e,
0x2600,0xf580,
0x2602,0x0004,
0x2604,0x0009,
0x2606,0xf590,
0x2608,0x0009,
0x260a,0xf59e,
0x260c,0x0009,
0x260e,0xf572,
0x2610,0x0009,
0x2612,0xf5ac,
0x2614,0xf580,
0x2616,0x0004,
0x2618,0x0212,
0x261a,0x0217,
0x261c,0x041f,
0x261e,0x1017,
0x2620,0x0413,
0x2622,0x0103,
0x2624,0x010b,
0x2626,0x1c0a,
0x2628,0x0202,
0x262a,0x0407,
0x262c,0x0205,
0x262e,0x0204,
0x2630,0x0114,
0x2632,0x0110,
0x2634,0xffff,
0x2636,0x0048,
0x2638,0x0090,
0x263a,0x0000,
0x263c,0x0000,
0x263e,0xf618,
0x2640,0x0000,
0x2642,0x0000,
0x2644,0x0060,
0x2646,0x0078,
0x2648,0x0060,
0x264a,0x0078,
0x264c,0x004f,
0x264e,0x0037,
0x2650,0x0048,
0x2652,0x0090,
0x2654,0x0000,
0x2656,0x0000,
0x2658,0xf618,
0x265a,0x0000,
0x265c,0x0000,
0x265e,0x0180,
0x2660,0x0780,
0x2662,0x0180,
0x2664,0x0780,
0x2666,0x04cf,
0x2668,0x0337,
0x266a,0xf636,
0x266c,0xf650,
0x266e,0xf5c8,
0x2670,0xf5dc,
0x2672,0xf5f0,
0x2674,0xf604,
0x2676,0x0100,
0x2678,0xff8a,
0x267a,0xffff,
0x267c,0x0104,
0x267e,0xff0a,
0x2680,0xffff,
0x2682,0x0108,
0x2684,0xff02,
0x2686,0xffff,
0x2688,0x010c,
0x268a,0xff82,
0x268c,0xffff,
0x268e,0x0004,
0x2690,0xf676,
0x2692,0xe4e4,
0x2694,0x4e4e,
0x2ffe,0xc114,
0x3224,0xf222,
0x322a,0xf23c,
0x3230,0xf03a,
0x3238,0xf09a,
0x323a,0xf012,
0x323e,0xf3f6,
0x32a0,0x0000,
0x32a2,0x0000,
0x32a4,0x0000,
0x32b0,0x0000,
0x32c0,0xf66a,
0x32c2,0xf66e,
0x32c4,0x0000,
0x32c6,0xf66e,
0x32c8,0x0000,
0x32ca,0xf68e,
//----- Initial -----//
0x0a7e,0x219c,
0x3244,0x8400,
0x3246,0xe400,
0x3248,0xc88e,
0x324e,0xfcd8,
0x3250,0xa060,
0x325a,0x7a37,
0x0734,0x4b0b,
0x0736,0xd8b0,
0x0600,0x1190,
0x0602,0x0052,
0x0604,0x1008,
0x0606,0x0200,
0x0616,0x0040,
0x0614,0x0040,
0x0612,0x0040,
0x0610,0x0040,
0x06b2,0x0500,
0x06b4,0x3ff0,
0x0618,0x0a80,
0x0668,0x4303,
0x06ca,0x02cc,
0x066e,0x0050,
0x0670,0x0050,
0x113c,0x0001,
0x11c4,0x1080,
0x11c6,0x0c34,
0x1104,0x0160,
0x1106,0x0138,
0x110a,0x010e,
0x110c,0x021d,
0x110e,0xba2e,
0x1110,0x0056,
0x1112,0x00ac,
0x1114,0x6907,
0x1122,0x0011,
0x1124,0x0022,
0x1126,0x2e8c,
0x1128,0x0016,
0x112a,0x002b,
0x112c,0x3483,
0x1130,0x0200,
0x1132,0x0200,
0x1102,0x0028,
0x113e,0x0200,
0x0d00,0x4000,
0x0d02,0x8004,
0x120a,0x0a00,
0x0214,0x0200,
0x0216,0x0200,
0x0218,0x0200,
0x021a,0x0200,
0x1000,0x0300,
0x1002,0xc319,
0x105a,0x0091,
0x105c,0x0f08,
0x105e,0x0000,
0x1060,0x0008,
0x1062,0x0000,
0x0202,0x0200,
0x0b10,0x400c,
0x0212,0x0000,
0x035e,0x0701,
0x040a,0x0000,
0x0420,0x0003,
0x0424,0x0c47,
0x0418,0x1010,
0x0740,0x004f,
0x0354,0x1000,
0x035c,0x0303,
0x050e,0x0000,
0x0510,0x0058,
0x0512,0x0058,
0x0514,0x0050,
0x0516,0x0050,
0x0260,0x0003,
0x0262,0x0700,
0x0266,0x0007,
0x0250,0x0000,
0x0258,0x0002,
0x025c,0x0002,
0x025a,0x03e8,
0x0256,0x0100,
0x0254,0x0001,
0x0440,0x000c,
0x0908,0x0003,
0x0708,0x2f00,
0x0c00,0x0021,
0x027e,0x0100,
};
#endif

static void sensor_init(void)
{
#if MULTI_WRITE
	LOG_INF("sensor_init multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_init_hi1336,
		sizeof(addr_data_pair_init_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_preview_hi1336[] ={
0x3250,0xa470,
0x0730,0x4a0b,
0x0732,0xe1b0,
0x1118,0x0004,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x0100,
0x0b20,0x8200,
0x0f00,0x0400,
0x1002,0xc319,
0x103e,0x0100,
0x1020,0xc105,
0x1022,0x0616,
0x1024,0x0506,
0x1026,0x0808,
0x1028,0x1207,
0x102a,0x090a,
0x102c,0x1400,
0x1010,0x07d0,
0x1012,0x0022,
0x1014,0xffcf,
0x1016,0xffcf,
0x101a,0xffcf,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0015,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x002c,
0x022e,0x0c61,
0x0f04,0x0004,
0x0f06,0x0000,
0x023a,0x2222,
0x0234,0x3311,
0x0238,0x3311,
0x0246,0x0020,
0x020a,0x0cfe,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0d02,
0x0b12,0x0838,
0x0b14,0x0618,
0x0204,0x0200,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x0010,
0x0c18,0x1070,
0x0c1a,0x061a,
0x0b04,0x037c,
} ;
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	LOG_INF("preview_setting multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_preview_hi1336,
		sizeof(addr_data_pair_preview_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_capture_30fps_hi1336[] = {
0x3250,0xa060,
0x0730,0x4a0b,
0x0732,0xe0b0,
0x1118,0x0006,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x0100,
0x0b20,0x8100,
0x0f00,0x0000,
0x1002,0xc319,
0x103e,0x0000,
0x1020,0xc10a,
0x1022,0x0b2c,
0x1024,0x050c,
0x1026,0x100e,
0x1028,0x190b,
0x102a,0x1311,
0x102c,0x2400,
0x1010,0x07d0,
0x1012,0x004c,
0x1014,0xffd2,
0x1016,0xffd2,
0x101a,0xffd2,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0017,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x002e,
0x022e,0x0c61,
0x0f04,0x0008,
0x0f06,0x0000,
0x023a,0x1111,
0x0234,0x1111,
0x0238,0x1111,
0x0246,0x0020,
0x020a,0x0cfe,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0d02,
0x0b12,0x1070,
0x0b14,0x0c30,
0x0204,0x0000,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x0010,
0x0c18,0x1070,
0x0c1a,0x0c32,
0x0b04,0x037c,
};

#endif


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! %s currefps:%d\n",__func__,currefps);
#if MULTI_WRITE
	LOG_INF("capture_setting multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_hi1336,
		sizeof(addr_data_pair_capture_30fps_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_video_hi1336[] = {
0x3250,0xa060,
0x0730,0x4a0b,
0x0732,0xe0b0,
0x1118,0x017e,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x0100,
0x0b20,0x8100,
0x0f00,0x0000,
0x1002,0xc319,
0x103e,0x0000,
0x1020,0xc10a,
0x1022,0x0b2c,
0x1024,0x050c,
0x1026,0x100e,
0x1028,0x190b,
0x102a,0x1311,
0x102c,0x2400,
0x1010,0x07d0,
0x1012,0x004c,
0x1014,0xffd2,
0x1016,0xffd2,
0x101a,0xffd2,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0017,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x01a6,
0x022e,0x0ae9,
0x0f04,0x0008,
0x0f06,0x0000,
0x023a,0x1111,
0x0234,0x1111,
0x0238,0x1111,
0x0246,0x0020,
0x020a,0x0cfe,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0d02,
0x0b12,0x1070,
0x0b14,0x0940,
0x0204,0x0000,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x0010,
0x0c18,0x1070,
0x0c1a,0x0942,
0x0b04,0x037c,
};
#endif

static void video_setting(void)
{
#if MULTI_WRITE
	LOG_INF("video_setting multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_video_hi1336,
		sizeof(addr_data_pair_video_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_hs_video_hi1336[] = {
0x3250,0xa470,
0x0730,0x4a0b,
0x0732,0xe1b0,
0x1118,0x01aa,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x00f0,
0x0b20,0x8200,
0x0f00,0x0400,
0x1002,0xc319,
0x103e,0x0100,
0x1020,0xc105,
0x1022,0x0616,
0x1024,0x0506,
0x1026,0x0808,
0x1028,0x1207,
0x102a,0x090a,
0x102c,0x1400,
0x1010,0x07d0,
0x1012,0x005b,
0x1014,0xffeb,
0x1016,0xffeb,
0x101a,0xffeb,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0015,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x020c,
0x022e,0x0a81,
0x0f04,0x0060,
0x0f06,0x0000,
0x023a,0x2222,
0x0234,0x3311,
0x0238,0x3311,
0x0246,0x0020,
0x020a,0x067d,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0681,
0x0b12,0x0780,
0x0b14,0x0438,
0x0204,0x0200,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x00c0,
0x0c18,0x0f00,
0x0c1a,0x043a,
0x0b04,0x037c,
};
#endif

static void hs_video_setting(void)
{
#if MULTI_WRITE
	hi1336_table_write_cmos_sensor(
		addr_data_pair_hs_video_hi1336,
		sizeof(addr_data_pair_hs_video_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_slim_video_hi1336[] = {
0x3250,0xa060,
0x0730,0x770f,
0x0732,0xe2b0,
0x1118,0x01a8,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x0100,
0x0b20,0x8300,
0x0f00,0x0800,
0x1002,0xc319,
0x103e,0x0200,
0x1020,0xc104,
0x1022,0x0411,
0x1024,0x0305,
0x1026,0x0608,
0x1028,0x1206,
0x102a,0x0704,
0x102c,0x1400,
0x1010,0x07d0,
0x1012,0x009c,
0x1014,0x0013,
0x1016,0x0013,
0x101a,0x0013,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0017,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x020a,
0x022e,0x0a83,
0x0f04,0x0040,
0x0f06,0x0000,
0x023a,0x3333,
0x0234,0x3333,
0x0238,0x3333,
0x0246,0x0020,
0x020a,0x033c,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0340,
0x0b12,0x0500,
0x0b14,0x02d0,
0x0204,0x0000,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x00c0,
0x0c18,0x0f00,
0x0c1a,0x02d2,
0x0b04,0x037c,
};
#endif


static void slim_video_setting(void)
{

#if MULTI_WRITE
	LOG_INF("slim_video_setting multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_slim_video_hi1336,
		sizeof(addr_data_pair_slim_video_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_custom1_hi1336[] = {
0x3250,0xa060,
0x0730,0x4a0b,
0x0732,0xe0b0,
0x1118,0x0006,
0x1200,0x011f,
0x1204,0x1c01,
0x1240,0x0100,
0x0b20,0x8100,
0x0f00,0x0000,
0x1002,0xc319,
0x103e,0x0000,
0x1020,0xc10a,
0x1022,0x0b2c,
0x1024,0x050c,
0x1026,0x100e,
0x1028,0x190b,
0x102a,0x1311,
0x102c,0x2400,
0x1010,0x07d0,
0x1012,0x004c,
0x1014,0xffd2,
0x1016,0xffd2,
0x101a,0xffd2,
0x0404,0x0008,
0x0406,0x1087,
0x0220,0x0008,
0x022a,0x0017,
0x0222,0x0c80,
0x022c,0x0c89,
0x0224,0x002e,
0x022e,0x0c61,
0x0f04,0x0008,
0x0f06,0x0000,
0x023a,0x1111,
0x0234,0x1111,
0x0238,0x1111,
0x0246,0x0020,
0x020a,0x0cfe,
0x021c,0x0008,
0x0206,0x05dd,
0x020e,0x0d02,
0x0b12,0x1070,
0x0b14,0x0c30,
0x0204,0x0000,
0x041c,0x0048,
0x041e,0x1047,
0x0c14,0x0010,
0x0c18,0x1070,
0x0c1a,0x0c32,
0x0b04,0x037c,
};
#endif
//-bug767771,liangyiyi.wt,MODIFY,2022/09/20,modify hi1336 592M mipi clk and 2mA mclk for RF interference
static void custom1_setting(void){
#if MULTI_WRITE
	LOG_INF("custom1_setting multi write\n");
	hi1336_table_write_cmos_sensor(
		addr_data_pair_custom1_hi1336,
		sizeof(addr_data_pair_custom1_hi1336) /
		sizeof(kal_uint16));
#else

#endif
}

//+bug767771, liangyiyi.wt, ADD, 2022/07/21, add hi1336 otp bringup code.
#if ENABLE_OTP
#define GROUP_FLAG 0x0400
#define GROUP1_ADDR 0x0401
#define GROUP2_ADDR 0x0B75

#define SN_INFO_SIZE 12
#define MODULE_INFO_SIZE 7
#define AWB_DATA_SIZE 16
#define LSC_DATA_SIZE 1868

#define OTP_READ_ADDR 0x0308

static unsigned char hi1336_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_lsc[LSC_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_awb[AWB_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char hi1336_data_module[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/

#if HI1336_OTP_DUMP
static void dumpEEPROMData1(int u4Length,u8* pu1Params)
{
	int i = 0;
	for(i = 0; i < u4Length; i += 16){
		if(u4Length - i  >= 16){
			LOG_INF("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
			i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
			,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
			,pu1Params[i+15]);
		}else{
			int j = i;
			for(;j < u4Length;j++)
			LOG_INF("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
		}
	}
	LOG_INF("\n");
}
#endif

static int hi1336_sensor_otp_info(void)
{
	uint8_t flag;
	int result = 0;
	int check_sum_cal = 0;
	uint16_t Address_start;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i = 0;

	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	/* 1.sensor init */
	write_cmos_sensor(0x0b00, 0x0000);
	write_cmos_sensor(0x2000, 0x0021);
	write_cmos_sensor(0x2002, 0x04A5);
	write_cmos_sensor(0x2004, 0x0004);
	write_cmos_sensor(0x2006, 0xC09C);
	write_cmos_sensor(0x2008, 0x0064);
	write_cmos_sensor(0x200A, 0x088E);
	write_cmos_sensor(0x200C, 0x01C2);
	write_cmos_sensor(0x200E, 0x00B4);
	write_cmos_sensor(0x2010, 0x4020);
	write_cmos_sensor(0x2012, 0x90F2);
	write_cmos_sensor(0x2014, 0x0010);
	write_cmos_sensor(0x2016, 0x0260);
	write_cmos_sensor(0x2018, 0x2002);
	write_cmos_sensor(0x201A, 0x12B0);
	write_cmos_sensor(0x201C, 0xD4AA);
	write_cmos_sensor(0x201E, 0x12B0);
	write_cmos_sensor(0x2020, 0xD5FA);
	write_cmos_sensor(0x2022, 0x4392);
	write_cmos_sensor(0x2024, 0x732A);
	write_cmos_sensor(0x2026, 0x4130);
	write_cmos_sensor(0x2FFE, 0xC114);
	write_cmos_sensor(0x3224, 0xF012);
	write_cmos_sensor(0x32A0, 0x0000);
	write_cmos_sensor(0x32A2, 0x0000);
	write_cmos_sensor(0x32A4, 0x0000);
	write_cmos_sensor(0x32B0, 0x0000);
	write_cmos_sensor(0x32C0, 0x0000);
	write_cmos_sensor(0x32C2, 0x0000);
	write_cmos_sensor(0x32C4, 0x0000);
	write_cmos_sensor(0x32C6, 0x0000);
	write_cmos_sensor(0x32C8, 0x0000);
	write_cmos_sensor(0x32CA, 0x0000);
	write_cmos_sensor(0x0734, 0x4b0b);
	write_cmos_sensor(0x0736, 0xd8b0);
	write_cmos_sensor(0x035e, 0x0701);
	write_cmos_sensor(0x027e, 0x0100);

	/* 2.otp read setting */
	write_cmos_sensor_8(0x0b02, 0x01);
	write_cmos_sensor_8(0x0809, 0x00);
	write_cmos_sensor_8(0x0b00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x0260, 0x10);
	write_cmos_sensor_8(0x0809, 0x01);
	write_cmos_sensor_8(0x0b00, 0x01);
	mdelay(1); // sleep 1msec

	/* 3.otp read flag */
	write_cmos_sensor_8(0x030a, 0x04);
	write_cmos_sensor_8(0x030b, 0x00);
	write_cmos_sensor_8(0x0302, 0x01); //read enable
	flag= read_cmos_sensor(0x0308); //eeprom address:0x0400
	LOG_INF("HI1336 OTP flag(0x%x)\n",flag);
	if (flag == 0x01)
	{
		Address_start=GROUP1_ADDR;
		LOG_INF("HI1336 OTP USR GROUP1\n");
	}else{
	    Address_start=GROUP2_ADDR;
		LOG_INF("HI1336 OTP USR GROUP2\n");
	}

	/* 4.read module info */
	write_cmos_sensor_8(0x030a, ((Address_start+i) >> 8)&0xff);
	write_cmos_sensor_8(0x030b, (Address_start+i)&0xff);
	write_cmos_sensor_8(0x0302, 0x01); //read enable
	check_sum_cal = 0;
	for (i =0; i <MODULE_INFO_SIZE; i++) {
		hi1336_data_module[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_module[i];
	}
	hi1336_data_module[MODULE_INFO_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	LOG_INF("=== HI1336 INFO module_id=0x%x position=0x%x ===\n", hi1336_data_module[0], hi1336_data_module[1]);
	LOG_INF("=== HI1336 INFO lens_id=0x%x,vcm_id=0x%x ===\n",hi1336_data_module[2], hi1336_data_module[3]);
	LOG_INF("=== HI1336 INFO date is %d-%d-%d ===\n",hi1336_data_module[4],hi1336_data_module[5],hi1336_data_module[6]);
	LOG_INF("=== HI1336 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", hi1336_data_module[7], check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(MODULE_INFO_SIZE,&hi1336_data_module[0]);
	#endif
	if(check_sum_cal != hi1336_data_module[MODULE_INFO_SIZE])
	{
		LOG_INF("HI1336 read module info failed!!!\n");
		result = -1;
		goto function_exit;
	}

	/* 5.read sn info */
	check_sum_cal = 0;
	for (i =0; i <SN_INFO_SIZE; i++) {
		hi1336_data_sn[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_sn[i];
		LOG_INF("=== HI1336 SN[%d] is 0x%x ===\n",i,hi1336_data_sn[i]);
	}
	hi1336_data_sn[SN_INFO_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	if(check_sum_cal != hi1336_data_sn[SN_INFO_SIZE])
	{
		LOG_INF("HI1336 read sn info failed!!!\n");
		//result = -1;
	}

	/* 6.read awb info */
	check_sum_cal = 0;
	for (i =0; i <AWB_DATA_SIZE; i++) {
		hi1336_data_awb[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_awb[i];
	}
	hi1336_data_awb[AWB_DATA_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	r = ((hi1336_data_awb[1]<<8)&0xff00)|(hi1336_data_awb[0]&0xff);
	b = ((hi1336_data_awb[3]<<8)&0xff00)|(hi1336_data_awb[2]&0xff);
	gr = ((hi1336_data_awb[5]<<8)&0xff00)|(hi1336_data_awb[4]&0xff);
	gb = ((hi1336_data_awb[7]<<8)&0xff00)|(hi1336_data_awb[6]&0xff);
	golden_r = ((hi1336_data_awb[9]<<8)&0xff00)|(hi1336_data_awb[8]&0xff);
	golden_b = ((hi1336_data_awb[11]<<8)&0xff00)|(hi1336_data_awb[10]&0xff);
	golden_gr = ((hi1336_data_awb[13]<<8)&0xff00)|(hi1336_data_awb[12]&0xff);
	golden_gb = ((hi1336_data_awb[15]<<8)&0xff00)|(hi1336_data_awb[14]&0xff);
	LOG_INF("=== HI1336 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== HI1336 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== HI1336 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",hi1336_data_awb[AWB_DATA_SIZE],check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(AWB_DATA_SIZE,&hi1336_data_awb[0]);
	#endif
	if(check_sum_cal != hi1336_data_awb[AWB_DATA_SIZE])
	{
		LOG_INF("HI1336 read AWB info failed!!!\n");
		result = -1;
		goto function_exit;
	}

	/* 7.read lsc info */
	check_sum_cal = 0;
	for (i =0; i <LSC_DATA_SIZE; i++) {
		hi1336_data_lsc[i] = read_cmos_sensor(OTP_READ_ADDR);	//otp data read
		check_sum_cal += hi1336_data_lsc[i];
	}
	hi1336_data_lsc[LSC_DATA_SIZE] = read_cmos_sensor(OTP_READ_ADDR); // module checksum_value
	check_sum_cal = (check_sum_cal % 255) + 1;
	LOG_INF("=== HI1336 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", hi1336_data_lsc[LSC_DATA_SIZE], check_sum_cal);
	#if HI1336_OTP_DUMP
	dumpEEPROMData1(LSC_DATA_SIZE,&hi1336_data_lsc[0]);
	#endif
	if(check_sum_cal != hi1336_data_lsc[LSC_DATA_SIZE])
	{
		LOG_INF("HI1336 read lsc info failed!!!\n");
		result = -1;
		goto function_exit;
	}

function_exit:
	write_cmos_sensor_8(0x0809, 0x00); // stream off
	write_cmos_sensor_8(0x0b00, 0x00); // stream off
	mdelay(10); // sleep 10msec
	write_cmos_sensor_8(0x0260, 0x00); // OTP mode display
	write_cmos_sensor_8(0x0809, 0x01); // stream on
	write_cmos_sensor_8(0x0b00, 0x01); // stream on
	mdelay(1);
	LOG_INF("%s Exit \n",__func__);
	return result;
}

#if 0
//+bug682590,zhanghengyuan.wt,ADD,2021/8/24,diff txd and st hi1336
static int read_hi1336_otp_moudle(void){

	int addr = imgsensor.i2c_write_id;
	char moudle_id;

	imgsensor.i2c_write_id = 0xA0;
	moudle_id = read_cmos_sensor(0x01);
	imgsensor.i2c_write_id = addr;

	if(moudle_id == 0x19){
		printk("this is txd hi1336 moudle\n");
		return 1;
	}

	return 0;
}
//-bug682590,zhanghengyuan.wt,ADD,2021/8/24,diff txd and st hi1336
#endif


//+bug682590,zhanghengyuan.wt,ADD,2021/8/25,add w2hi1336fronttruly OTP
#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT w2hi1336fronttruly_eeprom_data ={
	.sensorID= W2HI1336FRONTTRULY_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength =  0x0766,
	.sensorVendorid = 0x06050100,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
//-bug682590,zhanghengyuan.wt,ADD,2021/8/25,add w2hi1336fronttruly OTP
#endif
//-bug767771, liangyiyi.wt, ADD, 2022/07/21, add hi1336 otp bringup code.

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5; //bug 682590, zhanghao2.wt, Modify, 2021/11/05, modify main camera i2c speed 400k to 1M.
#if ENABLE_OTP
	int ret;
#endif

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
#if ENABLE_OTP
//+bug767771, liangyiyi.wt, ADD, 2022/07/21, add hi1336 otp bringup code.
//+bug682590,zhanghengyuan.wt,ADD,2021/8/24,diff txd and st hi1336
				ret = hi1336_sensor_otp_info();
				if(ret<0){
					pr_err("get eeprom moudle failed\n");
					//*sensor_id = 0xFFFFFFFF;
					//return ERROR_SENSOR_CONNECT_FAIL;
				}else{
					pr_info("get eeprom data success\n");
					w2hi1336fronttruly_eeprom_data.dataBuffer = kmalloc(w2hi1336fronttruly_eeprom_data.dataLength, GFP_KERNEL);
					if (w2hi1336fronttruly_eeprom_data.dataBuffer == NULL) {
						LOG_INF("w2hi1336fronttruly_eeprom_data->dataBuffer is malloc fail\n");
						return -EFAULT;
					}

					memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer, (u8 *)&hi1336_data_module, MODULE_INFO_SIZE);
					memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer+MODULE_INFO_SIZE, (u8 *)&hi1336_data_awb, AWB_DATA_SIZE);
					memcpy(w2hi1336fronttruly_eeprom_data.dataBuffer+MODULE_INFO_SIZE+AWB_DATA_SIZE, (u8 *)&hi1336_data_lsc, LSC_DATA_SIZE);

					imgSensorSetEepromData(&w2hi1336fronttruly_eeprom_data);
				}
//-bug682590,zhanghengyuan.wt,ADD,2021/8/25,add w2hi1336fronttruly OTP
//-bug767771, liangyiyi.wt, ADD, 2022/07/06, add hi1336 eeprom bringup code.
#endif
				pr_err("i2c write id : 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 5; //bug 682590, zhanghao2.wt, Modify, 2021/11/05, modify main camera i2c speed 400k to 1M.
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		pr_err("Read id fail,sensor id: 0x%x\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5; //bug 682590, zhanghao2.wt, Modify, 2021/11/05, modify main camera i2c speed 400k to 1M.
	kal_uint16 sensor_id = 0;

	LOG_INF("[open]: PLATFORM:MT6737,MIPI 24LANE\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_err("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 5; //bug 682590, zhanghao2.wt, Modify, 2021/11/05, modify main camera i2c speed 400k to 1M.
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		pr_err("open sensor id fail: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	//imgsensor.pdaf_mode = 1;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	video_setting();
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}    /*    slim_video     */
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
								MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}   /*  Custom1	*/

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;
	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
	imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;    // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
	sensor_info->PDAF_Support = 1;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;
	default:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}

	return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("preview\n");
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("capture\n");
	//case MSDK_SCENARIO_ID_CAMERA_ZSD:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("video preview\n");
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data); // Custom1
		break;
	default:
		LOG_INF("default mode\n");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
				scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
				imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap1.framelength) ?
			(frame_length - imgsensor_info.cap1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
			LOG_INF("fps %d fps not support,use cap: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
	    frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.custom1.framelength) ? (frame_length -
			imgsensor_info.custom1.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.custom1.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;	
	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	    LOG_INF("error scenario_id = %d, we use preview scenario\n",
				scenario_id);
	break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
				enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    *framerate = imgsensor_info.pre.max_framerate;
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    *framerate = imgsensor_info.normal_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    *framerate = imgsensor_info.cap.max_framerate;
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    *framerate = imgsensor_info.hs_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    *framerate = imgsensor_info.slim_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
	    *framerate = imgsensor_info.custom1.max_framerate;
	break;
	default:
	break;
	}

	return ERROR_NONE;
}


static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("set_test_pattern_mode enable: %d", enable);
	if (enable) {
		//write_cmos_sensor(0x1038, 0x0000); //mipi_virtual_channel_ctrl
		//write_cmos_sensor(0x1042, 0x0008); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x37d);
		write_cmos_sensor(0x0C0A, 0x0100);

	} else {
		//write_cmos_sensor(0x1038, 0x0000); //mipi_virtual_channel_ctrl
		//write_cmos_sensor(0x1042, 0x0008); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x037c);
		write_cmos_sensor(0x0C0A, 0x0000);

	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
//-bug767771,liangyiyi.wt,MODIFY,2022/09/20,modify for hi1336 Configure correct MIPI_ Data_ Rate and exposure line


static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0b00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0b00, 0x0000); // stream off

	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
    INT32 temperature_convert = 25;
    return temperature_convert;
}

static kal_uint32 feature_control(
			MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

#if ENABLE_PDAF
    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif

	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
	    *feature_return_para_16++ = imgsensor.line_length;
	    *feature_return_para_16 = imgsensor.frame_length;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	    *feature_return_para_32 = imgsensor.pclk;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*feature_return_para_32 = 1; /*BINNING_NONE*/
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (*(feature_data + 2))/* HDR on */
				*feature_return_para_32 = 1;/*BINNING_NONE*/
			else
				*feature_return_para_32 = 1;/*BINNING_AVERAGED*/
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d\n",
			*feature_return_para_32);

		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		* 1, if driver support new sw frame sync
		* set_shutter_frame_length() support third para auto_extend_en
		*/
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
	    set_shutter(*feature_data);
	break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    night_mode((BOOL) * feature_data);
	break;
	case SENSOR_FEATURE_SET_GAIN:
	    set_gain((UINT16) *feature_data);
	break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	break;
	case SENSOR_FEATURE_SET_REGISTER:
	    write_cmos_sensor(sensor_reg_data->RegAddr,
						sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
	    *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	    set_video_mode(*feature_data);
	break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
	    get_imgsensor_id(feature_return_para_32);
	break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
	    set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
	break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	    set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
	break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	    get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
	break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
        	LOG_INF("current fps :%d\n", imgsensor.current_fps);
	break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT32) *feature_data, (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_SET_HDR:
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (BOOL)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    /*LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));*/
	#if 0
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	#endif
	break;
	//+bug767771, liangyiyi.wt, ADD, 2022/8/31, modify for fixed sensor_fusion of its test fail
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
	    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1017500;
	    break;
	//-bug767771, liangyiyi.wt, ADD, 2022/8/31, modify for fixed sensor_fusion of its test fail
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.cap.pclk;
                break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.normal_video.pclk;
                break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.hs_video.pclk;
                break;
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.slim_video.pclk;
                break;
            case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.custom1.pclk;
                break;
            case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.custom2.pclk;
                break;
            case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.custom3.pclk;
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                    = imgsensor_info.pre.pclk;
                break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom1.framelength << 16)
                + imgsensor_info.custom1.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom2.framelength << 16)
                + imgsensor_info.custom2.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom3.framelength << 16)
                + imgsensor_info.custom3.linelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
    {
        kal_uint32 rate;

        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            rate = imgsensor_info.cap.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            rate = imgsensor_info.normal_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            rate = imgsensor_info.hs_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            rate = imgsensor_info.slim_video.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            rate = imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
        LOG_INF("hi1336 SENSOR_FEATURE_GET_MIPI_PIXEL_RATE");
        *(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
    }
    break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
#if ENABLE_PDAF
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		//PDAF capacity enable or not, 2p8 only full size support PDAF
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // type2 - VC enable
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // video & capture use same setting
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_info("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n", *feature_data);
		PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info_4208_2368,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		pr_info("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
#endif

	default:
	break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 W2HI1336FRONTTRULY_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	W2HI1336FRONTTRULY_MIPI_RAW_SensorInit	*/
