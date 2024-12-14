#include "defines.h"

#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "radio.h"

#define FREQ 103300000
#define FM_DEVIATION 85000

#define FM_GAIN (SAMP_RATE / (2 * M_PI * FM_DEVIATION))

fmice_radio::fmice_radio() {
	//Zero out everything
	radio = 0;
	radio_transfer_size = 0;
	radio_buffer = 0;
	iq_buffer = 0;
	iq_buffer_index = 0;
	mpx_buffer = 0;
	mpx_buffer_index = 0;
	mpx_buffer_decimation = 0;
	fm_last_sample = 0;
	output_mpx = 0;

	//Allocate buffers
	size_t alignment = volk_get_alignment();
	size_t iq_buffer_len = NTAPS_DEMOD * 2 * sizeof(lv_32fc_t);
	iq_buffer = (lv_32fc_t*)volk_malloc(iq_buffer_len, alignment);
	size_t mpx_buffer_len = NTAPS_COMPOSITE * 2 * sizeof(float);
	mpx_buffer = (float*)volk_malloc(mpx_buffer_len, alignment);
	if (iq_buffer == 0 || mpx_buffer == 0)
		throw std::runtime_error("Failed to allocate buffers.");
	radio_buffer = new fmice_circular_buffer<airspyhf_complex_float_t>(NTAPS_DEMOD * 2 * 32);

	//Clear out buffers
	memset(iq_buffer, 0, iq_buffer_len);
	memset(mpx_buffer, 0, mpx_buffer_len);
}

fmice_radio::~fmice_radio() {

}

void fmice_radio::set_mpx_output(fmice_icecast* ice) {
	output_mpx = ice;
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

void fmice_radio::filter_mpx(float input) {
	//Write to both the real position as well as an offset value
	mpx_buffer[mpx_buffer_index] = input;
	mpx_buffer[mpx_buffer_index + NTAPS_COMPOSITE] = input;

	//Calculate 1/3 samples
	if (mpx_buffer_decimation == 0) {
		//Compute
		float output;
		volk_32f_x2_dot_prod_32f(&output, &mpx_buffer[mpx_buffer_index], taps_composite, NTAPS_COMPOSITE);

		//Send composite to icecast
		if (output_mpx != 0)
			output_mpx->push(output);
	}
	mpx_buffer_decimation = (mpx_buffer_decimation + 1) % DECIM_RATE;

	//Update
	mpx_buffer_index = (mpx_buffer_index + 1) % NTAPS_COMPOSITE;
}

/// <summary>
/// STEP 2: Demodulate IQ to FM MPX
/// </summary>
/// <param name="input"></param>
void fmice_radio::demod_fm(lv_32fc_t input) {
	//Apply conjugate
	lv_32fc_t temp = 0;
	volk_32fc_conjugate_32fc(&temp, &fm_last_sample, 1);
	temp *= input;

	//Estimate angle, apply gain, and pass it onto next step
	filter_mpx(fast_atan2f(temp.imag(), temp.real()) * FM_GAIN);

	//Update state
	fm_last_sample = input;
}

void fmice_radio::work() {
	//Read into buffer
	lv_32fc_t bb_samples[NTAPS_DEMOD];
	radio_buffer->read((airspyhf_complex_float_t*)bb_samples, NTAPS_DEMOD);

	//Filter baseband
	lv_32fc_t result;
	for (int i = 0; i < NTAPS_DEMOD; i++) {
		//Write to both the real position as well as an offset value
		iq_buffer[iq_buffer_index] = bb_samples[i];
		iq_buffer[iq_buffer_index + NTAPS_DEMOD] = bb_samples[i];

		//Calculate
		volk_32fc_32f_dot_prod_32fc(&result, &iq_buffer[iq_buffer_index], taps_demod, NTAPS_DEMOD);

		//Update
		iq_buffer_index = (iq_buffer_index + 1) % NTAPS_DEMOD;

		//Push filtered sample
		demod_fm(result);
	}
}