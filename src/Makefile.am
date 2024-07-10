EXTRA_DIST = common.asm nasm_lt.sh

SUBDIRS =

AM_CPPFLAGS = \
  -I$(top_srcdir)/include

EXTRA_SOURCES=

if WITH_SIMD_AMD64
SUBDIRS += amd64 sse2
librfxencode_la_LIBADD = amd64/librfxencode-amd64.la sse2/librfxencode-sse2.la
AM_CPPFLAGS += -I$(top_srcdir)/src/sse2 -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_AMD64=1
endif

if WITH_SIMD_X86
SUBDIRS += x86 sse2
librfxencode_la_LIBADD = x86/librfxencode-x86.la sse2/librfxencode-sse2.la
AM_CPPFLAGS += -I$(top_srcdir)/src/sse2 -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_X86=1
endif

if WITH_SIMD_ARM64
SUBDIRS += neon
librfxencode_la_LIBADD = neon/librfxencode-neon.la
AM_CPPFLAGS += -I$(top_srcdir)/src/neon -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_ARM64=1
endif

noinst_HEADERS = \
  rfx_bitstream.h \
  rfxcommon.h \
  rfxencode_compose.h \
  rfxconstants.h \
  rfxencode_alpha.h \
  rfxencode_differential.h \
  rfxencode_dwt.h \
  rfxencode.h \
  rfxencode_quantization.h \
  rfxencode_rlgr1.h \
  rfxencode_rlgr3.h \
  rfxencode_tile.h \
  rfxencode_diff_rlgr1.h \
  rfxencode_diff_rlgr3.h \
  rfxencode_diff_rlgr_common.h \
  rfxencode_rgb_to_yuv.h \
  rfxencode_dwt_rem.h \
  rfxencode_dwt_shift_rem.h \
  rfxencode_dwt_shift_rem_common.h

lib_LTLIBRARIES = librfxencode.la

librfxencode_la_LDFLAGS = -all-static

librfxencode_la_SOURCES = $(noinst_HEADERS) rfxencode.c \
  rfxencode_compose.c rfxencode_tile.c rfxencode_dwt.c \
  rfxencode_quantization.c rfxencode_differential.c \
  rfxencode_rlgr1.c rfxencode_rlgr3.c rfxencode_alpha.c \
  rfxencode_diff_rlgr1.c rfxencode_diff_rlgr3.c \
  rfxencode_rgb_to_yuv.c \
  rfxencode_dwt_rem.c \
  rfxencode_dwt_shift_rem.c \
  $(EXTRA_SOURCES)
