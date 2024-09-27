
set(EXTRA_LIBS ${XML_LIBRARY} ${EXTRA_LIBS})

#CHECK_LIBRARY_EXISTS(m sqrtf "" HAVE_LIBM)
FIND_LIBRARY(HAVE_LIBM NAMES math m libm)
if(HAVE_LIBM)
  set(EXTRA_LIBS m ${EXTRA_LIBS})
endif(HAVE_LIBM)

FUNCTION(CREATE_CPP_TEST TEST_NAME)
  set_property(SOURCE ${TEST_NAME} APPEND PROPERTY COMPILE_FLAGS "-std=c++11")
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
  target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
  add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

FUNCTION(CREATE_TEST TEST_NAME)
  add_executable(${TEST_NAME} ${TEST_NAME}.c)
  target_link_libraries(${TEST_NAME} ${LIBAEONWAVE}-static ${EXTRA_LIBS})
  add_test(${TEST_NAME} ${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME})
ENDFUNCTION()

CREATE_CPP_TEST(testtieing)
CREATE_CPP_TEST(teststrings)
CREATE_CPP_TEST(testposition)
CREATE_CPP_TEST(testparam)

CREATE_TEST(test3dmath)
CREATE_TEST(testarch2d)
CREATE_TEST(testregistering)
CREATE_TEST(testlimiter)
CREATE_TEST(testrandom)
#CREATE_TEST(testfrequencyfilter)
if(GCC AND NOT ARM)
  CREATE_TEST(testfloat80)
endif()
CREATE_TEST(testfrequencyfilter)
CREATE_TEST(testdelayeffect)
CREATE_TEST(testnoise_ema_frequencyfilter)
CREATE_TEST(testmemory)
CREATE_TEST(testtypes)
CREATE_TEST(testdelay)
CREATE_TEST(testaaxs_fuzz)
CREATE_TEST(testdsp)
CREATE_TEST(testlfo)
CREATE_TEST(testurl)
CREATE_TEST(testc11threads)

CREATE_TEST(testmatrix)
message("SSE2: ${SSE2_FOUND}, AVX: ${AVX_FOUND}")
if(AVX_FOUND)
  set_property(SOURCE matrix_test.c APPEND
               PROPERTY COMPILE_FLAGS "-mavx -march=sandybridge"
  )
endif()
if(NOT X86_64 AND SSE2_FOUND)
  set_property(SOURCE matrix_test.c APPEND
               PROPERTY COMPILE_FLAGS "-msse2 -mfpmath=sse -march=pentium-m"
  )
endif()
if(ARM64)
  set_property(SOURCE matrix_test.c APPEND
                PROPERTY COMPILE_FLAGS "-march=armv8-a -funsafe-math-optimizations"
  )
elseif(ARM)
  set_property(SOURCE matrix_test.c APPEND
                PROPERTY COMPILE_FLAGS "-march=armv7-a -mfpu=neon -funsafe-math-optimizations"
  )
endif()
