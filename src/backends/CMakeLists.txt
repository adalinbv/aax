set(SRC_HEADERS
  devices.h
  driver.h
)

set(SRC_SOURCES
  devices.c
)

set(SOURCES "")
foreach(src ${SRC_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()


aax_subdirectory(software)
aax_subdirectory(linux)
if(HAVE_WINDOWS_H)
  aax_subdirectory(windows)
endif(HAVE_WINDOWS_H)
aax_subdirectory(oss)
aax_subdirectory(android)
aax_subdirectory(sdl)
