
set(BASE_HEADERS
  MurmurHash3.h
  geometry.h
  logging.h
  timer.h
  types.h
)

set(BASE_OBJS
  MurmurHash3.c
  types.c
  timer.c
)

set(LIBTYPE STATIC)

set(LIBBASE base)
ADD_LIBRARY(${LIBBASE} ${LIBTYPE} ${BASE_OBJS})
