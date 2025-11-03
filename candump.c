#include "lib.h"

void print_usage_candump(char *arg0, char *arg1)
{
	char prg[_MAX_FNAME];
	char *cmd;

	basename(arg0, prg, sizeof(prg));
	cmd = arg1;

	fprintf(stderr, "%s %s - dump CAN bus traffic with Kvaser driver.\n\n", prg, cmd);
	fprintf(stderr, "Usage: %s %s [options] <channel> [<channel> ...]\n", prg, cmd);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -t <type>                      (timestamp: (a)bsolute/(d)elta - default 'a')\n");
	fprintf(stderr, "  -v                             (verbose CAN flags)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Format of <channel>: <channel-num>{f|F}{_}{b|B<bitrate>}{d|D<data-bitrate>}\n");
	fprintf(stderr, "  examples:\n");
	fprintf(stderr, "    0                            (channel 0, CAN-CC)\n");
	fprintf(stderr, "    0F                           (channel 0, CAN-FD)\n");
	fprintf(stderr, "    0_b500K                      (channel 0, CAN-CC, bitrate 500K)\n");
	fprintf(stderr, "    0Fb500Kd2M                   (channel 0, CAN-FD, arbitration bitrate 500K, data bitrate 2M)\n");
}

// Thread-Safe Queue
typedef struct {
    can_log* buffer;
    int head;
    int tail;
    int count;
    int size;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
} log_queue;

int init_queue(log_queue* queue, int size) {
    queue->buffer = (can_log*)malloc(sizeof(can_log) * size);
    if (queue->buffer == NULL) {
        return -1;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->size = size;

    InitializeCriticalSection(&queue->mutex);
    InitializeConditionVariable(&queue->not_empty);
    InitializeConditionVariable(&queue->not_full);

    return 0;
}

void destroy_queue(log_queue* queue) {
    if (queue->buffer) {
        free(queue->buffer);
        queue->buffer = NULL;
    }

    DeleteCriticalSection(&queue->mutex);
}

int enqueue_frame(log_queue* queue, const can_log* frame) {
    EnterCriticalSection(&queue->mutex);

	while (queue->count >= queue->size && !stop_flag) {
		// Buffer is full - sleep so consumers can get items.
		SleepConditionVariableCS(&queue->not_full, &queue->mutex, INFINITE);
	}

    if (stop_flag) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

	// Copy the log to buffer
    memcpy(&queue->buffer[queue->head], frame, sizeof(can_log));
    queue->head = (queue->head + 1) % queue->size;
    queue->count++;

    LeaveCriticalSection(&queue->mutex);

	// If a consumer is waiting, wake it
    WakeConditionVariable(&queue->not_empty);

    return 0;
}

int dequeue_frame(log_queue* queue, can_log* frame) {
    EnterCriticalSection(&queue->mutex);

	while (queue->count == 0 && !stop_flag) {
		// Buffer is empty - sleep so producers can create items.
		SleepConditionVariableCS(&queue->not_empty, &queue->mutex, 50);
	}

	// Stop flag is set and queue is empty
    if (queue->count == 0 && stop_flag) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

	// Get frame
    memcpy(frame, &queue->buffer[queue->tail], sizeof(can_log));
    queue->tail = (queue->tail + 1) % queue->size;
    queue->count--;

    LeaveCriticalSection(&queue->mutex);

	// If a producer is waiting, wake it.
    WakeConditionVariable(&queue->not_full);

    return 0;
}

typedef struct {
	char timestamp_type;
	uint64_t start_time;
	int verbose;
} output_thread_param;

typedef struct {
	HANDLE thread_handle;
	DWORD thread_id;
} thread;

thread channel_threads[MAX_CHANNELS];

#define MAX_QUEUE_SIZE 10000
log_queue log_q;

DWORD WINAPI channel_thread(LPVOID param) {
    long id;
    unsigned char msg[CANFD_MAX_DLEN];
    unsigned int dlc;
    unsigned int flag;
    unsigned long timestamp;
	can_log log;
	can_channel *tp = (can_channel *)param;

	while(!stop_flag){
		if(kv_read(tp->channel, &id, msg, &dlc, &flag, &timestamp) == 0){
			log.channel = tp->channel;
			log.timestamp = timestamp;
			log.frame.id = id;
			log.frame.dlc = dlc;
			log.frame.flag = flag;
			memcpy(log.frame.msg, msg, dlc);

            enqueue_frame(&log_q, &log);
		}
	}

	return 0;
}

void adjust_timestamp(can_log *log, char type, uint64_t start_time){
	switch(type){
		case 'a':
			log->timestamp += start_time;
			break;
		case 'd':
			// nothing to do
			break;
	}
}

DWORD WINAPI output_thread(LPVOID param) {
	can_log log;
	output_thread_param *tp = (output_thread_param *)param;

	while(!stop_flag){
		if (dequeue_frame(&log_q, &log) == 0) {
			adjust_timestamp(&log, tp->timestamp_type, tp->start_time);
			fprint_log(stdout, &log, tp->verbose);
		}else{
			Sleep(1);
		}
	}


	// output all log before exiting
	while (dequeue_frame(&log_q, &log) == 0){
		adjust_timestamp(&log, tp->timestamp_type, tp->start_time);
		fprint_log(stdout, &log, tp->verbose);
	}

	return 0;
}

int candump(int argc, char *argv[]){
	int i, channel_num, verbose;
	char timestamp_type;
	can_channel ch;
	uint64_t start_time;
	output_thread_param output_tp;
	HANDLE output_thread_handle;
	DWORD output_thread_id;

	if(argc <= 2){
		print_usage_candump(argv[0], argv[1]);
		return EXIT_FAILURE;
	}

	timestamp_type = 'a';
	verbose = 0;

	kv_initialize();
	memset(channel_threads, '\0', sizeof(thread) * MAX_CHANNELS);

	for(i = 2; i < argc; i++){
		if(strcmp(argv[i], "-v") == 0){
			verbose = 1;
		}
		else if(strcmp(argv[i], "-t") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Error: Missing bitrate value after %s\n\n", argv[i]);
				print_usage_candump(argv[0], argv[1]);
				return EXIT_FAILURE;
			}

			i++;
			timestamp_type = argv[i][0];

			if(timestamp_type != 'a' && timestamp_type != 'd'){
				fprintf(stderr, "Error: Invalid timestamp type '%s'\n", argv[i]);
				print_usage_candump(argv[0], argv[1]);
				return EXIT_FAILURE;
			}
		}
		else if(strcmp(argv[i], "help") == 0){
			print_usage_candump(argv[0], argv[1]);
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

    if (init_queue(&log_q, MAX_QUEUE_SIZE) != 0) {
        fprintf(stderr, "Failed to initialize frame queue\n");
        kv_cleanup_channels();
        return 1;
    }

	start_time = get_unix_time();
	kv_sync_bus_on();

	// Run output thread
	output_tp.timestamp_type = timestamp_type;
	output_tp.start_time = start_time;
	output_tp.verbose = verbose;
    output_thread_handle = CreateThread(
        NULL,
        0,
        output_thread,
        &output_tp,
        0,
        &output_thread_id
    );

    if (output_thread_handle == NULL) {
        fprintf(stderr, "Failed to create output thread\n");
        stop_flag = 1;
		goto err;
    }

	// Run channel threads
	for(i = 0; i < MAX_CHANNELS; i++){
		if(channels[i].state){
			channel_threads[i].thread_handle = CreateThread(
				NULL,                  			// Default Security
				0,                      		// Default Stack Size
				channel_thread,         		// Thread Function
				&channels[i],            		// Paremeters
				0,                      		// Default Creation Flag
				&channel_threads[i].thread_id  	// Thread ID
			);
			if(channel_threads[i].thread_handle == NULL){
				fprintf(stderr, "Failed to create thread for channel %d\n", i);
				stop_flag = 1;
				goto err;
			}
		}
	}

	// wait until exiting
    WaitForSingleObject(output_thread_handle, INFINITE);

	// close threads
	for(i = 0; i < MAX_CHANNELS; i++){
		if(channels[i].state){
			CloseHandle(channel_threads[i].thread_handle);
		}
	}

	CloseHandle(output_thread_handle);

    kv_cleanup_channels();
    destroy_queue(&log_q);

	return EXIT_SUCCESS;

err:
    kv_cleanup_channels();
    destroy_queue(&log_q);

	return EXIT_FAILURE;
}
