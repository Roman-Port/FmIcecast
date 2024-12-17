#include "codec_flac.h"

#include <volk/volk.h>
#include <stdexcept>
#include <cassert>

fmice_codec_flac::fmice_codec_flac(int sampleRate, int channels, int blockSize) : fmice_codec(sampleRate, channels) {
    //Set
    this->input_buffer_samples = blockSize;

    //Allocate the input buffer
    input_buffer = (int32_t*)malloc(sizeof(int32_t) * blockSize * channels);
    if (input_buffer == NULL)
        throw new std::runtime_error("Failed to allocate input buffer.");

    //Initialize FLAC
    create_flac();
}

fmice_codec_flac::~fmice_codec_flac() {
    //Free input buffer
    free(input_buffer);

    //Destroy stream encoder
    if (flac != NULL) {
        FLAC__stream_encoder_delete(flac);
        flac = NULL;
    }
}

void fmice_codec_flac::create_flac() {
    //Allocate FLAC
    flac = FLAC__stream_encoder_new();
    if (!flac)
        throw new std::runtime_error("Failed to allocate FLAC.");

    //Set up FLAC
    FLAC__stream_encoder_set_verify(flac, false);
    FLAC__stream_encoder_set_compression_level(flac, 1);
    FLAC__stream_encoder_set_channels(flac, channels);
    FLAC__stream_encoder_set_bits_per_sample(flac, 16);
    FLAC__stream_encoder_set_sample_rate(flac, sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(flac, 0);

    //Init encoder
    if (FLAC__stream_encoder_init_ogg_stream(flac, 0, flac_push_cb_static, 0, 0, 0, this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        throw new std::runtime_error("Failed to init FLAC stream.");
}

void fmice_codec_flac::reset_safe() {
    //Destroy stream encoder
    if (flac != NULL) {
        FLAC__stream_encoder_delete(flac);
        flac = NULL;
    }

    //Reset base class
    fmice_codec::reset_safe();

    //Create a new one
    create_flac();
}

bool fmice_codec_flac::write_safe(float* samples, int count) {
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
        if (input_buffer_use == input_buffer_samples && !submit_buffer())
            return false;
    }
    return true;
}

bool fmice_codec_flac::submit_buffer() {
    //Sanity check
    assert(flac != NULL);

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
        printf("[CODEC-FLAC] WARN: %i samples in block were clipping.\n", clipping);

    //Process with FLAC
    bool success = FLAC__stream_encoder_process_interleaved(flac, input_buffer, input_buffer_samples);

    //Reset state
    input_buffer_use = 0;

    return success;
}

FLAC__StreamEncoderWriteStatus fmice_codec_flac::flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data) {
    return ((fmice_codec_flac*)client_data)->flac_push_cb(encoder, buffer, bytes, samples, current_frame);
}

FLAC__StreamEncoderWriteStatus fmice_codec_flac::flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame) {
    push_out(buffer, bytes);
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}