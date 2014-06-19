/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
*
* Copyright (c) 2012 Maxim Integrated Products.
* All rights reserved.
*
*******************************************************************************/

/*

Heres my mapping. first column is the connector pin, second the processor pin
(if TQFP) and the mux functions I can use

On 40 pins connector:
 3     19    GPMI_CLE    NAND_CLE    LCD_D16	* CS PIN *
 4     20    GPMI_ALE    NAND_ALE    LCD_D17
 5     28    GPMI_D07    NAND_D07    LCD_D15        SSP2_D7
 6     17    LCD_DOT     CK          GPMI_RDY3
 7     29    GPMI_D06    NAND_D06    LCD_D14        SSP2_D6
 8     16    LCD_VSYNC   LCD_BUSY
 9     26    GPMI_D05    NAND_D05    LCD_D13        SSP2_D5
10     15    LCD_HSYNC
11     27    GPMI_D04    NAND_D04    LCD_D12        SSP2_D4
12     11    LCD_ENABLE
13     25    GPMI_D03    NAND_D03    LCD_D11        SSP2_D3
14     12    LCD_RESET   GPMI_CE3N
15     24    GPMI_D02    NAND_D02    LCD_D10        SSP2_D2
16     13    LCD_WR
17     23    GPMI_D01    NAND_D01    LCD_D09        SSP2_D1
18     14    LCD_RS
19     22    GPMI_D00    NAND_D00    LCD_D08        SSP2_D0
20     10    LCD_CS
21      9    LCD_D07
22     91    PWM2        GPMI_RDY3
23      8    LCD_D06
24     31    GPMI_RND
25      7    LCD_D05
26     34    GPMI_WPN
27      6    LCD_D04
28     81    GPMI_CE1N
29      5    LCD_D03
30     82    GPMI_CE0N
31      4    LCD_D02
32    107    LRADC1
33      3    LCD_D01
34    108    LRADC0
35      2    LCD_D00

On 10 pins connector alongside:
3     127    UART1_TXD   GPMI_RDY2
4     128    UART1_RX    GPMI_CE2N
5      34    GPMI_WPN
6      31    GPMI_RDN
7      22    GPMI_D00    NAND_D00    SSP2_DATA0
8      21    GPMI_RDY1   SSP2_CMD    SSP2_MOSI
9      33    GPMI_WRN    SSP2_SCK

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "c_signal.h"
#include "fifo_declare.h"
#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "mxuvc.h"

int verbose = 0;
int spi_fd = -1;
int output_fd = -1;
int video_channel = 0;

typedef struct spi_buffer_t {
	uint8_t	buf_index;
	void * buffer;
	uint32_t size;
} spi_buffer_t, *spi_buffer_p;

DECLARE_FIFO(spi_buffer_t, spi_fifo, 16);
DEFINE_FIFO(spi_buffer_t, spi_fifo);

spi_fifo_t	fifo_out;	// to the SPI thread
spi_fifo_t	fifo_in;	// back from the SPI thread

c_signal_p	spi_signal = NULL;

c_time_t	base = 0;
c_time_t	usb_stamp = 0;
uint32_t	usb_total = 0;
c_time_t	spi_stamp = 0;
uint32_t	spi_total = 0;

static void *
spi_thread(void * ignore)
{
	printf("%s started\n", __func__);
	do {
		c_signal_wait(spi_signal, C_SIGNAL_END1, C_TIME_SECOND);
		while (!spi_fifo_isempty(&fifo_out)) {
			c_time_t start = c_time_get_stamp(&base);
			spi_buffer_t b = spi_fifo_read(&fifo_out);
			if (verbose > 2)
				printf("%s out buffer %d size %5d\n", __func__, b.buf_index, b.size);

			if (spi_fd != -1) {
				struct spi_ioc_transfer tx = {
						.delay_usecs = 0,
						.speed_hz = 0,
						.bits_per_word = 8,
						.tx_buf = (__u64)b.buffer,
						.len = b.size,
				};
				int r = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tx);
				if (r == -1)
					perror(__func__);
			} else if (output_fd != -1) {
				ssize_t w = write(output_fd, b.buffer, b.size);
				if (w != b.size) {
					perror(__func__);
					exit(1);
				}
			} else
				usleep(10000);
			spi_fifo_write(&fifo_in, b);

			c_time_t now = c_time_get_stamp(&base);
			spi_stamp += now - start;
			spi_total += b.size;
		}
	} while(1);
	return NULL;
}

static void
ch_cb(
		unsigned char *buffer,
		unsigned int size,
		video_info_t info,
		void *user_data)
{
	/*
	 * Queue the buffer, and signal
	 * the other thread to process output it.
	 */
	spi_buffer_t buf = {
			.buffer = buffer,
			.size = size,
			.buf_index = info.buf_index,
	};
	spi_fifo_write(&fifo_out, buf);
	c_signal(spi_signal, C_SIGNAL_END0, 1);

	usb_total += size;
	c_time_t now = c_time_get_stamp(&base);
	if (!usb_stamp)
		usb_stamp = now;
	c_time_t t = now - usb_stamp;
	if (t >= C_TIME_SECOND) {
		c_time_t out_time = spi_stamp + 1;
		uint32_t out_total = spi_total;
		if (out_time > C_TIME_SECOND) {
			out_time = 0;
			out_total = 0;
		}
		if (verbose) {
			uint32_t usb = usb_total / ((float)t / (float)C_TIME_SECOND);
			uint32_t out = out_total / ((float)out_time / (float)C_TIME_SECOND);

			printf("usb speed %4dKB/s output %5dKB/s\n", usb / 1024, out / 1024);
		}
		usb_stamp = now;
		usb_total = 0;
	}
	if (verbose > 2) {
		printf("%s buffer %d size %5d ", __func__, info.buf_index, size);
		for (int i = 0; i < 16; i++)
			printf("%02x", buffer[i]);
		printf("\n");
	}
	while (!spi_fifo_isempty(&fifo_in)) {
		spi_buffer_t b = spi_fifo_read(&fifo_in);
		if (verbose > 2)
			printf("%s freeing buffer %d\n", __func__, b.buf_index);
		mxuvc_video_cb_buf_done(video_channel, b.buf_index);
	}
//	fwrite(buffer, size, 1, fd[ch][fmt]);
}

static const char *format_name[NUM_VID_FORMAT] = {
		[VID_FORMAT_H264_RAW]		= "VID_FORMAT_H264_RAW",
		[VID_FORMAT_H264_TS]		= "VID_FORMAT_H264_TS",
		[VID_FORMAT_H264_AAC_TS]	= "VID_FORMAT_H264_AAC_TS",
		[VID_FORMAT_MUX]			= "VID_FORMAT_MUX",
		[VID_FORMAT_MJPEG_RAW]		= "VID_FORMAT_MJPEG_RAW",
		[VID_FORMAT_YUY2_RAW]		= "VID_FORMAT_YUY2_RAW",
		[VID_FORMAT_NV12_RAW]		= "VID_FORMAT_NV12_RAW",
		[VID_FORMAT_GREY_RAW]		= "VID_FORMAT_GREY_RAW",
};

int
main(
		int argc,
		char **argv)
{
	int ret = 0;
//	int count=0;
	video_format_t fmt;
	uint16_t width, height;
	uint32_t framerate = 0;
//	int goplen = 0;
	int value;
//	uint32_t comp_q;
	noise_filter_mode_t sel;
	white_balance_mode_t bal;
	pwr_line_freq_mode_t freq;
	zone_wb_set_t wb;
	const char * spidev = NULL;
	uint32_t spi_speed = 16000;	// 16Mhz default
	const char * filename = NULL;
	int counter = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--spi") && i < argc-1)
			spidev = argv[++i];
		else if (!strcmp(argv[i], "-o") && i < argc-1)
			filename = argv[++i];
		else if (!strcmp(argv[i], "--spi_khz") && i < argc-1)
			spi_speed = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-w") && i < argc-1)
			counter = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
			goto usage;
		else if (!strcmp(argv[i], "-v"))
			verbose++;
	}
	if (0) {
		usage:
		printf("%s [--spi </dev/spidevX.X>] output on spi\n", argv[0]);
		printf("\t[--spi_khz] : spi speed (default 500)\n");
		printf("\t[-v] : Increment verbosity\n");
		printf("\t[-w <seconds>] : Only runs for <seconds> then exit\n");
		exit(0);
	}
	/* Initialize camera */
	ret = mxuvc_video_init("v4l2", "dev_offset=0");
	if (ret < 0) {
		fprintf(stderr, "%s mxuvc_video_init error\n", argv[0]);
		exit(1);
	}

	ret = mxuvc_video_get_nf(CH1, &sel, (uint16_t *) &value);
	ret = mxuvc_video_get_wb(CH1, &bal, (uint16_t *) &value);
	ret = mxuvc_video_get_pwr_line_freq(CH1, &freq);
	printf("Power line freq: %d\n", freq);
	value = 0;
	mxuvc_video_get_zone_wb(CH1, &wb, (uint16_t *) &value);
	printf("wb %d %d\n", wb, (uint16_t) value);

	ret = mxuvc_video_set_format(CH1, VID_FORMAT_H264_TS);
	if (ret)
		printf("mxuvc_video_set_format failed error %d\n", ret);
	ret = mxuvc_video_set_resolution(CH1, 1920, 1080);
	ret = mxuvc_video_set_profile(CH1, PROFILE_HIGH);
	ret = mxuvc_video_set_bitrate(CH1, 5000000);

	ret = mxuvc_video_get_format(CH1, &fmt);
	if (ret < 0)
		goto error;
	printf("Format: %s\n", format_name[fmt]);
	ret = mxuvc_video_get_resolution(CH1, (uint16_t *) &width,
			(uint16_t *) &height);
	if (ret < 0)
		goto error;
	printf("width %d height %d\n", width, height);
	mxuvc_video_get_framerate(CH1, &framerate);
	printf("framerate %dfps\n", framerate);

	uint32_t rate;
	mxuvc_video_get_bitrate(CH1, &rate);
	printf("bitrate %d\n", (int) rate);

	video_profile_t profile;
	ret = mxuvc_video_get_profile(CH1, &profile);
	if (ret < 0)
		goto error;
	printf("CH1 profile %d\n", profile);

	/* Register callback functions */
	ret = mxuvc_video_register_cb(CH1, ch_cb, (void*) CH1);
	if (ret < 0)
		goto error;

	spi_fifo_reset(&fifo_out);
	spi_fifo_reset(&fifo_in);
	spi_signal = c_signal_new();

	if (spidev) {
		spi_fd = open(spidev, O_RDWR);
		perror(spidev);
		if (spi_fd != -1) {
			uint8_t bits = 8;
			int ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
			if (ret == -1)
				perror("SPI: can't set bits per word");
			spi_speed *= 1000;
			ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
			if (ret == -1)
				perror("SPI: can't set max speed hz");
		}
	}
	if (filename) {
		output_fd = open(filename, O_WRONLY|O_TRUNC|O_CREAT, 0644);
		perror(filename);
	}
	pthread_t  th;
	pthread_create(&th, NULL, spi_thread, NULL);

	ret = mxuvc_video_start(CH1);
	if (ret < 0)
		goto error;

	usleep(5000);
	if (counter)
		printf("Capturing for %d seconds\n", counter);
	else
		printf("Start capture loop\n");
	while (!counter || counter--) {
		sleep(1);
		if (!mxuvc_video_alive())
			goto error;
	}

	/* Stop streaming */
	ret = mxuvc_video_stop(CH1);

	/* Deinitialize and exit */
	mxuvc_video_deinit();


	printf("Success\n");

	return 0;
error:
	mxuvc_video_deinit();
	printf("Failure\n");
	sleep(5);
	return 1;
}
