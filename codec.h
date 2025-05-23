#pragma once

#include <stdint.h>
#include <dsp/types.h>
#include <shout/shout.h>

/// <summary>
/// Callback function for the encoder. Count specifies the number of bytes, which may be 0. If LESS THAN ZERO, identifies an error.
/// </summary>
typedef void(*fmice_codec_callback)(const uint8_t* data, int count, void* callbackClientData);

/// <summary>
/// The base class for an audio codec (like FLAC)
/// </summary>
class fmice_codec {

public:
	fmice_codec(int sampleRate, int channels);
	~fmice_codec();

	/// <summary>
	/// Re-initializes the codec and clears out the output buffer. Called from icecast thread.
	/// </summary>
	virtual void reset() = 0;

	/// <summary>
	/// Processes incoming data and calls back on the same thread. Called from icecast thread.
	/// </summary>
	/// <param name="samples">PCM samples in -1 to 1 format to process. OK to modify.</param>
	/// <param name="count">Number of input samples total.</param>
	/// <param name="callback">Callback to fire on emitted samples. MUST be on the calling thread.</param>
	/// <param name="callbackClientData">User-supplied data returned on the callback.</param>
	virtual void process(float* samples, int count) = 0;

	/// <summary>
	/// Sets up metadata for shoutcast, typically the content type.
	/// </summary>
	virtual void configure_shout(shout_t* ice) = 0;

	/// <summary>
	/// Sets up the callback for process. Callback will be called from the same thread calling process (or possibly resetting the codec).
	/// </summary>
	/// <param name="callback"></param>
	/// <param name="callbackClientData"></param>
	void set_callback(fmice_codec_callback callback, void* callbackClientData);

protected:
	int sample_rate;
	int channels;

	/// <summary>
	/// Pushes data out the callback.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="count"></param>
	void push_out(const uint8_t* data, int count);

	/// <summary>
	/// Signals to callback that there was an error.
	/// </summary>
	void signal_error();

private:
	fmice_codec_callback callback;
	void* callback_ctx;

};