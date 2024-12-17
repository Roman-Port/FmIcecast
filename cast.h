#pragma once

#include "codec.h"
#include <stdint.h>
#include <shout/shout.h>
#include <dsp/types.h>

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

	fmice_codec* codec;
	pthread_t worker_thread;

	static void* work_static(void* ctx);
	void work();

	void set_status(int status);
	void inc_retries();

};