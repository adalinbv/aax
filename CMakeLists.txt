cmake_minimum_required(VERSION 3.7)
set(cmake_verbose_makefile TRUE)

set(CMAKE_MAKEFILE_DEPENDS
  "CMakeModules/Symlink.cmake"
)

if(COMMAND cmake_policy)
 cmake_policy(SET CMP0003 NEW)
 cmake_policy(SET CMP0054 NEW)
endif(COMMAND cmake_policy)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)

set(CMAKE_GNUtoMS ON)
project(AeonWave)
set(PACKAGE_NAME "AeonWave")
set(PACKAGE "libaax4")

option(LTO "Enabel linkt-time optimizations" OFF)
option(RELEASE "Enable for final release builds" OFF)
option(DLOPEN "Check for the dlopen API for loading optional libs" ON)
option(WERROR "Treat compile warnings as errors"   OFF)
option(RMALLOC "Enable memory debugging functions" OFF)
option(BUILD_TESTING "Enable buildig test utilities" OFF)

add_definitions(-D_XOPEN_SOURCE=600)
if(WIN32)
  set(EXTRA_LIBS ${EXTRA_LIBS} "-lwsock32 -lws2_32 -lwinmm")
  add_definitions(-DAAX_BUILD_LIBRARY=1)
  add_definitions(-D_WIN32 -DWIN32)
  set(LIBAEONWAVE AAX)
  set(AEONWAVE_DIR aax)
else(WIN32)
  add_definitions(-D_GNU_SOURCE=1)
  set(LIBAEONWAVE aax)
  set(AEONWAVE_DIR aax)
endif(WIN32)

if(NOT LIBTYPE)
  set(LIBTYPE SHARED)
endif(NOT LIBTYPE)

MACRO (TODAY RESULT)
  if (WIN32)
    EXECUTE_PROCESS(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "... (..)/(..)/..(..).*" "\\3\\1\\2" ${RESULT} "${${RESULT}}")
  elseif(UNIX)
    EXECUTE_PROCESS(COMMAND "date" "+%d/%m/%Y" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "(..)/(..)/..(..).*" "\\3\\2\\1" ${RESULT} "${${RESULT}}")
  else (WIN32)
    MESSAGE(SEND_ERROR "date not implemented")
    set(${RESULT} 000000)
  endif (WIN32)
ENDMACRO (TODAY)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(SIZEOF_SIZE_T 8)
else ()
  set(SIZEOF_SIZE_T 4)
endif ()

#Set the include directory so that the project can be used as a sub project
if( NOT(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME}) )
    set(AeonWave_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src" PARENT_SCOPE)
    set(AeonWave_LIBRARIES "AeonWave" PARENT_SCOPE)
endif()

set(VES_EXPORT_LIBRARY_TARGETS aax AAX libAAX)
set(VES_CONFIGURE_VARS CMAKE_INSTALL_LIBDIR)


# read 'version' file into a variable (stripping any newlines or spaces)
TODAY(DATE)
file(READ version versionFile)
string(STRIP ${versionFile} AAX_VERSION)
set(VERSION AAX_VERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAX_VERSION})

set(AAX_MAJOR_VERSION ${CMAKE_MATCH_1})
set(AAX_MINOR_VERSION ${CMAKE_MATCH_2})
set(AAX_MICRO_VERSION ${CMAKE_MATCH_3})
set(AAX_PATCH_LEVEL ${DATE})

FILE(READ soversion soVersionFile)
string(STRIP ${soVersionFile} AAX_SOVERSION)
set(SOVERSION AAX_SOVERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_SOVERSION ${AAX_SOVERSION})

set(CPACK_PACKAGE_SOVERSION_MAJOR ${CMAKE_MATCH_1})
set(CPACK_PACKAGE_SOVERSION_MINOR ${CMAKE_MATCH_2})
set(CPACK_PACKAGE_SOVERSION_PATCH ${CMAKE_MATCH_3})

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross platform, hardware accelerated, multi-channel and 4D capable audio rendering engine")

# We have some custom .cmake scripts not in the official distribution.
set(CMAKE_MODULE_PATH "${AeonWave_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")
include(CPack_Modules)
include(CPack_Defaults)
include(GNUInstallDirs)
include(package)
include(inline)

if (WIN32)
else(WIN32)

  # DEBIAN
  set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1), debconf")
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "alsa-base | oss4-base")
  set(CPACK_DEBIAN_PACKAGE_SUGGESTS "libmpg123-0, libopus0")
  set(CPACK_DEBIAN_PACKAGE_REPLACES "libaax4-bin, libaax4-dev, libaax4-data")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${AeonWave_SOURCE_DIR}/admin/deb/postinst;${AeonWave_SOURCE_DIR}/admin/deb/postrm;")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

  execute_process(COMMAND "cp" -f -p ChangeLog debian/ChangeLog
                  COMMAND "gzip" -f -9 debian/ChangeLog
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE varRes)

  # RPM
  set(CPACK_RPM_PACKAGE_LICENSE "GPL")
  set(CPACK_RPM_PACKAGE_GROUP "System/Libraries")
  set(CPACK_RPM_PACKAGE_PROVIDES "libaax4-bin, libaax4-dev, libaax4-data")
endif(WIN32)

# ZIP
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAX_VERSION}" CACHE INTERNAL "tarball basename")

#$ Required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(PKG_PIPEWIRE libpipewire-0.3>=0.3.20)
if (PKG_PIPEWIRE_FOUND)
# pkg_check_modules(SPA libspa-0.1)
  set(HAVE_PIPEWIRE_H 1)
  set (PIPEWIRE_LIBRARY ${PKG_PIPEWIRE_LIBRARIES})
  add_compile_options(${PKG_PIPEWIRE_CFLAGS})
endif()

find_package(SSE REQUIRED)
if((GCC OR CLANG) AND RMALLOC)
  set(C_FLAGS "${C_FLAGS} -fsanitize=address")
  set(CXX_FLAGS "${CXX_FLAGS} -fsanitize=address")
  set(LDFLAGS "${LD_FLAGS} -fsanitize=address")
  set(RMALLOC OFF)
  find_package(XML REQUIRED)
else((GCC OR CLANG) AND RMALLOC)
  find_package(XML REQUIRED)
endif((GCC OR CLANG) AND RMALLOC)
if(NOT XML_FOUND)
  MESSAGE(FATAL_ERROR "ERROR: required XML library is not installed, aborting.")
endif(NOT XML_FOUND)

# Add definitions, compiler switches, etc.
include_directories("${AeonWave_SOURCE_DIR}/src"
  ${AeonWave_SOURCE_DIR}
  ${XML_INCLUDE_DIR}
  include
  base
  ${AeonWave_BINARY_DIR}/include
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

add_definitions(-DHAVE_CONFIG_H=1)
if(WERROR)
  add_definitions(-Werror)
endif()

set(C_FLAGS "${CMAKE_C_FLAGS}")
set(CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(MSVC)
  set(C_FLAGS "${C_FLAGS} /NODEFAULTLIB:msvcrt /NODEFAULTLIB:msvcrtd /NODEFAULTLIB:libcmt -DARCH64=1")
  set_property(SOURCE src/software/cpu/arch2d_sse.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE /fp:fast -D__SSE__"
  )
  set_property(SOURCE src/software/cpu/arch3d_sse.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE -D__SSE__"
  )
  set_property(SOURCE src/software/cpu/arch2d_sse2.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE2__"
  )
  set_property(SOURCE src/software/cpu/arch3d_sse2.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE2 -D__SSE2__"
  )
  set_property(SOURCE src/software/cpu/arch2d_sse3.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE3__"
  )
  set_property(SOURCE src/software/cpu/arch3d_sse3.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE2 -D__SSE3__"
  )
  set_property(SOURCE src/software/cpu/arch2d_sse4.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE4__"
  )
  set_property(SOURCE src/software/cpu/arch2d_sse_vex.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
  )
  set_property(SOURCE src/software/cpu/arch3d_sse_vex.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX -D__AVX__"
  )
  set_property(SOURCE src/software/cpu/arch2d_avx.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
  )
  set_property(SOURCE src/software/cpu/arch3d_avx.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX -D__AVX__"
  )
  set_property(SOURCE src/software/cpu/arch2d_fma3.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX2 /fp:fast -D__FMA__"
  )
  set_property(SOURCE src/software/cpu/arch3d_fma3.c APPEND
               PROPERTY COMPILE_FLAGS  "/arch:AVX2 -D__FMA__"
  )

  # don't use a /O[012x] flag if you want custom optimizations
  set(SPEED_OPTIMIZATIONS "/Ob2 /Oi /Ot /Oy /GT /GF /GS-")
  set(SIZE_OPTIMIZATIONS "/O1")
  # without inlining it's unusable, use custom optimizations again
  set(MIN_OPTIMIZATIONS "/Ob1")

  set(CMAKE_C_FLAGS_DEBUG "/LDd /MDd /Zi /D_DEBUG ${MIN_OPTIMIZATIONS}")
  set(CMAKE_C_FLAGS_RELEASE "/LD /MD /Zi /D NDEBUG /DRELEASE ${SPEED_OPTIMIZATIONS}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/LD /MD /Zi /DNDEBUG /DRELEASE ${SPEED_OPTIMIZATIONS}")
  set(CMAKE_C_FLAGS_MINSIZEREL "/LD /MD /DNDEBUG /DRELEASE ${SIZE_OPTIMIZATIONS}")

  set(CMAKE_CXX_FLAGS_DEBUG "/LDd /MDd /Zi /D_DEBUG ${MIN_OPTIMIZATIONS}")
  set(CMAKE_CXX_FLAGS_RELEASE "/LD /MD /Zi /D NDEBUG ${SPEED_OPTIMIZATIONS}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/LD /MD /Zi /DNDEBUG ${SPEED_OPTIMIZATIONS}")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "/LD /MD /DNDEBUG ${SIZE_OPTIMIZATIONS}")

  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)

  option(FORCE_STATIC_VCRT "Force /MT for static VC runtimes" ON)
  if(FORCE_STATIC_VCRT)
    foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
               string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
    endforeach(flag_var)
  endif()
else(MSVC)
  if(GCC AND LTO)
    set(C_FLAGS "${C_FLAGS} -flto=auto")
  elseif (TINYCC)
    set(C_FLAGS "${C_FLAGS} -b -Wall")
  endif(GCC AND LTO)

  if(NOT MINGW)
    if (GCC)
      set(C_FLAGS "${C_FLAGS} -Wl,--hash-style=both")
     endif(GCC)
    if (GCC OR CLANG)
      set(C_FLAGS "${C_FLAGS} -fPIC -Wpointer-arith -Wparentheses")
     endif(GCC OR CLANG)
   else(NOT MINGW)
     add_definitions(-D__STRICT_ANSI__)
     set(C_FLAGS "${C_FLAGS} -g -Werror=format-security -std=c11 -Wall -fvisibility=hidden -funroll-loops")
     set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--out-implib,${LIBAEONWAVE}.lib")
     foreach(flag_var
             CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
             CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
             CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
             CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
             if(${flag_var} MATCHES "-mthreads")
               string(REGEX REPLACE "-mthreads" "" ${flag_var} "${${flag_var}}")
             endif()
     endforeach(flag_var)
   endif(NOT MINGW)

   if (CLANG)
     set(C_FLAGS "${C_FLAGS} -ffp-contract=fast" CACHE STRING
         "Hide unused clang options"
         FORCE)
   endif(CLANG)

   if (GCC OR CLANG)
     set(CXX_FLAGS "${CXX_FLAGS} -g -std=c++17" CACHE STRING
         "Flags used by al build types."
         FORCE)
     set(C_FLAGS "${C_FLAGS} -g -Werror=format-security -std=c11 -Wall -fvisibility=hidden -funroll-loops" CACHE STRING
         "Flags used by al build types."
         FORCE)

     set(RELEASE_FLAGS "-O2 -DRELEASE=1")

     # Note: CMAKE_C_FLAGS_RELWITHDEBINFO adds profiler flags
     set(CMAKE_C_FLAGS_RELWITHDEBINFO "${C_FLAGS} -g ${RELEASE_FLAGS} -DNDEBUG" CACHE STRING
         "Flags used by the compiler during Release with Debug Info builds."
         FORCE)
     set(CMAKE_C_FLAGS_MINSIZEREL "${C_FLAGS} -O2 -s -fomit-frame-pointer -DNDEBUG -DRELEASE=1" CACHE STRING
         "Flags used by the compiler during release minsize builds."
         FORCE)
     set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} ${RELEASE_FLAGS} -fomit-frame-pointer -DNDEBUG"
             CACHE STRING "Flags used by the compiler during release builds"
            FORCE)
     set(CMAKE_C_FLAGS_DEBUG "${C_FLAGS} -g3 -D_DEBUG" CACHE STRING
         "Flags used by the compiler during debug builds."
         FORCE)

     set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${C_FLAGS} -g ${RELEASE_FLAGS} -DNDEBUG" CACHE STRING
         "Flags used by the compiler during Release with Debug Info builds."
         FORCE)
     set(CMAKE_CXX_FLAGS_MINSIZEREL "${C_FLAGS} -O2 -s -fomit-frame-pointer -DNDEBUG" CACHE STRING
         "Flags used by the compiler during release minsize builds."
         FORCE)
     set(CMAKE_CXX_FLAGS_RELEASE "${C_FLAGS} ${RELEASE_FLAGS}"
             CACHE STRING "Flags used by the compiler during release builds"
            FORCE)
     set(CMAKE_CXX_FLAGS_DEBUG "${C_FLAGS} -g3 -D_DEBUG" CACHE STRING
         "Flags used by the compiler during debug builds."
         FORCE)

     if(X86_64)
       set(C_FLAGS "${C_FLAGS} -march=athlon64 -DARCH64=1 -falign-functions=32 -falign-loops=32 -ftree-vectorize -ftree-slp-vectorize ")
       set_property(SOURCE src/software/cpu/arch2d_sse.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_sse.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2"
       )
       set_property(SOURCE src/software/cpu/arch2d_sse2.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_sse2.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2"
       )
       set_property(SOURCE src/software/cpu/arch2d_sse3.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3 -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_sse3.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3 -O2"
       )
       set_property(SOURCE src/software/cpu/arch2d_sse4.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse4.1 -march=silvermont -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch2d_sse_vex.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_sse_vex.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge -O2"
       )
       set_property(SOURCE src/software/cpu/arch2d_avx.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_avx.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge -O2"
       )
       set_property(SOURCE src/software/cpu/arch2d_fma3.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -mfma -march=haswell -O3 -ffast-math"
       )
       set_property(SOURCE src/software/cpu/arch3d_fma3.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -mfma -march=haswell -O2"
       )
       set_property(SOURCE src/software/cpu/rbuf_effects.c  APPEND
                   PROPERTY COMPILE_FLAGS "${C_FLAGS} ${OpenACC_C_FLAGS}"
       )
     elseif(X86)
       message("Unsupported build environment: X86")
     elseif(ARM)
       if(ARM64)
         set(C_FLAGS "${C_FLAGS} -march=armv8-a -moutline-atomics -DARCH64=1")
         set_property(SOURCE src/software/cpu/arch3d_vfpv3.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2 -D__ARM_VFPV3 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch2d_vfpv3.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3 -ffast-math -D__ARM_VFPV3 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch3d_vfpv4.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2 -D__ARM_VFPV4 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch2d_vfpv4.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3 -ffast-math -D__ARM_VFPV4 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch3d_neon.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch2d_neon.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3 -ffast-math -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch3d_neon64.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O2 -ftree-vectorize -ftree-slp-vectorize"
         )
         set_property(SOURCE src/software/cpu/arch2d_neon64.c APPEND
                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -O3  -ffast-math -ftree-vectorize -ftree-slp-vectorize"
         )
       else(ARM64) # ARM32
         message(FATAL_ERROR "Unsupported build environment: ARM")
       endif(ARM64)
     else()
       message("Build environment: Other (${CMAKE_SYSTEM_PROCESSOR})")
     endif()
     set_property(SOURCE src/software/cpu/arch2d_cpu.c APPEND
                  PROPERTY COMPILE_FLAGS "-O3 -ffast-math"
     )
     set_property(SOURCE 3rdparty/pdmp3.c APPEND
                  PROPERTY COMPILE_FLAGS "-O3 -ffast-math -DIMDCT_TABLES -DIMDCT_NTABLES -DPOW34_TABLE"
     )
   endif(GCC OR CLANG)

   message("Build type: ${CMAKE_BUILD_TYPE}")
   message("C flags: ${C_FLAGS}")
   message("C++ flags: ${CXX_FLAGS}")
   message("Linker flags: ${CMAKE_SHARED_LINKER_FLAGS}")
endif(MSVC)

# Check for the dlopen API (for alGetProcAddress)
check_include_file(windows.h HAVE_WINDOWS_H -D_WIN32_WINNT=0x0501)
if(NOT WIN32) 
check_include_file(iconv.h HAVE_ICONV_H)
endif()
check_include_file(locale.h HAVE_LOCALE_H)
check_include_file(threads.h HAVE_THREADS_H)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strlcpy HAVE_STRLCPY)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file(assert.h HAVE_ASSERT_H)
check_include_file(syslog.h HAVE_SYSLOG_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(values.h HAVE_VALUES_H)
check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(time.h HAVE_TIME_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/ioctl.h HAVE_IOCTL_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/random.h HAVE_SYS_RANDOM_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(Winsock2.h HAVE_WINSOCK2_H)
check_include_file(math.h HAVE_MATH_H)
check_include_file(io.h HAVE_IO_H)
check_include_file(guiddef.h HAVE_GUIDDEF_H)
check_include_file(initguid.h HAVE_INITGUID_H)
check_include_file(linux/types.h HAVE_LINUX_TYPES_H)
check_include_file(mmdeviceapi.h HAVE_MMDEVICEAPI_H)
check_include_file(pulse/pulseaudio.h HAVE_PULSEAUDIO_H)
if(UNIX)
  check_include_file(pthread.h HAVE_PTHREAD_H)
  check_include_file(arm_neon.h HAVE_ARM_NEON_H)
  check_include_file(linux/hpet.h HAVE_HPET_H)
endif(UNIX)

if(HAVE_PTHREAD_H)
  check_library_exists(pthread pthread_create "" HAVE_LIBPTHREAD)
  if(HAVE_LIBPTHREAD)
    add_definitions(-D_REENTRANT -D_POSIX_C_SOURCE=199309L -D__USE_POSIX199309)
    set(EXTRA_LIBS pthread ${EXTRA_LIBS})
  endif(HAVE_LIBPTHREAD)
endif(HAVE_PTHREAD_H)

check_library_exists(rt clock_getres "" HAVE_LIBRT)
if(HAVE_LIBRT)
  set(EXTRA_LIBS rt ${EXTRA_LIBS})
endif(HAVE_LIBRT)

if(HAVE_MATH_H)
  check_library_exists(m random "" HAVE_LIBM)
  if(HAVE_LIBM)
    set(EXTRA_LIBS m ${EXTRA_LIBS})
  endif(HAVE_LIBM)
endif(HAVE_MATH_H)

if(HAVE_DLFCN_H)
  check_library_exists(dl dlopen "" HAVE_LIBDL)
  if(HAVE_LIBDL)
    set(EXTRA_LIBS dl ${EXTRA_LIBS})
  endif(HAVE_LIBDL)
endif(HAVE_DLFCN_H)

add_subdirectory(test)

configure_file(
  "${AeonWave_SOURCE_DIR}/include/aax/aax.h.in"
  "${AeonWave_BINARY_DIR}/include/aax/aax.h"
)

aax_subdirectory(3rdparty)
aax_subdirectory(base)
aax_subdirectory(support)
aax_subdirectory(src)

# Build a static library
add_library(AAXOBJ_LIB OBJECT ${SOURCES})
add_library(${LIBAEONWAVE}-static STATIC $<TARGET_OBJECTS:AAXOBJ_LIB>)

# Build a shared library
set(LIB_VERSION "${CPACK_PACKAGE_SOVERSION_MAJOR}.${CPACK_PACKAGE_SOVERSION_MINOR}.${CPACK_PACKAGE_SOVERSION_PATCH}")
add_library(${LIBAEONWAVE} SHARED $<TARGET_OBJECTS:AAXOBJ_LIB>)
set_target_properties(${LIBAEONWAVE}
  PROPERTIES DEFINE_SYMBOL AAX_BUILD_LIBRARY
  VERSION ${LIB_VERSION}
  SOVERSION ${CPACK_PACKAGE_SOVERSION_MAJOR}
)
if(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
  set_target_properties(${LIBAEONWAVE} PROPERTIES PREFIX "")
else()
  set_target_properties(${LIBAEONWAVE} PROPERTIES COMPILE_FLAGS "-shared")
endif()

target_link_libraries(${LIBAEONWAVE}
  ${EXTRA_LIBS}
  ${XML_LIBRARY}
)

# Create the copyright file
execute_process(COMMAND "xxd" -include COPYING.v3 support/copyright.h
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE varRes
)

# Add an install target here
# Note Windows DLL are specified by RUNTIME
install(TARGETS ${LIBAEONWAVE}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Libraries
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Headers
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" NAMELINK_SKIP COMPONENT Libraries
)
install(TARGETS ${LIBAEONWAVE}-static
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Libraries
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Headers
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" NAMELINK_SKIP COMPONENT Libraries
)
install(TARGETS ${LIBAEONWAVE}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" NAMELINK_ONLY
        COMPONENT Headers
)
if(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
  set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
  !include Library.nsh
  !include x64.nsh
  Var /GLOBAL ALREADY_INSTALLED
    ifFileExists \\\"$SYSDIR\\\\${LIBAEONWAVE}.dll\\\" 0 new_installation
      StrCpy $ALREADY_INSTALLED 1
    new_installation:
    SetOutPath \\\"$SYSDIR\\\"
    SetOverwrite IfNewer
    ${If} ${RunningX64}
      !define LIBRARY_X64
      !insertmacro installLib DLL $ALREADY_INSTALLED REBOOT_PROTECTED \\\"${AeonWave_BINARY_DIR}\\\\${LIBAEONWAVE}.dll\\\" \\\"${LIBAEONWAVE}.dll\\\" \\\"$SYSDIR\\\"
      !undef LIBRARY_X64
    ${EndIf}
  ")
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    !include Library.nsh
    !include x64.nsh
    SetOutPath \\\"$SYSDIR\\\"
    ${If} ${RunningX64}
      !define LIBRARY_X64
      !insertmacro uninstallLib DLL SHARED REBOOT_NOTPROTECTED \\\"$SYSDIR\\\\bin\\\\${LIBAEONWAVE}.dll\\\"
    ${EndIf}
  ")
endif(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")

install(FILES
        ${AeonWave_BINARY_DIR}/include/aax/aax.h
        include/aax/aeonwave.hpp
        include/aax/aeonwave
        include/aax/strings
        include/aax/matrix
        DESTINATION "include/${AEONWAVE_DIR}"
        COMPONENT Headers
)

if(WIN32)
  set(SHARED_DATA_SUBDIR "${CMAKE_INSTALL_DATAROOTDIR}")
  set(APP_DATA_DIR "$ENV{ProgramFiles}\\${CPACK_PACKAGE_INSTALL_DIRECTORY}\\${SHARED_DATA_SUBDIR}")
  string(REPLACE "\\\\" "\\" APP_DATA_DIR ${APP_DATA_DIR})
  string(REPLACE "\\" "\\\\" APP_DATA_DIR ${APP_DATA_DIR})
  string(REPLACE "/" "\\\\" APP_DATA_DIR ${APP_DATA_DIR})
else()
  set(SHARED_DATA_SUBDIR "${CMAKE_INSTALL_DATAROOTDIR}/${AEONWAVE_DIR}")
  set(APP_DATA_DIR "/usr/${SHARED_DATA_SUBDIR}")
endif()
message("SHARED_DATA_SUBDIR: ${SHARED_DATA_SUBDIR}")
message("APP_DATA_DIR: ${APP_DATA_DIR}")

install( DIRECTORY
         admin/presets/
	 DESTINATION "${SHARED_DATA_SUBDIR}"
         COMPONENT Data
         PATTERN "*.in" EXCLUDE
)

if (UNIX AND NOT WIN32)
  macro(install_symlink filepath sympath)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filepath} ${sympath})")
    install(CODE "message(\"-- Created symlink: ${sympath} -> ${filepath}\")")
  endmacro(install_symlink)

  install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}${CPACK_PACKAGE_SOVERSION_MAJOR}.so.${CPACK_PACKAGE_SOVERSION_MAJOR} ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}${CPACK_PACKAGE_SOVERSION_MAJOR}.so.${CPACK_PACKAGE_SOVERSION_MAJOR})
  install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}${CPACK_PACKAGE_SOVERSION_MAJOR}.so ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}${CPACK_PACKAGE_SOVERSION_MAJOR}.so)
endif(UNIX AND NOT WIN32)

if(NOT SINGLE_PACKAGE)
  include (CPack)
  configure_file(
    ${PROJECT_SOURCE_DIR}/admin/nsis/NSIS.definitions.nsh.in
    ${CMAKE_CURRENT_BINARY_DIR}/admin/nsis/NSIS.definitions.nsh
  )
endif(NOT SINGLE_PACKAGE)

message("AAX version: ${AAX_VERSION}")
message("API version: ${AAX_SOVERSION}")

if (NOT TIMESTAMP)
  string(TIMESTAMP YEAR "%Y")
endif(NOT TIMESTAMP)
configure_file(
  "${AeonWave_SOURCE_DIR}/admin/presets/gmmidi.xml.in"
  "${AeonWave_SOURCE_DIR}/admin/presets/gmmidi.xml"
)

configure_file(
  "${AeonWave_SOURCE_DIR}/admin/presets/gmdrums.xml.in"
  "${AeonWave_SOURCE_DIR}/admin/presets/gmdrums.xml"
)
configure_file(
  "${AeonWave_SOURCE_DIR}/include/config.h.in"
  "${AeonWave_BINARY_DIR}/include/config.h"
)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
configure_file(
  "${AeonWave_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY
)
add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake"
)

