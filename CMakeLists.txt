cmake_minimum_required(VERSION 2.8.8)
set(cmake_verbose_makefile TRUE)

set(CMAKE_MAKEFILE_DEPENDS
    "CMakeModules/Symlink.cmake"
)

if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)

project(AeonWave C)
set(PACKAGE_NAME "AeonWave")
set(PACKAGE "libaax3")

option(LTO "Enabel linkt-time optimizations" OFF)
option(RELEASE "Enable for final release builds" OFF)
option(DLOPEN "Check for the dlopen API for loading optional libs" ON)
option(WERROR "Treat compile warnings as errors"   OFF)
option(RMALLOC "Enable memory debugging functions" OFF)

add_definitions(-D_XOPEN_SOURCE=600)
if(WIN32)
  set(CMAKE_GNUtoMS ON)
  set(EXTRA_LIBS ${EXTRA_LIBS} "-lwsock32 -lws2_32 -lwinmm")
  add_definitions(-DAAX_BUILD_LIBRARY=1)
  add_definitions(-D_WIN32 -DWIN32)
  set(LIBAEONWAVE AAX)
else(WIN32)
  add_definitions(-D_GNU_SOURCE=1)
  set(LIBAEONWAVE aax)
endif(WIN32)

if(NOT LIBTYPE)
  set(LIBTYPE SHARED)
endif(NOT LIBTYPE)

MACRO (TODAY RESULT)
  if (WIN32)
    EXECUTE_PROCESS(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "... (..)/(..)/..(..).*" "\\3\\1\\2" ${RESULT} ${${RESULT}})
  elseif(UNIX)
    EXECUTE_PROCESS(COMMAND "date" "+%d/%m/%Y" OUTPUT_VARIABLE ${RESULT})
    string(REGEX REPLACE "(..)/(..)/..(..).*" "\\3\\2\\1" ${RESULT} ${${RESULT}})
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

# read 'version' file into a variable (stripping any newlines or spaces)
TODAY(DATE)
FILE(READ version versionFile)
STRING(STRIP ${versionFile} AAX_VERSION)
set(VERSION AAX_VERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_VERSION ${AAX_VERSION})

set(AAX_MAJOR_VERSION ${CMAKE_MATCH_1})
set(AAX_MINOR_VERSION ${CMAKE_MATCH_2})
set(AAX_MICRO_VERSION ${CMAKE_MATCH_3})
set(AAX_PATCH_LEVEL ${DATE})

FILE(READ soversion soVersionFile)
STRING(STRIP ${soVersionFile} AAX_SOVERSION)
set(SOVERSION AAX_SOVERSION)

# split version string into components, note CMAKE_MATCH_0 is the entire regexp match
STRING(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" CPACK_PACKAGE_SOVERSION ${AAX_SOVERSION})

set(CPACK_PACKAGE_SOVERSION_MAJOR ${CMAKE_MATCH_1})
set(CPACK_PACKAGE_SOVERSION_MINOR ${CMAKE_MATCH_2})
set(CPACK_PACKAGE_SOVERSION_PATCH ${CMAKE_MATCH_3})

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross platform, hardware accelerated, multi-channel and 4D capable audio rendering engine")

# We have some custom .cmake scripts not in the official distribution.
set(CMAKE_MODULE_PATH "${AeonWave_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")
include(CPack_Modules)
include(CPack_Defaults)
include(inline)

if (WIN32)
else(WIN32)

  # DEBIAN
  set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1), debconf")
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "alsa-base | oss4-base")
  set(CPACK_DEBIAN_PACKAGE_SUGGESTS "libmpg123-0, libopus0")
  set(CPACK_DEBIAN_PACKAGE_REPLACES "libaax2-bin, libaax2-dev, libaax3-bin, libaax3-dev")
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${AeonWave_SOURCE_DIR}/admin/deb/postinst;${AeonWave_SOURCE_DIR}/admin/deb/postrm;")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

  # RPM
  set(CPACK_RPM_PACKAGE_LICENSE "License For Customer Use of ADALIN AEONWAVE AUDIO EXTENSIONS LIBRARY Software")
  set(CPACK_RPM_PACKAGE_GROUP "System/Libraries")
  set(CPACK_RPM_PACKAGE_PROVIDES "libaax2, libaax3")
endif(WIN32)

# ZIP
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${AAX_VERSION}" CACHE INTERNAL "tarball basename")

# Required libraries
FIND_PACKAGE(XML REQUIRED)
if(NOT XML_FOUND)
  MESSAGE(FATAL_ERROR "ERROR: required XML library is not installed, aborting.")
endif(NOT XML_FOUND)

# Add definitions, compiler switches, etc.
include_DIRECTORIES("${AeonWave_SOURCE_DIR}/src"
    ${AeonWave_SOURCE_DIR}
    ${XML_include_DIR}
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

if(RELEASE)
  set(C_FLAGS "${C_FLAGS} -DRELEASE=1")
endif(RELEASE)

if(MSVC)
    set(C_FLAGS "${C_FLAGS} /NODEFAULTLIB:msvcrt /NODEFAULTLIB:msvcrtd /NODEFAULTLIB:libcmt")
    set_PROPERTY(SOURCE src/software/cpu/arch2d_sse.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE /fp:fast -D__SSE__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch3d_sse.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE /fp:fast -D__SSE__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_sse2.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE2__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_sse3.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE3__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch3d_sse3.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE3__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_sse2.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:SSE2 /fp:fast -D__SSE2__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_sse_vex.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch3d_sse_vex.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_avx.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch3d_avx.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:AVX /fp:fast -D__AVX__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_fma3.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:AVX2 /fp:fast -D__FMA__"
    )
    set_PROPERTY(SOURCE src/software/cpu/arch2d_fma4.c APPEND
                 PROPERTY COMPILE_FLAGS  "/arch:XOP /fp:fast -D__FMA__"
    )

    # don't use a /O[012x] flag if you want custom optimizations
    set(SPEED_OPTIMIZATIONS "/Ob2 /Oi /Ot /Oy /GT /GF /GS-")
    set(SIZE_OPTIMIZATIONS "/O1")
    # without inlining it's unusable, use custom optimizations again
    set(MIN_OPTIMIZATIONS "/Ob1")

    set(CMAKE_C_FLAGS_DEBUG "/LDd /MDd /Zi /D_DEBUG ${MIN_OPTIMIZATIONS}")
    set(CMAKE_C_FLAGS_RELEASE "/LD /MD /Zi /D NDEBUG ${SPEED_OPTIMIZATIONS}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/LD /MD /Zi /DNDEBUG ${SPEED_OPTIMIZATIONS}")
    set(CMAKE_C_FLAGS_MINSIZEREL "/LD /MD /DNDEBUG ${SIZE_OPTIMIZATIONS}")

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)

    option(FORCE_STATIC_VCRT "Force /MT for static VC runtimes" ON)
    if(FORCE_STATIC_VCRT)
        FOREACH(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                STRING(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
            endif()
        ENDFOREACH(flag_var)
    endif()
else(MSVC)
    if(GCC AND LTO)
       set(C_FLAGS "${C_FLAGS} -flto")
    elseif (TINYCC)
       set(C_FLAGS "${C_FLAGS} -b -Wall")
    endif(GCC AND LTO)

    if(NOT MINGW)
       if (GCC OR CLANG)
         set(C_FLAGS "${C_FLAGS} -fPIC -Wl,--hash-style=both -Wpointer-arith -Wparentheses")
       endif(GCC OR CLANG)
    else(NOT MINGW)
       include_DIRECTORIES ("${AeonWave_SOURCE_DIR}/src/windows/mingw")
       add_definitions(-U__STRICT_ANSI__)
       set(C_FLAGS "${C_FLAGS} -static-libgcc")
       FOREACH(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "-mthreads")
                STRING(REGEX REPLACE "-mthreads" "" ${flag_var} "${${flag_var}}")
            endif()
        ENDFOREACH(flag_var)
    endif(NOT MINGW)

    if (CLANG)
      set(C_FLAGS "${C_FLAGS} -Qunused-arguments -ffp-contract=fast" CACHE STRING
          "Hide unused clang options"
          FORCE)
    elseif (MIPSPRO)
      set(CMAKE_C_COMPILER c99)
      set(C_FLAGS "${C_FLAGS} -c99 -Ofast -n32 -IPA" CACHE STRING
          "fastest options"
          FORCE)
    endif(CLANG)

    if (GCC OR CLANG)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" CACHE STRING
          "Flags used by al build types."
          FORCE)
      set(C_FLAGS "${C_FLAGS} -Werror=format-security -std=c99 -Wall -fvisibility=hidden -funroll-loops" CACHE STRING
          "Flags used by al build types."
          FORCE)

      # Note: CMAKE_C_FLAGS_RELWITHDEBINFO adds profiler flags
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${C_FLAGS} -O2 -fstack-protector-strong -g -DNDEBUG" CACHE STRING
          "Flags used by the compiler during Release with Debug Info builds."
          FORCE)
      set(CMAKE_C_FLAGS_MINSIZEREL "${C_FLAGS} -Os -s -fomit-frame-pointer -DNDEBUG" CACHE STRING
          "Flags used by the compiler during release minsize builds."
          FORCE)
      set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} -Os -s -fomit-frame-pointer -DNDEBUG"
              CACHE STRING "Flags used by the compiler during release builds"
             FORCE)
      set(CMAKE_C_FLAGS_DEBUG "${C_FLAGS} -fstack-protector-strong -g3 -D_DEBUG" CACHE STRING
          "Flags used by the compiler during debug builds."
          FORCE)


      if(X86_64)
          message("Build environment: X86_64")
          set(C_FLAGS "${C_FLAGS} -march=athlon64 -msse2 -mfpmath=sse -DI64=1")
          set_PROPERTY(SOURCE src/software/cpu/arch2d_sse3.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_sse3.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_sse_vex.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_sse_vex.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_avx.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_avx.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx -march=sandybridge"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_fma3.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -mavx2 -mfma -march=haswell"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_fma4.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -mxop -mfma4 -march=bdver1"
          )

#         set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} -Os -s -fomit-frame-pointer -DNDEBUG"
#             CACHE STRING "Flags used by the compiler during release builds"
#             FORCE)

      elseif(X86)
          message("Build environment: X86")
          set(C_FLAGS "${C_FLAGS} -march=i686 -DI32=1")
          set_PROPERTY(SOURCE src/software/cpu/arch2d_sse.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse -mfpmath=sse -march=pentium3m"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_sse.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse -mfpmath=sse -march=pentium3m"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_sse2.c APPEND
                    PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse2 -mfpmath=sse -march=pentium-m"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_sse3.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3 -mfpmath=sse -march=prescott"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_sse3.c APPEND
                     PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse3 -mfpmath=sse -march=prescott"
          )
#         set_PROPERTY(SOURCE 3rdparty/pffft.c APPEND
#                      PROPERTY COMPILE_FLAGS "${C_FLAGS} -msse -mfpmath=sse -march=pentium3m"
#         )
      elseif(ARM)
          message("Build environment: ARM")
          set(C_FLAGS "${C_FLAGS} -march=armv5 -marm -DA32=1")
          set_PROPERTY(SOURCE src/software/cpu/arch3d_vfpv2.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -mfpu=vfp -mfloat-abi=hard -D__ARM_VFPV2"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_vfpv2.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -mfpu=vfp -mfloat-abi=hard -D__ARM_VFPV2"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_vfpv3.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -march=armv7-a -mthumb -mfpu=vfpv3-d16 -mfloat-abi=hard -D__ARM_VFPV3"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_vfpv3.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -march=armv7-a -mthumb -mfpu=vfpv3-d16 -mfloat-abi=hard -D__ARM_VFPV3"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch3d_neon.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -march=armv7-a -mthumb -mfpu=neon"
          )
          set_PROPERTY(SOURCE src/software/cpu/arch2d_neon.c APPEND
                       PROPERTY COMPILE_FLAGS "${C_FLAGS} -march=armv7-a -mthumb -mfpu=neon -funsafe-math-optimizations"
          )

#         set(CMAKE_C_FLAGS_RELEASE "${C_FLAGS} -O3 -s -fomit-frame-pointer -DNDEBUG"
#             CACHE STRING "Flags used by the compiler during release builds"
#             FORCE)
      else()
          message("Build environment: Other (${CMAKE_SYSTEM_PROCESSOR})")
      endif()
      set_PROPERTY(SOURCE 3rdparty/pdmp3.c APPEND
                   PROPERTY COMPILE_FLAGS "${C_FLAGS} -Os -DIMDCT_TABLES -DIMDCT_NTABLES -DPOW34_TABLE"
          )
   endif(GCC OR CLANG)

    message("Build type: ${CMAKE_BUILD_TYPE}")
    message("Compiler flags: ${C_FLAGS}")
    message("Linker flags: ${CMAKE_SHARED_LINKER_FLAGS}")
endif(MSVC)

# Check for the dlopen API (for alGetProcAddress)
CHECK_INCLUDE_FILE(windows.h HAVE_WINDOWS_H -D_WIN32_WINNT=0x0501)
CHECK_FUNCTION_EXISTS(strdup HAVE_STRDUP)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/utsname.h HAVE_SYS_UTSNAME_H)
CHECK_INCLUDE_FILE(assert.h HAVE_ASSERT_H)
CHECK_INCLUDE_FILE(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILE(values.h HAVE_VALUES_H)
CHECK_INCLUDE_FILE(dlfcn.h HAVE_DLFCN_H)
CHECK_INCLUDE_FILE(libio.h HAVE_LIBIO_H)
CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)
CHECK_INCLUDE_FILE(poll.h HAVE_POLL_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/ioctl.h HAVE_IOCTL_H)
CHECK_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILE(Winsock2.h HAVE_WINSOCK2_H)
CHECK_INCLUDE_FILE(math.h HAVE_MATH_H)
CHECK_INCLUDE_FILE(io.h HAVE_IO_H)
CHECK_INCLUDE_FILE(guiddef.h HAVE_GUIDDEF_H)
CHECK_INCLUDE_FILE(initguid.h HAVE_INITGUID_H)
CHECK_INCLUDE_FILE(linux/types.h HAVE_LINUX_TYPES_H)
CHECK_INCLUDE_FILE(mmdeviceapi.h HAVE_MMDEVICEAPI_H)
if(UNIX)
CHECK_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
CHECK_INCLUDE_FILE(arm_neon.h HAVE_ARM_NEON_H)
endif(UNIX)
CHECK_INCLUDE_FILE(xml.h HAVE_XML_H)

if(HAVE_PTHREAD_H)
  CHECK_LIBRARY_EXISTS(pthread pthread_create "" HAVE_LIBPTHREAD)
  if(HAVE_LIBPTHREAD)
    add_definitions(-D_REENTRANT -D_POSIX_C_SOURCE=199309L -D__USE_POSIX199309)
    set(EXTRA_LIBS pthread ${EXTRA_LIBS})
  endif(HAVE_LIBPTHREAD)
endif(HAVE_PTHREAD_H)

CHECK_LIBRARY_EXISTS(rt clock_getres "" HAVE_LIBRT)
if(HAVE_LIBRT)
   set(EXTRA_LIBS rt ${EXTRA_LIBS})
endif(HAVE_LIBRT)

if(HAVE_MATH_H)
  CHECK_LIBRARY_EXISTS(m random "" HAVE_LIBM)
  if(HAVE_LIBM)
    set(EXTRA_LIBS m ${EXTRA_LIBS})
  endif(HAVE_LIBM)
endif(HAVE_MATH_H)

if(HAVE_DLFCN_H)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
  if(HAVE_LIBDL)
    set(EXTRA_LIBS dl ${EXTRA_LIBS})
  endif(HAVE_LIBDL)
endif(HAVE_DLFCN_H)

ADD_SUBDIRECTORY(test)

CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/include/aax/aax.h.in"
    "${AeonWave_BINARY_DIR}/include/aax/aax.h")

CONFIGURE_FILE(
    "${AeonWave_SOURCE_DIR}/include/config.h.in"
    "${AeonWave_BINARY_DIR}/include/config.h")

set( AEONWAVE_OBJS
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
     src/arch.c
     src/devices.c
     src/objects.c
#    src/analyze.c
     src/aax_audioframe.c
     src/aax_buffer.c
     src/aax_driver.c
     src/aax_matrix.c
     src/aax_mixer.c
     src/aax_scenery.c
     src/aax_sensor.c
     src/aax_support.c
     src/aax_emitter.c
     src/aax_eventmgr.c
     src/aax_instrument.c
     src/aax_midi.c
     src/aax_filters.c
     src/aax_effects.c
     src/dsp/filters.c
     src/dsp/effects.c
     src/dsp/effect_chorus.c
     src/dsp/effect_distortion.c
     src/dsp/effect_dynamic_pitch.c
     src/dsp/effect_flanging.c
     src/dsp/effect_phasing.c
     src/dsp/effect_pitch.c
     src/dsp/effect_reverb.c
     src/dsp/effect_convolution.c
     src/dsp/effect_timed_pitch.c
     src/dsp/effect_velocity.c
     src/dsp/filter_angular.c
     src/dsp/filter_compressor.c
     src/dsp/filter_distance.c
     src/dsp/filter_dynamic_gain.c
     src/dsp/filter_equalizer.c
     src/dsp/filter_frequency.c
     src/dsp/filter_graphic_equalizer.c
     src/dsp/filter_timed_gain.c
     src/dsp/filter_volume.c
     src/alsa/asound.c
     src/alsa/kernel.c
     src/dmedia/device.c
     src/oss/device.c
     src/stream/device.c
     src/stream/io.c
     src/stream/io_file.c
     src/stream/io_socket.c
     src/stream/protocol.c
     src/stream/prot_direct.c
     src/stream/prot_http.c
     src/stream/extension.c
     src/stream/ext_raw.c
     src/stream/ext_wav.c
     src/stream/ext_ogg.c
     src/stream/format.c
     src/stream/fmt_pcm.c
     src/stream/fmt_mp3.c
     src/stream/fmt_vorbis.c
     src/stream/fmt_flac.c
     src/stream/fmt_opus.c
     src/stream/fmt_binary.c
     src/android/opensl.c
     src/software/device.c
     src/software/arch.c
     src/software/arch_x86.c
     src/software/arch_arm.c
     src/software/mixer.c
     src/software/frame.c
     src/software/sensor.c
     src/software/emitter.c
     src/software/ringbuffer.c
     src/software/rbuf_lfo.c
     src/software/rbuf_codecs.c
     src/software/rbuf_codec_tables.c
     src/software/rbuf_mixers.c
     src/software/rbuf_mixmulti.c
     src/software/rbuf2d_mixsingle.c
     src/software/rbuf3d_mixsingle.c
     src/software/renderer.c
     src/software/cpu/render_monolithic.c
     src/software/cpu/render_threadpool.c
     src/software/cpu/waveforms.c
     src/software/cpu/rbuf_effects.c
     src/software/cpu/rbuf_limiter.c
     src/software/cpu/rbuf_limiter_tables.c
     src/software/cpu/rbuf_mixmulti.c
     src/software/cpu/rbuf_mixsingle.c
     src/software/cpu/arch3d_vfpv2.c
     src/software/cpu/arch3d_vfpv3.c
     src/software/cpu/arch3d_neon.c
     src/software/cpu/arch3d_sse.c
     src/software/cpu/arch3d_sse3.c
     src/software/cpu/arch2d_cpu.c
     src/software/cpu/arch2d_vfpv2.c
     src/software/cpu/arch2d_vfpv3.c
     src/software/cpu/arch2d_neon.c
     src/software/cpu/arch2d_sse2.c
     src/software/cpu/arch2d_sse3.c
     3rdparty/stb_vorbis.c
     3rdparty/pdmp3.c
#    3rdparty/pffft.c
)
if(SIZEOF_SIZE_T EQUAL 8)
  set(AEONWAVE_OBJS ${AEONWAVE_OBJS}
     src/software/cpu/arch3d_sse_vex.c
     src/software/cpu/arch2d_sse_vex.c
     src/software/cpu/arch2d_avx.c
     src/software/cpu/arch3d_avx.c
     src/software/cpu/arch2d_fma3.c
     src/software/cpu/arch2d_fma4.c
  )
endif(SIZEOF_SIZE_T EQUAL 8)

if(HAVE_WINDOWS_H)
  set(AEONWAVE_OBJS ${AEONWAVE_OBJS}
     src/windows/wasapi.c
     src/windows/audio.c
  )
endif(HAVE_WINDOWS_H)

# Build a static library
add_library(AAXOBJ_LIB OBJECT ${AEONWAVE_OBJS})
add_library(${LIBAEONWAVE}-static STATIC $<TARGET_OBJECTS:AAXOBJ_LIB>)

# Build a shared library
set(LIB_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
ADD_LIBRARY(${LIBAEONWAVE} SHARED $<TARGET_OBJECTS:AAXOBJ_LIB>)
set_TARGET_PROPERTIES(${LIBAEONWAVE}
    PROPERTIES DEFINE_SYMBOL AAX_BUILD_LIBRARY
    VERSION ${LIB_VERSION}
    SOVERSION ${CPACK_PACKAGE_SOVERSION_MAJOR}
)
if(WIN32 AND NOT LIBTYPE STREQUAL "STATIC")
    set_TARGET_PROPERTIES(${LIBAEONWAVE} PROPERTIES PREFIX "")
else()
    set_TARGET_PROPERTIES(${LIBAEONWAVE} PROPERTIES COMPILE_FLAGS "-shared")
endif()

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

INSTALL(FILES
        admin/presets/genres.xml
        DESTINATION etc/aax
        COMPONENT Libraries
)
INSTALL(FILES 
        ${AeonWave_BINARY_DIR}/include/aax/aax.h
        include/aax/aeonwave.hpp
        include/aax/strings.hpp
        include/aax/matrix.hpp
        DESTINATION include/aax
        COMPONENT Headers
)
INSTALL(FILES 
        DESTINATION include
        COMPONENT Headers
)

if (UNIX AND NOT WIN32)
    macro(install_symlink _dest _source)
      install(CODE "
             include(\"${AeonWave_SOURCE_DIR}/CMakeModules/Symlink.cmake\")
             InstallSymlink(\"${_dest}\" \"\$ENV{DESTDIR}${_source}\")")
    endmacro(install_symlink)

    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR} ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so.${CPACK_PACKAGE_VERSION_MAJOR})
    install_symlink(${CMAKE_LIBRARY_ARCHITECTURE}/lib${LIBAEONWAVE}.so ${CMAKE_INSTALL_PREFIX}/lib/lib${LIBAEONWAVE}.so)
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

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${AeonWave_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
  "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${AeonWave_BINARY_DIR}/cmake_uninstall.cmake")
