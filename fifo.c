#include "defines.h"
#include "volk/volk.h"
#include "string.h"
#include "assert.h"

static void reset_fifo(fifo_buffer_t* fifo) {
	fifo->size = 0;
	fifo->read_pos = 0;
	fifo->write_pos = 0;
	fifo->consumed = 0;
	fifo->buffer = 0;
}

int fifo_init(fifo_buffer_t* fifo, size_t size) {
	//Initialize
	reset_fifo(fifo);

	//Set
	fifo->size = size;

	//Allocate buffer
	fifo->buffer = (float*)volk_malloc(size * sizeof(float), volk_get_alignment());
	if (fifo->buffer == 0)
		return -1;

	return 0;
}

void fifo_free(fifo_buffer_t* fifo) {
	//Free buffer
	if (fifo->buffer != 0)
		volk_free(fifo->buffer);

	//Reset
	reset_fifo(fifo);
}

size_t fifo_push(fifo_buffer_t* fifo, float* input, size_t count) {
	//Insert into the buffer, looping around as needed, until we're out of input samples or the FIFO is full
	size_t written = 0;
	size_t writable;
	while (count > 0 && fifo->consumed < fifo->size) {
		//Calculate amount to be written
		writable = fifo->size - fifo->write_pos;
		if (writable > count)
			writable = count;

		//Write to the buffer
		memcpy(&fifo->buffer[fifo->write_pos], input, sizeof(float) * writable);

		//Update cursors
		input += writable;
		count -= writable;
		fifo->write_pos = (fifo->write_pos + writable) % fifo->size;
		fifo->consumed += writable;
		written += writable;
	}

	return writable;
}

size_t fifo_push_complex(fifo_buffer_t* fifo, lv_32fc_t* input, size_t count) {
	size_t result = fifo_push(fifo, (float*)input, count * 2);
	assert((count % 2) == 0);
	return result / 2;
}

size_t fifo_pop(fifo_buffer_t* fifo, float* output, size_t count, size_t mod) {
	//Determine how much is available to read
	if (count > fifo->consumed)
		count = fifo->consumed;

	//Make sure count is a multiple of mod by rounding down
	count -= count % mod;

	//Read
	size_t read = 0;
	size_t available;
	while (count > 0) {
		//Calculate amount that can be read in this cycle
		available = fifo->size - fifo->read_pos;
		if (available > count)
			available = count;

		//Write to the output
		memcpy(output, &fifo->buffer[fifo->read_pos], sizeof(float) * available);

		//Update state
		output += available;
		count -= available;
		read += available;
		fifo->read_pos = (fifo->read_pos + available) % fifo->size;
		fifo->consumed -= available;
	}

	//Sanity check
	assert((read % mod) == 0);

	return read;
}

size_t fifo_pop_complex(fifo_buffer_t* fifo, lv_32fc_t* output, size_t count, size_t mod) {
	size_t result = fifo_pop(fifo, (float*)output, count * 2, mod * 2);
	assert((count % 2) == 0);
	return result / 2;
}