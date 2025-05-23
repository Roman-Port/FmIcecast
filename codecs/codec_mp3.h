#pragma once

#include "../codec.h"
#include <lame/lame.h>

class fmice_codec_mp3 : public fmice_codec {

public:
	fmice_codec_mp3(int sampleRate, int channels, int blockSize = 65536);
	~fmice_codec_mp3();

	void configure_shout(shout_t* ice) override;

protected:
	void reset_safe() override;
	bool write_safe(float* samples, int count) override;

private:
	lame_global_flags* gfp;
	unsigned char* output_buffer;
	int output_buffer_size;

	/// <summary>
	/// Creates encoder. Assumes it is null.
	/// </summary>
	void create_encoder();

};