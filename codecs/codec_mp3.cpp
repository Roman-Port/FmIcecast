#include "codec_mp3.h"

#include <volk/volk.h>
#include <stdexcept>
#include <cassert>
#include "../defines.h"

fmice_codec_mp3::fmice_codec_mp3(int sampleRate, int channels) : fmice_codec(sampleRate, channels) {
    //Initialize encoder
    create_encoder();

    //Calculate the size of the MP3 data buffer using the worst-case scenario in https://github.com/gypified/libmp3lame/blob/master/API
    output_buffer_size = (int)(1.25 * FMICE_BLOCK_SIZE + 7200);
    output_buffer = (unsigned char*)malloc(output_buffer_size);
    if (output_buffer == nullptr)
        throw std::runtime_error("Failed to allocate output buffer.");
}

fmice_codec_mp3::~fmice_codec_mp3() {
    //Free output buffer
    free(output_buffer);

    //Destroy stream encoder
    if (gfp != NULL) {
        lame_close(gfp);
        gfp = NULL;
    }
}

void fmice_codec_mp3::configure_shout(shout_t* ice) {
    //Set the content type
    shout_set_content_format(ice, SHOUT_FORMAT_MP3, SHOUT_USAGE_AUDIO, NULL);
}

void fmice_codec_mp3::create_encoder() {
    //Initialize
    gfp = lame_init();
    if (gfp == nullptr)
        throw std::runtime_error("Failed to create encoder.");

    //Configure
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, sample_rate);
    lame_set_brate(gfp, 128);
    lame_set_mode(gfp, JOINT_STEREO);
    lame_set_quality(gfp, 2);   /* 2=high  5 = medium  7=low */

    //Validate
    if (lame_init_params(gfp) < 0)
        throw std::runtime_error("Invalid MP3 configuration.");
}

void fmice_codec_mp3::reset() {
    //Destroy stream encoder
    if (gfp != NULL) {
        lame_close(gfp);
        gfp = NULL;
    }

    //Recreate
    create_encoder();
}

void fmice_codec_mp3::process(float* samples, int count) {
    //Check all samples for clipping
    int clipping = 0;
    for (int i = 0; i < count; i++) {
        if (samples[i] > 1.0f) {
            samples[i] = 1.0f;
            clipping++;
        }
        if (samples[i] < -1.0f) {
            samples[i] = -1.0f;
            clipping++;
        }
    }
    
    //Warn on clipping
    if (clipping > 0)
        printf("[CODEC-MP3] WARN: %i samples in block were clipping.\n", clipping);

    //Encode
    int result = lame_encode_buffer_interleaved_ieee_float(gfp, samples, count, output_buffer, output_buffer_size);
    if (result < 0) {
        //Log error
        printf("[CODEC-MP3] Encoder returned bad error code: %i!\n", result);
        
        //Notify of error
        signal_error();
    }
    else {
        //Push output data
        push_out(output_buffer, result);
    }
}