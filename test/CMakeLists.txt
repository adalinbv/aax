
CHECK_LIBRARY_EXISTS(m floorf "" HAVE_LIBM)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)


set( SOURCES
     test3dmath.c
     ${AeonWave_SOURCE_DIR}/base/geometry.c
     ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse.c
     ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse3.c
)

add_executable(test3dmath ${SOURCES})
set_target_properties(test3dmath PROPERTIES COMPILE_FLAGS "-msse3")
target_link_libraries(test3dmath ${LIBAEONWAVE} ${EXTRA_LIBS} ${BASE_LIBRARY})
