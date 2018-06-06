
set(SOFTWARE_HEADERS
  device.h
)

set(SOFTWARE_SOURCES
  device.c
)

set(SOURCES "")
foreach(src ${SOFTWARE_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

