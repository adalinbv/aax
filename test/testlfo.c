
#include <stdio.h>
#include <stdlib.h>

#include "dsp/lfo.h"
#include "api.h"

#define TRY(a) if ((a) == 0) { printf("%i: %s\n", __LINE__, aaxGetErrorString(aaxGetErrorNo())); exit(-1); }

int main()
{
    int constant, state;
    _aaxMixerInfo info;
    _aaxLFOData *lfo;
    float period_rate;
    float fs, f, n;

    info.frequency = fs = 44100.0f;
    info.period_rate = period_rate = 90.0f;

    TRY(lfo = _lfo_create());

    state = AAX_SINE_WAVE;
    _lfo_setup(lfo, &info, state);

    lfo->min = 0.0f;
    lfo->max = 1.0f;

    lfo->min_sec = lfo->min/lfo->fs;
    lfo->max_sec = lfo->max/lfo->fs;
    lfo->f = 0.1f;

    constant = _lfo_set_timing(lfo);
    lfo->envelope = AAX_FALSE;

    TRY(_lfo_set_function(lfo, constant));

    for (f = 0.0f; f < 1.0f/lfo->f; f += 1.0f/period_rate)
    {
        n = lfo->get(lfo, NULL, NULL, 0, 0);
        printf("%.1f - %.4f\n", f, n);
    }

    _lfo_destroy(lfo);
}
