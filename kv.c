#include "lib.h"

volatile int stop_flag = 0;

void print_usage(char *arg0)
{
	char prg[_MAX_FNAME];
	basename(arg0, prg, sizeof(prg));

	fprintf(stderr, "%s - Kvaser CAN utility.\n\n", prg);
	fprintf(stderr, "Usage: %s [command] [options]\n", prg);
	fprintf(stderr, "Command:\n");
	fprintf(stderr, "  list    list CAN channels.\n");
	fprintf(stderr, "  dump    dump CAN bus traffic.\n");
	fprintf(stderr, "  send    send CAN frames.\n");
	fprintf(stderr, "  play    replay a compact CAN frame logfile to CAN devices.\n");
}

void signal_handler(int sig) {
    stop_flag = 1;
}

int main(int argc, char *argv[])
{
	int res;

	// Signal Handlers
	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

	if(argc == 1){
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	timeBeginPeriod(1);

	if(strcmp(argv[1], "list") == 0){
		res = list_channels(argc, argv);
	}
	else if(strcmp(argv[1], "dump") == 0){
		res = candump(argc, argv);
	}
	else if(strcmp(argv[1], "send") == 0){
		res = cansend(argc, argv);
	}
	else if(strcmp(argv[1], "play") == 0){
		res = canplay(argc, argv);
	}
	else{
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	timeEndPeriod(1);

	return res;
}
