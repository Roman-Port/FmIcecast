#include "defines.h"
#include <stdio.h>
#include <shout/shout.h>
#include <FLAC/stream_encoder.h>
#include <string.h>
#include <pthread.h>

#define CAST_BUFFER_SIZE 32768
#define CAST_FIFOS_COUNT 32

typedef struct {
    int32_t buffer[CAST_BUFFER_SIZE]; // The buffer allocated for this
} cast_buffer_t;

static shout_t* shout;
static FLAC__StreamEncoder* flac;

// In buffer - This is what cast_push_sample accesses and should only be used by its thread
static float in_buffer[CAST_BUFFER_SIZE];
static int in_buffer_use = 0;

// FIFO buffer - This holds data from the radio thread for the network thread to pick up
// To access any of these, make sure we are locked in a mutex
static pthread_mutex_t cast_lock;
static cast_buffer_t cast_buffers[CAST_FIFOS_COUNT];
static int cast_buffers_read = 0;
static int cast_buffers_write = 0;
static int cast_buffers_consumed = 0;
static int cast_buffers_dropped = 0; // Number of buffers dropped (i.e. the network can't keep up)

static FLAC__StreamEncoderWriteStatus flac_write_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data) {
    //printf("FLAC WRITE: %i bytes, %i samples, %i frame\n", bytes, samples, current_frame);

    //Send on wire
    if (shout_send(shout, buffer, bytes) != SHOUTERR_SUCCESS) {
        printf("Failed to send FLAC packet to Icecast: %s\n", shout_get_error(shout));
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

int cast_init(const char* host, unsigned short port, const char* mount, const char* username, const char* password) {
    //Initialize
    shout_init();

    //Init mutex
    if (pthread_mutex_init(&cast_lock, NULL) != 0) {
        printf("Failed to initialize mutex.\n");
        return -1;
    }

	//Allocate shoutcast
	shout = shout_new();
	if (!shout) {
		printf("Failed to allocate shout_t.\n");
		return -1;
	}

    //Set up paramters
    if (shout_set_host(shout, host) != SHOUTERR_SUCCESS) {
        printf("Error setting hostname: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
        printf("Error setting protocol: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
        printf("Error setting port: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_password(shout, password) != SHOUTERR_SUCCESS) {
        printf("Error setting password: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_mount(shout, mount) != SHOUTERR_SUCCESS) {
        printf("Error setting mount: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_user(shout, username) != SHOUTERR_SUCCESS) {
        printf("Error setting user: %s\n", shout_get_error(shout));
        return -1;
    }

    if (shout_set_content_format(shout, SHOUT_FORMAT_OGG, SHOUT_USAGE_UNKNOWN, NULL) != SHOUTERR_SUCCESS) {
        printf("Error setting format: %s\n", shout_get_error(shout));
        return -1;
    }

    //Connect
    printf("Connecting to the server...");
    if (shout_open(shout) != SHOUTERR_SUCCESS) {
        printf("Error: %s\n", shout_get_error(shout));
        return -1;
    }
    else {
        printf("Connected!\n");
    }

    //Now, allocate FLAC
    flac = FLAC__stream_encoder_new();
    if (!flac) {
        printf("Error: Failed to allocate FLAC.\n");
        return -1;
    }

    //Set up FLAC
    FLAC__stream_encoder_set_verify(flac, false);
    FLAC__stream_encoder_set_compression_level(flac, 5);
    FLAC__stream_encoder_set_channels(flac, 1);
    FLAC__stream_encoder_set_bits_per_sample(flac, 16);
    FLAC__stream_encoder_set_sample_rate(flac, MPX_SAMP_RATE);
    FLAC__stream_encoder_set_total_samples_estimate(flac, 1000);

    //Init encoder
    if (FLAC__stream_encoder_init_ogg_stream(flac, 0, flac_write_cb, 0, 0, 0, 0) != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        printf("Failed to initialize FLAC stream.\n");
        return -1;
    }

    //Test
    cast_buffers_consumed = 1;
    cast_buffers_write = 1;
    cast_transmit();

    /*int32_t temp[CAST_BUFFER_SIZE];
    for (int i = 0; i < CAST_BUFFER_SIZE; i++)
        temp[i] = i;
    for (int i = 0; i < 50; i++) {
        if (!FLAC__stream_encoder_process_interleaved(flac, temp, CAST_BUFFER_SIZE)) {
            printf("Failed to encode FLAC data.\n");
            return -1;
        }
    }*/
    return 0;
}

int cast_transmit() {
    //Lock
    pthread_mutex_lock(&cast_lock);

    //Check if any buffers are available, and if so copy it out while we're locking the mutex to minimize the time we're locking it
    int32_t working_buffer[CAST_BUFFER_SIZE];
    int available = cast_buffers_consumed;
    if (available > 0) {
        //Pop a buffer off and copy it in
        memcpy(working_buffer, cast_buffers[cast_buffers_read].buffer, sizeof(working_buffer));

        //Update FIFO state
        cast_buffers_read = (cast_buffers_read + 1) % CAST_FIFOS_COUNT;
        cast_buffers_consumed--;
    }

    //Unlock
    pthread_mutex_unlock(&cast_lock);

    //Send on wire
    if (available > 0) {
        if (!FLAC__stream_encoder_process_interleaved(flac, working_buffer, CAST_BUFFER_SIZE)) {
            printf("Failed to encode FLAC data: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(flac)]);
            return -1;
        }
    }

    return 0;
}

void cast_push_sample(float sample) {
    //Clip check, then push into the buffer
    if (sample > 1) {
        in_buffer[in_buffer_use++] = 1;
        printf("WARN: Sample %f clips >1!\n", sample);
    }
    else if (sample < -1) {
        in_buffer[in_buffer_use++] = -1;
        printf("WARN: Sample %f clips <-1!\n", sample);
    }
    else {
        in_buffer[in_buffer_use++] = sample;
    }

    //Check if the buffer is ready to be submitted
    if (in_buffer_use == CAST_BUFFER_SIZE) {
        //Lock
        pthread_mutex_lock(&cast_lock);

        //Check if there's room in the FIFO to push it
        if (cast_buffers_consumed < CAST_FIFOS_COUNT) {
            //Copy into the first available buffer. We also convert it to the sample format FLAC expects here
            volk_32f_s32f_convert_32i(cast_buffers[cast_buffers_write].buffer, in_buffer, 32767, CAST_BUFFER_SIZE);

            //Update FIFO state
            cast_buffers_write = (cast_buffers_write + 1) % CAST_FIFOS_COUNT;
            cast_buffers_consumed++;

            //TEST
            printf("DEBUG: read=%i; write=%i; use=%i; free=%i; dropped=%i\n", cast_buffers_read, cast_buffers_write, cast_buffers_consumed, CAST_FIFOS_COUNT - cast_buffers_consumed, cast_buffers_dropped);
        }
        else {
            //No room! We need to drop this buffer
            cast_buffers_dropped++;
            printf("WARN: Audio buffer dropped! Network can't keep up. Dropped %i buffers so far.\n", cast_buffers_dropped);
        }

        //Unlock
        pthread_mutex_unlock(&cast_lock);

        //Finally, reset counter
        in_buffer_use = 0;
    }
}