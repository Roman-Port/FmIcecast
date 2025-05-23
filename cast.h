#pragma once

#include "codec.h"
#include <stdint.h>
#include <shout/shout.h>
#include <dsp/types.h>
#include "circular_buffer.h"

#define FMICE_ICECAST_STATUS_INIT 0
#define FMICE_ICECAST_STATUS_CONNECTING 1
#define FMICE_ICECAST_STATUS_OK 2
#define FMICE_ICECAST_STATUS_CONNECTION_LOST 3

class fmice_icecast {

public:
	fmice_icecast(int channels, int sampRate, fmice_codec* codec);
	~fmice_icecast();

	void set_host(const char* hostname);
	void set_port(unsigned int port);
	void set_mount(const char* mount);
	void set_username(const char* username);
	void set_password(const char* password);

	int get_status();
	int get_retries();

	bool is_configured();
	void init();

	void push(dsp::stereo_t* samples, int count);

	/// <summary>
	/// Pushes data into the queue. Thread safe to be called from the radio thread.
	/// </summary>
	/// <param name="samples"></param>
	/// <param name="count"></param>
	void push(float* samples, int count);

private:
	int channels;
	int sample_rate;

	// Stats and settable settings - Protected by the mutex
	pthread_mutex_t mutex;
	char icecast_host[256];
	unsigned short icecast_port;
	char icecast_mount[256];
	char icecast_username[256];
	char icecast_password[256];
	int stat_status;
	int stat_retries;

	// Worker thread access ONLY
	shout_t* shout;
	float* working_buffer;

	fmice_codec* codec;
	fmice_circular_buffer<float> input_buffer;
	pthread_t worker_thread;

	static void* work_static(void* ctx);
	void work();

	/// <summary>
	/// Connects to Icecast. CALLED ONLY BY WORKER. Returns true on success, otherwise false.
	/// </summary>
	bool icecast_create();
	
	/// <summary>
	/// Disconnects from icecast. CALLED ONLY BY WORKER.
	/// </summary>
	void icecast_destroy();

	/// <summary>
	/// Callback on worker thread from the encoder, dummy static stub.
	/// </summary>
	/// <param name="samples"></param>
	/// <param name="count"></param>
	/// <param name="context"></param>
	/// <returns></returns>
	static void encoder_callback_static(const uint8_t* data, int count, void* context);

	/// <summary>
	/// Callback on worker thread from the encoder.
	/// </summary>
	/// <param name="samples"></param>
	/// <param name="count"></param>
	void encoder_callback(const uint8_t* data, int count);

	void set_status(int status);
	void inc_retries();

};