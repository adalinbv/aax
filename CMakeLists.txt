cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

SET(CMAKE_MAKEFILE_DEPENDS
    "CMakeModules/Symlink.cmake"
)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckFunctionExists)

PROJECT(AeonWave C)
SET(PACKAGE_NAME "AeonWave")
SET(PACKAGE "libaax2")

OPTION(LTO "Enabel linkt-time optimizations" OFF)
OPTION(DLOPEN "Check for the dlopen API for loading optional libs" ON)
OPTION(WERROR "Treat compile warnings as errors"   OFF)
OPTION(RMALLOC "Enable memory debugging functions" OFF)

ADD_DEFINITIONS(-D_XOPEN_SOURCE=600)
IF(WIN32)
  SET(EXTRA_LIBS Winmm ${EXTRA_LIBS})
  ADD_DEFINITIONS(-DAAX_BUILD_LIBRARY=1)
  ADD_DEFINITIONS(-D_WIN32 -DWIN32)
  SET(LIBAEONWAVE AAX32)
ELSE(WIN32)
  ADD_DEFINITIONS(-D_GNU_SOURCE=1)
  SET(LIBAEONWAVE aax)
ENDIF(WIN32)

IF(NOT LIBTYPE)
  SET(LIBTYPE SHARED)
ENDIF(NOT LIBTYPE)

MACRO (TODAY RESULT)
  IF (WIN32)
    EXECUTE_PROCESS(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "... (..)/(..)/..(..).*" "\\3\\1\\2" ${RESULT} ${${RESULT}})
  ELSEIF(UNIX)
    EXECUTE_PROCESS(COMMAND "date" "+%d/%m/%Y" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "(..)/(..)/..(..).*" "\\3\\2\\1" ${RESULT} ${${RESULT}})
  ELSE (WIN32)
    MESSAGE(SEND_ERROR "date not implemented")
    SET(${RESULT} 000000)
  ENDIF (WIN32)
ENDMACRO (TODAY)

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(SIZEOF_SIZE_T 8)
ELSE ()
  SET(SIZEOF_SIZE_T 4)
ENDIF ()

# read 'version' file into a variable (stripping any newlines or spaces)
TODAY(DATE)
FILE(READ version versionFile)
STRING(STRIP ${versionFile} AAX_VERSION)
SET(VERSION AAX_VERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAX_VERSION})

SET(AAX_MAJOR_VERSION ${CMAKE_MATCH_1})
SET(AAX_MINOR_VERSION ${CMAKE_MATCH_2})
SET(AAX_MICRO_VERSION ${CMAKE_MATCH_3})
SET(AAX_PATCH_LEVEL ${DATE})

SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross platform, hardware accelerated, multi-channel and 4D capable audio rendering engine")

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${AeonWave_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")
INCLUDE(CPack_Modules)
INCLUDE(CPack_Defaults)
INCLUDE(inline)

IF (WIN32)
  SET(CPACK_NSIS_MODIFY_PATH ON)

ELSE(WIN32)

  # DEBIAN
  SET(CPACK_DEBIAN_PACKAGE_SECTION "libs")
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1), debconf")
  SET(CPACK_DEBIAN_PACKAGE_RECOMMENDS "alsa-base | oss4-base")
  SET(CPACK_DEBIAN_PACKAGE_SUGGESTS "libmpg123-0")
  SET(CPACK_DEBIAN_PACKAGE_REPLACES "aeonwave, libaax2, libaax2-bin, libaax2-dev")
  SET(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${AeonWave_SOURCE_DIR}/admin/debian/config;${AeonWave_SOURCE_DIR}/admin/debian/postinst;${AeonWave_SOURCE_DIR}/admin/debian/postrm;${AeonWave_SOURCE_DIR}/admin/debian/templates;")
  SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# SET(CPACK_DEBIAN_PACKAGE_DEPENDS "debconf" COMPONENT Headers)
# SET(CPACK_DEBIAN_PACKAGE_SECTION "libdevel" COMPONENT Headers)
# SET(CPACK_DEBIAN_PACKAGE_REPLACES "aeonwave, libaax2, libaax2-dev" COMPONENT Headers)

  # RPM
  SET(CPACK_RPM_PACKAGE_LICENSE "License For Customer Use of ADALIN AEONWAVE AUDIO EXTENSIONS LIBRARY Software")
  SET(CPACK_RPM_PACKAGE_GROUP "System/Libraries")
  SET(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${AeonWave_SOURCE_DIR}/admin/rpm/aeonwave.postun")
  SET(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${AeonWave_SOURCE_DIR}/admin/rpm/aeonwave.preinst")
  SET(CPACK_RPM_PACKAGE_PROVIDES "libaax2")
ENDIF(WIN32)

# ZIP
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAX_VERSION}" CACHE INTERNAL "tarball basename")

# Required libraries
FIND_PACKAGE(XML REQUIRED)
IF(NOT XML_FOUND)
  MESSAGE(FATAL_ERROR "ERROR: required XML library is not installed, aborting.")
ENDIF(NOT XML_FOUND)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${AeonWave_SOURCE_DIR}/src"
    ${AeonWave_SOURCE_DIR}
    ${XML_INCLUDE_DIR}
    include
    base
    ${AeonWave_BINARY_DIR}/include
)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

ADD_DEFINITIONS(-DHAVE_CONFIG_H=1)
IF(WERROR)
  ADD_DEFINITIONS(-Werror)
ENDIF()

IF(MSVC)
    SET(C_FLAGS "${C_FLAGS} /NODEFAULTLIB:msvcrt /NODEFAULTLIB:msvcrtd /NODEFAULTLIB:libcmt")
    SET_PROPERTY(SOURCE src/software/arch_sse.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE /fp:fast -D__SSE__"
    )
    SET_PROPERTY(SOURCE src/software/arch_sse2.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE2__"
    )
    SET_PROPERTY(SOURCE src/software/arch_sse3.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE3__"
    )
    SET_PROPERTY(SOURCE src/software/arch_sse4.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE4__"
    )

    # don't use a /O[012x] flag if you want custom optimizations
    SET(SPEED_OPTIMIZATIONS "/Ob2 /Oi /Ot /Oy /GT /GF /GS-")
    SET(SIZE_OPTIMIZATIONS "/O1")
    # without inlining it's unusable, use custom optimizations again
    SET(MIN_OPTIMIZATIONS "/Ob1")

    SET(CMAKE_C_FLAGS_DEBUG "/LDd /MDd /Zi /D_DEBUG ${MIN_OPTIMIZATIONS}")
    SET(CMAKE_C_FLAGS_RELEASE "/LD /MD /Zi /D NDEBUG ${SPEED_OPTIMIZATIONS}")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/LD /MD /Zi /DNDEBUG ${SPEED_OPTIMIZATIONS}")
    SET(CMAKE_C_FLAGS_MINSIZEREL "/LD /MD /DNDEBUG ${SIZE_OPTIMIZATIONS}")

    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)

    OPTION(FORCE_STATIC_VCRT "Force /MT for static VC runtimes" ON)
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
    IF(LTO)
       SET(C_FLAGS "${C_FLAGS} -flto")
    ENDIF(LTO)

    IF(NOT MINGW)
       SET(C_FLAGS "${C_FLAGS} -fPIC -Wl,--hash-style=both")
    ELSE(NOT MINGW)
       INCLUDE_DIRECTORIES ("${AeonWave_SOURCE_DIR}/src/windows/mingw")
       ADD_DEFINITIONS(-U__STRICT_ANSI__)
       SET(C_FLAGS "${C_FLAGS} -static-libgcc")
       FOREACH(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            IF(${flag_var} MATCHES "-mthreads")
                STRING(REGEX REPLACE "-mthreads" "" ${flag_var} "${${flag_var}}")
            ENDIF()
        ENDFOREACH(flag_var)
    ENDIF(NOT MINGW)

    IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET(C_FLAGS "${C_FLAGS} -msse2 -mfpmath=sse")
      SET_PROPERTY(SOURCE src/software/arch_sse3.c APPEND
                   PROPERTY COMPILE_FLAGS  "-msse3 -mfpmath=sse"
      )
      SET_PROPERTY(SOURCE src/software/arch_sse4.c APPEND
                   PROPERTY COMPILE_FLAGS  "-msse4 -mfpmath=sse"
      )
    ELSE(CMAKE_SIZEOF_VOID_P EQUAL 8)
      SET(C_FLAGS "${C_FLAGS} -march=i586")
      SET_PROPERTY(SOURCE src/software/arch_sse.c APPEND 
                   PROPERTY COMPILE_FLAGS  "-msse -mfpmath=sse -march=pentium3m"
      )
      SET_PROPERTY(SOURCE src/software/arch_sse2.c APPEND 
                   PROPERTY COMPILE_FLAGS  "-msse2 -mfpmath=sse -march=pentium-m"
      )
      SET_PROPERTY(SOURCE src/software/arch_sse3.c APPEND 
                   PROPERTY COMPILE_FLAGS  "-msse3 -mfpmath=sse -march=prescott"
      )
      SET_PROPERTY(SOURCE src/software/arch_sse4.c APPEND 
                   PROPERTY COMPILE_FLAGS  "-msse4 -mfpmath=sse -march=corei7"
      )
    ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 8)

    SET(C_FLAGS "${C_FLAGS} -Werror=format-security -std=c99 -Wall -fvisibility=hidden -shared -funroll-loops -fomit-frame-pointer" CACHE STRING
        "Flags used by al build types."
        FORCE)
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${C_FLAGS} -g -O2 -D_DEBUG" CACHE STRING
        "Flags used by the compiler during Release with Debug Info builds."
        FORCE)
    SET(CMAKE_C_FLAGS_MINSIZEREL "${C_FLAGS} -Os -s -DNDEBUG" CACHE STRING
        "Flags used by the compiler during release minsize builds."
        FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} -Os -s -fomit-frame-pointer -DNDEBUG"
        CACHE STRING "Flags used by the compiler during release builds"
        FORCE)
    SET(CMAKE_C_FLAGS_DEBUG "${C_FLAGS} -g3 -D_DEBUG" CACHE STRING
        "Flags used by the compiler during debug builds."
        FORCE)
ENDIF(MSVC)

# Check for the dlopen API (for alGetProcAddress)
CHECK_INCLUDE_FILE(windows.h HAVE_WINDOWS_H -D_WIN32_WINNT=0x0501)
CHECK_FUNCTION_EXISTS(strdup HAVE_STRDUP)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/utsname.h HAVE_SYS_UTSNAME_H)
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
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/ioctl.h HAVE_IOCTL_H)
CHECK_INCLUDE_FILE(math.h HAVE_MATH_H)
CHECK_INCLUDE_FILE(io.h HAVE_IO_H)
CHECK_INCLUDE_FILE(guiddef.h HAVE_GUIDDEF_H)
CHECK_INCLUDE_FILE(initguid.h HAVE_INITGUID_H)
CHECK_INCLUDE_FILE(mmdeviceapi.h HAVE_MMDEVICEAPI_H)

IF(RMALLOC)
  CHECK_INCLUDE_FILE(rmalloc.h HAVE_RMALLOC_H)
  SET(USE_RMALLOC 1)
ENDIF(RMALLOC)


IF(HAVE_PTHREAD_H)
  CHECK_LIBRARY_EXISTS(pthread pthread_create "" HAVE_LIBPTHREAD)
  IF(HAVE_LIBPTHREAD)
    ADD_DEFINITIONS(-D_REENTRANT -D_POSIX_C_SOURCE=199309L)
    SET(EXTRA_LIBS pthread ${EXTRA_LIBS})
  ENDIF(HAVE_LIBPTHREAD)
ENDIF(HAVE_PTHREAD_H)

CHECK_LIBRARY_EXISTS(rt clock_getres "" HAVE_LIBRT)
IF(HAVE_LIBRT)
   SET(EXTRA_LIBS rt ${EXTRA_LIBS})
ENDIF(HAVE_LIBRT)

IF(HAVE_MATH_H)
  CHECK_LIBRARY_EXISTS(m floorf "" HAVE_LIBM)
  IF(HAVE_LIBM)
    SET(EXTRA_LIBS m ${EXTRA_LIBS})
  ENDIF(HAVE_LIBM)
ENDIF(HAVE_MATH_H)

IF(HAVE_DLFCN_H)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
  IF(HAVE_LIBDL)
    SET(EXTRA_LIBS dl ${EXTRA_LIBS})
  ENDIF(HAVE_LIBDL)
ENDIF(HAVE_DLFCN_H)

CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/include/aax/aax.h.in"
    "${AeonWave_BINARY_DIR}/include/aax/aax.h")

CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/include/cmake_config.h.in"
    "${AeonWave_BINARY_DIR}/include/config.h")

CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/admin/debian/control.in"
    "${AeonWave_BINARY_DIR}/admin/debian/control")
CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/admin/debian/control-dev.in"
    "${AeonWave_BINARY_DIR}/admin/debian/control-dev")

SET( AEONWAVE_OBJS
     base/buffers.c
     base/dlsym.c
     base/geometry.c
     base/gmath.c
     base/logging.c
     base/memory.c
     base/threads.c
     base/timer.c
     base/types.c
     src/api.c
     src/devices.c
     src/objects.c
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
     src/alsa/device.c
     src/dmedia/device.c
     src/oss/device.c
     src/file/device.c
     src/file/filetype.c
     src/file/wavext.c
     src/file/mp3ext.c
     src/software/device.c
     src/software/arch.c
     src/software/arch_cpu.c
     src/software/arch_neon.c
     src/software/arch_sse.c
     src/software/arch_sse2.c
     src/software/arch_sse3.c
     src/software/arch_sse4.c
     src/software/mixer.c
     src/software/frame.c
     src/software/sensor.c
     src/software/emitter.c
     src/software/rbuf_codecs.c
     src/software/rbuf_codec_tables.c
     src/software/rbuf_effects.c
     src/software/rbuf_mixers.c
     src/software/rbuf_mixer_tables.c
     src/software/rbufmix_multi.c
     src/software/rbufmix_single.c
     src/software/waveforms.c
     src/software/ringbuffer.c
   )

#IF(SIZEOF_SIZE_T EQUAL 8)
#  SET(AEONWAVE_OBJS ${AEONWAVE_OBJS}
#     src/software/arch_avx.c
#  )
#ENDIF(SIZEOF_SIZE_T EQUAL 8)

IF(HAVE_WINDOWS_H)
  SET(AEONWAVE_OBJS ${AEONWAVE_OBJS}
     src/windows/wasapi.c
     src/windows/audio.c
  )
ENDIF(HAVE_WINDOWS_H)

# Build a library
SET(LIB_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
ADD_LIBRARY(${LIBAEONWAVE} ${LIBTYPE} ${AEONWAVE_OBJS})
SET_TARGET_PROPERTIES(${LIBAEONWAVE}
    PROPERTIES DEFINE_SYMBOL AAX_BUILD_LIBRARY
    VERSION ${LIB_VERSION}
    SOVERSION ${CPACK_PACKAGE_VERSION_MAJOR}
)
IF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
    SET_TARGET_PROPERTIES(${LIBAEONWAVE} PROPERTIES PREFIX "")
ENDIF()

TARGET_LINK_LIBRARIES(${LIBAEONWAVE}
    ${EXTRA_LIBS}
    ${BASE_LIBRARY}
    ${XML_LIBRARY}
)

# Create the copyright file
EXECUTE_PROCESS(COMMAND "xxd" -include COPYING src/copyright.h
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE varRes)

# Add an install target here
# Note Windows DLL are specified by RUNTIME
INSTALL(TARGETS ${LIBAEONWAVE}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Libraries
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Headers
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" NAMELINK_SKIP COMPONENT Libraries
)
INSTALL(TARGETS ${LIBAEONWAVE}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" NAMELINK_ONLY
        COMPONENT Headers
)
IF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
  SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
      CopyFiles \\\"$INSTDIR\\\\bin\\\\${LIBAEONWAVE}.dll\\\" \\\"$SYSDIR\\\\${LIBAEONWAVE}.dll\\\"
      ")
  SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
      IfFileExists \\\"$SYSDIR\\\\${LIBAEONWAVE}.dll\\\" 0 next
        Delete \\\"$SYSDIR\\\\${LIBAEONWAVE}.dll\\\"
      next:
      ")
ENDIF(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")

INSTALL(FILES 
        ${AeonWave_BINARY_DIR}/include/aax/aax.h
        include/aax/defines.h
        DESTINATION include/aax
        COMPONENT Headers
)
INSTALL(FILES 
        include/aax.h
        include/aaxdefs.h
        DESTINATION include
        COMPONENT Headers
)

IF (UNIX AND NOT WIN32)
    macro(install_symlink _dest _source)
      install(CODE "
             include(\"${AeonWave_SOURCE_DIR}/CMakeModules/Symlink.cmake\")
             InstallSymlink(\"${_dest}\" \"\$ENV{DESTDIR}${_source}\")")
    endmacro(install_symlink)

    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR} ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR})
    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so)
ENDIF(UNIX AND NOT WIN32)


IF(NOT SINGLE_PACKAGE)
  INCLUDE (CPack)
ENDIF(NOT SINGLE_PACKAGE)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${AeonWave_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
#ADD_CUSTOM_TARGET(uninstall
#  "${CMAKE_COMMAND}" -P "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake")
