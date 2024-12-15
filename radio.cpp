#include "defines.h"

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "radio.h"

#include <dsp/taps/low_pass.h>
#include <dsp/taps/band_pass.h>
#include <dsp/math/conjugate.h>
#include <dsp/math/multiply.h>
#include <dsp/math/add.h>
#include <dsp/math/subtract.h>
#include <dsp/convert/l_r_to_stereo.h>
#include <cassert>

#define FM_DEVIATION 85000

#define RADIO_BUFFER_SIZE 65536

fmice_radio::fmice_radio() {
	//Zero out everything
	radio = 0;
	radio_transfer_size = 0;
	radio_buffer = 0;
	output_mpx = 0;
	output_audio = 0;
	deemphasis_alpha = 0;
	deemphasis_state_l = 0;
	deemphasis_state_r = 0;

	//Allocate buffers
	size_t alignment = volk_get_alignment();
	filter_bb_buffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, alignment);
	lmr = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, alignment);
	l = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, alignment);
	r = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, alignment);
	interleaved_buffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, alignment);
	if (filter_bb_buffer == 0 || lmr == 0 || l == 0 || r == 0 || interleaved_buffer == 0)
		throw std::runtime_error("Failed to allocate buffers.");
	radio_buffer = new fmice_circular_buffer<airspyhf_complex_float_t>(RADIO_BUFFER_SIZE * 16);

	//Create baseband filter
	filter_bb_taps = dsp::taps::lowPass(125000, 15000, SAMP_RATE);
	printf("Baseband filter taps: %i\n", filter_bb_taps.size);
	filter_bb.init(NULL, filter_bb_taps);
	filter_bb.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Configure FM demod
	fm_demod.init(NULL, FM_DEVIATION, SAMP_RATE);
	fm_demod.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Create composite filter
	filter_mpx_taps = dsp::taps::lowPass(61000, 1500, SAMP_RATE);
	printf("MPX filter taps: %i\n", filter_mpx_taps.size);
	filter_mpx.init(NULL, filter_mpx_taps, DECIM_RATE);
	filter_mpx.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Create pilot filter
	rtoc.init(NULL);
	pilot_filter_taps = dsp::taps::bandPass<dsp::complex_t>(18750.0, 19250.0, 3000.0, MPX_SAMP_RATE, true);
	printf("19 kHz pilot filter taps: %i\n", pilot_filter_taps.size);
	pilot_filter.init(NULL, pilot_filter_taps);
	pilot_filter.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Initialize PLL for pilot
	pilot_pll.init(NULL, 25000.0 / MPX_SAMP_RATE, 0.0, dsp::math::hzToRads(19000.0, MPX_SAMP_RATE), dsp::math::hzToRads(18750.0, MPX_SAMP_RATE), dsp::math::hzToRads(19250.0, MPX_SAMP_RATE));
	pilot_pll.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Set up delays
	lpr_delay.init(NULL, ((pilot_filter_taps.size - 1) / 2) + 1);
	pilot_pll.out.setBufferSize(RADIO_BUFFER_SIZE);
	lmr_delay.init(NULL, ((pilot_filter_taps.size - 1) / 2) + 1);
	pilot_pll.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Create audio filters
	filter_audio_taps = dsp::taps::lowPass(15000.0, 4000.0, MPX_SAMP_RATE);
	printf("Audio filter taps: %i\n", filter_audio_taps.size);
	filter_audio_l.init(NULL, filter_audio_taps, AUDIO_DECIM_RATE);
	filter_audio_l.out.setBufferSize(RADIO_BUFFER_SIZE);
	filter_audio_r.init(NULL, filter_audio_taps, AUDIO_DECIM_RATE);
	filter_audio_r.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Calculate deemphesis alpha
	deemphasis_alpha = 1.0f - exp(-1.0f / (AUDIO_SAMP_RATE * (75 * 1e-6f)));
}

fmice_radio::~fmice_radio() {

}

void fmice_radio::set_mpx_output(fmice_icecast* ice) {
	output_mpx = ice;
}

void fmice_radio::set_audio_output(fmice_icecast* ice) {
	output_audio = ice;
}

void fmice_radio::open(int freq) {
	//Open
	int result = airspyhf_open(&radio);
	if (result) {
		printf("Failed to open AirSpy HF device: %i.\n", result);
		throw std::runtime_error("Failed to open AirSpy HF Device.");
	}

	//Set sample rate
	result = airspyhf_set_samplerate(radio, SAMP_RATE);
	if (result) {
		printf("Failed to set device sample rate to %i (%i)!\n", SAMP_RATE, result);
		throw std::runtime_error("Failed to set device sample rate.");
	}

	//Set frequency
	result = airspyhf_set_freq(radio, freq);
	if (result) {
		printf("Failed to set device frequency to %i!\n", freq);
		throw std::runtime_error("Failed to set device frequency.");
	}

	//Fetch the buffer size to use and do some sanity checks
	radio_transfer_size = airspyhf_get_output_size(radio);
	if (radio_transfer_size <= 0)
		throw std::runtime_error("Invalid buffer size returned from device.");
}

void fmice_radio::start() {
	//Sanity check
	if (radio == 0)
		throw std::runtime_error("Radio is not opened. Call open function.");

	//Start
	if (airspyhf_start(radio, airspyhf_rx_cb_static, this))
		throw std::runtime_error("Failed to start radio.");
}

int fmice_radio::airspyhf_rx_cb_static(airspyhf_transfer_t* transfer) {
	return ((fmice_radio*)transfer->ctx)->airspyhf_rx_cb(transfer);
}

int fmice_radio::airspyhf_rx_cb(airspyhf_transfer_t* transfer) {
	//Warn on dropped samples
	if (transfer->dropped_samples > 0)
		printf("WARN: Device dropped %i samples!\n", transfer->dropped_samples);

	//Push into buffer
	size_t dropped = transfer->sample_count - radio_buffer->write(transfer->samples, transfer->sample_count);
	if (dropped > 0)
		printf("WARN: Processing dropped %i samples!\n", dropped);

	return 0;
}

void process_deemphasis(float alpha, float* state, float* buffer, int count) {
	for (int i = 0; i < count; i++)
	{
		*state += alpha * (buffer[i] - *state);
		buffer[i] = *state;
	}
}

void fmice_radio::work() {
	//Read into buffer
	int count = radio_buffer->read((airspyhf_complex_float_t*)filter_bb_buffer, RADIO_BUFFER_SIZE);

	//Filter baseband
	count = filter_bb.process(count, filter_bb_buffer, filter_bb.out.writeBuf);

	//Demodulate FM
	count = fm_demod.process(count, filter_bb.out.writeBuf, fm_demod.out.writeBuf);

	//Filter composite
	count = filter_mpx.process(count, fm_demod.out.writeBuf, filter_mpx.out.writeBuf);

	//Send composite to icecast
	if (output_mpx != 0)
		output_mpx->push(filter_mpx.out.writeBuf, count);

	//Demodulate audio if there's an output for it
	if (output_audio != 0) {
		//Convert to complex
		rtoc.process(count, filter_mpx.out.writeBuf, rtoc.out.writeBuf);

		//Filter out pilot and run through PLL
		pilot_filter.process(count, rtoc.out.writeBuf, pilot_filter.out.writeBuf);
		pilot_pll.process(count, pilot_filter.out.writeBuf, pilot_pll.out.writeBuf);

		//Delay
		lpr_delay.process(count, filter_mpx.out.writeBuf, filter_mpx.out.writeBuf);
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
		dsp::math::Add<float>::process(count, filter_mpx.out.writeBuf, lmr, l);
		dsp::math::Subtract<float>::process(count, filter_mpx.out.writeBuf, lmr, r);

		//Filter both audio channels
		int countL = filter_audio_l.process(count, l, filter_audio_l.out.writeBuf);
		count = filter_audio_r.process(count, r, filter_audio_r.out.writeBuf);
		assert(countL == count);

		//Process deemphesis
		process_deemphasis(deemphasis_alpha, &deemphasis_state_l, filter_audio_l.out.writeBuf, count);
		process_deemphasis(deemphasis_alpha, &deemphasis_state_r, filter_audio_r.out.writeBuf, count);

		//Interleave the two audio channels
		dsp::convert::LRToStereo::process(count, filter_audio_l.out.writeBuf, filter_audio_r.out.writeBuf, interleaved_buffer);
		count *= 2;

		//Send to output
		output_audio->push((float*)interleaved_buffer, count);
	}
}