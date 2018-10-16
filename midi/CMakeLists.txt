
set( MIDI_OBJS
     midi.cpp
   )

set(LIBTYPE STATIC)

set(LIBMIDI midi)
ADD_LIBRARY(${LIBMIDI} ${LIBTYPE} ${MIDI_OBJS})

