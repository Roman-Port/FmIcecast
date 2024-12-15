#include "defines.h"
#include <stdio.h>

#include <string.h>
#include <pthread.h>
#include <stdexcept>
#include "circular_buffer.h"
#include "cast.h"

#define CAST_FIFOS_COUNT 32

static int is_icecast_initialized = 0;

fmice_icecast::fmice_icecast(const char* host, unsigned short port, const char* mount, const char* username, const char* password, int channels, int sampRate) {
    //Init buffer
    circ_buffer = new fmice_circular_buffer<int32_t>(CAST_BUFFER_SIZE * CAST_FIFOS_COUNT);

    //Clear
    samples_dropped = 0;
    samples_sent = 0;
    this->channels = channels;

    //Init global icecast if it's not
    if (!is_icecast_initialized)
        shout_init();
    is_icecast_initialized = 1;

    //Allocate shoutcast
    shout = shout_new();
    if (!shout)
        throw new std::runtime_error("Failed to allocate shout_t.\n");

    //Set up paramters
    if (shout_set_host(shout, host) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting hostname.");
    if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting protocol.");
    if (shout_set_port(shout, port) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting port.");
    if (shout_set_password(shout, password) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting password.");
    if (shout_set_mount(shout, mount) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting mount.");
    if (shout_set_user(shout, username) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting user.");
    if (shout_set_content_format(shout, SHOUT_FORMAT_OGG, SHOUT_USAGE_UNKNOWN, NULL) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting format.");

    //Connect
    printf("Connecting to the server...");
    if (shout_open(shout) != SHOUTERR_SUCCESS) {
        throw new std::runtime_error("Failed to connect to server.");
    }
    else {
        printf("Connected!\n");
    }

    //Now, allocate FLAC
    flac = FLAC__stream_encoder_new();
    if (!flac)
        throw new std::runtime_error("Failed to allocate FLAC.");

    //Set up FLAC
    FLAC__stream_encoder_set_verify(flac, false);
    FLAC__stream_encoder_set_compression_level(flac, 5);
    FLAC__stream_encoder_set_channels(flac, channels);
    FLAC__stream_encoder_set_bits_per_sample(flac, 16);
    FLAC__stream_encoder_set_sample_rate(flac, sampRate);
    FLAC__stream_encoder_set_total_samples_estimate(flac, 0);

    //Init encoder
    if (FLAC__stream_encoder_init_ogg_stream(flac, 0, flac_push_cb_static, 0, 0, 0, this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        throw new std::runtime_error("Failed to init FLAC stream.");

    //Start worker thread
    pthread_create(&worker_thread, NULL, work_static, this);
}

fmice_icecast::~fmice_icecast() {

}

FLAC__StreamEncoderWriteStatus fmice_icecast::flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame) {
    //printf("FLAC WRITE: %i bytes, %i samples, %i frame\n", bytes, samples, current_frame);

    //Send on wire
    if (shout_send(shout, buffer, bytes) != SHOUTERR_SUCCESS) {
        printf("Failed to send FLAC packet to Icecast: %s\n", shout_get_error(shout));
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderWriteStatus fmice_icecast::flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data) {
    return ((fmice_icecast*)client_data)->flac_push_cb(encoder, buffer, bytes, samples, current_frame);
}

void* fmice_icecast::work_static(void* ctx) {
    while (((fmice_icecast*)ctx)->work() == 0);
    return 0;
}

int fmice_icecast::work() {
    //Wait for buffers to become available
    int32_t working_buffer[CAST_BUFFER_SIZE];
    circ_buffer->read(working_buffer, CAST_BUFFER_SIZE);

    //Send on wire
    if (!FLAC__stream_encoder_process_interleaved(flac, working_buffer, CAST_BUFFER_SIZE / channels)) {
        printf("Failed to encode FLAC data: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(flac)]);
        return -1;
    }

    return 0;
}

void fmice_icecast::push(float* samples, int count) {
    while (count > 0) {
        //Determine how much we can write to the buffer
        int readable = std::min(count, CAST_BUFFER_SIZE - in_buffer_use);

        //Loop through and remove any clipping samples - they break FLAC
        for (int i = 0; i < readable; i++) {
            if (samples[i] > 1) {
                printf("WARN: Sample %f clips >1!\n", samples[i]);
                samples[i] = 1;
            }
            if (samples[i] < -1) {
                printf("WARN: Sample %f clips <-1!\n", samples[i]);
                samples[i] = -1;
            }
        }

        //Read and convert simultaenously
        volk_32f_s32f_convert_32i(&in_buffer[in_buffer_use], samples, 32767, readable);

        //Update states
        in_buffer_use += readable;
        samples += readable;
        count -= readable;

        //If buffer is full, process
        if (in_buffer_use == CAST_BUFFER_SIZE) {
            //Push into circular buffer
            size_t dropped = CAST_BUFFER_SIZE - circ_buffer->write(in_buffer, CAST_BUFFER_SIZE);
            if (dropped > 0) {
                samples_dropped += dropped;
                printf("WARN: Audio samples dropped! Network can't keep up. Dropped %i samples so far.\n", samples_dropped);
            }

            //Finally, reset counter
            in_buffer_use = 0;
        }
    }
}