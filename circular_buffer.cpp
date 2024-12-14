#include "circular_buffer.h"

#include <stdexcept>
#include <string.h>
#include "libairspyhf/airspyhf.h"

template <typename T>
fmice_circular_buffer<T>::fmice_circular_buffer(size_t size) {
    //Allocate buffer
    buffer = (T*)malloc(sizeof(T) * size);
    if (buffer == nullptr)
        throw new std::runtime_error("Failed to initialize buffer.");

    //Init mutex
    if (pthread_mutex_init(&cast_lock, NULL) != 0)
        throw new std::runtime_error("Failed to initialize mutex.");

    //Init condition
    if (pthread_cond_init(&cast_cond, NULL) != 0)
        throw new std::runtime_error("Failed to initialize cond.");

    //Initialize
    this->size = size;
    this->use = 0;
    this->pos_write = 0;
    this->pos_read = 0;
}

template <typename T>
fmice_circular_buffer<T>::~fmice_circular_buffer() {
    //Free condition
    pthread_cond_destroy(&cast_cond);

    //Free mutex
    pthread_mutex_destroy(&cast_lock);

    //Free buffer
    free(buffer);
}

template <typename T>
size_t fmice_circular_buffer<T>::write(T* input, size_t incoming) {
    //Lock
    pthread_mutex_lock(&cast_lock);

    //Loop
    size_t written = 0;
    size_t writable;
    while (incoming > 0 && (size - use) > 0) {
        //Determine writable
        writable = std::min(std::min(incoming, size - use), size - pos_write);

        //Write to the buffer
        memcpy(&buffer[pos_write], input, sizeof(T) * writable);

        //Update state
        input += writable;
        incoming -= writable;
        pos_write = (pos_write + writable) % size;
        use += writable;
        written += writable;
    }

    //Signal a buffer is ready
    pthread_cond_signal(&cast_cond);

    //Unlock
    pthread_mutex_unlock(&cast_lock);

    return written;
}

template <typename T>
size_t fmice_circular_buffer<T>::read(T* output, size_t count) {
    //Lock
    pthread_mutex_lock(&cast_lock);

    //Wait for enough samples to be available
    while (use < count)
        pthread_cond_wait(&cast_cond, &cast_lock);

    //Loop reads
    size_t read = 0;
    size_t readable;
    while (count > 0 && use > 0) {
        //Determine how much is readable
        readable = std::min(count, std::min(size - pos_read, use));

        //Write to the output
        memcpy(output, &buffer[pos_read], sizeof(T) * readable);

        //Update state
        output += readable;
        count -= readable;
        read += readable;
        pos_read = (pos_read + readable) % size;
        use -= readable;
    }

    //Unlock
    pthread_mutex_unlock(&cast_lock);

    return read;
}

template class fmice_circular_buffer<float>;
template class fmice_circular_buffer<int32_t>;
template class fmice_circular_buffer<airspyhf_complex_float_t>;