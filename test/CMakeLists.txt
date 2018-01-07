
set(EXTRA_LIBS ${XML_LIBRARY} ${EXTRA_LIBS})

#CHECK_LIBRARY_EXISTS(m sqrtf "" HAVE_LIBM)
FIND_LIBRARY(HAVE_LIBM NAMES math m libm)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)

FUNCTION(CREATE_TEST TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.c)
  target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
  add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

CREATE_TEST(test3dmath)
CREATE_TEST(testarch2d)
CREATE_TEST(testmemory)
CREATE_TEST(testtypes)
CREATE_TEST(testurl)

CREATE_TEST(matrix_test)
if(X86_64)
  set_property(SOURCE matrix_test.c APPEND
               PROPERTY COMPILE_FLAGS "-mavx -march=sandybridge"
  )
elseif(X86)
  set_property(SOURCE matrix_test.c APPEND
               PROPERTY COMPILE_FLAGS "-msse2 -mfpmath=sse -march=pentium-m"
  )
elseif(ARM)
   set_property(SOURCE matrix_test.c APPEND
                PROPERTY COMPILE_FLAGS "-march=armv7-a -mthumb -mfpu=neon -funsafe-math-optimizations"
  )
endif()
