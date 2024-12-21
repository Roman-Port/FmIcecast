#pragma once

#include <dsp/filter/fir.h>
#include <dsp/channel/frequency_xlator.h>

class fmice_stereo_encode {

public:
	fmice_stereo_encode(int bufferSize, float pilotLevel, float sampleRate, float audioFilterCutoff, float audioFilterTrans);
	~fmice_stereo_encode();

	void process(float* mpxOut, const float* lprIn, const float* lmrIn, int count);

private:
	dsp::tap<float> audio_filter_taps;
	dsp::filter::FIR<float, float> audio_filter_lmr;
	dsp::filter::FIR<float, float> audio_filter_lpr;

	double pilot_osc_phase;
	double pilot_osc_inc;

	float pilot_level;

};