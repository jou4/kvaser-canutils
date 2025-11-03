#include "lib.h"

volatile int stop_flag = 0;

void print_usage(char *arg0)
{
	char prg[_MAX_FNAME];
	basename(arg0, prg, sizeof(prg));

	fprintf(stderr, "%s - Kvaser CAN utility.\n\n", prg);
	fprintf(stderr, "Usage: %s [command] [options]\n", prg);
	fprintf(stderr, "Command:\n");
	fprintf(stderr, "  dump    dump CAN bus traffic.\n");
	fprintf(stderr, "  send    send CAN frames.\n");
	fprintf(stderr, "  play    replay a compact CAN frame logfile to CAN devices.\n");
}

void signal_handler(int sig) {
    stop_flag = 1;
}

int main(int argc, char *argv[])
{
	int i;

	// Signal Handlers
	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	if(argc == 1){
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	timeBeginPeriod(1);

	for(i = 1; i < argc; i++){
		if(strcmp(argv[i], "dump") == 0){
			return candump(argc, argv);
		}
		else if(strcmp(argv[i], "send") == 0){
			return cansend(argc, argv);
		}
		else if(strcmp(argv[i], "play") == 0){
			return canplay(argc, argv);
		}
		else{
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	timeEndPeriod(1);

	return EXIT_SUCCESS;
}
