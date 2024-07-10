EXTRA_DIST =

SUBDIRS =

AM_CPPFLAGS = \
  -I$(top_srcdir)/include -I$(top_srcdir)/src

EXTRA_SOURCES=

if WITH_SIMD_ARM64
AM_CPPFLAGS += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_ARM64=1
endif

noinst_HEADERS = \
  rfxencode_diff_count_neon.h \
  rfxencode_dwt_shift_rem_neon.h

noinst_LTLIBRARIES = librfxencode-neon.la

librfxencode_neon_la_LDFLAGS = -all-static

librfxencode_neon_la_SOURCES = $(noinst_HEADERS) \
  rfxencode_diff_count_neon.c \
  rfxencode_dwt_shift_rem_neon.c \
  $(EXTRA_SOURCES)
