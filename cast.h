#pragma once

#include "circular_buffer.h"
#include <stdint.h>
#include <shout/shout.h>
#include <FLAC/stream_encoder.h>

#define CAST_BUFFER_SIZE 32768

class fmice_icecast {

public:
	fmice_icecast(int channels, int sampRate);
	~fmice_icecast();

	void set_host(const char* hostname);
	void set_port(unsigned int port);
	void set_mount(const char* mount);
	void set_username(const char* username);
	void set_password(const char* password);

	bool is_configured();
	void init();

	void push(float* samples, int count);

private:
	shout_t* shout;
	FLAC__StreamEncoder* flac;
	fmice_circular_buffer<int32_t>* circ_buffer;
	size_t samples_sent;
	size_t samples_dropped;
	pthread_t worker_thread;
	int channels;
	int sample_rate;

	char icecast_host[256];
	unsigned short icecast_port;
	char icecast_mount[256];
	char icecast_username[256];
	char icecast_password[256];

	// In buffer - This is what cast_push_sample accesses and should only be used by its thread
	int32_t in_buffer[CAST_BUFFER_SIZE];
	int in_buffer_use;

	static FLAC__StreamEncoderWriteStatus flac_push_cb_static(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void* client_data);
	FLAC__StreamEncoderWriteStatus flac_push_cb(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame);

	static void* work_static(void* ctx);
	int work();

};