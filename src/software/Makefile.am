noinst_LTLIBRARIES = \
	libsoftwaredriver.la \
	libarchsse4.la \
	libarchsse3.la \
	libarchsse2.la \
	libarchsse.la \
	libarchneon.la

libarchsse_la_SOURCES = arch_sse.c
libarchsse_la_CFLAGS = $(SSEFLAGS)

libarchsse2_la_SOURCES = arch_sse2.c
libarchsse2_la_CFLAGS = $(SSE2FLAGS)

libarchsse3_la_SOURCES = arch_sse3.c
libarchsse3_la_CFLAGS = $(SSE3FLAGS)

libarchsse4_la_SOURCES = arch_sse4.c
libarchsse4_la_CFLAGS = $(SSE4FLAGS)

libarchneon_la_SOURCES = arch_neon.c
libarchneon_la_CFLAGS = $(NEONFLAGS)


libsoftwaredriver_la_SOURCES = \
	arch.c \
	arch_cpu.c \
	device.c \
	mixer.c \
	rbuf_codecs.c \
	rbuf_codec_tables.c \
	rbuf_effects.c \
	rbuf_mixers.c \
	rbuf_mixer_tables.c \
	rbufmix_multi.c \
	rbufmix_single.c \
	ringbuffer.c \
	waveforms.c

libsoftwaredriver_la_LIBADD = \
	libarchsse.la libarchsse2.la libarchsse3.la

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src
