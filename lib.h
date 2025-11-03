#ifndef LIB_H
#define LIB_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <mmsystem.h>
#include <stdint.h>
#include <canlib.h>  // KVASER SDK
#include "linux/can.h"

#pragma comment(lib, "winmm.lib")

typedef __int8  __i8;
typedef __int32 __i32;
typedef unsigned __int8 __u8;
typedef unsigned __int32 __u32;


#define CAN_BITRATE_DEFAULT 500000
#define CANFD_DATA_BITRATE_DEFAULT 2000000
#define MAX_CHANNELS 16

extern volatile int stop_flag;

typedef struct {
	__i32 id;
	__u8  msg[CANFD_MAX_DLEN];
	__u32 dlc;
	__u32 flag;
} can_frame;

typedef struct {
	int channel;
	uint64_t timestamp;
	can_frame frame;
} can_log;

typedef struct {
	int channel;
    int fd;
	int bitrate;
	int data_bitrate;
	int state;
    CanHandle handle;
} can_channel;

extern can_channel channels[MAX_CHANNELS];


void basename(const char *path, char *fname, size_t len);
uint64_t get_unix_time();
int gettimeofday(struct timeval * tp);
int gettimeofday_ms(struct timeval * tp);

int parse_bitrate(const char* cs);
int parse_canchannel(const char *cs, can_channel *ch);
int parse_canframe(char *cs, can_frame *cf);

void pp_canframe(can_frame *cf);
void pp_canchannel(int channel_num, can_channel *ch);
void fprint_log(FILE *stream, can_log *log);

int candump(int argc, char *argv[]);
int cansend(int argc, char *argv[]);
int canplay(int argc, char *argv[]);

int kv_initialize(void);
int kv_setup_channel(int channel_num, can_channel *ch_param);
void kv_sync_bus_on();
int kv_write(int channel_num, can_frame *cf);
int kv_read(int channel_num, long *id, void *msg, unsigned int *dlc, unsigned int *flag, unsigned long *time);
void kv_close_channel(int channel_num);
void kv_cleanup_channels(void);

#endif // LIB_H
