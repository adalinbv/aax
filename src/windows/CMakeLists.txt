
set(WINDOWS_HEADERS
  audio.h
  wasapi.h
)

set(WINDOWS_SOURCES
  audio.c
  wasapi.c
)

set(SOURCES "")
foreach(src ${WINDOWS_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

