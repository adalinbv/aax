cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

SET(CMAKE_MAKEFILE_DEPENDS
    "CMakeModules/Symlink.cmake"
)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)


INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckFunctionExists)

PROJECT(aax C)
SET(PACKAGE "AeonWave")

OPTION(DLOPEN "Check for the dlopen API for loading optional libs" ON)
OPTION(WERROR "Treat compile warnings as errors"   OFF)

IF(WIN32)
    ADD_DEFINITIONS(-DAAX_BUILD_LIBRARY=1)
    ADD_DEFINITIONS(-D_WIN32)
    ADD_DEFINITIONS(/NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcrtd.lib)
    SET(LIBAEONWAVE AAX32)
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

# detect system type
IF(NOT DEFINED CPACK_SYSTEM_NAME)
  SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
ENDIF(NOT DEFINED CPACK_SYSTEM_NAME)
IF (UNIX AND NOT WIN32)
  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(CPACK_PACKAGE_ARCHITECTURE "x86_64")
  ELSE(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(CPACK_PACKAGE_ARCHITECTURE "i386")
  ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 8)

  If(NOT DEFINED CMAKE_LIBRARY_ARCHITECTURE)
     SET(CMAKE_LIBRARY_ARCHITECTURE "${CPACK_PACKAGE_ARCHITECTURE}-linux-gnu")
  ENDIF(NOT DEFINED CMAKE_LIBRARY_ARCHITECTURE)
ENDIF(UNIX AND NOT WIN32)

SET(CMAKE_INSTALL_LIBDIR lib/${CMAKE_LIBRARY_ARCHITECTURE} CACHE PATH "Output directory for libraries")

#  read 'description` file into a variable
file(STRINGS description descriptionFile)
STRING(REGEX REPLACE ";" " " descriptionFile "${descriptionFile}")

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAX_VERSION})

SET(AAX_MAJOR_VERSION ${CMAKE_MATCH_1})
SET(AAX_MINOR_VERSION ${CMAKE_MATCH_2})
SET(AAX_MICRO_VERSION ${CMAKE_MATCH_3})

SET(CPACK_PACKAGE_NAME "${PACKAGE}")
SET(CPACK_PACKAGE_VENDOR "Adalin B.V.")
SET(CPACK_PACKAGE_CONTACT "tech@adalin.org")
SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
SET(CPACK_PACKAGE_DESCRIPTION "A cross platform, hardware accelerated, versatile stereo and 4D audio mixing library")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${descriptionFile})
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
SET(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")

IF(WIN32)
  SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
  SET(CPACK_GENERATOR NSIS)

ELSE(WIN32)
  SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.el6.${CPACK_PACKAGE_ARCHITECTURE}")
  SET(CPACK_GENERATOR "DEB;RPM")

  # DEBIAN
  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    SET(SIZEOF_SIZE_T 8)
  ELSE ()
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    SET(SIZEOF_SIZE_T 4)
  ENDIF ()
  SET(CPACK_DEBIAN_PACKAGE_SECTION "libs")
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1)")
  SET(CPACK_DEBIAN_COMPONENT_INSTALL ON)

  # RPM
  SET(CPACK_RPM_PACKAGE_ARCHITECTURE ${PACK_PACKAGE_ARCHITECTURE})
  SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/description")
  SET(CPACK_RPM_CHANGELOG_FILE "${CMAKE_SOURCE_DIR}/ChangeLog")
  SET(CPACK_RPM_PACKAGE_LICENSE "License For Customer Use of ADALIN AEONWAVE AUDIO EXTENSIONS LIBRARY Software")
  SET(CPACK_RPM_PACKAGE_GROUP "System/Libraries")
  SET(CPACK_RPM_COMPONENT_INSTALL ON)

  # ZIP
  SET(CPACK_SOURCE_GENERATOR ZIP)
  SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAX_VERSION}" CACHE INTERNAL "tarball basename")
  SET(CPACK_SOURCE_IGNORE_FILES
    "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}")
  SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Adalin B.V.")
ENDIF(WIN32)

SET(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Applications")
SET(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION 
   "A command-line utility that tests and shows the capabilities of AeonWave")

SET(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Libraries")
SET(CPACK_COMPONENT_LIBRARIES_DESCRIPTION
   "AeonWave, A cross platform, hardware accelerated, versatile stereo and 4D audio mixing library that can handle almost any audio mixing demands; from studio effect mixing to immersive simulation.")

SET(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "Development Headers")
SET(CPACK_COMPONENT_HEADERS_DESCRIPTION
   "Development header files for use with AeonWave")

SET(CPACK_COMPONENT_HEADERS_DEPENDS libraries)
SET(CPACK_COMPONENT_APPLICATIONS_GROUP "Runtime")
SET(CPACK_COMPONENT_LIBRARIES_GROUP "Development")
SET(CPACK_COMPONENT_HEADERS_GROUP "Development")

SET(CPACK_COMPONENT_GROUP_DEVELOPMENT_DESCRIPTION
   "All of the tools you'll ever need to develop software")

SET(CPACK_COMPONENTS_ALL applications runtime devel)

# NSIS only (for now?)
SET(CPACK_ALL_INSTALL_TYPES Runtime Developer)
SET(CPACK_COMPONENT_HEADERS_INSTALL_TYPES Developer)
SET(CPACK_COMPONENT_LIBRARIES_INSTALL_TYPES Developer Runtime)
SET(CPACK_COMPONENT_APPLICATIONS_INSTALL_TYPES Runtime)

INCLUDE (CPack)

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# Required libraries
FIND_PACKAGE(XML REQUIRED)
IF(NOT XML_FOUND)
  MESSAGE(FATAL_ERROR "ERROR: required XML library is not installed, aborting.")
ENDIF(NOT XML_FOUND)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/src"
    ${CMAKE_SOURCE_DIR}
    ${XML_INCLUDE_DIR}
    include
    base
    ${CMAKE_BINARY_DIR}/include
)

ADD_DEFINITIONS(-DHAVE_CONFIG_H=1)
IF(WERROR)
  ADD_DEFINITIONS(-Werror)
ENDIF()

IF(MSVC)
    SET_PROPERTY(SOURCE src/software/arch_sse.c
                 PROPERTY C_FLAGS "${C_FLAGS} /arch:SSE"
    )
    SET_PROPERTY(SOURCE src/software/arch_sse2.c
                 PROPERTY C_FLAGS "${C_FLAGS} /arch:SSE2"
    )

    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -D_DEBUG /LD")
    SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DNDEBUG /LD")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG /LD")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG /LD")
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)

    SET_PROPERTY(SOURCE src/software/arch_sse.c
                 PROPERTY COMPILE_FLAGS /arch:SSE
    )
    SET_PROPERTY(SOURCE src/software/arch_sse2.c
                 PROPERTY  /arch:SSE2
    )

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
    SET(C_FLAGS "${C_FLAGS} -Os -march=i586 -Werror=format-security -std=c99 -Wall -fvisibility=hidden -Wl,--hash-style=both -shared -fPIC -funroll-loops -fomit-frame-pointer" CACHE STRING
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
CHECK_INCLUDE_FILE(windows.h HAVE_WINDOWS_H -D_WIN32_WINNT=0x0501)
CHECK_FUNCTION_EXISTS(strdup HAVE_STRDUP)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
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

IF(HAVE_PTHREAD_H)
  CHECK_LIBRARY_EXISTS(pthread pthread_create "" HAVE_LIBPTHREAD)
  IF(HAVE_LIBPTHREAD)
    ADD_DEFINITIONS(-D_REENTRANT -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=199309L)
    SET(EXTRA_LIBS pthread ${EXTRA_LIBS})
  ENDIF(HAVE_LIBPTHREAD)
ENDIF(HAVE_PTHREAD_H)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/include/aax/aax.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/aax/aax.h")

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")

#ADD_SUBDIRECTORY(base src)

SET( AEONWAVE_OBJS
     base/buffers.c
     base/dlsym.c
     base/geometry.c
     base/gmath.c
     base/logging.c
     base/memory.c
     base/threads.c
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
     src/file/null_device.c
     src/file/device.c
     src/file/filetype.c
     src/file/wavext.c
     src/software/arch.c
     src/software/arch_cpu.c
     src/software/arch_neon.c
     src/software/arch_sse2.c
     src/software/arch_sse.c
     src/software/mixer.c
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

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(AEONWAVE_OBJS ${AEONWAVE_OBJS}
     src/software/arch_avx.c
  )
ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 8)

IF(HAVE_WINDOWS_H)
  SET(AEONWAVE_OBJS ${AEONWAVE_OBJS}
     src/windows/mmdevice.c
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

# Add an install target here
SET(BIN_DIR bin)
SET(INCLUDE_DIR include)
SET(ARCH_DIR lib)
IF(WIN32)
  SET(LIB_DIR bin)	# In case this becomes a shared library
ELSE(WIN32)
  SET(LIB_DIR lib)
ENDIF(WIN32)

INSTALL(TARGETS ${LIBAEONWAVE}
        RUNTIME DESTINATION "${BIN_DIR}" COMPONENT runtime
        LIBRARY DESTINATION "${LIB_DIR}" COMPONENT devel
        ARCHIVE DESTINATION "${ARCH_DIR}" COMPONENT devel
)
INSTALL(FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/include/aax/aax.h
        include/aax/defines.h
        DESTINATION include/aax
        COMPONENT devel
)
INSTALL(FILES 
        include/aax.h
        include/aaxdefs.h
        DESTINATION include
        COMPONENT devel
)

IF (UNIX AND NOT WIN32)
    macro(install_symlink _dest _source)
      install(CODE "
             include(\"${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/Symlink.cmake\")
             InstallSymlink(\"${_dest}\" \"\$ENV{DESTDIR}${_source}\")")
    endmacro(install_symlink)

    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR} ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR})
    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so)
ENDIF(UNIX AND NOT WIN32)


#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
