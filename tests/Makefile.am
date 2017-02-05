EXTRA_DIST = readme.txt

AM_CPPFLAGS = \
  -I$(top_srcdir)/include

check_PROGRAMS = rfxcodectest rfxencode

rfxcodectest_SOURCES = rfxcodectest.c

rfxencode_SOURCES = rfxencode.c

rfxcodectest_LDADD = \
  $(top_builddir)/src/librfxencode.la

rfxencode_LDADD = \
  $(top_builddir)/src/librfxencode.la

