
set(THIRDPARTY_HEADERS
  MurmurHash3.h
  minimp3.h
  dr_flac.h
)

set(THIRDPARTY_SOURCES
  MurmurHash3.c
  pdmp3.c
  stb_vorbis.c
)

set(SOURCES "")
foreach(src ${THIRDPARTY_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

