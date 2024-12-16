#include "stereo_demod.h"

#include <dsp/taps/low_pass.h>
#include <dsp/taps/band_pass.h>
#include <dsp/math/conjugate.h>
#include <dsp/math/multiply.h>
#include <dsp/math/add.h>
#include <dsp/math/subtract.h>
#include <dsp/convert/l_r_to_stereo.h>
#include <cassert>

// Very inspired by SDR++ FM demodulator

fmice_stereo_demod::fmice_stereo_demod(int bufferSize) {
    //Set
    this->buffer_size = bufferSize;

    //Allocate buffers
    l = (float*)volk_malloc(sizeof(float) * bufferSize, volk_get_alignment());
    r = (float*)volk_malloc(sizeof(float) * bufferSize, volk_get_alignment());
    lmr = (float*)volk_malloc(sizeof(float) * bufferSize, volk_get_alignment());

    //Validate
    assert(l != NULL);
    assert(r != NULL);
    assert(lmr != NULL);
}

fmice_stereo_demod::~fmice_stereo_demod() {
    //Free buffers
    volk_free(l);
    volk_free(r);
    volk_free(lmr);
}

void fmice_stereo_demod::init(int sampleRate, int audioDecimRate, double audioFilterCutoff, double audioFilterTrans, double deemphasisRate) {
    //Init pilot filter
    pilot_filter_taps = dsp::taps::bandPass<dsp::complex_t>(18750.0, 19250.0, 3000.0, sampleRate, true);
    pilotFir.init(NULL, pilot_filter_taps);
    pilotFir.out.setBufferSize(buffer_size);

    //Init real to complex for converting mpx to complex
    rtoc.init(NULL);
    rtoc.out.setBufferSize(buffer_size);

    //Init pilot PLL
    pilot_pll.init(NULL, 25000.0 / sampleRate, 0.0, dsp::math::hzToRads(19000.0, sampleRate), dsp::math::hzToRads(18750.0, sampleRate), dsp::math::hzToRads(19250.0, sampleRate));
    pilot_pll.out.setBufferSize(buffer_size);

    //Init delays for the pilot filter
    lpr_delay.init(NULL, ((pilot_filter_taps.size - 1) / 2) + 1);
    lpr_delay.out.setBufferSize(buffer_size);
    lmr_delay.init(NULL, ((pilot_filter_taps.size - 1) / 2) + 1);
    lmr_delay.out.setBufferSize(buffer_size);

    //Init audio filters
    audio_filter_taps = dsp::taps::lowPass(audioFilterCutoff, audioFilterTrans, sampleRate);
    audio_filter_l.init(NULL, audio_filter_taps, audioDecimRate);
    audio_filter_l.out.setBufferSize(buffer_size);
    audio_filter_r.init(NULL, audio_filter_taps, audioDecimRate);
    audio_filter_r.out.setBufferSize(buffer_size);

    //Reset and calculate deemphesis alpha
    deemphasis_alpha = 0;
    deemphasis_state_l = 0;
    deemphasis_state_r = 0;
    if (deemphasisRate != 0)
        deemphasis_alpha = 1.0f - exp(-1.0f / ((sampleRate / audioDecimRate) * (deemphasisRate * 1e-6f)));
}

void process_deemphasis(float alpha, float* state, float* buffer, int count) {
    for (int i = 0; i < count; i++)
    {
        *state += alpha * (buffer[i] - *state);
        buffer[i] = *state;
    }
}

int fmice_stereo_demod::process(float* mpxIn, dsp::stereo_t* audioOut, int count) {
    //Convert to complex
    rtoc.process(count, mpxIn, rtoc.out.writeBuf);

    //Filter out pilot and run through PLL
    pilotFir.process(count, rtoc.out.writeBuf, pilotFir.out.writeBuf);
    pilot_pll.process(count, pilotFir.out.writeBuf, pilot_pll.out.writeBuf);

    //Delay to keep in phase
    lpr_delay.process(count, mpxIn, mpxIn);
    lmr_delay.process(count, rtoc.out.writeBuf, lmr_delay.out.writeBuf);

    //Conjugate PLL output to down convert twice the L-R signal
    dsp::math::Conjugate::process(count, pilot_pll.out.writeBuf, pilot_pll.out.writeBuf);
    dsp::math::Multiply<dsp::complex_t>::process(count, lmr_delay.out.writeBuf, pilot_pll.out.writeBuf, lmr_delay.out.writeBuf);
    dsp::math::Multiply<dsp::complex_t>::process(count, lmr_delay.out.writeBuf, pilot_pll.out.writeBuf, lmr_delay.out.writeBuf);

    //Convert output back to real for further processing
    dsp::convert::ComplexToReal::process(count, lmr_delay.out.writeBuf, lmr);

    //Amplify by 2x
    volk_32f_s32f_multiply_32f(lmr, lmr, 2.0f, count);

    //Do L = (L+R) + (L-R), R = (L+R) - (L-R)
    dsp::math::Add<float>::process(count, mpxIn, lmr, l);
    dsp::math::Subtract<float>::process(count, mpxIn, lmr, r);

    //Filter audio
    int countL = audio_filter_l.process(count, l, l);
    count = audio_filter_r.process(count, r, r);
    assert(countL == count);

    //Apply deemphesis
    if (deemphasis_alpha != 0) {
        process_deemphasis(deemphasis_alpha, &deemphasis_state_l, l, count);
        process_deemphasis(deemphasis_alpha, &deemphasis_state_r, r, count);
    }

    //Interleave into stereo
    dsp::convert::LRToStereo::process(count, l, r, audioOut);

    return count;
}