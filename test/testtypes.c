
#include <stdio.h>
#include <stdlib.h>
#include <aax/aax.h>

#define DIST(a,b)	testDist((a),(b),__LINE__)
#define WAVE(a,b)       testWave((a),(b),__LINE__)
#define FLT(a,b)	testFilter((a),(b),__LINE__)
#define EFF(a,b)	testEffect((a),(b),__LINE__)

void
testDist(const char *name, int type, int lineno)
{
    int res = aaxGetDistanceModelByName(name);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testWave(const char *name, int type, int lineno)
{   
    int res = aaxGetWaveformTypeByName(name);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testFilter(const char *name, int type, int lineno)
{
    int res = aaxFilterGetByName(NULL, name);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testEffect(const char *name, int type, int lineno)
{
    int res = aaxEffectGetByName(NULL, name);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

int main()
{

    /* waveforms */
    WAVE("AAX_CONSTANT_VALUE", AAX_CONSTANT_VALUE);
    WAVE("AAX_TRIANGLE_WAVE", AAX_TRIANGLE_WAVE);
    WAVE("AAX_SINE_WAVE", AAX_SINE_WAVE);
    WAVE("AAX_SQUARE_WAVE", AAX_SQUARE_WAVE);
    WAVE("AAX_SAWTOOTH_WAVE", AAX_SAWTOOTH_WAVE);
    WAVE("AAX_ENVELOPE_FOLLOW", AAX_ENVELOPE_FOLLOW);
    WAVE("AAX_INVERSE_TRIANGLE_WAVE", AAX_INVERSE_TRIANGLE_WAVE);
    WAVE("AAX_INVERSE_SINE_WAVE", AAX_INVERSE_SINE_WAVE);
    WAVE("AAX_INVERSE_SQUARE_WAVE", AAX_INVERSE_SQUARE_WAVE);
    WAVE("AAX_INVERSE_SAWTOOTH_WAVE", AAX_INVERSE_SAWTOOTH_WAVE);
    WAVE("AAX_INVERSE_ENVELOPE_FOLLOW", AAX_INVERSE_ENVELOPE_FOLLOW);

    WAVE("constant-value", AAX_CONSTANT_VALUE);
    WAVE("triangle-wave", AAX_TRIANGLE_WAVE);
    WAVE("sine-wave", AAX_SINE_WAVE);
    WAVE("square-wave", AAX_SQUARE_WAVE);
    WAVE("sawtooth-wave", AAX_SAWTOOTH_WAVE);
    WAVE("envelope-follow", AAX_ENVELOPE_FOLLOW);
    WAVE("inverse-triangle-wave", AAX_INVERSE_TRIANGLE_WAVE);
    WAVE("inverse-sine-wave", AAX_INVERSE_SINE_WAVE);
    WAVE("inverse-square-wave", AAX_INVERSE_SQUARE_WAVE);
    WAVE("inverse-sawtooth-wave", AAX_INVERSE_SAWTOOTH_WAVE);
    WAVE("inverse-envelope-follow", AAX_INVERSE_ENVELOPE_FOLLOW);

    WAVE("constant", AAX_CONSTANT_VALUE);
    WAVE("triangle", AAX_TRIANGLE_WAVE);
    WAVE("sine", AAX_SINE_WAVE);
    WAVE("square", AAX_SQUARE_WAVE);
    WAVE("sawtooth", AAX_SAWTOOTH_WAVE);
    WAVE("envelope", AAX_ENVELOPE_FOLLOW);
    WAVE("inverse_triangle", AAX_INVERSE_TRIANGLE_WAVE);
    WAVE("inverse_sine", AAX_INVERSE_SINE_WAVE);
    WAVE("inverse_square", AAX_INVERSE_SQUARE_WAVE);
    WAVE("inverse_sawtooth", AAX_INVERSE_SAWTOOTH_WAVE);
    WAVE("inverse_envelope", AAX_INVERSE_ENVELOPE_FOLLOW);

    WAVE("", AAX_CONSTANT_VALUE);
    WAVE("|", AAX_CONSTANT_VALUE);
    WAVE("||||", AAX_CONSTANT_VALUE);
    WAVE("sine|square", AAX_SINE_WAVE|AAX_SQUARE_WAVE);
    WAVE("sine|triangle|square|sawtooth", AAX_SINE_WAVE|AAX_TRIANGLE_WAVE|AAX_SQUARE_WAVE|AAX_SAWTOOTH_WAVE);
    WAVE("square|sawtooth|sine", AAX_SQUARE_WAVE|AAX_SAWTOOTH_WAVE|AAX_SINE_WAVE);
    WAVE("sine|inverse_triangle", AAX_SINE_WAVE|AAX_INVERSE_TRIANGLE_WAVE);

    /* distance model */
    DIST("AAX_DISTANCE_MODEL_NONE", AAX_DISTANCE_MODEL_NONE);
    DIST("AAX_EXPONENTIAL_DISTANCE", AAX_EXPONENTIAL_DISTANCE);
    DIST("AAX_EXPONENTIAL_DISTANCE_DELAY", AAX_EXPONENTIAL_DISTANCE_DELAY);
    DIST("AAX_AL_INVERSE_DISTANCE", AAX_AL_INVERSE_DISTANCE);
    DIST("AAX_AL_INVERSE_DISTANCE_CLAMPED", AAX_AL_INVERSE_DISTANCE_CLAMPED);
    DIST("AAX_AL_LINEAR_DISTANCE", AAX_AL_LINEAR_DISTANCE);
    DIST("AAX_AL_LINEAR_DISTANCE_CLAMPED", AAX_AL_LINEAR_DISTANCE_CLAMPED);
    DIST("AAX_AL_EXPONENT_DISTANCE", AAX_AL_EXPONENT_DISTANCE);
    DIST("AAX_AL_EXPONENT_DISTANCE_CLAMPED", AAX_AL_EXPONENT_DISTANCE_CLAMPED);

    DIST("distance-model-none", AAX_DISTANCE_MODEL_NONE);
    DIST("exponential-distance", AAX_EXPONENTIAL_DISTANCE);
    DIST("exponential-distance-delay", AAX_EXPONENTIAL_DISTANCE_DELAY);
    DIST("inverse-distance", AAX_AL_INVERSE_DISTANCE);
    DIST("inverse-distance-clamped", AAX_AL_INVERSE_DISTANCE_CLAMPED);
    DIST("linear-distance", AAX_AL_LINEAR_DISTANCE);
    DIST("linear-distance-clamped", AAX_AL_LINEAR_DISTANCE_CLAMPED);
    DIST("exponent-distance", AAX_AL_EXPONENT_DISTANCE);
    DIST("exponent-distance-clamped", AAX_AL_EXPONENT_DISTANCE_CLAMPED);

    /* filters */
    FLT("AAX_EQUALIZER", AAX_EQUALIZER);
    FLT("AAX_VOLUME_FILTER", AAX_VOLUME_FILTER);
    FLT("AAX_DYNAMIC_GAIN_FILTER", AAX_DYNAMIC_GAIN_FILTER);
    FLT("AAX_TREMOLO_FILTER", AAX_TREMOLO_FILTER);
    FLT("AAX_TIMED_GAIN_FILTER", AAX_TIMED_GAIN_FILTER);
    FLT("AAX_ANGULAR_FILTER", AAX_ANGULAR_FILTER);
    FLT("AAX_DISTANCE_FILTER", AAX_DISTANCE_FILTER);
    FLT("AAX_FREQUENCY_FILTER", AAX_FREQUENCY_FILTER);
    FLT("AAX_GRAPHIC_EQUALIZER", AAX_GRAPHIC_EQUALIZER);
    FLT("AAX_COMPRESSOR", AAX_COMPRESSOR);

    FLT("equalizer", AAX_EQUALIZER);
    FLT("volume", AAX_VOLUME_FILTER);
    FLT("dynamic-gain", AAX_DYNAMIC_GAIN_FILTER);
    FLT("tremolo", AAX_TREMOLO_FILTER);
    FLT("timed-gain", AAX_TIMED_GAIN_FILTER);
    FLT("angular", AAX_ANGULAR_FILTER);
    FLT("distance", AAX_DISTANCE_FILTER);
    FLT("frequency", AAX_FREQUENCY_FILTER);
    FLT("graphic-equalizer", AAX_GRAPHIC_EQUALIZER);
    FLT("compressor", AAX_COMPRESSOR);

    /* effect */
    EFF("AAX_PITCH_EFFECT", AAX_PITCH_EFFECT);
    EFF("AAX_DYNAMIC_PITCH_EFFECT", AAX_DYNAMIC_PITCH_EFFECT);
    EFF("AAX_VIBRATO_EFFECT", AAX_VIBRATO_EFFECT);
    EFF("AAX_TIMED_PITCH_EFFECT", AAX_TIMED_PITCH_EFFECT);
    EFF("AAX_DISTORTION_EFFECT", AAX_DISTORTION_EFFECT);
    EFF("AAX_PHASING_EFFECT", AAX_PHASING_EFFECT);
    EFF("AAX_CHORUS_EFFECT", AAX_CHORUS_EFFECT);
    EFF("AAX_FLANGING_EFFECT", AAX_FLANGING_EFFECT);
    EFF("AAX_VELOCITY_EFFECT", AAX_VELOCITY_EFFECT);
    EFF("AAX_REVERB_EFFECT", AAX_REVERB_EFFECT);
    EFF("AAX_CONVOLUTION_EFFECT", AAX_CONVOLUTION_EFFECT);

    EFF("pitch", AAX_PITCH_EFFECT);
    EFF("dynamic-pitch", AAX_DYNAMIC_PITCH_EFFECT);
    EFF("vibrato", AAX_VIBRATO_EFFECT);
    EFF("timed-pitch", AAX_TIMED_PITCH_EFFECT);
    EFF("distortion", AAX_DISTORTION_EFFECT);
    EFF("phasing", AAX_PHASING_EFFECT);
    EFF("chorus", AAX_CHORUS_EFFECT);
    EFF("flanging", AAX_FLANGING_EFFECT);
    EFF("velocity", AAX_VELOCITY_EFFECT);
    EFF("reverb", AAX_REVERB_EFFECT);
    EFF("convolution", AAX_CONVOLUTION_EFFECT);

    return 0;
}
