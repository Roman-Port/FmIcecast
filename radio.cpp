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
#include <math.h>

#define RADIO_BUFFER_SIZE 65536

fmice_radio::fmice_radio(fmice_radio_settings_t settings) :
	radio(NULL),
	radio_transfer_size(0),
	radio_buffer(0),
	output_mpx(0),
	output_audio(0),
	rds(0),
	samples_since_last_status(0),
	stereo_decoder(RADIO_BUFFER_SIZE),
	enable_status(settings.enable_status)
{
	//Allocate buffers
	size_t alignment = volk_get_alignment();
	filter_bb_buffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, alignment);
	interleaved_buffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, alignment);
	mpx_out_buffer = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, alignment);
	if (filter_bb_buffer == 0 || interleaved_buffer == 0 || mpx_out_buffer == 0)
		throw std::runtime_error("Failed to allocate buffers.");
	radio_buffer = new fmice_circular_buffer<airspyhf_complex_float_t>(SAMP_RATE);

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

	//Set up RDS if enabled (convert level from dB too)
	if (settings.rds_enable)
		rds = new fmice_rds(MPX_SAMP_RATE, RADIO_BUFFER_SIZE, settings.rds_max_skew, powf(10, settings.rds_level / 20));
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

static const char* ICECAST_STATUS_NAMES[4] = {
	"init",
	"connecting",
	"ok",
	"lost"
};

static void print_output_status(char* output, const char* name, fmice_icecast* cast) {
	//If icecast is NULL, don't print anything
	if (cast == NULL) {
		output[0] = 0;
		return;
	}

	//Format
	sprintf(output, "%s=[status=%s; retries=%i]; ", name, ICECAST_STATUS_NAMES[cast->get_status()], cast->get_retries());
}

static void print_rds_status(char* output, fmice_rds* rds) {
	//If not enabled, don't print anything
	if (rds == NULL) {
		output[0] = 0;
		return;
	}

	//Fetch stats
	fmice_rds_stats stats;
	rds->get_stats(&stats);

	//Format
	printf("rds=[sync=%s; overruns=%i; underruns=%i]; ", stats.has_sync ? "ok" : "none", stats.overruns, stats.underruns);
}

void fmice_radio::print_status() {
	//Format status
	char outputMpxStatus[256];
	print_output_status(outputMpxStatus, "mpx_icecast", output_mpx);
	char outputAudStatus[256];
	print_output_status(outputAudStatus, "aud_icecast", output_audio);
	char rdsStatus[256];
	print_rds_status(rdsStatus, rds);

	//Write status
	printf("[STATUS] radio_fifo_use=%i%% %s%s%s\n",
		(radio_buffer->get_use() * 100) / radio_buffer->get_size(),
		outputMpxStatus,
		outputAudStatus,
		rdsStatus
	);

	//Reset counter
	samples_since_last_status = 0;
}

void fmice_radio::work() {
	//Read into buffer
	int count = radio_buffer->read((airspyhf_complex_float_t*)filter_bb_buffer, RADIO_BUFFER_SIZE);
	samples_since_last_status += count;

	//Filter baseband
	count = filter_bb.process(count, filter_bb_buffer, filter_bb.out.writeBuf);

	//Demodulate FM
	count = fm_demod.process(count, filter_bb.out.writeBuf, fm_demod.out.writeBuf);

	//Filter composite
	count = filter_mpx.process(count, fm_demod.out.writeBuf, filter_mpx.out.writeBuf);

	//Copy into the output MPX buffer - This allows us to change it without clobering the original
	assert(count <= RADIO_BUFFER_SIZE);
	memcpy(mpx_out_buffer, filter_mpx.out.writeBuf, sizeof(float) * count);

	//Process RDS reencoding
	if (rds != 0)
		rds->process(filter_mpx.out.writeBuf, mpx_out_buffer, count);

	//Send composite to icecast
	if (output_mpx != 0)
		output_mpx->push(mpx_out_buffer, count);

	//Demodulate audio if there's an output for it
	if (output_audio != 0) {
		//Process stereo
		int audCount = stereo_decoder.process(filter_mpx.out.writeBuf, interleaved_buffer, count);

		//Send to output
		output_audio->push(interleaved_buffer, audCount);
	}

	//Write status once every second
	if (enable_status && samples_since_last_status >= SAMP_RATE)
		print_status();
}