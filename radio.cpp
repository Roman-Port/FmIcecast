#include "defines.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cassert>

#include "radio.h"

#include <dsp/taps/low_pass.h>
#include <dsp/taps/band_pass.h>
#include <dsp/math/conjugate.h>
#include <dsp/math/multiply.h>
#include <dsp/math/add.h>
#include <dsp/math/subtract.h>
#include <dsp/convert/l_r_to_stereo.h>

#define RADIO_BUFFER_SIZE 65536

fmice_radio::fmice_radio(fmice_radio_settings_t settings) :
	stereo_decoder(RADIO_BUFFER_SIZE)
{
	//Zero out everything
	radio = 0;
	radio_transfer_size = 0;
	radio_buffer = 0;
	output_mpx = 0;
	output_audio = 0;

	//Allocate buffers
	size_t alignment = volk_get_alignment();
	filter_bb_buffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, alignment);
	interleaved_buffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, alignment);
	if (filter_bb_buffer == 0 || interleaved_buffer == 0)
		throw std::runtime_error("Failed to allocate buffers.");
	radio_buffer = new fmice_circular_buffer<airspyhf_complex_float_t>(RADIO_BUFFER_SIZE * 16);

	//Create baseband filter
	filter_bb_taps = dsp::taps::lowPass(settings.bb_filter_cutoff, settings.bb_filter_trans, SAMP_RATE);
	printf("Baseband filter taps: %i\n", filter_bb_taps.size);
	filter_bb.init(NULL, filter_bb_taps);
	filter_bb.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Configure FM demod
	fm_demod.init(NULL, settings.fm_deviation, SAMP_RATE);
	fm_demod.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Create composite filter
	filter_mpx_taps = dsp::taps::lowPass(settings.mpx_filter_cutoff, settings.mpx_filter_trans, SAMP_RATE);
	printf("MPX filter taps: %i\n", filter_mpx_taps.size);
	filter_mpx.init(NULL, filter_mpx_taps, DECIM_RATE);
	filter_mpx.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Configure stereo decoder
	stereo_decoder.init(MPX_SAMP_RATE, AUDIO_DECIM_RATE, settings.aud_filter_cutoff, settings.aud_filter_trans, settings.deemphasis_rate);
}

fmice_radio::~fmice_radio() {
	//TODO
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
		//Process stereo
		int audCount = stereo_decoder.process(filter_mpx.out.writeBuf, interleaved_buffer, count);

		//Send to output
		output_audio->push((float*)interleaved_buffer, audCount * 2);
	}
}