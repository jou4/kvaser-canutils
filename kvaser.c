#include "lib.h"

#define KV_TIMEOUT 100

can_channel channels[MAX_CHANNELS];

void print_kvaser_error(const char* function, canStatus status) {
    char error_text[256];

    if (canGetErrorText(status, error_text, sizeof(error_text)) == canOK) {
        fprintf(stderr, "%s failed: %s (status: %d)\n", function, error_text, status);
    } else {
        fprintf(stderr, "%s failed with status: %d\n", function, status);
    }
}

int kv_initialize(void){
	canInitializeLibrary();
	memset(channels, '\0', sizeof(can_channel) * MAX_CHANNELS);
	return 0;
}

void kv_close_channel(int channel_num){
    canStatus status;
	if (channels[channel_num].state) {
		status = canBusOff(channels[channel_num].handle);
		if (status != canOK) {
			print_kvaser_error("canBusOff", status);
		}

		status = canClose(channels[channel_num].handle);
		if (status != canOK) {
			print_kvaser_error("canClose", status);
		}

		channels[channel_num].state = 0;
	}
}

void kv_cleanup_channels(void) {
    int i;

    for (i = 0; i < MAX_CHANNELS; i++) {
		kv_close_channel(i);
    }
}

can_channel *kv_channel_info(int channel_num){
	return &channels[channel_num];
}

kvTimeDomain time_domain;
kvTimeDomainData time_data;

int kv_time_sync(){
	int i;
    canStatus status;

	status = kvTimeDomainCreate(&time_domain);
	if (status != canOK) {
		print_kvaser_error("kvTimeDomainCreate", status);
		return -1;
	}

	for(i = 0; i < MAX_CHANNELS; i++){
		status = kvTimeDomainAddHandle(time_domain, channels[i].channel);
		if (status != canOK) {
			print_kvaser_error("kvTimeDomainAddHandle", status);
			fprintf(stderr, "Failed to add handle %u to TimeDomain, ", channels[i].channel);
			fprintf(stderr, "status == %d\n", status);
		}
	}

	// Request some data from the domain and try to interpret it.
	status = kvTimeDomainGetData(time_domain, &time_data, sizeof(time_data));
	if (status != canOK) {
		print_kvaser_error("kvTimeDomainGetData", status);
		return -1;
	}
	if (time_data.nMagiSyncGroups > 1) {
		fprintf(stderr, "Consider connecting the Kvaser MagiSync&tm; enabled ");
		fprintf(stderr, "interfaces through the same USB root hub!");
	}
	if (time_data.nMagiSyncGroups == 1) {
		fprintf(stderr, "All my Kvaser MagiSync&tm; enabled interfaces are ");
		fprintf(stderr, "connected through the same USB root hub and hence ");
		fprintf(stderr, "perfectly synchronized!");
	}
	if (time_data.nMagiSyncedMembers == MAX_CHANNELS) {
		printf("All handles have the Kvaser MagiSync&tm; feature enabled!");
	}

	// Reset the time on all handles in tDomain.
	status = kvTimeDomainResetTime(time_domain);
	if (status != canOK) {
		print_kvaser_error("kvTimeDomainResetTime", status);
		return -1;
	}
	return 0;
}

int kv_time_cleanup(){
    canStatus status;
	status = kvTimeDomainDelete(time_domain);
	if (status != canOK) {
		print_kvaser_error("kvTimeDomainDelete", status);
		return -1;
	}
	return 0;
}

int kv_setup_channel(int channel_num, can_channel *ch_param){
    canStatus status;
	can_channel *ch;
	int bitrate;
	int dbitrate;

	ch = kv_channel_info(channel_num);
	memcpy(ch, ch_param, sizeof(can_channel));

    int flags = canOPEN_ACCEPT_VIRTUAL;
    if (ch->fd) {
        flags |= canOPEN_CAN_FD;
    }

    ch->handle = canOpenChannel(channel_num, flags);
    if (ch->handle < 0) {
        print_kvaser_error("canOpenChannel", ch->handle);
        return -1;
    }

	//
	// Set timestamp clock resolution
	//
	DWORD resolution = 1;	// 1 microsecond
	status = canIoCtl(ch->handle, canIOCTL_SET_TIMER_SCALE, &resolution, sizeof(resolution));
	if (status) {
        print_kvaser_error("canIoCtl", status);
		kv_close_channel(channel_num);
		return -1;
	}

	// bit rates
    if (ch->fd) {
		switch(ch->bitrate){
			case 1000000 : bitrate = canFD_BITRATE_1M_80P; break;
			case  500000 : bitrate = canFD_BITRATE_500K_80P; break;
			default:
				fprintf(stderr, "Unsupported arbitration bitrate: %d\n", ch->bitrate);
				kv_close_channel(channel_num);
				return -1;
		}

        status = canSetBusParams(ch->handle, bitrate, 0, 0, 0, 0, 0);
        if (status != canOK) {
            print_kvaser_error("canSetBusParams (arbitration)", status);
			kv_close_channel(channel_num);
            return -1;
        }

		switch(ch->data_bitrate){
			case 8000000 : dbitrate = canFD_BITRATE_8M_60P; break;
			case 4000000 : dbitrate = canFD_BITRATE_4M_80P; break;
			case 2000000 : dbitrate = canFD_BITRATE_2M_80P; break;
			case 1000000 : dbitrate = canFD_BITRATE_1M_80P; break;
			default:
				fprintf(stderr, "Unsupported data bitrate: %d\n", ch->data_bitrate);
				kv_close_channel(channel_num);
				return -1;
		}

        status = canSetBusParamsFd(ch->handle, dbitrate, 0, 0, 0);
        if (status != canOK) {
            print_kvaser_error("canSetBusParamsFd", status);
			kv_close_channel(channel_num);
            return -1;
        }
    } else {
		switch(ch->bitrate){
			case 1000000 : bitrate = canBITRATE_1M; break;
			case  500000 : bitrate = canBITRATE_500K; break;
			case  250000 : bitrate = canBITRATE_250K; break;
			case  125000 : bitrate = canBITRATE_125K; break;
			default:
				fprintf(stderr, "Unsupported bitrate: %d\n", ch->bitrate);
				kv_close_channel(channel_num);
				return -1;
		}

        status = canSetBusParams(ch->handle, canBITRATE_500K, 0, 0, 0, 0, 0);
        if (status != canOK) {
            print_kvaser_error("canSetBusParams", status);
			kv_close_channel(channel_num);
            return -1;
        }
    }

    status = canBusOn(ch->handle);
    if (status != canOK) {
        print_kvaser_error("canBusOn", status);
		kv_close_channel(channel_num);
        return -1;
    }

	ch->state = 1;

    return 0;
}

int kv_write(int channel_num, can_frame *cf){
    canStatus status;
    can_channel* ch = NULL;

	ch = kv_channel_info(channel_num);

	if (!ch->state){
        fprintf(stderr, "channel %d is not opened yet\n", channel_num);
        return -1;
	}

	status = canWriteWait(ch->handle, cf->id, cf->msg, cf->dlc, cf->flag, KV_TIMEOUT);

    if (status != canOK) {
        print_kvaser_error("canWriteWait", status);
        return -1;
    }

	return 0;
}

int kv_read(int channel_num, long *id, void *msg, unsigned int *dlc, unsigned int *flag, unsigned long *time){
    canStatus status;
    can_channel* ch = NULL;

	ch = kv_channel_info(channel_num);
	if (!ch->state){
        fprintf(stderr, "channel %d is not opened yet\n", channel_num);
        return -1;
	}

	status = canReadWait(ch->handle, id, msg, dlc, flag, time, KV_TIMEOUT);

    if (status != canOK) {
        //print_kvaser_error("canWriteWait", status);
        return -1;
    }

	return 0;
}
