#pragma once

#include <stdint.h>
#include <dsp/types.h>
#include <shout/shout.h>

#include "circular_buffer.h"

/// <summary>
/// The base class for an audio codec (like FLAC)
/// </summary>
class fmice_codec {

public:
	fmice_codec(int sampleRate, int channels);
	~fmice_codec();

	/// <summary>
	/// Re-initializes the codec and clears out the output buffer.
	/// </summary>
	void reset();

	/// <summary>
	/// Writes samples to be encoded.
	/// </summary>
	/// <param name="samples">Samples to be encoded.</param>
	/// <param name="count">Number of samples PER CHANNEL.</param>
	/// <returns></returns>
	void write(float* samples, int count);

	/// <summary>
	/// Writes samples to be encoded.
	/// </summary>
	/// <param name="samples">Samples to be encoded.</param>
	/// <param name="count">Number of samples PER CHANNEL.</param>
	/// <returns></returns>
	void write(dsp::stereo_t* samples, int count);

	/// <summary>
	/// Reads ENCODED samples to be sent on wire. Waits until count are available.
	/// </summary>
	/// <param name="output">Output buffer for bytes.</param>
	/// <param name="count">Number of bytes to read.</param>
	/// <returns></returns>
	int read(uint8_t* output, int count);

	/// <summary>
	/// Checks if the error flag is set. Thread safe.
	/// </summary>
	/// <returns></returns>
	bool has_error();

	/// <summary>
	/// Sets up metadata for shoutcast, typically the content type.
	/// </summary>
	virtual void configure_shout(shout_t* ice) = 0;

protected:
	int sample_rate;
	int channels;

	/// <summary>
	/// Re-initializes the codec and clears out the output buffer. Thread safe.
	/// </summary>
	virtual void reset_safe();

	/// <summary>
	/// Internal write function that is called while the mutex is locked. Thread safe. Returns true on success, otherwise false.
	/// </summary>
	/// <param name="samples">Samples to be encoded.</param>
	/// <param name="count">Number of samples PER CHANNEL.</param>
	virtual bool write_safe(float* samples, int count) = 0;

	/// <summary>
	/// Pushes processed bytes to a circular buffer to be sent on wire.
	/// </summary>
	/// <param name="output"></param>
	/// <param name="count"></param>
	/// <returns></returns>
	void push_out(const uint8_t* output, int count);

private:
	pthread_mutex_t mutex;
	fmice_circular_buffer<uint8_t> output_buffer;
	bool error_flag;

};