
set(DSP_HEADERS
  lfo.h
  common.h
  effects.h
  filters.h
)

set(DSP_SOURCES
  lfo.c
  common.c
  filters.c
  effects.c
  effect_pitch.c
  effect_timed_pitch.c
  effect_dynamic_pitch.c
  effect_delay.c
  effect_chorus.c
  effect_distortion.c
  effect_reverb.c
  effect_convolution.c
  effect_ringmodulate.c
  effect_velocity.c
  filter_directional.c
  filter_compressor.c
  filter_distance.c
  filter_dynamic_gain.c
  filter_equalizer.c
  filter_frequency.c
  filter_dynamic_layer.c
  filter_graphic_equalizer.c
  filter_timed_gain.c
  filter_bitcrusher.c
  filter_volume.c
)

set(SOURCES "")
foreach(src ${DSP_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()
