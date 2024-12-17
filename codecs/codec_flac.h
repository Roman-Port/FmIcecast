#pragma once

#include "../codec.h"
#include <FLAC/stream_encoder.h>

class fmice_codec_flac : public fmice_codec {
	
public:
	fmice_codec_flac(int sampleRate, int channels, int blockSize = 65536);
	~fmice_codec_flac();

protected:
	void reset_safe() override;
	bool write_safe(float* samples, int count) override;

private:
	FLAC__StreamEncoder* flac;

	int input_buffer_samples; // Number of samples PER CHANNEL in the input buffer
	int32_t* input_buffer; // Length is input_buffer_samples * channels
	int input_buffer_use; // Number of samples in the buffer PER CHANNEL

	/// <summary>
	/// Creates FLAC encoder. Assumes it is null.
	/// </summary>
	void create_flac();

	/// <summary>
	/// Submits a finished input buffer and returns if it was successful or not.
	/// </summary>
	/// <returns></returns>
	bool submit_buffer();

	static FLAC__StreamEncoderWriteStatus flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data);
	FLAC__StreamEncoderWriteStatus flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame);

};