#ifndef FMICE_DEFINES
#define FMICE_DEFINES

#define SAMP_RATE 384000
#define DECIM_RATE 3
#define AUDIO_DECIM_RATE 2
#define MPX_SAMP_RATE (SAMP_RATE / DECIM_RATE)
#define AUDIO_SAMP_RATE (MPX_SAMP_RATE / AUDIO_DECIM_RATE)

#define FMICE_BLOCK_SIZE 32768 /* Block size going to encoder */
#define FMICE_BLOCK_COUNT 8

#include "volk/volk.h"

#endif