
CHECK_LIBRARY_EXISTS(m floorf "" HAVE_LIBM)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)


set (2DMATH_OBJS
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_cpu.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_sse2.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_sse3.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_sse_vex.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_avx.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_neon.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_vfpv2.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch2d_vfpv3.c
)

set (3DMATH_OBJS
    ${AeonWave_SOURCE_DIR}/base/geometry.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_sse3.c
    ${AeonWave_SOURCE_DIR}/src/software/cpu/arch3d_neon.c
)

FUNCTION(CREATE3D_TEST TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.c ${3DMATH_OBJS})
    target_link_libraries(${TEST_NAME} ${LIBAEONWAVE} ${EXTRA_LIBS} ${BASE_LIBRARY})
    if (ARM)
       set_target_properties(${TEST_NAME} PROPERTIES
            COMPILE_FLAGS "-march=native -mneon -mfpu=neon"
       )
    else()
       set_target_properties(${TEST_NAME} PROPERTIES
            COMPILE_FLAGS "-march=native -msse3 -mfpmath=sse"
       )
    endif()
ENDFUNCTION()

FUNCTION(CREATE2D_TEST TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.c ${2DMATH_OBJS})
    target_link_libraries(${TEST_NAME} ${LIBAEONWAVE} ${EXTRA_LIBS} ${BASE_LIBRARY})
    if (ARM)
       set_target_properties(${TEST_NAME} PROPERTIES
            COMPILE_FLAGS "-march=native -mfpu=neon"
       )
    else()
       set_target_properties(${TEST_NAME} PROPERTIES
            COMPILE_FLAGS "-march=native  -msse3 -mfpmath=sse"
       )
    endif()
ENDFUNCTION()



if (ARM)
  CREATE3D_TEST(test3dmath_neon)
else()
  CREATE3D_TEST(test3dmath_sse)
  CREATE2D_TEST(testarch2d_sse)
endif()

