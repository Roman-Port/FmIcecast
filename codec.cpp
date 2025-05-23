#include "codec.h"

#include <cassert>

fmice_codec::fmice_codec(int sampleRate, int channels)
{
	//Set
	this->sample_rate = sampleRate;
	this->channels = channels;
	this->callback = nullptr;
}

fmice_codec::~fmice_codec() {
	
}

void fmice_codec::set_callback(fmice_codec_callback callback, void* callbackClientData) {
	this->callback = callback;
	this->callback_ctx = callbackClientData;
}

void fmice_codec::push_out(const uint8_t* data, int count) {
	assert(callback != nullptr);
	callback(data, count, callback_ctx);
}

void fmice_codec::signal_error() {
	//Errors are indicated just by a negative count
	push_out(NULL, -1);
}