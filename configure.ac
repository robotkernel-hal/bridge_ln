#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT(
	[bridge_ln],
	m4_esyscmd_s([cat project.properties | grep VERSION    | cut -d'=' -f2 | xargs printf "%s"]),
	m4_esyscmd_s([cat project.properties | grep MAINTAINER | cut -d'=' -f2 | xargs printf "%s"]))
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AX_CHECK_COMPILE_FLAG([-std=gnu11], [CFLAGS+=" -std=gnu11"], [AC_PROG_CC_C99])
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11([ext], [mandatory])
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_MAKE_SET
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG])
m4_ifdef([PKG_INSTALLDIR], [PKG_INSTALLDIR], [pkgconfigdir='${libdir}/pkgconfig' AC_SUBST([pkgconfigdir])])

# Check dependencies
PKG_CHECK_MODULES([ROBOTKERNEL], [robotkernel])
PKG_CHECK_MODULES([LINKS_AND_NODES], [liblinks_and_nodes])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h termios.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([strdup])

# Checks for library functions.
AC_CHECK_LIB(dl, dlopen)

AC_CONFIG_FILES([Makefile src/main/cpp/Makefile src/generator/cpp/Makefile bridge_ln.pc])

AC_OUTPUT

