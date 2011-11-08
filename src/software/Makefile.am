noinst_LTLIBRARIES = \
	libsoftwaredriver.la libarchsse2.la libarchsse.la libarchneon.la

libarchsse_la_SOURCES = arch_sse.c
libarchsse_la_CFLAGS = $(SSEFLAGS)

libarchsse2_la_SOURCES = arch_sse2.c
libarchsse2_la_CFLAGS = $(SSE2FLAGS)

libarchneon_la_SOURCES = arch_neon.c
libarchneon_la_CFLAGS = $(NEONFLAGS)


libsoftwaredriver_la_SOURCES = \
	null_device.c \
	device.c \
	arch.c \
	arch_cpu.c \
	ringbuffer.c \
	rbuf_mixers.c \
	rbuf_mixer_tables.c \
	rbuf_codecs.c \
	rbuf_codec_tables.c \
	rbuf_effects.c \
        rbufmix_multi.c \
        rbufmix_single.c

libsoftwaredriver_la_LIBADD = \
	libarchsse.la libarchsse2.la

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src
