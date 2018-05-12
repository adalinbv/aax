
set(OSS_HEADERS
  audio.h
  device.h
)

set(OSS_SOURCES
  device.c
)

set(SOURCES "")
foreach(src ${OSS_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

