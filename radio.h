#pragma once

#include "cast.h"
#include "circular_buffer.h"
#include "libairspyhf/airspyhf.h"

class fmice_radio {

public:
	fmice_radio();
	~fmice_radio();

	/// <summary>
	/// Sets an output for MPX to stream.
	/// </summary>
	/// <param name="ice"></param>
	void set_mpx_output(fmice_icecast* ice);

	/// <summary>
	/// Opens the radio.
	/// </summary>
	void open(int freq);

	/// <summary>
	/// Starts radio RX.
	/// </summary>
	void start();

	/// <summary>
	/// Processes a block of smaples. Call this over and over.
	/// </summary>
	void work();

private:
	airspyhf_device_t* radio;
	int radio_transfer_size;

	fmice_circular_buffer<airspyhf_complex_float_t>* radio_buffer;

	lv_32fc_t* iq_buffer;
	int iq_buffer_index;

	float* mpx_buffer;
	int mpx_buffer_index;
	int mpx_buffer_decimation;

	lv_32fc_t fm_last_sample;

	fmice_icecast* output_mpx; // May be null

	static int airspyhf_rx_cb_static(airspyhf_transfer_t* transfer);
	int airspyhf_rx_cb(airspyhf_transfer_t* transfer);

	void demod_fm(lv_32fc_t input);
	void filter_mpx(float input);

};