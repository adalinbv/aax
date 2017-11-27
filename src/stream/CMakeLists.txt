
set(STREAM_HEADERS
  audio.h
  device.h
  extension.h
  ext_ogg.h
  fmt_flac.h
  fmt_mp3.h
  fmt_opus.h
  fmt_vorbis.h
  format.h
  io.h
  protocol.h
)

set(STREAM_SOURCES
  device.c
  extension.c
  ext_ogg.c
  ext_raw.c
  ext_wav.c
  fmt_binary.c
  fmt_flac.c
  fmt_mp3.c
  fmt_opus.c
  fmt_pcm.c
  fmt_vorbis.c
  format.c
  io.c
  io_file.c
  io_socket.c
  prot_direct.c
  prot_http.c
  protocol.c
)

set(SOURCES "")
foreach(src ${STREAM_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

