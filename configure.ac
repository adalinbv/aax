dnl Process this file with aclocal ; automake -a ; autoconf to produce a 
dnl working configure script.
dnl

AC_INIT(include/aaxdefs.h)

dnl Detect the canonical host and target build environment
AC_CANONICAL_HOST

dnl Require at least automake 2.58
AC_PREREQ(2.58)

#
# Making releases:
#   MICRO_VERSION += 1;
#   INTERFACE_AGE += 1;
#   BINARY_AGE += 1;
# if any functions have been added, set INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set BINARY_AGE and INTERFACE_AGE to 0.
#
AAX_MAJOR_VERSION=2
AAX_MINOR_VERSION=2
AAX_MICRO_VERSION=0
AAX_INTERFACE_AGE=0
AAX_BINARY_AGE=0
AAX_VERSION=$AAX_MAJOR_VERSION.$AAX_MINOR_VERSION.$AAX_MICRO_VERSION

AC_SUBST(AAX_MAJOR_VERSION)
AC_SUBST(AAX_MINOR_VERSION)
AC_SUBST(AAX_MICRO_VERSION)
AC_SUBST(AAX_CONFIG_VERSION)
AC_SUBST(AAX_INTERFACE_AGE)
AC_SUBST(AAX_BINARY_AGE)
AC_SUBST(AAX_VERSION)


# libtool versioning
LT_AAX_RELEASE=$AAX_MAJOR_VERSION.$AAX_MINOR_VERSION
LT_AAX_CURRENT=`expr $AAX_MICRO_VERSION - $AAX_INTERFACE_AGE`
LT_AAX_REVISION=$AAX_INTERFACE_AGE
LT_AAX_AGE=`expr $AAX_BINARY_AGE - $AAX_INTERFACE_AGE`

AC_SUBST(LT_AAX_RELEASE)
AC_SUBST(LT_AAX_CURRENT)
AC_SUBST(LT_AAX_REVISION)
AC_SUBST(LT_AAX_AGE)

#dnl Initialize the automake stuff
AM_INIT_AUTOMAKE(AeonWave, $AAX_VERSION)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_C_RESTRICT

#dnl check the size of size_t to detect 32-bit or 64-bit systems
AC_CHECK_SIZEOF([size_t])

dnl system dependent compiler flags
OPTFLAGS="-Os -fPIC -fomit-frame-pointer -funroll-loops"
case "${host}" in
i?86-*-*-*)
  CFLAGS="$CFLAGS -shared"
  if test "$ac_cv_sizeof_size_t" = 4 ; then 
    CFLAGS="$CFLAGS -march=i586"
    case "$CC" in
    *opencc*)
      CFLAGS="$CFLAGS -ipa"
      SSEFLAGS="$OPTFLAGS -march=pentium3m -msse"
      SSE2FLAGS="$OPTFLAGS -march=pentium-m -msse2"
      ;;
    *gcc*)
      CFLAGS="$CFLAGS -Wl,--hash-style=both"
      SSEFLAGS="$OPTFLAGS"
      SSE2FLAGS="$OPTFLAGS"
      ;;
    esac
  fi
  ;;
arm-*-eabi)
  NEONFLAGS="$OPTFLAGS -static -fno-short-enums"
  ;;
esac
AC_SUBST(SSEFLAGS)
AC_SUBST(SSE2FLAGS)
AC_SUBST(NEONFLAGS)

dnl echo "SSE: "$SSEFLAGS
dnl echo "SSE2: "$SSE2FLAGS

dnl options
AC_ARG_ENABLE(debug, 
[  --enable-debug          Enable debugging (disabled by default)],
 [if test "x$enableval" = "xyes"; then
    AC_DEFINE(DEBUG, 1, [Define this to enable nano debug messages.])
    CFLAGS="$CFLAGS -DMALLOC_DEBUG"
    debug_support=yes
 fi])
if test "$debug_support" != "yes"; then
   AC_DEFINE(NDEBUG, 1, [Omit the assert warnings])
fi
AM_CONDITIONAL(NDEBUG, test "x$debug_support" != "xyes")

dnl enable/disable rmalloc
AC_ARG_WITH(rmalloc,
[  --with-rmalloc           Define wether to use rmalloc debuging or not],
 [if test "x$withval" = "xyes"; then
    AC_DEFINE(USE_RMALLOC, 1, [use rmalloc])
 fi])

dnl enable-simd
AC_ARG_ENABLE(lite,
[  --enable-lite           Enable Lite version (disabled by default)],
 [if test "x$enableval" = "xyes"; then
    AC_DEFINE(ENABLE_LITE, 1, [Define this to disable lite version.])
  else
    AC_DEFINE(ENABLE_LITE, 0, [Undefine this to enable lite version.])
 fi], [
  AC_DEFINE(ENABLE_LITE, 0, [Define this to enable lite version.])
])

dnl Checks for header files.
if test "x$GCC" = "xyes" ; then
   CFLAGS="$CFLAGS -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=199309L -fvisibility=hidden"
fi

dnl Thread related checks
AC_CHECK_HEADER(pthread.h,
  AC_SEARCH_LIBS([pthread_create], [pthread c_r], [
    AC_DEFINE([HAVE_PTHREAD_H], 1, [Define to 1 if you have the <pthread.h> header file.])
   ]))

if test "x$ac_cv_header_pthread_h" = "xyes"; then
  CFLAGS="$CFLAGS -D_REENTRANT"
  LDFLAGS="$LDFLAGS -lpthread"
#  if test "x$ac_cv_search_pthread_create" = "x-lc_r"; then
#    CFLAGS="-pthread $CFLAGS"
#  fi
fi

AC_HEADER_STDC
AC_CHECK_HEADERS([alloca.h math.h time.h termios.h assert.h syslog.h libio.h values.h sys/utsname.h sys/time.h])

OLD_CFLAGS="$CFLAGS"
OLD_CPPFLAGS="$CPPFLAGS"
CFLAGS="$NEONFLAGS $CFLAGS"
CPPFLAGS="$NEONFLAGS $CPPFLAGS"
AC_CHECK_HEADERS([machine/cpu-features.h arm_neon.h])
CPPFLAGS="$OLD_CPPFLAGS"
CFLAGS="$OLD_CFLAGS"

AC_C_CONST
AC_C_INLINE

AC_CHECK_FUNCS([snprintf strcasestr memmove memset])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([stat])
AC_CHECK_FUNCS([nanosleep])
AC_CHECK_FUNCS([ioctl])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([isnan])
AC_FUNC_SNPRINTF_C99
AC_FUNC_VSNPRINTF_C99
AC_FUNC_PRINTF_UNIX98

AC_CHECK_HEADER(xml_cache.h)
if test "x$ac_cv_header_xml_cache_h" != "xyes"; then
    echo
    echo "ZeroXML library not found."
    echo "configure aborted."
    echo
fi

AC_CHECK_LIB(c, oserror,
   AC_DEFINE([HAVE_OSERROR_LIB], 1, [Define if oserror is supported]))

dnl check for some default libraries
AC_SEARCH_LIBS([sqrt], [m] )

dnl Create output files
AM_CONFIG_HEADER(include/config.h)

AC_CONFIG_FILES([ \
	Makefile \
	include/Makefile \
	include/aax/aax.h \
	src/Makefile \
	src/file/Makefile \
	src/alsa/Makefile \
	src/dmedia/Makefile \
	src/windows/Makefile \
	src/oss/Makefile \
	src/software/Makefile \
	base/Makefile \
	admin/rpm/aeonwave.spec
	admin/debian/control \
	admin/debian/control-dev \
	admin/debian/control64
])
AC_OUTPUT
