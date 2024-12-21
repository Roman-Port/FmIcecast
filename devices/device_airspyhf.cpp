#include "device_airspyhf.h"

#include <stdexcept>

fmice_device_airspyhf::fmice_device_airspyhf(int sampleRate) :
	radio(NULL),
	radio_buffer(new fmice_circular_buffer<airspyhf_complex_float_t>(sampleRate)),
	sample_rate(sampleRate),
	dropped_samples(0)
{
	//Init mutex
	if (pthread_mutex_init(&mutex, NULL) != 0)
		throw new std::runtime_error("Failed to initialize mutex.");
}

fmice_device_airspyhf::~fmice_device_airspyhf() {
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

void fmice_device_airspyhf::open(int freq) {
	//Open
	int result = airspyhf_open(&radio);
	if (result) {
		printf("Failed to open AirSpy HF device: %i.\n", result);
		throw std::runtime_error("Failed to open AirSpy HF Device.");
	}

	//Set sample rate
	result = airspyhf_set_samplerate(radio, sample_rate);
	if (result) {
		printf("Failed to set device sample rate to %i (%i)!\n", sample_rate, result);
		throw std::runtime_error("Failed to set device sample rate.");
	}

	//Set frequency
	result = airspyhf_set_freq(radio, freq);
	if (result) {
		printf("Failed to set device frequency to %i!\n", freq);
		throw std::runtime_error("Failed to set device frequency.");
	}
}

void fmice_device_airspyhf::start() {
	//Sanity check
	if (radio == 0)
		throw std::runtime_error("Radio is not opened. Call open function.");

	//Start
	if (airspyhf_start(radio, airspyhf_rx_cb_static, this))
		throw std::runtime_error("Failed to start radio.");
}

int fmice_device_airspyhf::get_dropped_samples() {
	//Enter mutex
	pthread_mutex_lock(&mutex);

	//Read
	int result = dropped_samples;

	//Release mutex
	pthread_mutex_unlock(&mutex);

	return result;
}

int fmice_device_airspyhf::airspyhf_rx_cb_static(airspyhf_transfer_t* transfer) {
	return ((fmice_device_airspyhf*)transfer->ctx)->airspyhf_rx_cb(transfer);
}

int fmice_device_airspyhf::airspyhf_rx_cb(airspyhf_transfer_t* transfer) {
	//Warn on dropped samples
	if (transfer->dropped_samples > 0)
		printf("WARN: Device dropped %i samples!\n", transfer->dropped_samples);

	//Push into buffer
	size_t dropped = transfer->sample_count - radio_buffer->write(transfer->samples, transfer->sample_count);
	if (dropped > 0)
		printf("WARN: Processing dropped %i samples!\n", dropped);

	//If samples were dropped from either device or processing, enter the mutex and add them to the stats
	if (dropped > 0 || transfer->dropped_samples > 0) {
		pthread_mutex_lock(&mutex);
		dropped_samples += transfer->dropped_samples + dropped;
		pthread_mutex_unlock(&mutex);
	}

	return 0;
}

int fmice_device_airspyhf::read(dsp::complex_t* samples, int count) {
	return radio_buffer->read((airspyhf_complex_float_t*)samples, count);
}