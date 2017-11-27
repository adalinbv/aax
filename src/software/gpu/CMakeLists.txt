
set(GPU_HEADERS
  gpu.h
)

set(GPU_SOURCES
  opencl.c
)

set(SOURCES "")
foreach(src ${GPU_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

