
SET( DRIVER_OBJS
     driver.c
     driver_internal.c
     wavfile.c
   )

SET(LIBDRIVER driver)
SET(LIBTYPE STATIC)
ADD_LIBRARY(${LIBDRIVER} ${LIBTYPE} ${DRIVER_OBJS})

FUNCTION(CREATE_TEST TEST_NAME)
    ADD_EXECUTABLE(${TEST_NAME} ${TEST_NAME}.c)
    TARGET_LINK_LIBRARIES(${TEST_NAME} ${LIBOPENAL} ${LIBDRIVER})
    SET_TARGET_PROPERTIES(${TEST_NAME} PROPERTIES
            COMPILE_DEFINITIONS  "SRC_PATH=\"${PROJECT_SOURCE_DIR}/../sounds\"")
ENDFUNCTION()


CREATE_TEST(aaxinfo)
INSTALL(TARGETS aaxinfo
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION "lib${LIB_SUFFIX}"
          ARCHIVE DESTINATION "lib${LIB_SUFFIX}"
       )

CREATE_TEST(testcapture)
CREATE_TEST(testcodec)
CREATE_TEST(testcone)
CREATE_TEST(testdistance)
CREATE_TEST(testdistortion)
CREATE_TEST(testenvelope)
CREATE_TEST(testfileio)
CREATE_TEST(testformats)
CREATE_TEST(testframe_detached)
CREATE_TEST(testinstrument)
CREATE_TEST(testleftright)
CREATE_TEST(testloop)
CREATE_TEST(testlowpass)
CREATE_TEST(testlst_rotation)
CREATE_TEST(testmono3d)
CREATE_TEST(testmono3d_frame)
CREATE_TEST(testmono3d_oneshot)
CREATE_TEST(testphasing)
CREATE_TEST(testposition)
CREATE_TEST(testringbuf)
CREATE_TEST(teststereo)
CREATE_TEST(teststereo_equalizer)
CREATE_TEST(teststereo_frame)
CREATE_TEST(teststereo_grapheq)
CREATE_TEST(teststream)
CREATE_TEST(teststream_phasing)
CREATE_TEST(testwaves)
CREATE_TEST(testxml)
