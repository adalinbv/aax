SUBDIRS = software oss alsasoft alsa dmedia

if NDEBUG
RMALLOC_LIB = -lzeroxml
else
RMALLOC_LIB = -lzeroxml
#RMALLOC_LIB = -lzeroxml-rmalloc
endif

lib_LTLIBRARIES = libaax.la

noinst_HEADRERS = \
	api.h \
	arch.h \
	devices.h \
	driver.h \
	objects.h \
	ringbuffer.h

libaax_la_SOURCES = \
        devices.c \
	objects.c \
	aax_driver.c \
	aax_mixer.c \
	aax_sensor.c \
	aax_emitter.c \
	aax_buffer.c \
	aax_scenery.c \
	aax_filters.c \
	aax_effects.c \
	aax_matrix.c \
	aax_support.c \
	aax_waveforms.c \
	aax_audioframe.c \
	aax_instrument.c

libaax_la_LIBADD = \
	$(top_builddir)/base/libbase.la \
        $(top_builddir)/src/software/libsoftwaredriver.la \
	$(top_builddir)/src/oss/libossdriver.la \
	$(top_builddir)/src/alsasoft/libalsasoftdriver.la \
	$(top_builddir)/src/alsa/libalsadriver.la \
        $(top_builddir)/src/dmedia/libdmediadriver.la \
	$(RMALLOC_LIB)

# -version-info `grep -v '^\#' $(srcdir)/libtool-version
# -release `grep -v '^\#' $(srcdir)/libtool-release`
libaax_la_LDFLAGS = -version-info `grep -v '^\#' $(srcdir)/libtool-version` -lm -ldl

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)/include
