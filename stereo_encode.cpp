#include "stereo_encode.h"

#include <volk/volk.h>
#include <dsp/taps/low_pass.h>
#include <math.h>
#include <cassert>

fmice_stereo_encode::fmice_stereo_encode(int bufferSize, float pilotLevel, float sampleRate, float audioFilterCutoff, float audioFilterTrans) :
	pilot_level(pilotLevel)
{
	//Init audio filters
	audio_filter_taps = dsp::taps::lowPass(audioFilterCutoff, audioFilterTrans, sampleRate);
	printf("Stereo Generator Audio taps: %i\n", audio_filter_taps.size);
	audio_filter_lpr.init(NULL, audio_filter_taps);
	audio_filter_lpr.out.setBufferSize(bufferSize);
	audio_filter_lmr.init(NULL, audio_filter_taps);
	audio_filter_lmr.out.setBufferSize(bufferSize);

	//Set up pilot generator
	pilot_osc_phase = 0;
	pilot_osc_inc = 2 * M_PI * 19000 / sampleRate;
}

fmice_stereo_encode::~fmice_stereo_encode() {

}

void fmice_stereo_encode::process(float* mpxOut, const float* lprIn, const float* lmrIn, int count) {
	//Filter L+R and L-R
	audio_filter_lpr.process(count, lprIn, audio_filter_lpr.out.writeBuf);
	audio_filter_lmr.process(count, lmrIn, audio_filter_lmr.out.writeBuf);

	//Add L+R into buffer
	//volk_32f_x2_add_32f(mpxOut, mpxOut, audio_filter_lpr.out.writeBuf, count);
	memcpy(mpxOut, audio_filter_lpr.out.writeBuf, sizeof(float) * count);

	//Generate
	float pilotx1;
	float pilotx2;
	for (int i = 0; i < count; i++) {
		//Get pilot
		pilotx1 = sinf(pilot_osc_phase);
		pilotx2 = sinf(pilot_osc_phase * 2);

		//Step
		pilot_osc_phase += pilot_osc_inc;
		if (abs(pilot_osc_phase) > M_PI) {
			while (pilot_osc_phase > M_PI)
				pilot_osc_phase -= 2 * M_PI;
			while (pilot_osc_phase < -M_PI)
				pilot_osc_phase += 2 * M_PI;
		}

		//Add 19 kHz pilot
		mpxOut[i] += pilotx1 * pilot_level;

		//Add mix + L-R
		mpxOut[i] += pilotx2 * audio_filter_lmr.out.writeBuf[i];
	}
}