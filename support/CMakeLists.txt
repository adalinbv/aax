set(SRC_HEADERS
  copyright.h
)

set(SRC_SOURCES
  aax_matrix.c
  aax_support.c
)

set(SOURCES "")
foreach(src ${SRC_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()
