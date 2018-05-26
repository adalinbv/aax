
set(THIRDPARTY_HEADERS
  minimp3.h
  dr_flac.h
  pffft.h
)

set(THIRDPARTY_SOURCES
  pdmp3.c
  pffft.c
  stb_vorbis.c
)

set(SOURCES "")
foreach(src ${THIRDPARTY_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

