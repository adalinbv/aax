
set(SDL_HEADERS
  audio.h
  device.h
)

set(SDL_SOURCES
  device.c
)

set(SOURCES "")
foreach(src ${SDL_SOURCES})
  set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${src};${SOURCES}")
endforeach()

