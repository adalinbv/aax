cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)


INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

PROJECT(aax-utils C CXX)
SET(PACKAGE_NAME "AeonWave-utils")
SET(PACKAGE "aaxutils")

OPTION(WERROR "Treat compile warnings as errors"   OFF)

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(SIZEOF_SIZE_T 8)
ELSE ()
  SET(SIZEOF_SIZE_T 4)
ENDIF ()

# read 'version' file into a variable (stripping any newlines or spaces)
FILE(READ version versionFile)
STRING(STRIP ${versionFile} AAXUTILS_VERSION)
SET(VERSION AAXUTILS_VERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAXUTILS_VERSION})

SET(AAX_UTILS_MAJOR_VERSION ${CMAKE_MATCH_1})
SET(AAX_UTILS_MINOR_VERSION ${CMAKE_MATCH_2})
SET(AAX_UTILS_MICRO_VERSION ${CMAKE_MATCH_3})
SET(AAX_UTILS_PATCH_LEVEL ${DATE})

SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Utilities to show the capabilities of AeonWave")

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${aax-utils_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

INCLUDE(CPack_Modules)
INCLUDE(CPack_Defaults)

IF(WIN32)

ELSE(WIN32)
  ADD_DEFINITIONS(-D_REENTRANT -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=199309L)

  # DEBIAN
  SET(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1), libaax3-bin (>= 3.0.0)")

  # RPM
  SET(CPACK_RPM_PACKAGE_LICENSE "BSD")
  SET(CPACK_RPM_PACKAGE_GROUP "System/System/GUI/KDE")
  SET(CPACK_RPM_PACKAGE_REQUIRES "libaax3-bin >= 3.0.0")
ENDIF(WIN32)

# ZIP
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAXUTILS_VERSION}" CACHE INTERNAL "tarball basename")

# Add definitions, compiler switches, etc.
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
    IF(MINGW)
       ADD_DEFINITIONS(-U__STRICT_ANSI__)
    ENDIF(MINGW)
 
    if (GCC OR CLANG)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" CACHE STRING
          "Flags used by al build types."
          FORCE)
      set(C_FLAGS "${C_FLAGS} -Werror=format-security -std=c99 -Wall -fvisibility=hidden -funroll-loops" CACHE STRING
e         "Flags used by al build types."
    ELSEdd
          FORCE)
    endif (GCC OR CLANG)

    IF(X86_64)
       SET(C_FLAGS "${C_FLAGS} -march=athlon64 -msse2 -mfpmath=sse")
    ELSEIF(X86)
       SET(C_FLAGS "${C_FLAGS} -march=i686")
    ELSEIF(ARM)
       SET(C_FLAGS "${C_FLAGS} -march=armv5 -marm")
    ENDIF()

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" CACHE STRING
          "Flags used by al build types."
          FORCE)
    SET(C_FLAGS "${C_FLAGS} -Os -Werror=format-security -std=c99 -Wall -fvisibility=hidden -funroll-loops -fomit-frame-pointer" CACHE STRING
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

# Required libraries
FIND_PACKAGE(AAX COMPONENTS aax REQUIRED)
FIND_PACKAGE(XML COMPONENTS xml REQUIRED)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${aax-utils_SOURCE_DIR}"
    base
    ${AAX_INCLUDE_DIR}
    ${RMALLOC_INCLUDE_DIR}
    "${aax-utils_BINARY_DIR}/include"
)

CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(limits.h HAVE_LIMITS_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(rmalloc.h HAVE_RMALLOC_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/ioctl.h HAVE_SYS_IOCTL_H)
CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")

# Man pages
IF (UNIX AND NOT WIN32)
  CONFIGURE_FILE(
      "${CMAKE_CURRENT_SOURCE_DIR}/admin/aaxcvt.1.in"
      "${CMAKE_CURRENT_BINARY_DIR}/aaxcvt.1")
  CONFIGURE_FILE(
      "${CMAKE_CURRENT_SOURCE_DIR}/admin/aaxplay.1.in"
      "${CMAKE_CURRENT_BINARY_DIR}/aaxplay.1")
  CONFIGURE_FILE(
      "${CMAKE_CURRENT_SOURCE_DIR}/admin/aaxinfo.1.in"
      "${CMAKE_CURRENT_BINARY_DIR}/aaxinfo.1")

  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/aaxcvt.1
                ${CMAKE_CURRENT_BINARY_DIR}/aaxplay.1
                ${CMAKE_CURRENT_BINARY_DIR}/aaxinfo.1
          DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1"
          COMPONENT Applications)
ENDIF()

SET(BASE_OBJS
     base/logging.c
     base/geometry.c
     base/types.c
)

IF(WIN32)
  SET(KERNEL_LIBRARY Kernel32)
  SET(EXTRA_LIBS "-ldriver")
ELSE(WIN32)
  SET(EXTRA_LIBS "-ldriver -lm")
ENDIF(WIN32)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)
LINK_DIRECTORIES(${PROJECT_BINARY_DIR}/src)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)
ADD_SUBDIRECTORY(test++)

IF(NOT SINGLE_PACKAGE)
  INCLUDE (CPack)
ENDIF(NOT SINGLE_PACKAGE)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
#ADD_CUSTOM_TARGET(uninstall
#  "${CMAKE_COMMAND}" -P "${aax-utils_BINARY_DIR}/cmake_uninstall.cmake")
