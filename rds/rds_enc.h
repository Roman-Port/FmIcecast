#pragma once

#include <stdint.h>

class fmice_rds_enc {

public:
	fmice_rds_enc(int sampleRate);
	~fmice_rds_enc();

	/// <summary>
	/// Resets the state of the encoder.
	/// </summary>
	void reset();

	/// <summary>
	/// Gets the number of bits output by each sample. This will never change once this class is created.
	/// </summary>
	/// <returns></returns>
	int get_samples_per_bit();

	/// <summary>
	/// Processes a bit and outputs the resulting waveform. Output size MUST equal the constant returned by get_samples_per_bit. OutputSize is only checked for sanity.
	/// </summary>
	/// <param name="bit">The bit to push.</param>
	/// <param name="output">The output to set to the waveform.</param>
	/// <param name="outputSize">The length of the output array. Just checked for sanity.</param>
	void push(uint8_t bit, float* output, int outputSize);

private:
	int filter_size;
	int samples_per_bit;
	int bits_per_filter;
	float* waveform[2];

	float* working_buffer;
	int working_buffer_bits; // bits_per_filter + 1
	int working_buffer_size; // working_buffer_bits * samples_per_bit

	int current_bit; // Current bit about to be written in the working buffer

};