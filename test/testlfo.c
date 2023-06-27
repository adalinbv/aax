
#include <stdio.h>
#include <stdlib.h>

#include "dsp/lfo.h"
#include "api.h"

#define TRY(a, d) if ((a) == 0) { \
  printf("%i: %s %s\n", __LINE__, name, d); \
  exit(-1); \
}

void testLFO(enum aaxSourceType state, const char *name)
{
    int constant;
    _aaxMixerInfo info;
    _aaxLFOData *lfo;
    float period_rate;
    float fs, f, n;

    info.frequency = fs = 44100.0f;
    info.period_rate = period_rate = 90.0f;

    TRY(lfo = _lfo_create(), "_lfo_create");
    _lfo_setup(lfo, &info, state);

    lfo->min =-9.9f;
    lfo->max = 9.9f;

    lfo->min_sec = lfo->min/lfo->fs;
    lfo->max_sec = lfo->max/lfo->fs;
    lfo->f = 5.0f;

    constant = _lfo_set_timing(lfo);
    lfo->envelope = AAX_FALSE;

    TRY(_lfo_set_function(lfo, constant), "_lfo_set_function");

    printf("%s\n", name);
    for (f = 0.0f; f < 1.0f/lfo->f; f += 1.0f/period_rate)
    {
        n = lfo->get(lfo, NULL, NULL, 0, 0);
        printf("%- .1f ", n);
    }
    printf("\n\n");

    _lfo_destroy(lfo);
}

int main()
{
    testLFO(AAX_IMPULSE, "impulse");
    testLFO(AAX_CYCLOID, "cycloid");
    testLFO(AAX_SINE, "sine");
    testLFO(AAX_TRIANGLE, "triangle");
    testLFO(AAX_SQUARE, "square");
    testLFO(AAX_SAWTOOTH, "sawtooth");

    testLFO(AAX_INVERSE_IMPULSE, "inverse-impulse");
    testLFO(AAX_INVERSE_CYCLOID, "inverse-cycloid");
    testLFO(AAX_INVERSE_SINE, "inverse-sine");
    testLFO(AAX_INVERSE_TRIANGLE, "inverse-triangle");
    testLFO(AAX_INVERSE_SQUARE, "inverse-square");
    testLFO(AAX_INVERSE_SAWTOOTH, "inverse-sawtooth");
}
