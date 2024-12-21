#include "rds.h"

#include <stdio.h>
#include <cassert>
#include <dsp/taps/low_pass.h>

#define RDS_SAMPLE_RATE 190000 // Must be a multiple of the baud rate and greater than the mpx sample rate

fmice_rds::fmice_rds(int inputSampleRate, int outputSampleRate, int bufferSize, float maxSkewSeconds, float scale) :
	dec(bufferSize),
	enc(RDS_SAMPLE_RATE),
	scale(scale),
	bit_buffer(0),
	bit_buffer_len(0),
	bit_buffer_use(0),
	bit_buffer_write(0),
	bit_buffer_read(0),
	decoder_buffer(0),
	encoder_buffer(0),
	encoder_buffer_len(0),
	rds_buffer(0),
	rds_buffer_len(0),
	rds_buffer_read(0),
	rds_buffer_aval(0),
	is_underflow(true),
	is_overflow(false),
	osc_phase(0),
	osc_phase_inc(0)
{
	//Configure decoder
	dec.configure(inputSampleRate);

	//Calculate bit buffer size from the skew and baud rate
	bit_buffer_len = (int)(1187.5 * maxSkewSeconds);

	//Allocate bit buffer
	bit_buffer = (uint8_t*)calloc(sizeof(uint8_t), bit_buffer_len);
	assert(bit_buffer != 0);

	//Allocate output buffer for decoder
	decoder_buffer = (uint8_t*)malloc(sizeof(uint8_t) * bufferSize);
	assert(decoder_buffer != 0);
	
	//Allocate output buffer for encoder
	encoder_buffer_len = enc.get_samples_per_bit();
	encoder_buffer = (float*)malloc(sizeof(float) * encoder_buffer_len);
	assert(encoder_buffer != 0);

	//Init MPX filter...this is a very tight filter so prepare for a lot of taps!
	mpx_filter_taps = dsp::taps::lowPass(38000 + 17200, 500, outputSampleRate);
	printf("rds re-encode mpx taps: %i\n", mpx_filter_taps.size);
	mpx_filter.init(NULL, mpx_filter_taps);

	//Init resampler that takes it from the RDS rate to the output rate
	assert(RDS_SAMPLE_RATE >= outputSampleRate);
	rds_resamp.init(NULL, RDS_SAMPLE_RATE, outputSampleRate);
	rds_resamp.out.setBufferSize(bufferSize);

	//Allocate RDS buffer
	rds_buffer_len = encoder_buffer_len;
	rds_buffer = (float*)malloc(sizeof(float) * rds_buffer_len);
	assert(rds_buffer != 0);

	//Calculate parameters for 57 kHz oscilator
	osc_phase = 0;
	osc_phase_inc = 2 * M_PI * 57000 / outputSampleRate;

	//Init mutex for stats
	bool mutexOk = pthread_mutex_init(&stat_lock, NULL) == 0;
	assert(mutexOk);

	//Initialize stats
	stats.overruns = 0;
	stats.underruns = 0;
	stats.has_sync = false;
}

fmice_rds::~fmice_rds() {
	//Free buffers
	free(bit_buffer);
	free(decoder_buffer);
	free(encoder_buffer);
	free(rds_buffer);

	//Free mutex
	pthread_mutex_destroy(&stat_lock);
}

void fmice_rds::get_stats(fmice_rds_stats* output) {
	//Lock
	pthread_mutex_lock(&stat_lock);

	//Copy
	*output = stats;

	//Unlock
	pthread_mutex_unlock(&stat_lock);
}

void fmice_rds::update_stats(int addUnderrun, int addOverrun) {
	//Lock
	pthread_mutex_lock(&stat_lock);

	//Update
	stats.underruns += addUnderrun;
	stats.overruns += addOverrun;
	stats.has_sync = !is_overflow && !is_underflow;

	//Unlock
	pthread_mutex_unlock(&stat_lock);
}

void fmice_rds::push_in(const float* mpxIn, int count) {
	//Push into decoder
	int decodedBits = dec.process(mpxIn, decoder_buffer, count);

	//Push decoded bits into the bit buffer
	bit_buffer_push(decoder_buffer, decodedBits);
}

void fmice_rds::process(const float* mpxIn, float* mpxOut, int count, bool filter) {
	//Filter composite to remove old RDS
	if (filter)
		mpx_filter.process(count, mpxIn, mpxOut);

	//Begin encoder stage to fill up mpxOut with RDS samples
	int encCount = 0;
	int writable;
	while (encCount < count) {
		//Read out of the rds buffer and add it to the output until we run out of samples in the output or the RDS buffer
		while (rds_buffer_read < rds_buffer_aval && encCount < count) {
			mpxOut[encCount++] += rds_buffer[rds_buffer_read++];
		}

		//If the RDS buffer is empty, encode a new bit
		if (rds_buffer_read == rds_buffer_aval) {
			//Encode a sample
			enc.push(bit_buffer_pop(), encoder_buffer, encoder_buffer_len);

			//Resample
			rds_buffer_aval = rds_resamp.process(encoder_buffer_len, encoder_buffer, rds_buffer);
			assert(rds_buffer_aval <= rds_buffer_len);
			rds_buffer_read = 0;

			//Mix with 57 kHz
			for (int i = 0; i < rds_buffer_aval; i++) {
				//Multiply
				rds_buffer[i] *= sinf(osc_phase) * scale;

				//Step
				osc_phase += osc_phase_inc;
				if (fabs(osc_phase) > M_PI) {
					while (osc_phase > M_PI)
						osc_phase -= 2 * M_PI;
					while (osc_phase < -M_PI)
						osc_phase += 2 * M_PI;
				}
			}
		}
	}
}

void fmice_rds::bit_buffer_push(const uint8_t* buffer, int count) {
	//If already overflowing, allow buffer to drain significantly instead of writing
	if (is_overflow) {
		//Determine if we are still in overflow state
		is_overflow = bit_buffer_use > (bit_buffer_len / 2);

		//If still overflowing abort, otherwise update stats (to update sync flag)
		if (is_overflow)
			return;
		else
			update_stats(0, 0);
	}

	//Copy into the buffer
	int readOffset = 0;
	int writable;
	while (count > 0) {
		//Calculate how much we can copy
		writable = std::min(std::min(count, bit_buffer_len - bit_buffer_use), bit_buffer_len - bit_buffer_write);

		//Copy
		if (writable > 0)
			memcpy(&bit_buffer[bit_buffer_write], &buffer[readOffset], sizeof(uint8_t) * writable);
		bit_buffer_write = (bit_buffer_write + writable) % bit_buffer_len;
		bit_buffer_use += writable;
		readOffset += writable;
		count -= writable;

		//Check if we are out of room in the bit buffer
		if (bit_buffer_use == bit_buffer_len) {
			//Handle overrun
			handle_overrun();

			//Abort
			return;
		}
	}
}

uint8_t fmice_rds::bit_buffer_pop() {
	//If already underflowing, allow buffer to fill significantly instead of reading
	if (is_underflow) {
		//Determine if we are still in underflow state
		is_underflow = bit_buffer_use < (bit_buffer_len / 2);

		//If still underflowing abort, otherwise update stats (to update sync flag)
		if (is_underflow)
			return 0;
		else
			update_stats(0, 0);
	}

	//If a sample is available for reading, pop it
	uint8_t samp = 0;
	if (bit_buffer_use > 0) {
		samp = bit_buffer[bit_buffer_read];
		bit_buffer_read = (bit_buffer_read + 1) % bit_buffer_len;
		bit_buffer_use--;
	}
	else {
		//Handle underrun
		handle_underrun();
	}

	return samp;
}

void fmice_rds::handle_overrun() {
	//Set flag for processing
	is_overflow = true;

	//Update stats
	update_stats(0, 1);
}

void fmice_rds::handle_underrun() {
	//Set flag for processing
	is_underflow = true;

	//Update stats
	update_stats(1, 0);
}