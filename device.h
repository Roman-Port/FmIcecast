#pragma once

#include <dsp/types.h>

// Abstract class for a source IQ device.
class fmice_device {

public:
	virtual void start() = 0;

	virtual int get_dropped_samples() = 0;

	virtual int read(dsp::complex_t* samples, int count) = 0;

};