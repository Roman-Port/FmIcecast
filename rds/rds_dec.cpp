#include "rds_dec.h"

#include <dsp/taps/band_pass.h>
#include <dsp/convert/complex_to_real.h>
#include <dsp/digital/binary_slicer.h>

fmice_rds_dec::fmice_rds_dec(int bufferSize) :
    buffer_size(bufferSize)
{

}

fmice_rds_dec::~fmice_rds_dec() {

}

void fmice_rds_dec::configure(int sampleRate) {
    //Init RTOC
    rtoc.init(NULL);
    rtoc.out.setBufferSize(buffer_size);

    //Init xlator
    xlator.init(NULL, -57000.0, sampleRate);
    xlator.out.setBufferSize(buffer_size);

    //Init resampler
    rds_resamp.init(NULL, sampleRate, 5000.0);
    rds_resamp.out.setBufferSize(buffer_size);

    //Init AGC
    agc.init(NULL, 1.0, 1e6, 0.1);
    agc.out.setBufferSize(buffer_size);

    //Init costas loop
    costas.init(NULL, 0.005f);
    costas.out.setBufferSize(buffer_size);

    //Init filter
    taps = dsp::taps::bandPass<dsp::complex_t>(0, 2375, 100, 5000);
    fir.init(NULL, taps);
    fir.out.setBufferSize(buffer_size);

    //Init second costas loop
    double baudfreq = dsp::math::hzToRads(2375.0 / 2.0, 5000);
    costas2.init(NULL, 0.01, 0.0, baudfreq, baudfreq - (baudfreq * 0.1), baudfreq + (baudfreq * 0.1));
    costas2.out.setBufferSize(buffer_size);

    //Init clock recovery
    recov.init(NULL, 5000.0 / (2375.0 / 2.0), 1e-6, 0.01, 0.01);
    recov.out.setBufferSize(buffer_size);
}

int fmice_rds_dec::process(const float* mpx, uint8_t* bitsOut, int count) {
    //Convert MPX to complex
    rtoc.process(count, mpx, rtoc.out.writeBuf);

    //Translate to 0Hz
    xlator.process(count, rtoc.out.writeBuf, rtoc.out.writeBuf);

    //Resample to the output samplerate
    count = rds_resamp.process(count, rtoc.out.writeBuf, rds_resamp.out.writeBuf);

    count = agc.process(count, rds_resamp.out.writeBuf, costas.out.readBuf);
    count = costas.process(count, costas.out.readBuf, costas.out.writeBuf);
    count = fir.process(count, costas.out.writeBuf, costas.out.writeBuf);
    count = costas2.process(count, costas.out.writeBuf, costas.out.readBuf);
    count = dsp::convert::ComplexToReal::process(count, costas.out.readBuf, recov.out.readBuf);
    count = recov.process(count, recov.out.readBuf, recov.out.writeBuf);
    count = dsp::digital::BinarySlicer::process(count, recov.out.writeBuf, bitsOut);

    return count;
}