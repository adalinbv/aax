

SET(BASE_LIBRARY base)
SET (LIBTYPE STATIC)

SET(BASE_OBJS
     buffers.c
     dlsym.c
     geometry.c
     gmath.c
     logging.c
     memory.c
     threads.c
     types.c
   )

ADD_LIBRARY(${BASE_LIBRARY} ${LIBTYPE} ${BASE_OBJS})

IF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
    SET_TARGET_PROPERTIES(${BASE_LIBRARY} PROPERTIES PREFIX "")
ENDIF()

