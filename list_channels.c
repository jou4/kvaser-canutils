#include "lib.h"

void print_usage_list_channels(char *arg0, char *arg1)
{
	char prg[_MAX_FNAME];
	char *cmd;

	basename(arg0, prg, sizeof(prg));
	cmd = arg1;

	fprintf(stderr, "%s %s - list CAN channels.\n\n", prg, cmd);
	fprintf(stderr, "Usage: %s %s\n", prg, cmd);
}

int list_channels(int argc, char *argv[]){
	int i, num_of_channels, channel_num;
	char device_name[255];
    canStatus status;

	kv_initialize();

	status = canGetNumberOfChannels(&num_of_channels);
	if(status != canOK){
		print_kvaser_error("canGetNumberOfChannels", status);
		return EXIT_FAILURE;
	}

	if(num_of_channels == 0){
		printf("Could not find any CAN interface.\n");
		return EXIT_FAILURE;
	}

	for(i = 0; i < num_of_channels; i++){
		status = canGetChannelData(i, canCHANNELDATA_DEVDESCR_ASCII, device_name, sizeof(device_name));
		if(status != canOK){
			print_kvaser_error("canGetChannelData", status);
			return EXIT_FAILURE;
		}

		status = canGetChannelData(i, canCHANNELDATA_CHAN_NO_ON_CARD, &channel_num, sizeof(channel_num));
		if(status != canOK){
			print_kvaser_error("canGetChannelData", status);
			return EXIT_FAILURE;
		}

		fprintf(stdout, "%d: %s\n", channel_num, device_name);
	}

	return EXIT_SUCCESS;
}
