dnl Process this file with autoconf to produce a configure script
AC_INIT([osmo-sysmon],
	m4_esyscmd([./git-version-gen .tarball-version]),
	[openbsc@lists.osmocom.org])

dnl *This* is the root dir, even if an install-sh exists in ../ or ../../
AC_CONFIG_AUX_DIR([.])

AM_INIT_AUTOMAKE([dist-bzip2 foreign])

CFLAGS="$CFLAGS -std=gnu11"

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl include release helper
RELMAKE='-include osmo-release.mk'
AC_SUBST([RELMAKE])

dnl checks for programs
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_INSTALL
LT_INIT

dnl check for pkg-config (explained in detail in libosmocore/configure.ac)
AC_PATH_PROG(PKG_CONFIG_INSTALLED, pkg-config, no)
if test "x$PKG_CONFIG_INSTALLED" = "xno"; then
        AC_MSG_WARN([You need to install pkg-config])
fi
PKG_PROG_PKG_CONFIG([0.20])

dnl checks for libraries
AC_SEARCH_LIBS([dlopen], [dl dld], [LIBRARY_DL="$LIBS";LIBS=""])
AC_SUBST(LIBRARY_DL)


PKG_CHECK_MODULES(LIBOSMOCORE, libosmocore >= 1.0.1)
PKG_CHECK_MODULES(LIBOSMOVTY, libosmovty >= 1.0.1)
PKG_CHECK_MODULES(LIBOSMOCTRL, libosmoctrl >= 1.0.1)
PKG_CHECK_MODULES(LIBOSMOGSM, libosmogsm >= 1.0.1)
PKG_CHECK_MODULES(LIBOSMONETIF, libosmo-netif >= 0.4.0)
PKG_CHECK_MODULES(LIBMNL, libmnl)
dnl FIXME: bump to 1.10.0 once it's available on build slaves and remove workaround from osysmon_ping.c
dnl FIXME: debian8 provides 1.7.0, debian9 provides 1.9.0
PKG_CHECK_MODULES(LIBOPING, liboping >= 1.7.0)

dnl checks for header files
AC_HEADER_STDC

dnl Checks for typedefs, structures and compiler characteristics

AC_ARG_ENABLE(sanitize,
	[AS_HELP_STRING(
		[--enable-sanitize],
		[Compile with address sanitizer enabled],
	)],
	[sanitize=$enableval], [sanitize="no"])
if test x"$sanitize" = x"yes"
then
	CFLAGS="$CFLAGS -fsanitize=address -fsanitize=undefined"
	CPPFLAGS="$CPPFLAGS -fsanitize=address -fsanitize=undefined"
fi

AC_ARG_ENABLE(werror,
	[AS_HELP_STRING(
		[--enable-werror],
		[Turn all compiler warnings into errors, with exceptions:
		 a) deprecation (allow upstream to mark deprecation without breaking builds);
		 b) "#warning" pragmas (allow to remind ourselves of errors without breaking builds)
		]
	)],
	[werror=$enableval], [werror="no"])
if test x"$werror" = x"yes"
then
	WERROR_FLAGS="-Werror"
	WERROR_FLAGS+=" -Wno-error=deprecated -Wno-error=deprecated-declarations"
	WERROR_FLAGS+=" -Wno-error=cpp" # "#warning"
	CFLAGS="$CFLAGS $WERROR_FLAGS"
	CPPFLAGS="$CPPFLAGS $WERROR_FLAGS"
fi

# The following test is taken from WebKit's webkit.m4
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden "
AC_MSG_CHECKING([if ${CC} supports -fvisibility=hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char foo;])],
      [ AC_MSG_RESULT([yes])
        SYMBOL_VISIBILITY="-fvisibility=hidden"],
        AC_MSG_RESULT([no]))
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)

# Coverage build taken from WebKit's configure.in
AC_MSG_CHECKING([whether to enable code coverage support])
AC_ARG_ENABLE(coverage,
              AC_HELP_STRING([--enable-coverage],
                             [enable code coverage support [default=no]]),
              [],[enable_coverage="no"])
AC_MSG_RESULT([$enable_coverage])
if test "$enable_coverage" = "yes"; then
   COVERAGE_CFLAGS="-ftest-coverage -fprofile-arcs"
   COVERAGE_LDFLAGS="-ftest-coverage -fprofile-arcs"
   AC_SUBST([COVERAGE_CFLAGS])
   AC_SUBST([COVERAGE_LDFLAGS])
fi

AC_ARG_ENABLE(profile,
		[AS_HELP_STRING([--enable-profile], [Compile with profiling support enabled], )],
		[profile=$enableval], [profile="no"])
if test x"$profile" = x"yes"
then
	CFLAGS="$CFLAGS -pg"
	CPPFLAGS="$CPPFLAGS -pg"
fi

AC_MSG_RESULT([CFLAGS="$CFLAGS"])
AC_MSG_RESULT([CPPFLAGS="$CPPFLAGS"])

dnl Generate the output
AM_CONFIG_HEADER(config.h)

AC_OUTPUT(
    doc/Makefile
    doc/examples/Makefile
    src/Makefile
    Makefile)
