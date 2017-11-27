
set(DSP_HEADERS
  common.h
  effects.h
  filters.h
)

set(DSP_SOURCES
  common.c
  filters.c
  effects.c
  effect_chorus.c
  effect_distortion.c
  effect_dynamic_pitch.c
  effect_flanging.c
  effect_phasing.c
  effect_pitch.c
  effect_reverb.c
  effect_convolution.c
  effect_timed_pitch.c
  effect_velocity.c
  filter_angular.c
  filter_compressor.c
  filter_distance.c
  filter_dynamic_gain.c
  filter_equalizer.c
  filter_frequency.c
  filter_graphic_equalizer.c
  filter_timed_gain.c
  filter_volume.c
)

set(SOURCES "")
foreach(src ${DSP_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()
