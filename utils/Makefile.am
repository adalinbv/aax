noinst_PROGRAMS = \
	createwaves

check_PROGRAMS = \
	alsa-info alsa-params create_cannonwav diffwave cvt16k32k \
	oss-audioctl

if NDEBUG
RMALLOC_LIB = -lzeroxml
else
XML_LIB = -lzeroxml
#RMALLOC_LIB = -lzeroxml-rmalloc
endif

create_cannonwav_SOURCES = create_cannonwav.c
create_cannonwav_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(RMALLOC_LIB) -lopenal -lalut 

alsa_info_SOURCES = alsa-info.c
alsa_info_LDADD = -lasound

alsa_params_SOURCES = alsa-params.c
alsa_params_LDADD = -lasound

oss_audioctl_SOURCES = oss-audioctl.c

diffwave_SOURCES = diffwave.c
diffwave_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/al/libopenal.la

createwaves_SOURCES = createwaves.c
createwaves_LDADD = \
        $(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(RMALLOC_LIB)

cvt16k32k_SOURCES = cvt16k32k.c
cvt16k32k_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/al/libopenal.la


AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)/include -DSRC_PATH=\"$(top_srcdir)/../sounds/\"
