cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)


INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

PROJECT(aax C)
SET(PACKAGE "AeonWave")

OPTION(DLOPEN "Check for the dlopen API for loading optional libs" ON)
OPTION(WERROR "Treat compile warnings as errors"   OFF)

IF(WIN32)
    SET(LIBAEONWAVE AAX32)
    ADD_DEFINITIONS("-D_WIN32")
ELSE(WIN32)
    SET(LIBAEONWAVE aax)
ENDIF(WIN32)

IF(NOT LIBTYPE)
    SET(LIBTYPE SHARED)
ENDIF(NOT LIBTYPE)

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ version versionFile)
STRING(STRIP ${versionFile} AAX_VERSION)
SET(VERSION AAX_VERSION)

#  read 'description` file into a variable
file(STRINGS description descriptionFile)
STRING(REGEX REPLACE ";" " " descriptionFile "${descriptionFile}")

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAX_VERSION})
SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
SET(CPACK_PACKAGE_NAME ${PACKAGE})
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
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAX_VERSION}" CACHE INTERNAL "tarball basename")
SET(CPACK_SOURCE_IGNORE_FILES
  "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}")

SET(CPACK_GENERATOR "DEB;RPM")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Adalin B.V.")

INCLUDE (CPack)

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# Required libraries
#FIND_PACKAGE(AAX COMPONENTS aax REQUIRED)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}" include base "${CMAKE_BINARY_DIR}/include")

#CHECK_C_SOURCE_COMPILES("int *restrict foo;
#                         int main() {return 0;}" HAVE_RESTRICT)
#CHECK_C_SOURCE_COMPILES("int *__restrict foo;
#                         int main() {return 0;}" HAVE___RESTRICT)

IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  SET(LIB_POSTFIX "64" CACHE STRING "suffix for 32/64 dir placement")
  MARK_AS_ADVANCED(LIB_POSTFIX)
ENDIF()
IF(NOT DEFINED LIB_POSTFIX)
  SET(LIB_POSTFIX "")
ENDIF()

ADD_DEFINITIONS(-DHAVE_CONFIG_H=1)
IF(WERROR)
  ADD_DEFINITIONS(-Werror)
ENDIF()

IF(MSVC)
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -D_DEBUG /LD")
    SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG /LD")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG /LD")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG /LD")
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)

    OPTION(FORCE_STATIC_VCRT "Force /MT for static VC runtimes" OFF)
    IF(FORCE_STATIC_VCRT)
        FOREACH(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            IF(${flag_var} MATCHES "/MD")
                STRING(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            ENDIF()
        ENDFOREACH(flag_var)
    ENDIF()
ELSE(MSVC)
    ADD_DEFINITIONS(-Wall)
    SET(C_FLAGS "${C_FLAGS} -march=i386 -Werror=format-security" CACHE STRING
        "Flags used by al build types."
        FORCE)
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${C_FLAGS} -g -O2 -D_DEBUG" CACHE STRING
        "Flags used by the compiler during Release with Debug Info builds."
        FORCE)
    SET(CMAKE_C_FLAGS_MINSIZEREL "${C_FLAGS} -Os -DNDEBUG" CACHE STRING
        "Flags used by the compiler during release minsize builds."
        FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} -Os -fomit-frame-pointer -DNDEBUG"
        CACHE STRING "Flags used by the compiler during release builds"
        FORCE)
    SET(CMAKE_C_FLAGS_DEBUG "${C_FLAGS} -g3 -D_DEBUG" CACHE STRING
        "Flags used by the compiler during debug builds."
        FORCE)
ENDIF(MSVC)

# Check for the dlopen API (for alGetProcAddress)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
CHECK_INCLUDE_FILE(assert.h HAVE_ASSERT_H)
CHECK_INCLUDE_FILE(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(values.h HAVE_VALUES_H)
CHECK_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
CHECK_INCLUDE_FILE(libio.h HAVE_LIBIO_H)
CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)
CHECK_INCLUDE_FILE(math.h HAVE_MATH_H)
IF(HAVE_DLFCN_H)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
  IF(HAVE_LIBDL)
    SET(EXTRA_LIBS dl ${EXTRA_LIBS})
  ENDIF()
ENDIF()

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/aax.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/aax.h")

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")

IF(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -Os -fPIC -fomit-frame-pointer -funroll-loops"
  )
  SET(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -shared -march=i586 -std=c99 -Wl,--hash-style=both -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=199309L -fvisibility=hidden"
     )
ENDIF(CMAKE_COMPILER_IS_GNUCC)

SET(BASE_OBJS
     base/buffers.c
     base/dlsym.c
     base/geometry.c
     base/gmath.c
     base/logging.c
     base/memory.c
     base/threads.c
     base/types.c
   )

SET( AEONWAVE_OBJS
     src/aax_audioframe.c
     src/aax_buffer.c
     src/aax_driver.c
     src/aax_effects.c
     src/aax_emitter.c
     src/aax_filters.c
     src/aax_instrument.c
     src/aax_matrix.c
     src/aax_mixer.c
     src/aax_scenery.c
     src/aax_sensor.c
     src/aax_support.c
     src/aax_waveforms.c
     src/devices.c
     src/objects.c
     src/alsa/device.c
     src/dmedia/device.c
     src/mmdevapi/device.c
     src/oss/device.c
     src/software/arch.c
     src/software/arch_cpu.c
     src/software/arch_neon.c
     src/software/arch_sse2.c
     src/software/arch_sse.c
     src/software/device.c
     src/software/mixer.c
     src/software/null_device.c
     src/software/rbuf_codecs.c
     src/software/rbuf_codec_tables.c
     src/software/rbuf_effects.c
     src/software/rbuf_mixers.c
     src/software/rbuf_mixer_tables.c
     src/software/rbufmix_multi.c
     src/software/rbufmix_single.c
     src/software/ringbuffer.c
   )
ADD_SUBDIRECTORY(base src)

# Build a library
SET(LIB_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
ADD_LIBRARY(${LIBAEONWAVE} ${LIBTYPE} ${BASE_OBJS} ${AEONWAVE_OBJS})
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
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/aax.h
              include/aaxdefs.h
        DESTINATION include
)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
