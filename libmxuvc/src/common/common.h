/*******************************************************************************
*
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
*
* Copyright (c) 2011 Maxim Integrated Products.
* All rights reserved.
*
*******************************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__
#include <string.h>
#include "mxuvc.h"

/* Debug defines */
#define DEBUG_AUDIO_QUEUE 0 /* 1 to enable or 2 for more messages */
#define DEBUG_AUDIO_CALLBACK 0
#define DEBUG_VIDEO_CALLBACK 0

/* Timeout (in ms) for libusb i/o functions */
#define USB_TIMEOUT             5000

#define TRACE(msg, ...)                                               \
	do {                                                          \
		if (MXUVC_TRACE_LEVEL >= 1)                           \
			printf(msg, ##__VA_ARGS__);                   \
	} while(0)
#define TRACE2(msg, ...)                                              \
	do {                                                          \
		if (MXUVC_TRACE_LEVEL >= 2)                           \
			printf(msg, ##__VA_ARGS__);                   \
	} while(0)

#define SHOW(var) TRACE("\t\t" #var " = %i\n", var)
#define SHOW2(var) TRACE2("\t\t" #var " = %i\n", var)

#define WARNING(msg, ...)                                             \
	do {                                                          \
		fprintf(stdout, "WARNING: " msg "\n", ##__VA_ARGS__); \
	} while(0)
#define ERROR(ret_val, msg, ...)                                      \
	do {                                                          \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);   \
		return ret_val;                                       \
	} while(0)
#define ERROR_NORET(msg, ...)                                         \
	do {                                                          \
		fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);   \
	} while(0)

#define CHECK_ERROR(cond, ret_val, msg, ...)                       \
	do {                                                       \
		if (cond)                                          \
			ERROR(ret_val, msg,  ##__VA_ARGS__);       \
	} while(0)

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define CST2STR(cst) [cst] = #cst
const char* chan2str(video_channel_t ch);
const char* chan2str_mux(video_channel_t ch);
const char* vidformat2str(video_format_t format);
const char* profile2str(video_profile_t profile);
const char* audformat2str(audio_format_t format);
int next_opt(char *str1, char **opt, char **value);

#define RECORD(fmt, ...) {                                                   \
	extern FILE* debug_recfd;                                            \
	extern int debug_getusleep();                                        \
	if (debug_recfd) {                                                   \
		char str[256];                                               \
		sprintf(str, "usleep(%i);\n", debug_getusleep());            \
		fwrite(str, 1, strlen(str), debug_recfd);                    \
		sprintf(str, "%s(" fmt ");\n", __FUNCTION__, ##__VA_ARGS__); \
		fwrite(str, 1, strlen(str), debug_recfd);                    \
		fflush(debug_recfd);                                         \
	}                                                                    \
}

#ifdef WIN32
#define PACKED
#else
#define PACKED  __attribute__ ((packed))
#endif


#endif
