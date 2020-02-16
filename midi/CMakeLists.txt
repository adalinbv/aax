set(SRC_HEADERS
  aax_midi.hpp
)

set(SRC_SOURCES
  aax_midi.cpp
  midi.cpp
)

set(SOURCES "")
foreach(src ${SRC_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

