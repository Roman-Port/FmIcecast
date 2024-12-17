#pragma once

#include <stdint.h>
#include <pthread.h>

template <typename T>
class fmice_circular_buffer {

public:
	fmice_circular_buffer(size_t size);
	~fmice_circular_buffer();

	/// <summary>
	/// Writes to the buffer. Thread safe.
	/// </summary>
	/// <param name="input"></param>
	/// <param name="count"></param>
	size_t write(const T* input, size_t count);

	/// <summary>
	/// Reads from the buffer. Thread safe. Hangs until count samples are recieved.
	/// </summary>
	/// <param name="output"></param>
	/// <param name="count"></param>
	/// <returns></returns>
	size_t read(T* output, size_t count);

	size_t get_size();
	size_t get_use();
	size_t get_free();

	/// <summary>
	/// Clears all samples in the circular buffer. Thread safe.
	/// </summary>
	void reset();

private:
	pthread_mutex_t cast_lock;
	pthread_cond_t cast_cond;

	// must be in mutex for following
	T* buffer;
	size_t size;
	size_t use;
	size_t pos_write;
	size_t pos_read;

};