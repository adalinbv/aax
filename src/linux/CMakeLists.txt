
set(LINUX_HEADERS
  audio.h
  device.h
  kernel.h
)

set(LINUX_SOURCES
  asound.c
  kernel.c
  pulseaudio.c
)

set(SOURCES "")
foreach(src ${LINUX_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

