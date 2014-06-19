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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include "libusb/handle_events.h"
#include <mxuvc.h>
#include <alert.h>
#include <common.h>
#include <sys/stat.h>
#include <fcntl.h>

static struct libusb_device_handle *camera = NULL;
static struct libusb_context *ctxt = NULL;
static struct libusb_interface *intf = NULL;

int open_counter = 0;

typedef enum {
	FW_UPGRADE_START = 0x02,
	FW_UPGRADE_COMPLETE,//0x3
	START_TRANSFER,//0x4
	TRANSFER_COMPLETE,//0x5
	TX_IMAGE,//0x6
	GET_NVM_PGM_STATUS,//0x7
	GET_SNOR_IMG_HEADER,//0x8
	GET_EEPROM_CONFIG,//0x9
	GET_EEPROM_CONFIG_SIZE,//0xA
	REQ_GET_EEPROM_VALUE,//0xB
	GET_EEPROM_VALUE,//0xC
	SET_EEPROM_KEY_VALUE,//0xD
	REMOVE_EEPROM_KEY,//0xE
	SET_OR_REMOVE_KEY_STATUS,//0xF
	ERASE_EEPROM_CONFIG,//0x10
	SAVE_EEPROM_CONFIG,//0x11
	RESET_BOARD,//0x12
	I2C_WRITE,//0x13
	I2C_READ,//0x14
	I2C_WRITE_STATUS, //0x15
	I2C_READ_STATUS, //0x16
	TCW_WRITE,//0x17
	TCW_WRITE_STATUS,//0x18
	TCW_READ,//0x19
	TCW_READ_STATUS,//0x1A
	ISP_WRITE,//0x1B
	ISP_READ,//0x1C
	ISP_WRITE_STATUS,//0x1D
	ISP_READ_STATUS,//0x1E
    	ISP_ENABLE,//0x1F
    	ISP_ENABLE_STATUS,//0x20
	REQ_GET_CCRKEY_VALUE,//0x21
	GET_CCRKEY_VALUE, //0x22
	GET_CCR_SIZE, //0x23
	GET_CCR_LIST, //0x24
	GET_IMG_HEADER,//0x25
	GET_BOOTLOADER_HEADER, //0x26
	GET_CMD_BITMAP, //0x27
	CMD_WHO_R_U, //0x28	
	AV_ALARM, //0x29
	A_INTENSITY,//0x2A
	AUDIO_VAD, //0x2B
	GET_SPKR_VOL, //0x2C
	SET_SPKR_VOL, //0x2D	

	MEMTEST, //0x2E	
	MEMTEST_RESULT, //0x2F

	ADD_FONT, //0x30
	SEND_FONT_FILE, //0x31	
	FONT_FILE_DONE, //0x32
	ADD_TEXT, //0x33
	TEXT, //0x34
	REMOVE_TEXT, //0x35

	SHOW_TIME, //0x36
	HIDE_TIME, //0x37
	UPDATE_TIME, //0x38

	QHAL_VENDOR_REQ_START = 0x60,
	QCC_READ = QHAL_VENDOR_REQ_START,
	QCC_WRITE,
	QCC_READ_STATUS,
	QCC_WRITE_STATUS,
	MEM_READ,
	MEM_WRITE,
	MEM_READ_STATUS,
	MEM_WRITE_STATUS,
	QHAL_VENDOR_REQ_END,

	VENDOR_REQ_LAST = 0x80
} uvc_vendor_req_id_t;

#define FWPACKETSIZE 4088

static int usb_send_file(struct libusb_device_handle *dhandle, 
			const char *filename, int fwpactsize,unsigned char brequest)
{
	int r, ret;
	int total_size;
	struct stat stfile;
	FILE *fd; 
	char *buffer;

	if(stat(filename,&stfile))
		return -1;
	if(stfile.st_size <= 0){
		printf("ERR: Invalid file provided\n");
		return -1;
	}

#if !defined(_WIN32)
	fd = fopen(filename, "rb");
#else
	ret = fopen_s(&fd,filename, "rb");
#endif
	
	total_size = stfile.st_size;
	buffer = malloc(fwpactsize);

	printf("Sending file of size %d\n",total_size);

	while(total_size > 0){
		int readl = 0;
		if(fwpactsize > total_size)
			readl = total_size;
		else
			readl = fwpactsize;

		ret = (int)fread(buffer, readl, 1, fd);

		r = libusb_control_transfer(dhandle,
				/* bmRequestType*/
				LIBUSB_ENDPOINT_OUT |LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE,
				/* bRequest     */brequest,
				/* wValue       */0,
				/* wIndex       */0,
				/* Data         */
				(unsigned char *)buffer,
				/* wLength       */ readl,
				0); 
		if(r<0){
			printf("ERR: Req 0x%x failed\n",brequest);	
			ret = -1;
			break;
		}
		if(ret == EOF){
			ret = 0;
			break;
		}

		total_size = total_size - readl;
	}

	if(fd)fclose(fd);
	if(buffer)free(buffer);

	return ret;
}

int mxuvc_burnin_init(int font_size, char* file_name)
{
	struct libusb_device *camera_dev;
	int ret=0, count;
	struct libusb_config_descriptor *conf_desc;
	
	if (camera == NULL){
		ret = init_libusb(&ctxt);
		//ret = libusb_init(&ctxt);
		if (ret) {
			TRACE("libusb_init failed\n");
			return -1;
		}
		camera = libusb_open_device_with_vid_pid(ctxt, 0x0b6a, 0x4d52);
		if (camera == NULL) {
			TRACE("Opening camera failed\n");
			return -1;
		}
		open_counter++;
	}

	if(intf == NULL){
		camera_dev = libusb_get_device(camera);
		if (camera_dev == NULL) {
			TRACE("Cannot get device from handle\n");
			return -1;
		}

		ret = libusb_get_active_config_descriptor(camera_dev, &conf_desc);
		if (ret < 0) {
			TRACE("Cannot retrive active config descriptor\n");
			return -1;
		}
		for (count = 0; count < conf_desc->bNumInterfaces; count++) {
			intf = (conf_desc->interface + count);
			/* FIXME TODO not the best check; but sufficient for now */
			if ((intf->altsetting->bInterfaceClass == 
						LIBUSB_CLASS_VENDOR_SPEC) &&
				(intf->altsetting->bInterfaceSubClass == 0xf2))
				break;
		}
		if (count < conf_desc->bNumInterfaces) {
			ret = libusb_claim_interface(camera, 
						intf->altsetting->bInterfaceNumber);
			if (ret < 0) {
				TRACE("Cannot claim interface %d\n", 
						intf->altsetting->bInterfaceNumber);
				return -1;
			}
		} else {
			TRACE("Cannot find a suitable interface\n");
			return -1;
		}
	}else
		return -1;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ ADD_FONT,
				/* wValue        */ font_size,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "ADD_FONT failed");
	
	ret = usb_send_file(camera, file_name, FWPACKETSIZE, SEND_FONT_FILE);

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ FONT_FILE_DONE,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "FONT_FILE_DONE failed");
	//50ms delay for raptor to load and decompress the font
	usleep(50000);
	return ret;
}

int mxuvc_burnin_deinit(void){
	int ret = 0;

	if(intf && camera){
		ret = libusb_set_interface_alt_setting(camera, 
				intf->altsetting->bInterfaceNumber,
				0);
		libusb_release_interface (camera, 
					intf->altsetting->bInterfaceNumber);

		open_counter--;
		if(open_counter == 0){
			libusb_close (camera);
			exit_libusb(&ctxt);

			ctxt = NULL;
			intf = NULL;
			camera = NULL;
		}
	}

	return ret;

}

int mxuvc_burnin_add_text(int idx, char *str, int length, uint16_t X, uint16_t Y )
{
	int ret = 0;
	uint32_t position = 0;
	if(str == NULL || length<= 0){
		printf("ERR: %s str/length is invalid\n",__func__);
		return -1;
	}
	position = (X<<16) + Y;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ ADD_TEXT,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&position,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	CHECK_ERROR(ret < 0, -1, "ADD_TEXT failed");
	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ TEXT,
				/* wValue        */ idx,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)str,
				/* wLength       */ length,
				/* timeout*/   0 
				);

	CHECK_ERROR(ret < 0, -1, "TEXT failed");
	//5ms delay for add text to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_remove_text(int idx)
{
	int ret = 0;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ REMOVE_TEXT,
				/* wValue        */ idx,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	//5ms delay for remove text to complete in raptor
	usleep(5000);
	return ret;	
}

int mxuvc_burnin_show_time(int x, int y, int frame_num_enable)
{
	int ret = 0;
	int position = (x<<16) + y;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ SHOW_TIME,
				/* wValue        */ frame_num_enable,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&position,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	//5ms delay for show time to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_hide_time(void)
{
	int ret = 0;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ HIDE_TIME,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ NULL,
				/* wLength       */ 0,
				/* timeout*/   0 
				);
	//5ms delay for hide time to complete in raptor
	usleep(5000);
	return ret;
}

int mxuvc_burnin_update_time(int h, int m, int sec)
{
	int ret = 0;
	int Time = ((h<<16) | (m<<8)) + sec;

	ret = libusb_control_transfer(camera,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
				 LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ UPDATE_TIME,
				/* wValue        */ 0,
				/* MSB 4 bytes   */
				/* wIndex        */ 0,
				/* Data          */ (unsigned char *)&Time,
				/* wLength       */ 4,
				/* timeout*/   0 
				);
	//5ms delay for update time to complete in raptor
	usleep(5000);
	return ret;	
}
