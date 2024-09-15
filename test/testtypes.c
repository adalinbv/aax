
#include <stdio.h>
#include <stdlib.h>
#include <aax/aax.h>

#define DIST(a,b)	testDist((a),(b),__LINE__)
#define WAVE(a,b)       testSource((a),(b),__LINE__)
#define FREQ(a,b)	testFreq((a),(b),__LINE__)
#define FLT(a,b)	testFilter((a),(b),__LINE__)
#define EFF(a,b)	testEffect((a),(b),__LINE__)
#define MOD(a,b)	testModulation((a),(b),__LINE__)

void
testDist(const char *name, int type, int lineno)
{
    int res = aaxGetByName(name, AAX_DISTANCE_MODEL_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testSource(const char *name, int type, int lineno)
{   
    int res = aaxGetByName(name, AAX_SOURCE_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testFreq(const char *name, int type, int lineno)
{
    int res = aaxGetByName(name, AAX_FREQUENCY_FILTER_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testFilter(const char *name, int type, int lineno)
{
    int res = aaxGetByName(name, AAX_FILTER_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testEffect(const char *name, int type, int lineno)
{
    int res = aaxGetByName(name, AAX_EFFECT_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

void
testModulation(const char *name, int type, int lineno)
{
    int res = aaxGetByName(name, AAX_MODULATION_NAME);
    if (res != type)
    {
        printf("at line: %i, %s:\t\t0x%x != 0x%x\n", lineno, name, res, type);
        exit(-1);
    }
}

int main()
{

    WAVE("AAX_WAVE_NONE", AAX_WAVE_NONE);
    WAVE("AAX_CONSTANT", AAX_CONSTANT);

    WAVE("false", AAX_WAVE_NONE);
    WAVE("true", AAX_CONSTANT);

    /* waveforms */
    WAVE("AAX_CONSTANT", AAX_CONSTANT);
    WAVE("AAX_TRIANGLE", AAX_TRIANGLE);
    WAVE("AAX_SINE", AAX_SINE);
    WAVE("AAX_SQUARE", AAX_SQUARE);
    WAVE("AAX_SAWTOOTH", AAX_SAWTOOTH);
    WAVE("AAX_PURE_TRIANGLE", AAX_PURE_TRIANGLE);
    WAVE("AAX_PURE_SINE", AAX_PURE_SINE);
    WAVE("AAX_PURE_SQUARE", AAX_PURE_SQUARE);
    WAVE("AAX_PURE_SAWTOOTH", AAX_PURE_SAWTOOTH);
    WAVE("AAX_ENVELOPE_FOLLOW", AAX_ENVELOPE_FOLLOW);
    WAVE("AAX_INVERSE_TRIANGLE", AAX_INVERSE_TRIANGLE);
    WAVE("AAX_INVERSE_SINE", AAX_INVERSE_SINE);
    WAVE("AAX_INVERSE_SQUARE", AAX_INVERSE_SQUARE);
    WAVE("AAX_INVERSE_SAWTOOTH", AAX_INVERSE_SAWTOOTH);
    WAVE("AAX_INVERSE_PURE_TRIANGLE", AAX_INVERSE_PURE_TRIANGLE);
    WAVE("AAX_INVERSE_PURE_SINE", AAX_INVERSE_PURE_SINE);
    WAVE("AAX_INVERSE_PURE_SQUARE", AAX_INVERSE_PURE_SQUARE);
    WAVE("AAX_INVERSE_PURE_SAWTOOTH", AAX_INVERSE_PURE_SAWTOOTH);
    WAVE("AAX_INVERSE_ENVELOPE_FOLLOW", AAX_INVERSE_ENVELOPE_FOLLOW);

    WAVE("constant", AAX_CONSTANT);
    WAVE("triangle", AAX_TRIANGLE);
    WAVE("sine", AAX_SINE);
    WAVE("square", AAX_SQUARE);
    WAVE("sawtooth", AAX_SAWTOOTH);
    WAVE("pure-triangle", AAX_PURE_TRIANGLE);
    WAVE("pure-sine", AAX_PURE_SINE);
    WAVE("pure-square", AAX_PURE_SQUARE);
    WAVE("pure-sawtooth", AAX_PURE_SAWTOOTH);
    WAVE("envelope-follow", AAX_ENVELOPE_FOLLOW);
    WAVE("inverse-triangle", AAX_INVERSE_TRIANGLE);
    WAVE("inverse-sine", AAX_INVERSE_SINE);
    WAVE("inverse-square", AAX_INVERSE_SQUARE);
    WAVE("inverse-sawtooth", AAX_INVERSE_SAWTOOTH);
    WAVE("inverse-pure-triangle", AAX_INVERSE_PURE_TRIANGLE);
    WAVE("inverse-pure-sine", AAX_INVERSE_PURE_SINE);
    WAVE("inverse-pure-square", AAX_INVERSE_PURE_SQUARE);
    WAVE("inverse-pure-sawtooth", AAX_INVERSE_PURE_SAWTOOTH);
    WAVE("inverse-envelope-follow", AAX_INVERSE_ENVELOPE_FOLLOW);

    WAVE("1st-order", AAX_EFFECT_1ST_ORDER);
    WAVE("2nd-order", AAX_EFFECT_2ND_ORDER);

    WAVE("logarithmic", AAX_LOGARITHMIC_CURVE|AAX_LFO_EXPONENTIAL|AAX_ENVELOPE_FOLLOW);
    WAVE("exponential", AAX_EXPONENTIAL_CURVE|AAX_LFO_EXPONENTIAL|AAX_ENVELOPE_FOLLOW);
    WAVE("square-root", AAX_SQUARE_ROOT_CURVE);
    WAVE("linear", AAX_LINEAR_CURVE);

    WAVE("", AAX_CONSTANT);
    WAVE("|", AAX_CONSTANT);
    WAVE("||||", AAX_CONSTANT);

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

    /* frequency filter */
    FREQ("AAX_1ST_ORDER", AAX_1ST_ORDER);
    FREQ("AAX_2ND_ORDER", AAX_2ND_ORDER);
    FREQ("AAX_4TH_ORDER", AAX_4TH_ORDER);
    FREQ("AAX_6TH_ORDER", AAX_6TH_ORDER);
    FREQ("AAX_8TH_ORDER", AAX_8TH_ORDER);
    FREQ("AAX_1ST_ORDER|AAX_BESSEL", AAX_1ST_ORDER|AAX_BESSEL);
    FREQ("AAX_2ND_ORDER|AAX_BESSEL", AAX_2ND_ORDER|AAX_BESSEL);
    FREQ("AAX_4TH_ORDER|AAX_BESSEL", AAX_4TH_ORDER|AAX_BESSEL);
    FREQ("AAX_6TH_ORDER|AAX_BESSEL", AAX_6TH_ORDER|AAX_BESSEL);
    FREQ("AAX_8TH_ORDER|AAX_BESSEL", AAX_8TH_ORDER|AAX_BESSEL);

    FREQ("AAX_6DB_OCT", AAX_1ST_ORDER);
    FREQ("AAX_12DB_OCT", AAX_2ND_ORDER);
    FREQ("AAX_24DB_OCT", AAX_4TH_ORDER);
    FREQ("AAX_36DB_OCT", AAX_6TH_ORDER);
    FREQ("AAX_48DB_OCT", AAX_8TH_ORDER);
    FREQ("AAX_BESSEL|AAX_6DB_OCT", AAX_BESSEL|AAX_1ST_ORDER);
    FREQ("AAX_BESSEL|AAX_12DB_OCT", AAX_BESSEL|AAX_2ND_ORDER);
    FREQ("AAX_BESSEL|AAX_24DB_OCT", AAX_BESSEL|AAX_4TH_ORDER);
    FREQ("AAX_BESSEL|AAX_36DB_OCT", AAX_BESSEL|AAX_6TH_ORDER);
    FREQ("AAX_BESSEL|AAX_48DB_OCT", AAX_BESSEL|AAX_8TH_ORDER);

    FREQ("1st-order", AAX_EFFECT_1ST_ORDER);
    FREQ("2nd-order", AAX_EFFECT_2ND_ORDER);
    FREQ("4th-order", AAX_4TH_ORDER);
    FREQ("6th-order", AAX_6TH_ORDER);
    FREQ("8th-order", AAX_8TH_ORDER);
    FREQ("bessel|1st-order", AAX_BESSEL|AAX_EFFECT_1ST_ORDER);
    FREQ("bessel|2nd-order", AAX_BESSEL|AAX_EFFECT_2ND_ORDER);
    FREQ("bessel|4th-order", AAX_BESSEL|AAX_4TH_ORDER);
    FREQ("bessel|6th-order", AAX_BESSEL|AAX_6TH_ORDER);
    FREQ("bessel|8th-order", AAX_BESSEL|AAX_8TH_ORDER);

    FREQ("6db/oct", AAX_6DB_OCT);
    FREQ("12db/oct", AAX_12DB_OCT);
    FREQ("24db/oct", AAX_24DB_OCT);
    FREQ("36db/oct", AAX_36DB_OCT);
    FREQ("48db/oct", AAX_48DB_OCT);
    FREQ("6db/oct|bessel", AAX_BESSEL|AAX_6DB_OCT);
    FREQ("12db/oct|bessel", AAX_BESSEL|AAX_12DB_OCT);
    FREQ("24db/oct|bessel", AAX_BESSEL|AAX_24DB_OCT);
    FREQ("36db/oct|bessel", AAX_BESSEL|AAX_36DB_OCT);
    FREQ("48db/oct|bessel", AAX_BESSEL|AAX_48DB_OCT);

    FREQ("1st-order|sine", AAX_EFFECT_1ST_ORDER|AAX_SINE);
    FREQ("4th-order|bessel|sawtooth", AAX_BESSEL|AAX_4TH_ORDER|AAX_SAWTOOTH);

    /* filters */
    FLT("AAX_EQUALIZER", AAX_EQUALIZER);
    FLT("AAX_VOLUME_FILTER", AAX_VOLUME_FILTER);
    FLT("AAX_DYNAMIC_GAIN_FILTER", AAX_DYNAMIC_GAIN_FILTER);
    FLT("AAX_TIMED_GAIN_FILTER", AAX_TIMED_GAIN_FILTER);
    FLT("AAX_DIRECTIONAL_FILTER", AAX_DIRECTIONAL_FILTER);
    FLT("AAX_DISTANCE_FILTER", AAX_DISTANCE_FILTER);
    FLT("AAX_FREQUENCY_FILTER", AAX_FREQUENCY_FILTER);
    FLT("AAX_BITCRUSHER_FILTER", AAX_BITCRUSHER_FILTER);
    FLT("AAX_GRAPHIC_EQUALIZER", AAX_GRAPHIC_EQUALIZER);
    FLT("AAX_COMPRESSOR", AAX_COMPRESSOR);

    FLT("equalizer", AAX_EQUALIZER);
    FLT("volume", AAX_VOLUME_FILTER);
    FLT("dynamic-gain", AAX_DYNAMIC_GAIN_FILTER);
    FLT("timed-gain", AAX_TIMED_GAIN_FILTER);
    FLT("directional", AAX_DIRECTIONAL_FILTER);
    FLT("distance", AAX_DISTANCE_FILTER);
    FLT("frequency", AAX_FREQUENCY_FILTER);
    FLT("bitcrusher", AAX_BITCRUSHER_FILTER);
    FLT("graphic-equalizer", AAX_GRAPHIC_EQUALIZER);
    FLT("compressor", AAX_COMPRESSOR);

    /* effect */
    EFF("AAX_PITCH_EFFECT", AAX_PITCH_EFFECT);
    EFF("AAX_DYNAMIC_PITCH_EFFECT", AAX_DYNAMIC_PITCH_EFFECT);
    EFF("AAX_TIMED_PITCH_EFFECT", AAX_TIMED_PITCH_EFFECT);
    EFF("AAX_DISTORTION_EFFECT", AAX_DISTORTION_EFFECT);
    EFF("AAX_PHASING_EFFECT", AAX_PHASING_EFFECT);
    EFF("AAX_CHORUS_EFFECT", AAX_CHORUS_EFFECT);
    EFF("AAX_FLANGING_EFFECT", AAX_FLANGING_EFFECT);
    EFF("AAX_VELOCITY_EFFECT", AAX_VELOCITY_EFFECT);
    EFF("AAX_REVERB_EFFECT", AAX_REVERB_EFFECT);
    EFF("AAX_CONVOLUTION_EFFECT", AAX_CONVOLUTION_EFFECT);
    EFF("AAX_RINGMODULATOR_EFFECT", AAX_RINGMODULATOR_EFFECT);

    EFF("pitch", AAX_PITCH_EFFECT);
    EFF("dynamic-pitch", AAX_DYNAMIC_PITCH_EFFECT);
    EFF("timed-pitch", AAX_TIMED_PITCH_EFFECT);
    EFF("distortion", AAX_DISTORTION_EFFECT);
    EFF("phasing", AAX_PHASING_EFFECT);
    EFF("chorus", AAX_CHORUS_EFFECT);
    EFF("flanging", AAX_FLANGING_EFFECT);
    EFF("velocity", AAX_VELOCITY_EFFECT);
    EFF("reverb", AAX_REVERB_EFFECT);
    EFF("convolution", AAX_CONVOLUTION_EFFECT);
    EFF("ringmodulator", AAX_RINGMODULATOR_EFFECT);

    /* MIDI modulation */
    MOD("gain", AAX_MIDI_GAIN_CONTROL);
    MOD("pitch", AAX_MIDI_PITCH_CONTROL);
    MOD("frequency", AAX_MIDI_FILTER_CONTROL);
    MOD("chorus", AAX_MIDI_CHORUS_CONTROL);
    MOD("3", AAX_MIDI_GAIN_CONTROL|AAX_MIDI_PITCH_CONTROL);
    MOD("gain|pitch|frequency|chorus", AAX_MIDI_GAIN_CONTROL|AAX_MIDI_PITCH_CONTROL|AAX_MIDI_FILTER_CONTROL|AAX_MIDI_CHORUS_CONTROL);

    return 0;
}
