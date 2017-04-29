
set(EXTRA_LIBS ${XML_LIBRARY} ${EXTRA_LIBS})

CHECK_LIBRARY_EXISTS(m floorf "" HAVE_LIBM)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)

FUNCTION(CREATE2D_TEST TEST_NAME)
    if(X86_64)
        SET_PROPERTY(SOURCE ${TEST_NAME}.c
                     PROPERTY COMPILE_FLAGS "-march=athlon64 -static-libgcc"
        )
    else()
        SET_PROPERTY(SOURCE ${TEST_NAME}.c APPEND
                     PROPERTY COMPILE_FLAGS "-march=i686 -static-libgcc"
        )
    endif()

    add_executable(${TEST_NAME} ${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
    add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

FUNCTION(CREATE3D_TEST TEST_NAME)
    if(X86_64)
        SET_PROPERTY(SOURCE ${TEST_NAME}.c
                     PROPERTY COMPILE_FLAGS "-march=athlon64 -static-libgcc"
        )
    else()
        SET_PROPERTY(SOURCE ${TEST_NAME}.c APPEND
                     PROPERTY COMPILE_FLAGS "-march=i686 -static-libgcc"
        )
    endif()

    add_executable(${TEST_NAME} ${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
    add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

if (ARM)
  CREATE3D_TEST(test3dmath_neon)
else()
  CREATE3D_TEST(test3dmath_sse)
  CREATE2D_TEST(testarch2d_sse)
endif()

