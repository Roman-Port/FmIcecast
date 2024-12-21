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

fmice_radio::fmice_radio(fmice_device* device, fmice_radio_settings_t settings) :
	device(device),
	output_mpx(0),
	output_audio(0),
	rds(0),
	samples_since_last_status(0),
	stereo_decoder(RADIO_BUFFER_SIZE),
	stereo_encoder(RADIO_BUFFER_SIZE, powf(10, settings.stereo_generator_level / 20), MPX_SAMP_RATE, settings.aud_filter_cutoff, settings.aud_filter_trans),
	enable_status(settings.enable_status),
	enable_stereo_generator(settings.stereo_generator_enable)
{
	//Allocate buffers
	size_t alignment = volk_get_alignment();
	filter_bb_buffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, alignment);
	interleaved_buffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, alignment);
	mpx_out_buffer = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, alignment);
	if (filter_bb_buffer == 0 || interleaved_buffer == 0 || mpx_out_buffer == 0)
		throw std::runtime_error("Failed to allocate buffers.");

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
		rds = new fmice_rds(SAMP_RATE, MPX_SAMP_RATE, RADIO_BUFFER_SIZE, settings.rds_max_skew, powf(10, settings.rds_level / 20));
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
	sprintf(output, "rds=[sync=%s; overruns=%i; underruns=%i]; ", stats.has_sync ? "ok" : "none", stats.overruns, stats.underruns);
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
	printf("[STATUS] dropped_samples=%i %s%s%s\n",
		device->get_dropped_samples(),
		outputMpxStatus,
		outputAudStatus,
		rdsStatus
	);

	//Reset counter
	samples_since_last_status = 0;
}

void fmice_radio::work() {
	//Read into buffer
	int count = device->read(filter_bb_buffer, RADIO_BUFFER_SIZE);
	samples_since_last_status += count;

	//Filter baseband
	count = filter_bb.process(count, filter_bb_buffer, filter_bb.out.writeBuf);

	//Demodulate FM
	count = fm_demod.process(count, filter_bb.out.writeBuf, fm_demod.out.writeBuf);

	//Use composite to decode RDS -- Allows composite filter to be wider
	if (rds != 0)
		rds->push_in(fm_demod.out.writeBuf, count);

	//Filter composite
	count = filter_mpx.process(count, fm_demod.out.writeBuf, filter_mpx.out.writeBuf);

	//Copy into the output MPX buffer - This allows us to change it without clobering the original
	assert(count <= RADIO_BUFFER_SIZE);
	memcpy(mpx_out_buffer, filter_mpx.out.writeBuf, sizeof(float) * count);

	//Demodulate audio if there's an output for it or we're re-generating stereo
	if (output_audio != 0 || enable_stereo_generator) {
		//Process stereo
		int audCount = stereo_decoder.process(filter_mpx.out.writeBuf, interleaved_buffer, count);

		//Send to output
		if (output_audio != 0)
			output_audio->push(interleaved_buffer, audCount);
	}

	//Encode stereo (this wipes out the MPX)
	if (enable_stereo_generator) {
		stereo_encoder.process(mpx_out_buffer, stereo_decoder.lpr, stereo_decoder.lmr, count);
		volk_32f_s32f_multiply_32f(mpx_out_buffer, mpx_out_buffer, 0.5f, count);
	}

	//Process RDS reencoding
	if (rds != 0)
		rds->process(mpx_out_buffer, mpx_out_buffer, count, false);

	//Send composite to icecast
	if (output_mpx != 0)
		output_mpx->push(mpx_out_buffer, count);

	//Write status once every second
	if (enable_status && samples_since_last_status >= SAMP_RATE)
		print_status();
}