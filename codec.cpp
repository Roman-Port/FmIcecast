#include "codec.h"

#include <stdexcept>

fmice_codec::fmice_codec(int sampleRate, int channels) :
	output_buffer(262144),
	error_flag(false)
{
	//Set
	this->sample_rate = sampleRate;
	this->channels = channels;

	//Init mutex
	if (pthread_mutex_init(&mutex, NULL) != 0)
		throw new std::runtime_error("Failed to initialize mutex.");
}

fmice_codec::~fmice_codec() {
	//Destroy mutex
	pthread_mutex_destroy(&mutex);
}

void fmice_codec::reset() {
	//Lock
	pthread_mutex_lock(&mutex);

	//Process
	reset_safe();

	//Unlock
	pthread_mutex_unlock(&mutex);
}

void fmice_codec::reset_safe() {
	//Reset ciruclar buffer
	output_buffer.reset();

	//Clear error flag
	error_flag = false;
}

void fmice_codec::write(float* samples, int count) {
	//Lock
	pthread_mutex_lock(&mutex);

	//Process
	bool success = write_safe((float*)samples, count);
	error_flag = error_flag || !success;

	//Unlock
	pthread_mutex_unlock(&mutex);
}

void fmice_codec::write(dsp::stereo_t* samples, int count) {
	write((float*)samples, count);
}

int fmice_codec::read(uint8_t* output, int count) {
	return output_buffer.read(output, count);
}

// This should only ever be called during a write so assume we are already in the mutex.
void fmice_codec::push_out(const uint8_t* samples, int count) {
	//Push to the buffer
	bool success = output_buffer.write(samples, count) == count;

	//If not successful, signal a fatal error
	error_flag = error_flag || !success;
}

bool fmice_codec::has_error() {
	//Lock
	pthread_mutex_lock(&mutex);

	//Read
	bool result = error_flag;

	//Unlock
	pthread_mutex_unlock(&mutex);

	return result;
}