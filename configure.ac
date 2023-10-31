dnl Process this file with autoconf to produce a configure script.

AC_INIT([searchmonkey],[0.8.3])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
IT_PROG_INTLTOOL

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_CHECK_FUNCS([mblen])
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_C_RESTRICT

AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_PROG_CXX
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

pkg_modules="libzip poppler-glib gtk+-2.0 gthread-2.0"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

GETTEXT_PACKAGE=searchmonkey
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Add the languages which your application supports here.
ALL_LINGUAS="en_GB en_UK en_US fr_FR fr ru_RU"
AM_GLIB_GNU_GETTEXT

AC_CONFIG_FILES([
Makefile
src/Makefile
po/Makefile.in
pixmaps/Makefile
])
AC_OUTPUT

