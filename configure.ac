# Process this file with autoconf to produce a configure script

AC_PREREQ(2.69)
AC_INIT([rfxcodec], [0.1.6], [xrdp-devel@googlegroups.com])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PROG_CC
AC_C_CONST
LT_INIT
PKG_INSTALLDIR

AX_CFLAGS_WARN_ALL
AX_APPEND_COMPILE_FLAGS([-Wwrite-strings])
AX_APPEND_COMPILE_FLAGS([-Wmissing-prototypes], ,[-Werror])

# SIMD is optional
AC_ARG_WITH([simd],
    AS_HELP_STRING([--without-simd],[Omit SIMD extensions.]))
if test "x${with_simd}" != "xno"; then
  # Check if we're on a supported CPU
  AC_MSG_CHECKING([if we have SIMD optimisations for cpu type])
  case "$host_cpu" in
    x86_64 | amd64)
      AC_MSG_RESULT([yes (x86_64)])
      AC_PROG_NASM
      simd_arch=x86_64
      AC_DEFINE([RFX_USE_ACCEL_AMD64], [1], [Use x86_64 SIMD instructions])
    ;;
    i*86 | x86 | ia32)
      AC_MSG_RESULT([yes (i386)])
      AC_PROG_NASM
      simd_arch=i386
      AC_DEFINE([RFX_USE_ACCEL_X86], [1], [Use x86 SIMD instructions])
    ;;
    *)
      AC_MSG_RESULT([no ("$host_cpu")])
      AC_MSG_WARN([SIMD support not available for this CPU.  Performance will suffer.])
    ;;
  esac
fi

AM_CONDITIONAL(WITH_SIMD_AMD64, [test x$simd_arch = xx86_64])
AM_CONDITIONAL(WITH_SIMD_X86, [test x$simd_arch = xi386])

# parent project will propagate these options to us when building
AC_ARG_ENABLE(devel_all, AS_HELP_STRING([--enable-devel-all]),
              [devel_all=$enableval], [devel_all=no])
AC_ARG_ENABLE(devel_debug, AS_HELP_STRING([--enable-devel-debug],
              [Build library with support for getting better backtraces [default=no]]),
              [devel_debug=$enableval], [devel_debug=$devel_all])
AM_CONDITIONAL(DEVEL_DEBUG, [test x$devel_debug = xyes ])

# pass debug option to the assembler if specified
AM_COND_IF([DEVEL_DEBUG],
  [AX_APPEND_COMPILE_FLAGS([-DDEBUG], [NAFLAGS])])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile
                 src/amd64/Makefile
                 src/x86/Makefile
                 tests/Makefile
                 rfxcodec.pc
                 rfxcodec-uninstalled.pc
])

AC_OUTPUT

