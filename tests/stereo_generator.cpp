#include "stdio.h"

#include <volk/volk.h>
#include <dsp/demod/quadrature.h>
#include <dsp/filter/fir.h>
#include <dsp/taps/low_pass.h>
#include "../stereo_demod.h"
#include "../stereo_encode.h"

#define RADIO_BUFFER_SIZE 100//(65536/16)
#define INPUT_SAMP_RATE 768000
#define DECIM_RATE 2
#define SAMP_RATE (INPUT_SAMP_RATE/DECIM_RATE)

unsigned char wavHeader[44] = {
	0x52, 0x49, 0x46, 0x46, 0x08, 0x60, 0x2D, 0x09, 0x57, 0x41, 0x56, 0x45,
	0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
	0x40, 0xF5, 0x06, 0x00, 0x00, 0xAA, 0x37, 0x00, 0x04, 0x00, 0x10, 0x00,
	0x64, 0x61, 0x74, 0x61, 0x00, 0x60, 0x2D, 0x09
};

int main() {
	//Open input and output files
	//FILE* fileInput = fopen("/media/sf_VMShare/kqlx stereo test.bin", "rb");
	FILE* fileInput = fopen("/media/sf_VMShare/stereo test in.bin", "rb");
	FILE* fileOutput = fopen("/media/sf_VMShare/stereo remod.wav", "wb");
	FILE* fileOutputRaw = fopen("/media/sf_VMShare/stereo remod raw.wav", "wb");
	FILE* fileOutputConf = fopen("/media/sf_VMShare/stereo remod conf.wav", "wb");

	//Set up and write WAV buffer
	*((int*)&wavHeader[24]) = SAMP_RATE;
	*((short*)&wavHeader[22]) = 1;
	fwrite(wavHeader, 1, sizeof(wavHeader), fileOutput);

	//Set up and write WAV buffer for confidence
	*((int*)&wavHeader[24]) = SAMP_RATE / 4;
	*((short*)&wavHeader[22]) = 4;
	fwrite(wavHeader, 1, sizeof(wavHeader), fileOutputConf);

	//Set up and write WAV buffer for raw
	*((int*)&wavHeader[24]) = SAMP_RATE;
	*((short*)&wavHeader[22]) = 4;
	fwrite(wavHeader, 1, sizeof(wavHeader), fileOutputRaw);

	//Allocate buffers
	dsp::complex_t* inputBuffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, volk_get_alignment());
	float* mpxBuffer = (float*)volk_malloc(sizeof(float) * RADIO_BUFFER_SIZE, volk_get_alignment());
	dsp::stereo_t* audBuffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, volk_get_alignment());
	dsp::stereo_t* confBuffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, volk_get_alignment());
	int16_t* outputConvertedBuffer = (int16_t*)volk_malloc(sizeof(int16_t) * RADIO_BUFFER_SIZE * 4, volk_get_alignment());

	//Set up baseband filter
	dsp::tap<float> filter_bb_taps = dsp::taps::lowPass(125000, 15000, INPUT_SAMP_RATE);
	dsp::filter::DecimatingFIR<dsp::complex_t, float> filter_bb;
	filter_bb.init(NULL, filter_bb_taps, DECIM_RATE);
	filter_bb.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Init FM
	dsp::demod::Quadrature demod;
	demod.init(NULL, 85000, SAMP_RATE);
	demod.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Init demod
	fmice_stereo_demod decoder(RADIO_BUFFER_SIZE);
	decoder.init(SAMP_RATE, 4, 17000, 2000, 75);

	//Init delay for the first demod
	dsp::math::Delay<dsp::stereo_t> audioDelay;
	audioDelay.init(NULL, decoder.delay_samples);
	audioDelay.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Init modulator
	fmice_stereo_encode encoder(RADIO_BUFFER_SIZE, 0.05f, SAMP_RATE, 17000, 2000);

	//Init confidence demod
	fmice_stereo_demod decoder2(RADIO_BUFFER_SIZE);
	decoder2.init(SAMP_RATE, 4, 17000, 2000, 75);
	
	//Init audio filters
	dsp::tap<float> audio_filter_taps = dsp::taps::lowPass(17000, 2000, SAMP_RATE);
	printf("Stereo Generator Audio taps: %i\n", audio_filter_taps.size);
	dsp::filter::FIR<float, float>audio_filter_lpr;
	audio_filter_lpr.init(NULL, audio_filter_taps);
	audio_filter_lpr.out.setBufferSize(RADIO_BUFFER_SIZE);
	dsp::filter::FIR<float, float>audio_filter_lmr;
	audio_filter_lmr.init(NULL, audio_filter_taps);
	audio_filter_lmr.out.setBufferSize(RADIO_BUFFER_SIZE);
	dsp::filter::FIR<float, float>audio_filter_lpr2;
	audio_filter_lpr2.init(NULL, audio_filter_taps);
	audio_filter_lpr2.out.setBufferSize(RADIO_BUFFER_SIZE);
	dsp::filter::FIR<float, float>audio_filter_lmr2;
	audio_filter_lmr2.init(NULL, audio_filter_taps);
	audio_filter_lmr2.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Loop
	int count;
	int totalSamples = 0;
	while (1) {
		//Read
		count = fread(inputBuffer, sizeof(dsp::complex_t), RADIO_BUFFER_SIZE, fileInput);
		if (count == 0)
			break;

		//Filter baseband
		count = filter_bb.process(count, inputBuffer, filter_bb.out.writeBuf);

		//Demodulate FM
		count = demod.process(count, filter_bb.out.writeBuf, demod.out.writeBuf);

		//Process stereo demod
		decoder.process(demod.out.writeBuf, audBuffer, count);

		//Process stereo mod
		encoder.process(mpxBuffer, decoder.lpr, decoder.lmr, count);

		//Process confidence stereo demod
		int audioCount = decoder2.process(mpxBuffer, confBuffer, count);

		//Convert and write MPX
		for (int i = 0; i < count; i++) {
			outputConvertedBuffer[i] = mpxBuffer[i] * 32767.0f;
		}
		fwrite(outputConvertedBuffer, sizeof(int16_t), count, fileOutput);

		//Skip the first few samples
		if (totalSamples >= RADIO_BUFFER_SIZE) {
			//Delay audio buffer to catch it up with confidence
			//audioDelay.process(count, audBuffer, audioDelay.out.writeBuf);

			//Convert and write audio
			for (int i = 0; i < audioCount; i++) {
				outputConvertedBuffer[(i * 4) + 0] = audBuffer[i].l * 32767.0f;
				outputConvertedBuffer[(i * 4) + 1] = audBuffer[i].r * 32767.0f;
				outputConvertedBuffer[(i * 4) + 2] = confBuffer[i].l * 32767.0f;
				outputConvertedBuffer[(i * 4) + 3] = confBuffer[i].r * 32767.0f;
			}
			fwrite(outputConvertedBuffer, sizeof(int16_t), audioCount * 4, fileOutputConf);

			//Filter L-R for debugging
			//memcpy(audio_filter_lpr.out.writeBuf, decoder.lpr, sizeof(float) * count);
			//memcpy(audio_filter_lmr.out.writeBuf, decoder.lmr, sizeof(float) * count);
			//memcpy(audio_filter_lpr2.out.writeBuf, decoder2.lpr, sizeof(float) * count);
			//memcpy(audio_filter_lmr2.out.writeBuf, decoder2.lmr, sizeof(float) * count);
			audio_filter_lpr.process(count, decoder.lpr, audio_filter_lpr.out.writeBuf);
			audio_filter_lmr.process(count, decoder.lmr, audio_filter_lmr.out.writeBuf);
			audio_filter_lpr2.process(count, decoder2.lpr, audio_filter_lpr2.out.writeBuf);
			audio_filter_lmr2.process(count, decoder2.lmr, audio_filter_lmr2.out.writeBuf);

			//Convert and write raw
			for (int i = 0; i < count; i++) {
				outputConvertedBuffer[(i * 4) + 0] = audio_filter_lpr.out.writeBuf[i] * 32767.0f;
				outputConvertedBuffer[(i * 4) + 1] = audio_filter_lmr.out.writeBuf[i] * 32767.0f;
				outputConvertedBuffer[(i * 4) + 2] = audio_filter_lpr2.out.writeBuf[i] * 32767.0f;
				outputConvertedBuffer[(i * 4) + 3] = audio_filter_lmr2.out.writeBuf[i] * 32767.0f;
			}
			fwrite(outputConvertedBuffer, sizeof(int16_t), count * 4, fileOutputRaw);
		}
		totalSamples += count;
	}

	printf("Done.\n");
	return 0;
}