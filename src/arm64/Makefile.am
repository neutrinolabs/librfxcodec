NAFLAGS += -DASM_ARCH_ARM64

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/src

noinst_LTLIBRARIES = librfxencode-arm64.la

librfxencode_arm64_la_SOURCES = \
  funcs_arm64.h \
  arm64_is_neon_supported.c \
  rfxencode_tile_arm64.c

