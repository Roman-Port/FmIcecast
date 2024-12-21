#pragma once

#include "rds_enc.h"
#include "rds_dec.h"

#include <dsp/multirate/rational_resampler.h>
#include <dsp/taps/tap.h>
#include <dsp/filter/fir.h>
#include <pthread.h>

struct fmice_rds_stats {

	int underruns;
	int overruns;
	bool has_sync;

};

class fmice_rds {

public:
	fmice_rds(int inputSampleRate, int outputSampleRate, int bufferSize, float maxSkewSeconds, float scale);
	~fmice_rds();

	/// <summary>
	/// Reads RDS from the input.
	/// </summary>
	void push_in(const float* mpxIn, int count);

	/// <summary>
	/// Processes mpxIn into mpxOut, reencoding RDS.
	/// </summary>
	void process(const float* mpxIn, float* mpxOut, int count, bool filter);

	/// <summary>
	/// Reads stats and copies them into the struct being pointed to. Thread safe.
	/// </summary>
	void get_stats(fmice_rds_stats* output);

private:
	fmice_rds_dec dec;
	fmice_rds_enc enc;

	float scale;

	uint8_t* bit_buffer; // Buffer that holds decoded bits waiting for the encoder
	int bit_buffer_len;
	int bit_buffer_use;
	int bit_buffer_write;
	int bit_buffer_read;

	uint8_t* decoder_buffer; // Buffer of RDS bits after being decoded
	float* encoder_buffer; // Buffer of an RDS sample after being encoded
	int encoder_buffer_len;

	float* rds_buffer; // Buffer of RDS samples waiting to be written to output - These are all already shifted up to 57 kHz
	int rds_buffer_len;
	int rds_buffer_read;
	int rds_buffer_aval;

	bool is_overflow;
	bool is_underflow;

	double osc_phase;
	double osc_phase_inc;
	
	dsp::tap<float> mpx_filter_taps;
	dsp::filter::FIR<float, float> mpx_filter;
	dsp::multirate::RationalResampler<float> rds_resamp;

	pthread_mutex_t stat_lock; // the following stats are accessible cross-thread so must be protected by the mutex
	fmice_rds_stats stats;

	/// <summary>
	/// Pushes into the bit buffer. Handles overflows.
	/// </summary>
	void bit_buffer_push(const uint8_t* buffer, int count);

	/// <summary>
	/// Pops a sample off of the bit buffer. Even if it's empty, this will return a valid result.
	/// </summary>
	uint8_t bit_buffer_pop();

	/// <summary>
	/// Locks the mutex and updates statistics. Thread safe (when called on work thread).
	/// </summary>
	void update_stats(int addUnderrun, int addOverrun);

	/// <summary>
	/// Called when there is a bit buffer overrun (too many RDS samples decoded vs encoded).
	/// </summary>
	void handle_overrun();

	/// <summary>
	/// Called when there is a bit buffer underrun (too many RDS samples encoded vs decoded).
	/// </summary>
	void handle_underrun();

};