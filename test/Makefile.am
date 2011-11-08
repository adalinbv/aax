if NDEBUG
XML_LIB = -lzeroxml
else
XML_LIB = -lzeroxml-rmalloc
endif

bin_PROGRAMS = aaxinfo

noinst_PROGRAMS = \
	teststereo teststereo_equalizer teststereo_frame testmono3d testlowpass\
        testphasing testmono3d_frame testlst_rotation testdistance testcone \
	testposition teststream testcapture testenvelope testwaves testloop \
        testformats teststream_phasing testmono3d_oneshot testframe_detached \
	testleftright testdistortion testinstrument

check_PROGRAMS = \
        testringbuf testerrors testfileio testxml testsource testcodec

noinst_LTLIBRARIES = libtest.la libtest_internal.la

libtest_la_SOURCES = wavfile.c driver.c
libtest_la_LIBADD = \
	$(top_builddir)/base/libbase.la

libtest_internal_la_SOURCES = driver_internal.c
libtest_internal_la_LIBADD = \
	$(top_builddir)/src/software/libsoftwaredriver.la \
	$(top_builddir)/src/alsasoft/libalsasoftdriver.la \
	$(top_builddir)/src/dmedia/libdmediadriver.la \
	$(top_builddir)/src/alsa/libalsadriver.la \
	$(top_builddir)/src/oss/libossdriver.la

testcodec_SOURCES = testcodec.c
testcodec_LDADD = \
	$(top_builddir)/src/libaax.la \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(XML_LIB)

aaxinfo_SOURCES = aaxinfo.c
aaxinfo_LDADD = \
        $(top_builddir)/test/libtest.la \
	$(XML_LIB) -laax


teststereo_SOURCES = teststereo.c
teststereo_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

teststereo_frame_SOURCES = teststereo_frame.c
teststereo_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

teststereo_equalizer_SOURCES = teststereo_equalizer.c
teststereo_equalizer_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testloop_SOURCES = testloop.c
testloop_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testformats_SOURCES = testformats.c
testformats_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testleftright_SOURCES = testleftright.c
testleftright_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testmono3d_SOURCES = testmono3d.c
testmono3d_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testmono3d_oneshot_SOURCES = testmono3d_oneshot.c
testmono3d_oneshot_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testlowpass_SOURCES = testlowpass.c
testlowpass_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testphasing_SOURCES = testphasing.c
testphasing_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testmono3d_frame_SOURCES = testmono3d_frame.c
testmono3d_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testframe_detached_SOURCES = testframe_detached.c
testframe_detached_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testlst_rotation_SOURCES = testlst_rotation.c
testlst_rotation_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testdistance_SOURCES = testdistance.c
testdistance_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testcone_SOURCES = testcone.c
testcone_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testposition_SOURCES = testposition.c
testposition_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testinstrument_SOURCES = testinstrument.c
testinstrument_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testwaves_SOURCES = testwaves.c
testwaves_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testdistortion_SOURCES = testdistortion.c
testdistortion_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

teststream_SOURCES = teststream.c
teststream_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

teststream_phasing_SOURCES = teststream_phasing.c
teststream_phasing_LDADD = \
        $(top_builddir)/test/libtest.la \
        $(top_builddir)/base/libbase.la \
        $(top_builddir)/src/libaax.la \
        $(XML_LIB)

testcapture_SOURCES = testcapture.c
testcapture_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

testenvelope_SOURCES = testenvelope.c
testenvelope_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/base/libbase.la \
	$(top_builddir)/src/libaax.la \
	$(XML_LIB)

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)/include -DSRC_PATH=\"$(top_srcdir)/../sounds/\"

