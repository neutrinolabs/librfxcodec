NAFLAGS += -DASM_ARCH_AMD64

AMD64_ASM = \
  cpuid_amd64.asm \
  rfxcodec_encode_dwt_shift_amd64_sse2.asm \
  rfxcodec_encode_dwt_shift_amd64_sse41.asm

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/src

noinst_LTLIBRARIES = librfxencode-amd64.la

librfxencode_amd64_la_SOURCES = \
  funcs_amd64.h \
  rfxencode_tile_amd64.c \
  $(AMD64_ASM)

nasm_verbose = $(nasm_verbose_@AM_V@)
nasm_verbose_ = $(nasm_verbose_@AM_DEFAULT_V@)
nasm_verbose_0 = @echo "  NASM     $@";

.asm.lo:
	$(nasm_verbose)$(LIBTOOL) $(AM_V_lt) --mode=compile \
	  $(top_srcdir)/src/nasm_lt.sh $(NASM) $(NAFLAGS) \
	  -I$(top_srcdir)/src $< -o $@
