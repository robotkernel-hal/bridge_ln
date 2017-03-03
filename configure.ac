#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT(
	[service_provider_ln],
	m4_esyscmd_s([cat project.properties | grep VERSION    | cut -d'=' -f2 | xargs printf "%s"]),
	m4_esyscmd_s([cat project.properties | grep MAINTAINER | cut -d'=' -f2 | xargs printf "%s"]))
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# load env-file with each make invocation
if test -n "$SOURCE_ENV"; then
   echo "loading $SOURCE_ENV"
   source $SOURCE_ENV
fi

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_MAKE_SET

if test -z "$MAKE"; then
   MAKE=make
fi
# expand path with each make invocation
if test -n "$TOOLCHAIN"; then
   MAKE="$MAKE PATH=$TOOLCHAIN:$PATH"
   PATH=$TOOLCHAIN:$PATH
fi
# load env-file with each make invocation
if test -n "$SOURCE_ENV"; then
   echo "loading $SOURCE_ENV"
   source $SOURCE_ENV
   MAKE="source $SOURCE_ENV; $MAKE"
fi
if test -n "$MAKE"; then
   SET_MAKE="MAKE := $MAKE"
fi
AC_SUBST(SET_MAKE)

# setup rmpm
RMPM_ARCH

if test "$rmpm_host" == "arm-angstrom-linux-gnueabi"; then
   ARCH_CFLAGS="-DNO_RDTSC"
fi
AC_SUBST(ARCH_CFLAGS)

m4_esyscmd([scripts/m4_deps.py])
RM_FIND_LN_FROM_ENV

AC_DEFINE_UNQUOTED(BUILD_DATE,"`date -u`",[Date (usually date of extraction from SVN).])
AC_DEFINE_UNQUOTED(BUILD_USER,"`whoami`",[User (usually user logged in on shell).])
AC_DEFINE_UNQUOTED(BUILD_HOST,"`hostname`",[Hostname (build host).])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h termios.h])

AM_CONDITIONAL([HAVE_TERMIOS], [test "$ac_cv_header_termios_h" == "yes"])
AC_SUBST(HAVE_TERMIOS)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([strdup])

# Checks for library functions.
AC_CHECK_LIB(dl, dlopen, DL_LIBS="-ldl", DL_LIBS=)
AC_SUBST(DL_LIBS)


AC_CONFIG_FILES([Makefile src/main/cpp/Makefile])

AC_OUTPUT

