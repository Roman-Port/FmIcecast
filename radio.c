#include "defines.h"
#include "libairspyhf/airspyhf.h"
#include "stdio.h"
#include "stdint.h"

#define FREQ 103300000
#define FIFO_SIZE 16384
#define FM_DEVIATION 85000

#define FM_GAIN (SAMP_RATE / (2 * M_PI * FM_DEVIATION))

static airspyhf_device_t* radio;
static int radio_transfer_size;

static lv_32fc_t* iq_buffer;
static int iq_buffer_index = 0;

static float* mpx_buffer;
static int mpx_buffer_index = 0;
static int mpx_buffer_decimation = 0;

static fifo_buffer_t iq_fifo;

static lv_32fc_t fm_last_sample = 0;

static void filter_mpx(float input) {
	//Write to both the real position as well as an offset value
	mpx_buffer[mpx_buffer_index] = input;
	mpx_buffer[mpx_buffer_index + NTAPS_COMPOSITE] = input;

	//Calculate 1/3 samples
	if (mpx_buffer_decimation == 0) {
		//Compute
		float output;
		volk_32f_x2_dot_prod_32f(&output, &mpx_buffer[mpx_buffer_index], taps_composite, NTAPS_COMPOSITE);

		//Push output
		cast_push_sample(output);
	}
	mpx_buffer_decimation = (mpx_buffer_decimation + 1) % DECIM_RATE;

	//Update
	mpx_buffer_index = (mpx_buffer_index + 1) % NTAPS_COMPOSITE;
}

/// <summary>
/// STEP 2: Demodulate IQ to FM MPX
/// </summary>
/// <param name="input"></param>
static void demod_fm(lv_32fc_t input) {
	//Apply conjugate
	lv_32fc_t temp = 0;
	volk_32fc_conjugate_32fc(&temp, &fm_last_sample, 1);
	temp *= input;

	//Estimate angle, apply gain, and pass it onto next step
	filter_mpx(fast_atan2f(cimagf(temp), crealf(temp)) * FM_GAIN);

	//Update state
	fm_last_sample = input;
}

/// <summary>
/// STEP 1: Filter baseband IQ
/// </summary>
/// <param name="bb_samples"></param>
/// <param name="count"></param>
static void filter_baseband(lv_32fc_t* bb_samples, int count) {
	lv_32fc_t result;
	for (int i = 0; i < count; i++) {
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

/// <summary>
/// Async callback from the AirSpy HF driver when samples are ready
/// </summary>
static int sample_cb(airspyhf_transfer_t* transfer) {
	//Log
	//printf("Got %i samples (dropped %i).\n", transfer->sample_count, transfer->dropped_samples);

	//Warn on dropped samples
	if (transfer->dropped_samples > 0)
		printf("WARN: Device dropped %i samples!\n", transfer->dropped_samples);

	//Filter baseband
	filter_baseband((lv_32fc_t*)transfer->samples, transfer->sample_count);

	return 0;
}

/// <summary>
/// Opens the AirSpy HF device into radio. Returns 0 on success.
/// </summary>
/// <returns></returns>
int radio_init() {
	//Initialize
	iq_buffer_index = 0;
	mpx_buffer_index = 0;
	mpx_buffer_decimation = 0;

	//Allocate buffers
	size_t alignment = volk_get_alignment();
	iq_buffer = (lv_32fc_t*)volk_malloc(NTAPS_DEMOD * 2 * sizeof(lv_32fc_t), alignment);
	mpx_buffer = (float*)volk_malloc(NTAPS_COMPOSITE * 2 * sizeof(float), alignment);
	if (iq_buffer == 0 || mpx_buffer == 0) {
		printf("Failed to allocate buffers.\n");
		return -1;
	}

	//Initialize FIFOs
	if (fifo_init(&iq_fifo, FIFO_SIZE*2)) {
		printf("Failed to allocate FIFOs.\n");
		return -1;
	}

	//Open
	printf("Opening AirSpy HF device...\n");
	int result = airspyhf_open(&radio);
	if (result) {
		printf("Failed to open AirSpy HF device: %i.\n", result);
		return -1;
	}

	//Set sample rate
	result = airspyhf_set_samplerate(radio, SAMP_RATE);
	if (result) {
		printf("Failed to set device sample rate to %i (%i)!\n", SAMP_RATE, result);
		return -1;
	}

	//Set frequency
	result = airspyhf_set_freq(radio, FREQ);
	if (result) {
		printf("Failed to set device frequency to %i!\n", FREQ);
		return -1;
	}

	//Fetch the buffer size to use and do some sanity checks
	radio_transfer_size = airspyhf_get_output_size(radio);
	if (radio_transfer_size <= 0) {
		printf("Invalid buffer size returned by driver: %i\n", radio_transfer_size);
		return -1;
	}

	return 0;
}

int radio_start() {
	if (airspyhf_start(radio, sample_cb, 0)) {
		printf("Failed to start radio.\n");
		return -1;
	}
	return 0;
}