
SET( SOURCES
     test3dmath.c
)
#     ${AeonWave_SOURCE_DIR}/base/geometry.c
#     ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse.c
#     ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse3.c

ADD_EXECUTABLE(test3dmath
               ${SOURCES}
)
