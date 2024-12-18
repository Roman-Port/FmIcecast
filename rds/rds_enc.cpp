#include "rds_enc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cassert>
#include <stdexcept>

// adapted version of https://github.com/ChristopheJacquet/Pydemod/blob/master/src/pydemod/filters/shaping.py
// which is itself a version of https://github.com/veeresht/CommPy/blob/master/commpy/filters.py
static float* rrcos_filter(int n, double alpha, double Ts, double Fs) {
	float* h_rrc = (float*)malloc(sizeof(float) * n);
	assert(h_rrc != 0);
	double T_delta = 1.0f / Fs;
	double t;
	for (int x = 0; x < n; x++) {
		t = ((double)x - (double)n / 2.0) * T_delta;
		if (t == 0.0)
			h_rrc[x] = 1.0 - alpha + (4 * alpha / M_PI);
		else if (alpha != 0 && t == Ts / (4 * alpha))
			h_rrc[x] = (alpha / sqrt(2.0)) * (((1.0 + 2.0 / M_PI) * (sin(M_PI / (4.0 * alpha)))) + ((1.0 - 2.0 / M_PI) * (cos(M_PI / (4.0 * alpha)))));
		else if (alpha != 0 && t == -Ts / (4 * alpha))
			h_rrc[x] = (alpha / sqrt(2.0)) * (((1.0 + 2.0 / M_PI) * (sin(M_PI / (4.0 * alpha)))) + ((1.0 - 2.0 / M_PI) * (cos(M_PI / (4.0 * alpha)))));
		else
			h_rrc[x] = (sin(M_PI * t * (1.0 - alpha) / Ts) + 4.0 * alpha * (t / Ts) * cos(M_PI * t * (1 + alpha) / Ts)) / (M_PI * t * (1 - (4.0 * alpha * t / Ts) * (4.0 * alpha * t / Ts)) / Ts);
	}
	return h_rrc;
}

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))

// copied from https://lloydrochester.com/post/c/convolution/ because I am not a math major
static float* convolve(float h[], float x[], int lenH, int lenX, int* lenY)
{
	int nconv = lenH + lenX - 1;
	(*lenY) = nconv;
	int i, j, h_start, x_start, x_end;

	float* y = (float*)calloc(nconv, sizeof(float));

	for (i = 0; i < nconv; i++)
	{
		x_start = MAX(0, i - lenH + 1);
		x_end = MIN(i + 1, lenX);
		h_start = MIN(i, lenH - 1);
		for (j = x_start; j < x_end; j++)
		{
			assert(h_start >= 0 && h_start < lenH);
			assert(j >= 0 && j < lenX);
			y[i] += h[h_start--] * x[j];
		}
	}
	return y;
}

// derived from https://github.com/Anthony96922/mpxgen/blob/pthread/src/generate_waveforms.py
static float* generate_waveform(int sample_rate, int* lenOut) {
	//Check if the sample rate is a multiple of the baud rate
	//Otherwise, we get clock drift. I'm sure there are ways around it but for now this is an easier fix
	if ((sample_rate * 2) % 2375 != 0)
		throw std::runtime_error("Sample rate must be a multiple of RDS baud rate.");

	int count = 7;
	int l = (int)(sample_rate / 1187.5) / 2;

	//Set up sample - Really inefficent...
	float* sample = (float*)calloc(sizeof(float), count * l);
	assert(sample != 0);
	sample[l] = 1;
	sample[2 * l] = -1;

	//Set up root raised cosine filter - same as pulse_shaping_filter line
	int cosFilterLen = l * 16;
	float* sf = rrcos_filter(cosFilterLen, 1, 1.0 / (2.0 * 1187.5), sample_rate);

	//Convolve
	int shapedSamplesLen = 0;
	float* shapedSamples = convolve(sample, sf, count * l, cosFilterLen, &shapedSamplesLen);

	//Derive offset from the length of the shaped samples. This is a modification made to the original code
	//I honestly don't know where it comes from. I'm just going to use the same fraction of the original offset (760) divided by the original length of the shaped samples (1839)
	int offset = (int)(shapedSamplesLen * 0.41326808) + 1;

	//Extract what is needed
	int start = offset - l * count;
	int end = offset + l * count;
	int len = end - start;
	assert(start > 0 && start < shapedSamplesLen);
	assert(end > 0 && end < shapedSamplesLen);

	//Copy + Scale as to not clip
	float* out = (float*)malloc(sizeof(float) * len);
	assert(out != 0);
	for (int i = 0; i < len; i++)
		out[i] = shapedSamples[start + i] / 2.5f;

	//Free buffers
	free(sample);
	free(sf);
	free(shapedSamples);

	*lenOut = len;
	return out;
}

fmice_rds_enc::fmice_rds_enc(int sampleRate) {
	//Generate the waveform
	filter_size = 0;
	waveform[0] = generate_waveform(sampleRate, &filter_size);

	//Calculate sizes
	samples_per_bit = filter_size / 7;
	bits_per_filter = filter_size / samples_per_bit;

	//Allocate and generate an inverted form of the generated waveform
	waveform[1] = (float*)malloc(sizeof(float) * filter_size);
	assert(waveform[1] != 0);
	for (int i = 0; i < filter_size; i++)
		waveform[1][i] = -waveform[0][i];

	//Calculate sizes of buffer
	working_buffer_bits = bits_per_filter + 1;
	working_buffer_size = working_buffer_bits * samples_per_bit;

	//Allocate working buffer and clear
	working_buffer = (float*)malloc(sizeof(float) * working_buffer_size);
	assert(working_buffer != 0);

	//Reset to re-initialize
	reset();
}

fmice_rds_enc::~fmice_rds_enc() {
	//Free buffers
	for (int i = 0; i < 2; i++)
		free(waveform[i]);
	free(working_buffer);
}

void fmice_rds_enc::reset() {
	//Clear working buffer
	for (int i = 0; i < working_buffer_size; i++)
		working_buffer[i] = 0;

	//Reset state
	current_bit = 0;
}

int fmice_rds_enc::get_samples_per_bit() {
	return samples_per_bit;
}

void fmice_rds_enc::push(uint8_t bit, float* output, int outputSize) {
	//Sanity check
	assert(outputSize == samples_per_bit);

	//Calculate the block to write within the working buffer
	int writeOffset = current_bit * samples_per_bit;
	for (int i = 0; i < filter_size; i++)
		working_buffer[(writeOffset + i) % working_buffer_size] += waveform[bit][i];

	//Determine block to read - It is the one right before the one we just wrote
	int readBit = (current_bit + (working_buffer_bits - 1)) % working_buffer_bits;
	int readOffset = readBit * samples_per_bit;

	//Copy this block to the output
	memcpy(output, &working_buffer[readOffset], sizeof(float) * samples_per_bit);

	//Zero it out for next cycle - We don't need to check for overflows because the working buffer is always a multiple of the block size
	for (int i = 0; i < samples_per_bit; i++)
		working_buffer[readOffset + i] = 0;

	//Update offset
	current_bit = (current_bit + 1) % working_buffer_bits;
}