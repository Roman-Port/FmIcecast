#pragma once

#include "circular_buffer.h"
#include <stdint.h>
#include <shout/shout.h>
#include <FLAC/stream_encoder.h>

#define CAST_BUFFER_SIZE 32768

class fmice_icecast {

public:
	fmice_icecast(const char* host, unsigned short port, const char* mount, const char* username, const char* password, int channels, int sampRate);
	~fmice_icecast();

	void push(float* samples, int count);

private:
	shout_t* shout;
	FLAC__StreamEncoder* flac;
	fmice_circular_buffer<int32_t>* circ_buffer;
	size_t samples_sent;
	size_t samples_dropped;
	pthread_t worker_thread;
	int channels;

	// In buffer - This is what cast_push_sample accesses and should only be used by its thread
	int32_t in_buffer[CAST_BUFFER_SIZE];
	int in_buffer_use;

	static FLAC__StreamEncoderWriteStatus flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data);
	FLAC__StreamEncoderWriteStatus flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame);

	static void* work_static(void* ctx);
	int work();

};