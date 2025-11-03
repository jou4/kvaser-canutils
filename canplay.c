#include "lib.h"

#define LOG_TM_SIZE 21
#define LOG_CH_SIZE 10
#define LOG_FR_SIZE 256
#define LOG_LN_SIZE (LOG_TM_SIZE + LOG_CH_SIZE + LOG_FR_SIZE)

void print_usage_canplay(char *arg0, char *arg1)
{
	char prg[_MAX_FNAME];
	char *cmd;

	basename(arg0, prg, sizeof(prg));
	cmd = arg1;

	fprintf(stderr, "%s %s - replay a compact CAN frame logfile with Kvaser driver.\n\n", prg, cmd);
	fprintf(stderr, "Usage: %s %s [options] <channel> [<channel> ...]\n", prg, cmd);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -I <infile>                    (logfile to replay)\n");
	fprintf(stderr, "  -l <num>                       (process input file <num> times)\n");
	fprintf(stderr, "                                 (use 'i' for infinite loop - default: 1)\n");
	fprintf(stderr, "  -g <ms>                        (gap in milli seconds - default 1ms)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Format of <channel>: <channel-num>{f|F}{_}{b|B<bitrate>}{d|D<data-bitrate>}\n");
	fprintf(stderr, "  examples:\n");
	fprintf(stderr, "    0                            (channel 0, CAN-CC)\n");
	fprintf(stderr, "    0F                           (channel 0, CAN-FD)\n");
	fprintf(stderr, "    0_b500K                      (channel 0, CAN-CC, bitrate 500K)\n");
	fprintf(stderr, "    0Fb500Kd2M                   (channel 0, CAN-FD, arbitration bitrate 500K, data bitrate 2M)\n");
}

int timeval_cmp(struct timeval *tv1, struct timeval *tv2){
	if(tv1->tv_sec < tv2->tv_sec){
		return -1;
	}
	else if(tv1->tv_sec > tv2->tv_sec){
		return 1;
	}

	return tv1->tv_usec - tv2->tv_usec;
}

void timeval_diff(struct timeval *tv1, struct timeval *tv2, struct timeval *diff){
	diff->tv_sec = tv2->tv_sec - tv1->tv_sec;
	diff->tv_usec = tv2->tv_usec - tv1->tv_usec;
}

void timeval_add(struct timeval *tv1, struct timeval *tv2){
	tv1->tv_sec += tv2->tv_sec;
	tv1->tv_usec += tv2->tv_usec;

	if(tv1->tv_usec < 0){
		tv1->tv_sec -= 1;
		tv1->tv_usec += 1000000L;
	}

	if(tv1->tv_usec >= 1000000L){
		tv1->tv_sec += 1;
		tv1->tv_usec -= 1000000L;
	}
}

int canplay(int argc, char *argv[]){
	int i, channel_num, count, gap, eof;
	long sec, usec;
	char buf[LOG_LN_SIZE], frbuf[LOG_FR_SIZE];
	char *filepath;
	char *fret;
	FILE *infile;
	can_frame cf;
	can_channel ch;
	struct timeval base_tv, log_tv, diff_tv;

	if(argc <= 2){
		print_usage_canplay(argv[0], argv[1]);
		return EXIT_FAILURE;
	}

	count = 1;	// infinite when a negative number
	gap = 1;
	channel_num = -1;

	kv_initialize();

	for(i = 2; i < argc; i++){
		if(strcmp(argv[i], "-I") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing infile value after %s\n\n", argv[i]);
				print_usage_canplay(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			filepath = argv[i];
		}
		else if(strcmp(argv[i], "-l") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing num value after %s\n\n", argv[i]);
				print_usage_canplay(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			if(argv[i][0] == 'i'){
				count = -1;
			}else{
				count = atoi(argv[i]);

				if (count <= 0) {
					fprintf(stderr, "Invalid num value: %s\n\n", argv[i]);
					print_usage_canplay(argv[0], argv[1]);
					return EXIT_FAILURE;
				}
			}
		}
		else if(strcmp(argv[i], "-g") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing gap value after %s\n\n", argv[i]);
				print_usage_canplay(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			gap = atoi(argv[i]);

			if (gap < 0) {
				fprintf(stderr, "Invalid gap value: %s\n\n", argv[i]);
				print_usage_canplay(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "help") == 0){
			print_usage_canplay(argv[0], argv[1]);
			return EXIT_FAILURE;
		}
		else{
			ch.fd = 0;
			ch.bitrate = CAN_BITRATE_DEFAULT;
			ch.data_bitrate = CANFD_DATA_BITRATE_DEFAULT;
			ch.state = 0;
			channel_num = parse_canchannel(argv[i], &ch);
			if(channel_num >= MAX_CHANNELS){
				fprintf(stderr, "Invalid channel value: %d\n\n", channel_num);
				return EXIT_FAILURE;
			}
			kv_setup_channel(channel_num, &ch);
		}
	}

	i = 0;
	eof = 0;

	if(fopen_s(&infile, filepath,"r") != 0){
		fprintf(stderr, "cannot open: %s\n", filepath);
		return 1;
	}

	while(count < 0 || (i++ < count)){
		// skip until first non-comment line
		while ((fret = fgets(buf, 1024 - 1, infile)) != NULL && buf[0] != '(') {
		}

		if(!fret){
			// nothing to read
			return EXIT_SUCCESS;
		}

		if (sscanf_s(buf, "(%ld.%ld) %d %255s", &sec, &usec, &channel_num, frbuf, LOG_FR_SIZE-1) != 4) {
			fprintf(stderr, "incorrect line format in logfile\n");
			return 1;
		}

		log_tv.tv_sec = sec;
		log_tv.tv_usec = usec;

		gettimeofday(&base_tv);
		timeval_diff(&base_tv, &log_tv, &diff_tv);
		timeval_add(&base_tv, &diff_tv);

		while(!eof){
			while(timeval_cmp(&base_tv, &log_tv) >= 0){
				parse_canframe(frbuf, &cf);
				kv_write(channel_num, &cf);

				// skip until next non-comment line
				while ((fret = fgets(buf, 1024 - 1, infile)) != NULL && buf[0] != '(') {
				}

				if(!fret){
					eof = 1;
					break;
				}

				if (sscanf_s(buf, "(%ld.%ld) %d %255s", &sec, &usec, &channel_num, frbuf, LOG_FR_SIZE-1) != 4) {
					fprintf(stderr, "incorrect line format in logfile\n");
					return 1;
				}

				log_tv.tv_sec = sec;
				log_tv.tv_usec = usec;

				if(stop_flag){
					goto out;
				}

			} // while(timeval_cmp(&base_tv, &log_tv) > 0)

			Sleep(gap);

			gettimeofday(&base_tv);
			timeval_add(&base_tv, &diff_tv);

		} // while(!eof)

		rewind(infile);
		eof = 0;

	} // while(count < 0 || (++i < count))

out:
	kv_cleanup_channels();

	return EXIT_SUCCESS;
}
