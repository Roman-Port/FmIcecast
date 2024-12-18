#pragma once

#include <dsp/convert/real_to_complex.h>
#include <dsp/channel/frequency_xlator.h>
#include <dsp/multirate/rational_resampler.h>
#include <dsp/loop/fast_agc.h>
#include <dsp/loop/costas.h>
#include <dsp/taps/tap.h>
#include <dsp/filter/fir.h>
#include <dsp/clock_recovery/mm.h>
#include <dsp/digital/differential_decoder.h>

class fmice_rds_dec {

public:
	fmice_rds_dec(int bufferSize);
	~fmice_rds_dec();

	void configure(int sampleRate);
	int process(const float* mpx, uint8_t* bitsOut, int count);

private:
	int buffer_size;

	dsp::convert::RealToComplex rtoc;
	dsp::channel::FrequencyXlator xlator;
	dsp::multirate::RationalResampler<dsp::complex_t> rds_resamp;
	dsp::loop::FastAGC<dsp::complex_t> agc;
	dsp::loop::Costas<2> costas;
	dsp::tap<dsp::complex_t> taps;
	dsp::filter::FIR<dsp::complex_t, dsp::complex_t> fir;
	dsp::loop::Costas<2> costas2;
	dsp::clock_recovery::MM<float> recov;

};