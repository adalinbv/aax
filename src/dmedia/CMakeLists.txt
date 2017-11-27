
set(DMEDIA_HEADERS
  audio.h
  device.h
)

set(DMEDIA_SOURCES
  device.c
)

set(SOURCES "")
foreach(src ${DMEDIA_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

