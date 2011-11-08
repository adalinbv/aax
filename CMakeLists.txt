cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)


INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

PROJECT(aax C)

OPTION(DLOPEN "Check for the dlopen API for loading optional libs" ON)
OPTION(UTILS  "Build and install utility programs" ON)
OPTION(WERROR "Treat compile warnings as errors"   OFF)

IF(WIN32)
    SET(LIBAEONWAVE AAX32)
    ADD_DEFINITIONS("-D_WIN32")
ELSE()
    SET(LIBAEONWAVE aax)
ENDIF()

IF(NOT LIBTYPE)
    SET(LIBTYPE SHARED)
ENDIF()

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ version versionFile)
STRING(STRIP ${versionFile} AEONWAVE_VERSION)
SET(VERSION AEONWAVE_VERSION)

#  read 'description` file into a variable
file(STRINGS description descriptionFile)
STRING(REGEX REPLACE ";" " " descriptionFile "${descriptionFile}")

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AEONWAVE_VERSION})
SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
SET(CPACK_PACKAGE_NAME "AeonWave")
SET(CPACK_PACKAGE_CONTACT "tech@adalin.org")
SET(CPACK_PACKAGE_VENDOR "tech@adalin.org")
SET(CPACK_PACKAGE_DESCRIPTION "A cross platform, hardware accelerated, versatile stereo and 4D audio mixing library")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${descriptionFile})
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
SET(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")

# DEBIAN
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
ELSE ()
  SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
ENDIF ()
SET(CPACK_DEBIAN_PACKAGE_SECTION "sound")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1)")

# RPM
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
ELSE ()
  SET(CPACK_RPM_PACKAGE_ARCHITECTURE "i386")
ENDIF ()
SET(CPACK_RPM_PACKAGE_GROUP "System/Libraries")


SET(CPACK_SOURCE_GENERATOR TBZ2 ZIP)
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AEONWAVE_VERSION}" CACHE INTERNAL "tarball basename")
SET(CPACK_SOURCE_IGNORE_FILES
  "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}")

SET(CPACK_GENERATOR "DEB;RPM")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Adalin B.V.")

INCLUDE (CPack)

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# Required libraries
FIND_PACKAGE(AAX COMPONENTS aax REQUIRED)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}" include base "${CMAKE_BINARY_DIR}/include")

IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  SET(LIB_POSTFIX "64" CACHE STRING "suffix for 32/64 dir placement")
  MARK_AS_ADVANCED(LIB_POSTFIX)
ENDIF()
IF(NOT DEFINED LIB_POSTFIX)
  SET(LIB_POSTFIX "")
ENDIF()

ADD_DEFINITIONS(-Wall -DHAVE_CONFIG_H=1)
IF(WERROR)
  ADD_DEFINITIONS(-Werror)
ENDIF()

# Check for the dlopen API (for alGetProcAddress)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(assert.h HAVE_ASSERT_H)
CHECK_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
IF(HAVE_DLFCN_H)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
  IF(HAVE_LIBDL)
    SET(EXTRA_LIBS dl ${EXTRA_LIBS})
  ENDIF()
ENDIF()

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")


SET( AEONWAVE_OBJS
     src/alContext.c
     src/alSource.c
     src/alBuffer.c
     src/alListener.c
     src/alState.c
     src/aax_support.c
     src/api.c
   )
ADD_SUBDIRECTORY(base src)

# Build a library
SET(LIB_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
ADD_LIBRARY(${LIBAEONWAVE} ${LIBTYPE} ${LIBBASE} ${LIBAEONWAVE_INT})
SET_TARGET_PROPERTIES(${LIBAEONWAVE} PROPERTIES DEFINE_SYMBOL AL_BUILD_LIBRARY
                                       COMPILE_FLAGS -DAL_ALEXT_PROTOTYPES
                                       VERSION ${LIB_VERSION}
                                       SOVERSION ${CPACK_PACKAGE_VERSION_MAJOR})
IF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
    SET_TARGET_PROPERTIES(${LIBAEONWAVE} PROPERTIES PREFIX "")
ENDIF()

TARGET_LINK_LIBRARIES(${LIBAEONWAVE} ${EXTRA_LIBS} ${AAX_LIBRARY})

# Add an install target here
INSTALL(TARGETS ${LIBAEONWAVE}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION "lib${LIB_POSTFIX}"
        ARCHIVE DESTINATION "lib${LIB_POSTFIX}"
)
INSTALL(FILES include/AL/al.h
              include/AL/alc.h
              include/AL/alext.h
        DESTINATION include/AL
)

IF(UTILS)
  ADD_SUBDIRECTORY(test)
ENDIF()

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
