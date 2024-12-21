#pragma once

#include "cast.h"
#include "circular_buffer.h"
#include "stereo_demod.h"
#include "stereo_encode.h"
#include "rds/rds.h"

#include <libairspyhf/airspyhf.h>
#include <dsp/filter/fir.h>
#include <dsp/filter/decimating_fir.h>
#include <dsp/demod/quadrature.h>
#include <dsp/convert/real_to_complex.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/loop/pll.h>
#include <dsp/math/delay.h>

struct fmice_radio_settings_t {

	bool enable_status;

	double fm_deviation;
	double deemphasis_rate;

	double bb_filter_cutoff;
	double bb_filter_trans;

	double mpx_filter_cutoff;
	double mpx_filter_trans;

	double aud_filter_cutoff;
	double aud_filter_trans;

	bool rds_enable;
	float rds_max_skew;
	float rds_level;

	bool stereo_generator_enable;
	float stereo_generator_level;

};

class fmice_radio {

public:
	fmice_radio(fmice_radio_settings_t settings);
	~fmice_radio();

	/// <summary>
	/// Sets an output for MPX to stream.
	/// </summary>
	/// <param name="ice"></param>
	void set_mpx_output(fmice_icecast* ice);

	/// <summary>
	/// Sets an output for audio to stream.
	/// </summary>
	/// <param name="ice"></param>
	void set_audio_output(fmice_icecast* ice);

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

	dsp::tap<float> filter_bb_taps;
	dsp::filter::FIR<dsp::complex_t, float> filter_bb;
	dsp::complex_t* filter_bb_buffer;

	dsp::demod::Quadrature fm_demod;
	fmice_stereo_demod stereo_decoder;
	fmice_stereo_encode stereo_encoder;

	dsp::tap<float> filter_mpx_taps;
	dsp::filter::DecimatingFIR<float, float> filter_mpx;

	float* mpx_out_buffer;
	dsp::stereo_t* interleaved_buffer;

	fmice_icecast* output_mpx; // May be null
	fmice_icecast* output_audio; // May be null
	fmice_rds* rds; // May be null

	bool enable_status;
	int samples_since_last_status;
	bool enable_stereo_generator;

	static int airspyhf_rx_cb_static(airspyhf_transfer_t* transfer);
	int airspyhf_rx_cb(airspyhf_transfer_t* transfer);

	void print_status();

};