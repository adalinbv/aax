cmake_minimum_required(VERSION 2.8.3)
SET(cmake_verbose_makefile TRUE)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)


INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE (CheckIncludeFiles)

PROJECT(aax-utils C)
SET(PACKAGE_NAME "AeonWave-utils")
SET(PACKAGE "aaxutils")

OPTION(WERROR "Treat compile warnings as errors"   OFF)

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ version versionFile)
STRING(STRIP ${versionFile} AAXUTILS_VERSION)
SET(VERSION AAXUTILS_VERSION)

IF(NOT DEFINED CPACK_SYSTEM_NAME)
  SET(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
ENDIF(NOT DEFINED CPACK_SYSTEM_NAME)

IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(SIZEOF_SIZE_T 8)
ELSE ()
  SET(SIZEOF_SIZE_T 4)
ENDIF ()

IF (UNIX AND NOT WIN32)
  IF(SIZEOF_SIZE_T EQUAL 8)
    SET(CPACK_PACKAGE_ARCHITECTURE "x86_64")
  ELSE(SIZEOF_SIZE_T EQUAL 8)
    SET(CPACK_PACKAGE_ARCHITECTURE "i386")
  ENDIF(SIZEOF_SIZE_T EQUAL 8)
ENDIF(UNIX AND NOT WIN32)

#  read 'description` file into a variable
file(STRINGS description descriptionFile)
STRING(REGEX REPLACE "; \\.?" "\n" rpmDescription "${descriptionFile}")
STRING(REGEX REPLACE ";" "\n" debDescription "${descriptionFile}")

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAXUTILS_VERSION})

SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})

SET(CPACK_PACKAGE_NAME "${PACKAGE}")
SET(CPACK_PACKAGE_VENDOR "Adalin B.V.")
SET(CPACK_PACKAGE_CONTACT "tech@adalin.org")
SET(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
SET(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
SET(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Utilities for the AeonWave Audio eXtensions library")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")

IF(WIN32)
  SET(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
  SET(CPACK_PACKAGE_INSTALL_DIRECTORY "${PACKAGE_NAME}")
  SET(CPACK_GENERATOR NSIS)

ELSE(WIN32)
  SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.el6.${CPACK_PACKAGE_ARCHITECTURE}")
  SET(CPACK_GENERATOR "DEB;RPM")

  # DEBIAN
  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  ELSE ()
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
  ENDIF ()
  SET(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1)")
  SET(CPACK_DEBIAN_COMPONENT_INSTALL ON)

  # RPM
  IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
  ELSE ()
    SET(CPACK_RPM_PACKAGE_ARCHITECTURE "i386")
  ENDIF ()
  SET(CPACK_RPM_PACKAGE_GROUP "System/Console")
  SET(CPACK_RPM_COMPONENT_INSTALL ON)

  # ZIP
  SET(CPACK_SOURCE_GENERATOR ZIP)
  SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAXUTILS_VERSION}" CACHE INTERNAL "tarball basename")
  SET(CPACK_SOURCE_IGNORE_FILES
    "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}")
ENDIF(WIN32)

# Component support
INCLUDE(CPack_Modules)

INCLUDE (CPack)

# We have some custom .cmake scripts not in the official distribution.
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

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
    SET(C_FLAGS "${C_FLAGS} -Os -march=i586 -Werror=format-security -std=c99 -Wall -fvisibility=hidden -funroll-loops -fomit-frame-pointer" CACHE STRING
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

# Required libraries
FIND_PACKAGE(AAX REQUIRED)
if(NOT AAX_FOUND)
  message(STATUS "ERROR: required AAX library is not installed, aborting.")
  return()
endif(NOT AAX_FOUND)
FIND_PACKAGE(XML REQUIRED)

# Add definitions, compiler switches, etc.
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}"
    base
    ${AAX_INCLUDE_DIR}
    ${RMALLOC_INCLUDE_DIR}
    "${CMAKE_BINARY_DIR}/include"
)

CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(limits.h HAVE_LIMITS_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(rmalloc.h HAVE_RMALLOC_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")


SET(BASE_OBJS
     base/logging.c
     base/geometry.c
     base/types.c
)

ADD_SUBDIRECTORY(src)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
