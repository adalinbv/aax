
set(COREAUDIO_HEADERS
  audio.h
  device.h
)

set(COREAUDIO_SOURCES
  device.c
)

set(SOURCES "")
foreach(src ${COREAUDIO_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

