#include "lib.h"

void print_usage_cansend(char *arg0, char *arg1)
{
	char prg[_MAX_FNAME];
	char *cmd;

	basename(arg0, prg, sizeof(prg));
	cmd = arg1;

	fprintf(stderr, "%s %s - send CAN frames with Kvaser driver.\n\n", prg, cmd);
	fprintf(stderr, "Usage: %s %s [options] <channel> <can-frame>\n", prg, cmd);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -f, --fd                       (CAN-FD)\n");
	fprintf(stderr, "  -b, --bitrate <rate>           (CAN-CC bitrate or CAN-FD arbitration bitrate - default: 500Kbps)\n");
	fprintf(stderr, "  -d, --data-bitrate <rate>      (CAN-FD data bitrate - default: 2Mbps)\n");
	fprintf(stderr, "  -r                             (send repeatedly)\n");
	fprintf(stderr, "  -n <count>                     (repeat <count> times - default infinite)\n");
	fprintf(stderr, "  -g <ms>                        (gap in milli seconds - default 200ms)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Format of <channel>: <channel-num>{f|F}{_}{b|B<bitrate>}{d|D<data-bitrate>}\n");
	fprintf(stderr, "  examples:\n");
	fprintf(stderr, "    0                            (channel 0, CAN-CC)\n");
	fprintf(stderr, "    0F                           (channel 0, CAN-FD)\n");
	fprintf(stderr, "    0_b500K                      (channel 0, CAN-CC, bitrate 500K)\n");
	fprintf(stderr, "    0Fb500Kd2M                   (channel 0, CAN-FD, arbitration bitrate 500K, data bitrate 2M)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Format of <can-frame>: <can-id>#{#}{<flag>}<data>\n");
	fprintf(stderr, "  examples:\n");
	fprintf(stderr, "    123#1122334455667788         (CAN-CC, standard can-id 0x123, data 0x1122334455667788)\n");
	fprintf(stderr, "    123##11122334455667788       (CAN-FD, standard can-id 0x123, data 0x1122334455667788, flag 0x1(BRS))\n");
	fprintf(stderr, "    12345678#1122334455667788    (CAN-CC, extended can-id 0x12345678, data 0x1122334455667788)\n");
}

int cansend(int argc, char *argv[]){
	int i, channel_num, repeat, count, gap;
	can_frame cf;
	can_channel ch = {
		.fd = 0,
		.bitrate = CAN_BITRATE_DEFAULT,
		.data_bitrate = CANFD_DATA_BITRATE_DEFAULT,
		.state = 0
	};

	if(argc <= 2){
		print_usage_cansend(argv[0], argv[1]);
		return EXIT_FAILURE;
	}

	repeat = 0;
	count = -1;	// infinite when a negative number
	gap = 200;
	channel_num = -1;

	for(i = 2; i < argc; i++){
		if(strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fd") == 0){
			ch.fd = 1;
		}
		else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bitrate") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing bitrate value after %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			ch.bitrate = parse_bitrate(argv[i]);

			if(ch.bitrate < 0){
				fprintf(stderr, "Error: Invalid bitrate '%s'\n", argv[i]);
				fprintf(stderr, "Supported formats: 125000, 250k, 500k, 1M, etc.\n\n");
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data-bitrate") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing data-bitrate value after %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			ch.data_bitrate = parse_bitrate(argv[i]);

			if(ch.data_bitrate < 0){
				fprintf(stderr, "Error: Invalid data-bitrate '%s'\n", argv[i]);
				fprintf(stderr, "Supported formats: 125000, 250k, 500k, 1M, etc.\n\n");
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "-r") == 0){
			repeat = 1;
		}
		else if(strcmp(argv[i], "-n") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing count value after %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			count = atoi(argv[i]);

			if (count == 0) {
				fprintf(stderr, "Invalid count value: %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "-g") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing gap value after %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			gap = atoi(argv[i]);

			if (gap <= 0) {
				fprintf(stderr, "Invalid gap value: %s\n\n", argv[i]);
				print_usage_cansend(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "help") == 0){
			print_usage_cansend(argv[0], argv[1]);
			return EXIT_FAILURE;
		}
		else{
			if(channel_num < 0){
				channel_num = parse_canchannel(argv[i], &ch);
				if(channel_num >= MAX_CHANNELS){
					fprintf(stderr, "Invalid channel value: %d\n\n", channel_num);
					return EXIT_FAILURE;
				}
			}
			else{
				parse_canframe(argv[i], &cf);
			}
		}
	}

	// Debug
	pp_canframe(&cf);

	kv_initialize();
	kv_setup_channel(channel_num, &ch);

	i = 0;
	if(!repeat){
		count = 1;
	}

	kv_write(channel_num, &cf);
	while(!stop_flag && (count < 0 || (++i < count))){
		Sleep(gap);
		kv_write(channel_num, &cf);
	}

	kv_close_channel(channel_num);

	return EXIT_SUCCESS;
}
