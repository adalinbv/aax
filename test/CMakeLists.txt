
set(EXTRA_LIBS ${XML_LIBRARY} ${EXTRA_LIBS})

#CHECK_LIBRARY_EXISTS(m sqrtf "" HAVE_LIBM)
FIND_LIBRARY(HAVE_LIBM NAMES math m libm)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)

FUNCTION(CREATE_TEST TEST_NAME)
    if(ARM)
        SET_PROPERTY(SOURCE ${TEST_NAME}.c
                     PROPERTY COMPILE_FLAGS "-march=armv5 -marm -static-libgcc -Wno-inline"
        )
    elseif(X86_64)
        SET_PROPERTY(SOURCE ${TEST_NAME}.c
                     PROPERTY COMPILE_FLAGS "-march=athlon64 -static-libgcc -Wno-inline"
        )
    else()
        SET_PROPERTY(SOURCE ${TEST_NAME}.c
                     PROPERTY COMPILE_FLAGS "-march=i686 -static-libgcc -Wno-inline"
        )
    endif()

    add_executable(${TEST_NAME} ${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
    add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

if (ARM)
  CREATE_TEST(test3dmath_neon)
else()
  CREATE_TEST(test3dmath_sse)
  CREATE_TEST(testarch2d_sse)
endif()
CREATE_TEST(testmemory)
