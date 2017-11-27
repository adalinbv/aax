
set(ANDROID_HEADERS
  audio.h
  opensl.h
)

set(ANDROID_SOURCES
  opensl.c
)

set(SOURCES "")
foreach(src ${ANDROID_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

