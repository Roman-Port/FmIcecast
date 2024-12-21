#pragma once

#include <dsp/filter/fir.h>
#include <dsp/filter/decimating_fir.h>
#include <dsp/convert/real_to_complex.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/loop/pll.h>
#include <dsp/math/delay.h>

class fmice_stereo_demod {

public:
	fmice_stereo_demod(int bufferSize);
	~fmice_stereo_demod();

	void init(int sampleRate, int audioDecimRate, double audioFilterCutoff, double audioFilterTrans, double deemphasisRate);
    int process(float* mpxIn, dsp::stereo_t* audioOut, int count);

    float* lmr; // L-R buffer at input sample rate, used for re-encoding stereo
    float* lpr; // L+R buffer at input sample rate, used for re-encoding stereo

private:
    int buffer_size;

    float* l;
    float* r;

    dsp::tap<dsp::complex_t> pilot_filter_taps;
    dsp::filter::FIR<dsp::complex_t, dsp::complex_t> pilotFir;
    dsp::convert::RealToComplex rtoc;
    dsp::loop::PLL pilot_pll;
    dsp::math::Delay<float> lpr_delay;
    dsp::math::Delay<dsp::complex_t> lmr_delay;
    dsp::tap<float> audio_filter_taps;
    dsp::filter::DecimatingFIR<float, float> audio_filter_l;
    dsp::filter::DecimatingFIR<float, float> audio_filter_r;

    float deemphasis_alpha;
    float deemphasis_state_l;
    float deemphasis_state_r;

};