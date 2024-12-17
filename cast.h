#pragma once

#include "circular_buffer.h"
#include <stdint.h>
#include <shout/shout.h>
#include <FLAC/stream_encoder.h>
#include <dsp/types.h>

class fmice_icecast {

public:
	fmice_icecast(int channels, int sampRate, int inputBufferSamples = 65536);
	~fmice_icecast();

	void set_host(const char* hostname);
	void set_port(unsigned int port);
	void set_mount(const char* mount);
	void set_username(const char* username);
	void set_password(const char* password);

	bool is_configured();
	void init();

	void push(dsp::stereo_t* samples, int count);
	void push(float* samples, int count);

private:
	int channels;
	int sample_rate;
	int input_buffer_samples; // Number of samples PER CHANNEL in the input buffer

	int32_t* input_buffer; // Length is input_buffer_samples * channels
	int input_buffer_use; // Number of samples in the buffer PER CHANNEL

	shout_t* shout;
	FLAC__StreamEncoder* flac;
	fmice_circular_buffer<uint8_t>* circ_buffer;
	size_t samples_sent;
	size_t samples_dropped;
	pthread_t worker_thread;
	

	char icecast_host[256];
	unsigned short icecast_port;
	char icecast_mount[256];
	char icecast_username[256];
	char icecast_password[256];

	

	static FLAC__StreamEncoderWriteStatus flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data);
	FLAC__StreamEncoderWriteStatus flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame);

	static void* work_static(void* ctx);
	void work();

	/// <summary>
	/// Submits samples in the input buffer for processing
	/// </summary>
	void submit_buffer();

};