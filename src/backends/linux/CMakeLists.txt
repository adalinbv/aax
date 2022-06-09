
set(LINUX_HEADERS
  alsa.h
  audio.h
  device.h
  kernel.h
  pipewire.h
)

set(LINUX_SOURCES
  asound.c
  kernel.c
  pulseaudio.c
  pipewire.c
)

set(SOURCES "")
foreach(src ${LINUX_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

