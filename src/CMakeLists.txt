
set(SRC_HEADERS
  api.h
  arch.h
  objects.h
  ringbuffer.h
)

set(SRC_SOURCES
  aax_audioframe.c
  aax_buffer.c
  aax_driver.c
  aax_effects.c
  aax_emitter.c
  aax_filters.c
  aax_mixer.c
  aax_scenery.c
  aax_sensor.c
  api.c
  objects.c
)

set(SOURCES "")
foreach(src ${SRC_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

aax_subdirectory(dsp)
aax_subdirectory(stream)
aax_subdirectory(software)
aax_subdirectory(backends)
