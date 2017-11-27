
set(SOFTWARE_HEADERS
  audio.h
  device.h
  renderer.h
)

set(SOFTWARE_SOURCES
  arch_arm.c
  arch.c
  arch_x86.c
  device.c
  emitter.c
  frame.c
  mixer.c
  rbuf2d_mixsingle.c
  rbuf3d_mixsingle.c
  rbuf_codecs.c
  rbuf_codec_tables.c
  rbuf_int.h
  rbuf_lfo.c
  rbuf_mixers.c
  rbuf_mixmulti.c
  renderer.c
  ringbuffer.c
  sensor.c
)

set(SOURCES "")
foreach(src ${SOFTWARE_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES};")
endforeach()

aax_subdirectory(cpu)
aax_subdirectory(gpu)
