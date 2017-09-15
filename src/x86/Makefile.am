NAFLAGS += -DASM_ARCH_I386

X86_ASM = \
  cpuid_x86.asm \
  rfxcodec_encode_dwt_shift_x86_sse2.asm \
  rfxcodec_encode_dwt_shift_x86_sse41.asm

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/src

noinst_LTLIBRARIES = librfxencode-x86.la

librfxencode_x86_la_SOURCES = \
  funcs_x86.h \
  rfxencode_tile_x86.c \
  $(X86_ASM)

nasm_verbose = $(nasm_verbose_@AM_V@)
nasm_verbose_ = $(nasm_verbose_@AM_DEFAULT_V@)
nasm_verbose_0 = @echo "  NASM     $@";

.asm.lo:
	$(nasm_verbose)$(LIBTOOL) $(AM_V_lt) --mode=compile \
	  $(top_srcdir)/src/nasm_lt.sh $(NASM) $(NAFLAGS) \
	  -I$(top_srcdir)/src $< -o $@
