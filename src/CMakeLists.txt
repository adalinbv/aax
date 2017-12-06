
set(SRC_HEADERS
  analyze.h
  api.h
  arch.h
  copyright.h
  devices.h
  driver.h
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
  aax_matrix.c
  aax_mixer.c
  aax_scenery.c
  aax_sensor.c
  aax_support.c
  analyze.c
  api.c
  arch.c
  devices.c
  objects.c
)
# aax_eventmgr.c
# aax_instrument.c
# aax_midi.c

set(SOURCES "")
foreach(src ${SRC_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

aax_subdirectory(dmedia)
aax_subdirectory(android)
aax_subdirectory(oss)
aax_subdirectory(linux)
if(HAVE_WINDOWS_H)
  aax_subdirectory(windows)
endif(HAVE_WINDOWS_H)
aax_subdirectory(software)
aax_subdirectory(stream)
aax_subdirectory(dsp)

