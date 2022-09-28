
set(BASE_HEADERS
  databuffer.h
  buffers.h
  dlsym.h
  geometry.h
  gmath.h
  logging.h
  memory.h
  threads.h
  timer.h
  types.h
  random.h
)

set(BASE_SOURCES
  databuffer.c
  buffers.c
  dlsym.c
  geometry.c
  gmath.c
  logging.c
  memory.c
  threads.c
  timer.c
  types.c
  random.c
)

set(SOURCES "")
foreach(src ${BASE_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()
