
SET(LIBDSP dsp)

SET(DSP_OBJS
    filter_angular.c
    filter_compressor.c
    filter_distance.c
    filter_dynamic_gain.c
    filter_equalizer.c
    filter_frequency.c
    filter_graphic_equalizer.c
    filter_volume.c
    effect_chorus.c
    effect_distortion.c
    effect_dynamic_pitch.c
    effect_flanging.c
    effect_phasing.c
    effect_pitch.c
    effect_reverb.c
    effect_timed_pitch.c
    effect_velocity.c
   )   

ADD_LIBRARY(${LIBDSP} STATIC ${DSP_OBJS})

IF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
    SET_TARGET_PROPERTIES(${DSP_LIBRARY} PROPERTIES PREFIX "")
ENDIF()

