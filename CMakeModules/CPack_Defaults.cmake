
# Default to release build type
IF(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE INTERNAL "Release type")
endif(NOT CMAKE_BUILD_TYPE)

message("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message("CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("WIN64: ${WIN64}")

IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|amd64.*|x86_64.* OR CMAKE_GENERATOR MATCHES "Visual Studio.*Win64")
  IF(CMAKE_C_FLAGS MATCHES -m32 OR CMAKE_CXX_FLAGS MATCHES -m32)
    set(X86 1)
  else(CMAKE_C_FLAGS MATCHES -m32 OR CMAKE_CXX_FLAGS MATCHES -m32)
    set(X86_64 1)
  endif(CMAKE_C_FLAGS MATCHES -m32 OR CMAKE_CXX_FLAGS MATCHES -m32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES i686.*|i386.*|x86.* OR WIN32)
  IF(CMAKE_C_FLAGS MATCHES -m64 OR CMAKE_CXX_FLAGS MATCHES -m64)
    set(X86_64 1)
  else(CMAKE_C_FLAGS MATCHES -m32 OR CMAKE_CXX_FLAGS MATCHES -m32)
    set(X86 1)
  endif(CMAKE_C_FLAGS MATCHES -m64 OR CMAKE_CXX_FLAGS MATCHES -m64)
elseif((CMAKE_SYSTEM_PROCESSOR MATCHES arm.* OR CMAKE_SYSTEM_PROCESSOR MATCHES aarch64) AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ARM 1)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES aarch64)
        set(ARM64 1)
	set(ARCH32 0)
    endif(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES mips)
    set(MIPS 1)
endif()

IF ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  # using Clang
  set(CLANG 1)
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "TinyCC")
  # using TinyCC
  set(TINYCC 1)
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  # using GCC
  set(GCC 1)
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
  set(INTELCC 1)
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
  set(MSVC 1)
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MIPSpro")
  # using SGI MIPSpro
  set(MIPSPRO 1)
endif()

# Set default libdir
IF(NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "library destination directory")
endif(NOT DEFINED CMAKE_INSTALL_LIBDIR)

# Set default bindir
IF(NOT DEFINED CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "executable destination directory")
endif(NOT DEFINED CMAKE_INSTALL_BINDIR)


# detect system type
IF(NOT DEFINED CPACK_SYSTEM_NAME)
  set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
endif(NOT DEFINED CPACK_SYSTEM_NAME)

IF (UNIX AND NOT WIN32)
  IF(X86)
    set(CPACK_PACKAGE_ARCHITECTURE "i386")
  elseif(X86_64)
    set(CPACK_PACKAGE_ARCHITECTURE "x86_64")
  elseif(ARM64)
    set(CPACK_PACKAGE_ARCHITECTURE "aarch64")
  elseif(ARM)
    set(CPACK_PACKAGE_ARCHITECTURE "armhf")
  else()
    set(CPACK_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
  endif()

  OPTION(MULTIARCH "Enable for multi-arch aware systems (debian)" OFF)
  if(NOT DEFINED CMAKE_LIBRARY_ARCHITECTURE)
    set(CMAKE_LIBRARY_ARCHITECTURE "${CPACK_PACKAGE_ARCHITECTURE}-linux-gnu")
  endif(NOT DEFINED CMAKE_LIBRARY_ARCHITECTURE)
  set(CMAKE_INSTALL_LIBDIR lib/${CMAKE_LIBRARY_ARCHITECTURE} CACHE PATH "Output directory for libraries" FORCE)
endif(UNIX AND NOT WIN32)

set(CPACK_PACKAGE_NAME "${PACKAGE}")
set(CPACK_PACKAGE_VENDOR "Adalin B.V." CACHE INTERNAL "Vendor name")
set(CPACK_PACKAGE_CONTACT "tech@adalin.org" CACHE INTERNAL "Contact")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING.v2" CACHE INTERNAL "Copyright" FORCE)

#  read 'description` file into a variable
file(STRINGS description descriptionFile)
STRING(REGEX REPLACE "; \\.?" "\n" rpmDescription "${descriptionFile}")
STRING(REGEX REPLACE ";" "\n" debDescription "${descriptionFile}")

IF(WIN32)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Adalin\\\\AeonWave")
  set(CPACK_NSIS_CONTACT "info@adalin.com")
  set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${PACKAGE_NAME}")
  set(CPACK_NSIS_DISPLAY_NAME "${PACKAGE_NAME}")
  set(CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
# set(CPACK_NSIS_MODIFY_PATH ON)
# set(CPACK_STRIP_FILES 1)
  set(CPACK_GENERATOR NSIS)

else(WIN32)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.el6.${CPACK_PACKAGE_ARCHITECTURE}")
  set(CPACK_GENERATOR "DEB;RPM;STGZ")

  IF(X86)
    set(CPACK_DEBIAN_ARCHITECTURE "i386")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "i686")
  elseif(X86_64)
    set(CPACK_DEBIAN_ARCHITECTURE "amd64")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "amd64")
  elseif(ARM64)
    set(PACK_DEBIAN_ARCHITECTURE "aarch64")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "aarch64")
  elseif(ARM)
    set(CPACK_DEBIAN_ARCHITECTURE "armhf")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "armhf")
  else()
    set(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
    set(CPACK_RPM_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
  endif()
  set(CPACK_DEBIAN_DATA_PACKAGE_ARCHITECTURE "all")
  set(CPACK_RPM_DATA_PACKAGE_ARCHITECTURE "noarch")

  set(CPACK_DEBIAN_DATA_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-data.deb")
  set(CPACK_RPM_DATA_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-data.rpm")

  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${debDescription})
  IF(MULTIARCH)
    set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "multiarch-support")
  endif(MULTIARCH)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR} <${CPACK_PACKAGE_CONTACT}>")
  set(CPACK_DEBIAN_CHANGELOG_FILE "${PROJECT_SOURCE_DIR}/ChangeLog")
  set(CPACK_DEB_COMPONENT_INSTALL ON)

  INSTALL(FILES
          "${PROJECT_SOURCE_DIR}/COPYING.v2"
          "${PROJECT_SOURCE_DIR}/COPYING.v3"
          "${PROJECT_SOURCE_DIR}/COPYING.foss_exception"
          DESTINATION /usr/share/doc/${PACKAGE}-bin
          COMPONENT Libraries
  )

  EXECUTE_PROCESS(COMMAND "cp" -f -p ChangeLog debian/ChangeLog
                  COMMAND "gzip" -f -9 debian/ChangeLog
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE varRes)
  INSTALL(FILES
          debian/ChangeLog.gz
          DESTINATION /usr/share/doc/${PACKAGE}-bin
          RENAME changelog.gz
          COMPONENT Libraries
  )
  INSTALL(FILES
          debian/ChangeLog.gz
          DESTINATION /usr/share/doc/${PACKAGE}-dev
          RENAME changelog.gz
          COMPONENT Headers
  )

  # RPM
  set(CPACK_RPM_PACKAGE_ARCHITECTURE ${PACK_PACKAGE_ARCHITECTURE})
  set(CPACK_RPM_PACKAGE_DESCRIPTION ${rpmDescription})
  set(CPACK_RPM_CHANGELOG_FILE "${PROJECT_SOURCE_DIR}/ChangeLog")
  set(CPACK_RPM_COMPONENT_INSTALL ON)
endif(WIN32)

# ZIP
IF(EXISTS ${PROJECT_SOURCE_DIR}/.gitignore)
  FILE(STRINGS ${PROJECT_SOURCE_DIR}/.gitignore CPACK_SOURCE_IGNORE_FILES)
endif(EXISTS ${PROJECT_SOURCE_DIR}/.gitignore)

set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "^${PROJECT_SOURCE_DIR}/.git;\\\\.gitignore;Makefile.am;~$;${CPACK_SOURCE_IGNORE_FILES}" CACHE INTERNAL "Ignore files" FORCE)

