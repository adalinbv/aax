
CHECK_LIBRARY_EXISTS(m floorf "" HAVE_LIBM)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)


if (ARM)
  set( SOURCE_3DMATH test3dmath_neon.c )
else()
  set( SOURCE_3DMATH test3dmath_sse.c )
endif()

set (SRC_3DMATH_OBJS
    ${AeonWave_SOURCE_DIR}/base/geometry.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse3.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_neon.c
)

add_executable(test3dmath ${SOURCE_3DMATH} ${SRC_3DMATH_OBJS})
if (ARM)
  set_target_properties(test3dmath PROPERTIES COMPILE_FLAGS "-mfpu=neon")
else()
  set_target_properties(test3dmath PROPERTIES COMPILE_FLAGS "-msse3")
endif()
target_link_libraries(test3dmath ${LIBAEONWAVE} ${EXTRA_LIBS} ${BASE_LIBRARY})
