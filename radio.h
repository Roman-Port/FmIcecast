#pragma once

#include "cast.h"
#include "circular_buffer.h"
#include "libairspyhf/airspyhf.h"
#include <dsp/filter/fir.h>
#include <dsp/filter/decimating_fir.h>
#include <dsp/demod/quadrature.h>
#include <dsp/convert/real_to_complex.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/loop/pll.h>
#include <dsp/math/delay.h>

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

	dsp::tap<float> filter_mpx_taps;
	dsp::filter::DecimatingFIR<float, float> filter_mpx;

	dsp::convert::RealToComplex rtoc;

	dsp::tap<dsp::complex_t> pilot_filter_taps;
	dsp::filter::FIR<dsp::complex_t, dsp::complex_t> pilot_filter;

	dsp::loop::PLL pilot_pll;
	dsp::math::Delay<float> lpr_delay;
	dsp::math::Delay<dsp::complex_t> lmr_delay;

	dsp::tap<float> filter_audio_taps;
	dsp::filter::DecimatingFIR<float, float> filter_audio_l;
	dsp::filter::DecimatingFIR<float, float> filter_audio_r;

	float deemphasis_alpha;
	float deemphasis_state_l;
	float deemphasis_state_r;

	float* lmr;
	float* l;
	float* r;
	dsp::stereo_t* interleaved_buffer;

	fmice_icecast* output_mpx; // May be null
	fmice_icecast* output_audio; // May be null

	static int airspyhf_rx_cb_static(airspyhf_transfer_t* transfer);
	int airspyhf_rx_cb(airspyhf_transfer_t* transfer);

};