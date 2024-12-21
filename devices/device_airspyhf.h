#pragma once

#include "../device.h"
#include "../circular_buffer.h"

#include <libairspyhf/airspyhf.h>

class fmice_device_airspyhf : public fmice_device {

public:
	fmice_device_airspyhf(int sampleRate);
	~fmice_device_airspyhf();

	void open(int freq);

	virtual void start() override;

	virtual int get_dropped_samples() override;

	virtual int read(dsp::complex_t* samples, int count) override;

private:
	int sample_rate;
	airspyhf_device_t* radio;
	fmice_circular_buffer<airspyhf_complex_float_t>* radio_buffer;

	pthread_mutex_t mutex;
	uint64_t dropped_samples; // must only be accessed while in mutex

	static int airspyhf_rx_cb_static(airspyhf_transfer_t* transfer);
	int airspyhf_rx_cb(airspyhf_transfer_t* transfer);

};