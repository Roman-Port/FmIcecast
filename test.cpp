#include "stdio.h"

#include <volk/volk.h>
#include <dsp/demod/quadrature.h>
#include <dsp/filter/fir.h>
#include <dsp/taps/low_pass.h>
#include "stereo_demod.h"

#define RADIO_BUFFER_SIZE 65536
#define INPUT_SAMP_RATE 912000
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
	FILE* fileInput = fopen("/media/sf_VMShare/kqlx stereo test.bin", "rb");
	FILE* fileOutput = fopen("/media/sf_VMShare/stereo demod.wav", "wb");

	//Set up and write WAV buffer
	*((int*)&wavHeader[24]) = SAMP_RATE / 4;
	fwrite(wavHeader, 1, sizeof(wavHeader), fileOutput);

	//Allocate buffers
	dsp::complex_t* inputBuffer = (dsp::complex_t*)volk_malloc(sizeof(dsp::complex_t) * RADIO_BUFFER_SIZE, volk_get_alignment());
	dsp::stereo_t* outputBuffer = (dsp::stereo_t*)volk_malloc(sizeof(dsp::stereo_t) * RADIO_BUFFER_SIZE, volk_get_alignment());
	int16_t* outputConvertedBuffer = (int16_t*)volk_malloc(sizeof(int16_t) * RADIO_BUFFER_SIZE * 2, volk_get_alignment());

	//Set up baseband filter
	dsp::tap<float> filter_bb_taps = dsp::taps::lowPass(125000, 15000, INPUT_SAMP_RATE);
	dsp::filter::DecimatingFIR<dsp::complex_t, float> filter_bb;
	filter_bb.init(NULL, filter_bb_taps, DECIM_RATE);
	filter_bb.out.setBufferSize(RADIO_BUFFER_SIZE);

	//Init FM
	dsp::demod::Quadrature demod;
	demod.init(NULL, 85000, SAMP_RATE);
	demod.out.setBufferSize(RADIO_BUFFER_SIZE);
	fmice_stereo_demod stereo(RADIO_BUFFER_SIZE);
	stereo.init(SAMP_RATE, 4, 17000, 2000, 75);

	//Loop
	int count;
	while (1) {
		//Read
		count = fread(inputBuffer, sizeof(dsp::complex_t), RADIO_BUFFER_SIZE, fileInput);
		if (count == 0)
			break;

		//Filter baseband
		count = filter_bb.process(count, inputBuffer, filter_bb.out.writeBuf);

		//Demodulate FM
		count = demod.process(count, filter_bb.out.writeBuf, demod.out.writeBuf);

		//Process stereo
		count = stereo.process(demod.out.writeBuf, outputBuffer, count);

		//Convert
		for (int i = 0; i < count; i++) {
			outputConvertedBuffer[(i * 2) + 0] = outputBuffer[i].l * 32767.0f;
			outputConvertedBuffer[(i * 2) + 1] = outputBuffer[i].r * 32767.0f;
		}

		//Write to output
		fwrite(outputConvertedBuffer, sizeof(int16_t), count * 2, fileOutput);
	}

	printf("Done.\n");
	return 0;
}