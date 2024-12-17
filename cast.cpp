#include "defines.h"
#include <stdio.h>

#include <string.h>
#include <pthread.h>
#include <stdexcept>
#include "circular_buffer.h"
#include "cast.h"

#define NET_BUFFER_BYTES 65536

static int is_icecast_initialized = 0;

fmice_icecast::fmice_icecast(int channels, int sampRate, int inputBufferSamples) {
    //Set parameters
    this->channels = channels;
    this->sample_rate = sampRate;
    this->input_buffer_samples = inputBufferSamples;

    //Allocate the input buffer
    input_buffer = (int32_t*)malloc(sizeof(int32_t) * inputBufferSamples * channels);
    if (input_buffer == NULL)
        throw new std::runtime_error("Failed to allocate input buffer.");

    //Init buffer
    circ_buffer = new fmice_circular_buffer<uint8_t>(NET_BUFFER_BYTES * 32);

    //Clear
    samples_dropped = 0;
    samples_sent = 0;
    memset(icecast_host, 0, sizeof(icecast_host));
    icecast_port = 0;
    memset(icecast_mount, 0, sizeof(icecast_mount));
    memset(icecast_username, 0, sizeof(icecast_username));
    memset(icecast_password, 0, sizeof(icecast_password));

    //Init global icecast if it's not
    if (!is_icecast_initialized)
        shout_init();
    is_icecast_initialized = 1;
}

fmice_icecast::~fmice_icecast() {

}

void fmice_icecast::set_host(const char* hostname) {
    strncpy(icecast_host, hostname, sizeof(icecast_host) - 1);
    icecast_host[sizeof(icecast_host) - 1] = 0;
}

void fmice_icecast::set_port(unsigned int port) {
    icecast_port = port;
}

void fmice_icecast::set_mount(const char* mount) {
    strncpy(icecast_mount, mount, sizeof(icecast_mount) - 1);
    icecast_mount[sizeof(icecast_mount) - 1] = 0;
}

void fmice_icecast::set_username(const char* username) {
    strncpy(icecast_username, username, sizeof(icecast_username) - 1);
    icecast_username[sizeof(icecast_username) - 1] = 0;
}

void fmice_icecast::set_password(const char* password) {
    strncpy(icecast_password, password, sizeof(icecast_password) - 1);
    icecast_password[sizeof(icecast_password) - 1] = 0;
}

bool fmice_icecast::is_configured() {
    return strlen(icecast_host) > 0 &&
        icecast_port > 0 &&
        strlen(icecast_mount) > 0 &&
        strlen(icecast_username) > 0 &&
        strlen(icecast_password);
}

void fmice_icecast::init() {
    //Make sure we're ready
    if (!is_configured())
        throw new std::runtime_error("Icecast is not configured.");

    //Allocate shoutcast
    shout = shout_new();
    if (!shout)
        throw new std::runtime_error("Failed to allocate shout_t.\n");

    //Set up paramters
    if (shout_set_host(shout, icecast_host) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting hostname.");
    if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting protocol.");
    if (shout_set_port(shout, icecast_port) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting port.");
    if (shout_set_password(shout, icecast_password) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting password.");
    if (shout_set_mount(shout, icecast_mount) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting mount.");
    if (shout_set_user(shout, icecast_username) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting user.");
    if (shout_set_content_format(shout, SHOUT_FORMAT_OGG, SHOUT_USAGE_UNKNOWN, NULL) != SHOUTERR_SUCCESS)
        throw new std::runtime_error("Error setting format.");

    //Connect
    if (shout_open(shout) != SHOUTERR_SUCCESS) {
        throw new std::runtime_error("Failed to connect to server.");
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
    FLAC__stream_encoder_set_sample_rate(flac, sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(flac, 0);

    //Init encoder
    if (FLAC__stream_encoder_init_ogg_stream(flac, 0, flac_push_cb_static, 0, 0, 0, this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        throw new std::runtime_error("Failed to init FLAC stream.");

    //Start worker thread
    pthread_create(&worker_thread, NULL, work_static, this);
}

FLAC__StreamEncoderWriteStatus fmice_icecast::flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data) {
    return ((fmice_icecast*)client_data)->flac_push_cb(encoder, buffer, bytes, samples, current_frame);
}

void fmice_icecast::push(dsp::stereo_t* samples, int count) {
    push((float*)samples, count);
}

void fmice_icecast::push(float* samples, int count) {
    int readOffset = 0;
    while (count > 0) {
        //Determine how many samples PER CHANNEL we can write to the buffer (avail in buffer - avail in input)
        int readable = std::min(count, input_buffer_samples - input_buffer_use);

        //Read and convert simultaenously
        volk_32f_s32f_convert_32i(&input_buffer[input_buffer_use * channels], &samples[readOffset * channels], 32767, readable * channels);

        //Update states
        input_buffer_use += readable;
        readOffset += readable;
        count -= readable;

        //If buffer is full, process
        if (input_buffer_use == input_buffer_samples)
            submit_buffer();
    }
}

void fmice_icecast::submit_buffer() {
    //Check all samples for clipping - They break FLAC
    int clipping = 0;
    for (int i = 0; i < input_buffer_samples * channels; i++) {
        if (input_buffer[i] > 32767) {
            input_buffer[i] = 32767;
            clipping++;
        }
        if (input_buffer[i] < -32767) {
            input_buffer[i] = -32767;
            clipping++;
        }
    }

    //Warn on clipping
    if (clipping > 0)
        printf("[CAST] WARN: %i samples in block were clipping.\n", clipping);

    //Process with FLAC
    if (!FLAC__stream_encoder_process_interleaved(flac, input_buffer, input_buffer_samples)) {
        printf("[CAST} ERR: Failed to encode FLAC data: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(flac)]);
    }

    //Reset state
    input_buffer_use = 0;
}

FLAC__StreamEncoderWriteStatus fmice_icecast::flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t count, uint32_t samples, uint32_t current_frame) {
    //Push into circular buffer
    size_t dropped = count - circ_buffer->write((const uint8_t*)buffer, count);
    if (dropped > 0) {
        samples_dropped += dropped;
        printf("WARN: FLAC data dropped! Network can't keep up. Dropped %i bytes so far.\n", samples_dropped);
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

void* fmice_icecast::work_static(void* ctx) {
    ((fmice_icecast*)ctx)->work();
    return 0;
}

void fmice_icecast::work() {
    uint8_t workingBuffer[NET_BUFFER_BYTES];
    while (1) {
        //Wait for buffers to become available
        circ_buffer->read(workingBuffer, NET_BUFFER_BYTES);

        //Send
        if (shout_send(shout, workingBuffer, NET_BUFFER_BYTES) != SHOUTERR_SUCCESS) {
            printf("Failed to send FLAC packet to Icecast: %s\n", shout_get_error(shout));
        }
    }
}