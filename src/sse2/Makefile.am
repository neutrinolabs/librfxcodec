EXTRA_DIST =

SUBDIRS =

AM_CPPFLAGS = \
  -I$(top_srcdir)/include -I$(top_srcdir)/src

EXTRA_SOURCES=

if WITH_SIMD_AMD64
AM_CPPFLAGS += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_AMD64=1 -msse2 -mfpmath=sse
endif

if WITH_SIMD_X86
AM_CPPFLAGS += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_X86=1 -msse2 -mfpmath=sse
endif

noinst_HEADERS = \
  rfxencode_diff_count_sse2.h \
  rfxencode_dwt_shift_rem_sse2.h

noinst_LTLIBRARIES = librfxencode-sse2.la

librfxencode_sse2_la_LDFLAGS = -all-static

librfxencode_sse2_la_SOURCES = $(noinst_HEADERS) \
  rfxencode_diff_count_sse2.c \
  rfxencode_dwt_shift_rem_sse2.c \
  $(EXTRA_SOURCES)
