#ifndef FMICE_DEFINES
#define FMICE_DEFINES

#define SAMP_RATE 384000
#define DECIM_RATE 3
#define MPX_SAMP_RATE (SAMP_RATE / DECIM_RATE)

#include "volk/volk.h"

#define NTAPS_DEMOD 43
extern const float taps_demod[NTAPS_DEMOD];

#define NTAPS_COMPOSITE 291
extern const float taps_composite[NTAPS_COMPOSITE];

float fast_atan2f(float y, float x);

/// <summary>
/// Initializes and connects to Icecast. Returns 0 on success.
/// </summary>
/// <param name="host"></param>
/// <param name="port"></param>
/// <param name="mount"></param>
/// <param name="username"></param>
/// <param name="password"></param>
/// <returns></returns>
int cast_init(const char* host, unsigned short port, const char* mount, const char* username, const char* password);

/// <summary>
/// Pushes a sample to the buffer.
/// </summary>
/// <param name="sample"></param>
void cast_push_sample(float sample);

/// <summary>
/// Call continously to transmit buffers to the internet.
/// </summary>
/// <returns></returns>
int cast_transmit();

/// <summary>
/// Initializes the radio. Returns 0 on success.
/// </summary>
/// <returns></returns>
int radio_init();

/// <summary>
/// Starts the radio streaming samples. Returns 0 on success.
/// </summary>
/// <returns></returns>
int radio_start();

typedef struct {
	size_t size; // Number of samples
	size_t write_pos; // Write position, in samples
	size_t read_pos; // Read position, in samples
	size_t consumed; // Number of samples for reading
	float* buffer; // The buffer allocated for this
} fifo_buffer_t;

/// <summary>
/// Initializes the fifo struct and allocates the buffer. Returns 0 on success.
/// </summary>
/// <param name="fifo">The struct to initialize.</param>
/// <param name="size">The number of samples in the buffer.</param>
/// <returns></returns>
int fifo_init(fifo_buffer_t* fifo, size_t size);

/// <summary>
/// Releases the buffer in the FIFO struct.
/// </summary>
/// <param name="fifo">The FIFO struct.</param>
void fifo_free(fifo_buffer_t* fifo);

/// <summary>
/// Pushes samples into the FIFO. Returns the number of samples written.
/// </summary>
/// <param name="fifo">The FIFO struct.</param>
/// <param name="input">The samples to read from.</param>
/// <param name="count">The number of samples to push in.</param>
/// <returns></returns>
size_t fifo_push(fifo_buffer_t* fifo, float* input, size_t count);

/// <summary>
/// Same as fifo_push, except the input is complex.
/// </summary>
/// <param name="fifo"></param>
/// <param name="input"></param>
/// <param name="count"></param>
/// <returns></returns>
size_t fifo_push_complex(fifo_buffer_t* fifo, lv_32fc_t* input, size_t count);

/// <summary>
/// Pops samples from the FIFO. Returns the number of samples read.
/// </summary>
/// <param name="fifo">THe FIFO struct.</param>
/// <param name="output">The buffer to push samples into.</param>
/// <param name="count">The maximum number of samples to read.</param>
/// <param name="mod">The number of samples in each "block". The result will always be a multiple of this.</param>
/// <returns></returns>
size_t fifo_pop(fifo_buffer_t* fifo, float* output, size_t count, size_t mod);

/// <summary>
/// Same as fifo_pop, but the output is complex.
/// </summary>
/// <param name="fifo"></param>
/// <param name="output"></param>
/// <param name="count"></param>
/// <param name="mod"></param>
/// <returns></returns>
size_t fifo_pop_complex(fifo_buffer_t* fifo, lv_32fc_t* output, size_t count, size_t mod);

#endif