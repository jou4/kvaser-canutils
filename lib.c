#include "lib.h"

void basename(const char *path, char *fname, size_t len){
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char ext[_MAX_EXT];

    _splitpath_s(path, drive, sizeof(drive), dir, sizeof(dir), fname, len, ext, sizeof(ext));
}

uint64_t get_unix_time(){
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    FILETIME    file_time;
    uint64_t    time;

	GetSystemTimeAsFileTime(&file_time);
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;
	time -= EPOCH;

	// convert 100ns -> 1us
	return time / 10;
}

int gettimeofday(struct timeval *tp)
{
    uint64_t    time;
	time = get_unix_time();

    tp->tv_sec  = (long) (time / 1000000L);
    tp->tv_usec = (long) (time % 1000000L);

    return 0;
}

int gettimeofday_ms(struct timeval * tp){
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

	SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

	GetSystemTime( &system_time );
	SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);

    return 0;
}

int parse_bitrate(const char* cs){
	char* endptr;
	long bitrate;

	if(cs == NULL || *cs == '\0'){
		return -1;
	}

	bitrate = strtol(cs, &endptr, 10);

	if(bitrate <= 0 || bitrate > INT_MAX){
		return -1;
	}

	if(*endptr == '\0'){
		return (int)bitrate;
	}
	else if(endptr[0] == 'k' || endptr[0] == 'K'){
		// k/K Suffix
		if(bitrate > INT_MAX / 1000){
			return -1; // Prevent overflow
		}
		return (int)(bitrate * 1000);
	}
	else if(endptr[0] == 'm' || endptr[0] == 'M'){
		// m/M Suffix
		if(bitrate > INT_MAX / 1000000){
			return -1; // Prevent overflow
		}
		return (int)(bitrate * 1000000);
	}

	return -1;
}

#define MAX_BITRATE_STRING_LEN 16

/**
 *
 * Examples:
 *  0             channel 0
 *  0F            channel 0 (FD)
 *  0_b500000     channel 0, bitrate 500K
 *  0f_B500KD2M   channel 0 (FD), bitrate 500K, data-bitrate 2M
 *
 */
int parse_canchannel(const char* cs, can_channel *ch){
	char *endptr, *tmp;
	long channel_num;
	char substr[MAX_BITRATE_STRING_LEN];
	int len, br;

	channel_num = strtol(cs, &endptr, 10);

	if(*endptr == 'f' || *endptr == 'F'){
		ch->fd = 1;
		endptr++;
	}
	else if(*endptr == '_'){
		endptr++;
	}

	while(*endptr != '\0'){
		if(*endptr == 'b' || *endptr == 'B'){
			tmp = ++endptr;
			len = 0;
			while(*tmp != '\0' && *tmp != 'd' && *tmp != 'D'){
				len++;
				tmp++;
			}
			if(len > 0){
				strncpy_s(substr, MAX_BITRATE_STRING_LEN, endptr, len);
				substr[len] = '\0';
				br = parse_bitrate(substr);
				if(br > 0){
					ch->bitrate = br;
				}
				endptr += len;
			}
		}
		else if(*endptr == 'd' || *endptr == 'D'){
			tmp = ++endptr;
			len = 0;
			while(*tmp != '\0' && *tmp != 'b' && *tmp != 'B'){
				len++;
				tmp++;
			}
			if(len > 0){
				strncpy_s(substr, MAX_BITRATE_STRING_LEN, endptr, len);
				substr[len] = '\0';
				br = parse_bitrate(substr);
				if(br > 0){
					ch->data_bitrate = br;
				}
				endptr += len;
			}
		}
		else{
			endptr++;
		}
	}

	ch->channel = (int)channel_num;

	return ch->channel;
}

void pp_canframe(can_frame *cf)
{
	unsigned int i;

	printf("%x %c%c%c%c%c%c%c %02u ",
		cf->id,
		(cf->flag & canMSG_EXT)        ? 'x' : ' ',
		(cf->flag & canMSG_RTR)        ? 'R' : ' ',
		(cf->flag & canMSGERR_OVERRUN) ? 'o' : ' ',
		(cf->flag & canMSG_NERR)       ? 'N' : ' ', // TJA 1053/1054 transceivers only
		(cf->flag & canFDMSG_FDF)      ? 'F' : ' ',
		(cf->flag & canFDMSG_BRS)      ? 'B' : ' ',
		(cf->flag & canFDMSG_ESI)      ? 'E' : ' ',
		cf->dlc);

	for(i=0; i<(cf->dlc); i++){
		printf("%02x", cf->msg[i]);
	}

	printf("\n");
}

void pp_canchannel(int channel_num, can_channel *ch)
{
	printf("ch=%d, fd=%d, b=%d, d=%d, s=%d, h=%d\n",
		channel_num,
		ch->fd,
		ch->bitrate,
		ch->data_bitrate,
		ch->state,
		ch->handle
		);
}

const char* format_can_id(unsigned long id, unsigned int flag) {
    static char id_buffer[16];

    if (flag & canMSG_EXT) {
        // extended (29bit)
        snprintf(id_buffer, sizeof(id_buffer), "%08lX", id & 0x1FFFFFFF);
    } else {
        // standard (11bit)
        snprintf(id_buffer, sizeof(id_buffer), "%03lX", id & 0x7FF);
    }

    return id_buffer;
}

const char* format_msg(const unsigned char* msg, unsigned char dlc) {
    static char buffer[CANFD_MAX_DLEN * 2 + 1];
    int i;

    if (dlc == 0) {
        buffer[0] = '\0';
        return buffer;
    }

    // clip when DLC exceeds
    if (dlc > CANFD_MAX_DLEN) {
        dlc = CANFD_MAX_DLEN;
    }

    // convert to HEX
    for (i = 0; i < dlc; i++) {
        snprintf(&buffer[i * 2], 3, "%02X", msg[i]);
    }
    buffer[dlc * 2] = '\0';

    return buffer;
}

void fprint_log(FILE *stream, can_log *log){
	int flag;

	// microsecond
    fprintf(stream, "(%010d.%06d) ",
           (int)(log->timestamp / 1000000L),
           (int)(log->timestamp % 1000000L));

    fprintf(stream, "%d ", log->channel);

    const char* id_str = format_can_id(log->frame.id, log->frame.flag);
    fprintf(stream, "%s#", id_str);

	if(log->frame.flag & canFDMSG_FDF /* && (frame->flag & canMSG_RTR) == 0 */) {
		flag = CANFD_FDF;
		if(log->frame.flag & canFDMSG_BRS){
			flag |= CANFD_BRS;
		}
		if(log->frame.flag & canFDMSG_ESI){
			flag |= CANFD_ESI;
		}
		fprintf(stream, "#%d", flag);
	}

    const char* data_str = format_msg(log->frame.msg, log->frame.dlc);
    fprintf(stream, "%s", data_str);

	fprintf(stream, " [%c%c%c%c%c%c%c]",
		(log->frame.flag & canMSG_EXT)        ? 'x' : ' ',
		(log->frame.flag & canMSG_RTR)        ? 'R' : ' ',
		(log->frame.flag & canMSGERR_OVERRUN) ? 'o' : ' ',
		(log->frame.flag & canMSG_NERR)       ? 'N' : ' ', // TJA 1053/1054 transceivers only
		(log->frame.flag & canFDMSG_FDF)      ? 'F' : ' ',
		(log->frame.flag & canFDMSG_BRS)      ? 'B' : ' ',
		(log->frame.flag & canFDMSG_ESI)      ? 'E' : ' ');

    fprintf(stream, "\n");
}
