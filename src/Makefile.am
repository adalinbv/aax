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
	testleftright testdistortion testinstrument teststereo_grapheq \
	testregisteredsensor testregisteredsensor_frame testdistortion_frame

check_PROGRAMS = \
        testringbuf testerrors testfileio testxml testsource testcodec

noinst_LTLIBRARIES = libtest.la libtest_internal.la

libtest_la_SOURCES = wavfile.c driver.c
libtest_la_LIBADD = \
	$(top_builddir)/base/libbase.la \
	$(XML_LIB)

libtest_internal_la_SOURCES = driver_internal.c
libtest_internal_la_LIBADD = \
	$(top_builddir)/src/software/libsoftwaredriver.la \
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
	-laax


teststereo_SOURCES = teststereo.c
teststereo_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

teststereo_frame_SOURCES = teststereo_frame.c
teststereo_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

teststereo_equalizer_SOURCES = teststereo_equalizer.c
teststereo_equalizer_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

teststereo_grapheq_SOURCES = teststereo_grapheq.c
teststereo_grapheq_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testloop_SOURCES = testloop.c
testloop_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testformats_SOURCES = testformats.c
testformats_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testleftright_SOURCES = testleftright.c
testleftright_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testmono3d_SOURCES = testmono3d.c
testmono3d_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testmono3d_oneshot_SOURCES = testmono3d_oneshot.c
testmono3d_oneshot_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testlowpass_SOURCES = testlowpass.c
testlowpass_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testphasing_SOURCES = testphasing.c
testphasing_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testmono3d_frame_SOURCES = testmono3d_frame.c
testmono3d_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testframe_detached_SOURCES = testframe_detached.c
testframe_detached_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testlst_rotation_SOURCES = testlst_rotation.c
testlst_rotation_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testdistance_SOURCES = testdistance.c
testdistance_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testcone_SOURCES = testcone.c
testcone_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testposition_SOURCES = testposition.c
testposition_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testinstrument_SOURCES = testinstrument.c
testinstrument_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testwaves_SOURCES = testwaves.c
testwaves_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testdistortion_SOURCES = testdistortion.c
testdistortion_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testdistortion_frame_SOURCES = testdistortion_frame.c
testdistortion_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la


teststream_SOURCES = teststream.c
teststream_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

teststream_phasing_SOURCES = teststream_phasing.c
teststream_phasing_LDADD = \
        $(top_builddir)/test/libtest.la \
        $(top_builddir)/src/libaax.la

testenvelope_SOURCES = testenvelope.c
testenvelope_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la


testcapture_SOURCES = testcapture.c
testcapture_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testregisteredsensor_SOURCES = testregisteredsensor.c
testregisteredsensor_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

testregisteredsensor_frame_SOURCES = testregisteredsensor_frame.c
testregisteredsensor_frame_LDADD = \
	$(top_builddir)/test/libtest.la \
	$(top_builddir)/src/libaax.la

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)/include -DSRC_PATH=\"$(top_srcdir)/../sounds/\"

