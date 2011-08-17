# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT(enesim-neko, 0.0.1, enesim-devel@googlegroups.com)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.6 dist-bzip2)
AM_CONFIG_HEADER(src/include/config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_C_CONST
AC_C_BIGENDIAN

AC_LIBTOOL_WIN32_DLL
define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL

VMAJ=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $1);}'`
VMIN=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $2);}'`
VMIC=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $3);}'`
SNAP=`echo $PACKAGE_VERSION | awk -F. '{printf("%s", $4);}'`
version_info=`expr $VMAJ + $VMIN`":$VMIC:$VMIN"
AC_SUBST(version_info)
AC_DEFINE_UNQUOTED(VERSION_MAJOR, ${VMAJ}, [Major version])
AC_DEFINE_UNQUOTED(VERSION_MINOR, ${VMIN}, [Minor version])
AC_DEFINE_UNQUOTED(VERSION_MICRO, ${VMIC}, [Micro version])

create_shared_lib=""
case "$host_os" in
    mingw|mingw32)
	create_shared_lib="-no-undefined"
    ;;
esac
AC_SUBST(create_shared_lib)

# Common
PKG_CHECK_MODULES(ENDER, [ender >= 0.0.1])

# Scripting
## Neko
NEKO_LIBS=
AC_CHECK_LIB(neko, neko_val_callEx,
             [AC_CHECK_HEADERS(neko.h neko_vm.h, have_neko="yes", have_neko="no") ],
             have_neko="yes")
AC_CHECK_PROG(have_neko_bin, nekoc, yes, no)
if test "x$have_neko" = "xyes" && test "x$have_neko_bin" = "xyes"; then
        NEKO_LIBS=-lneko
        AC_DEFINE(BUILD_SCRIPT_NEKO, [1], [Build support for NekoVM scripts])
else
	AC_ERROR("No ender found")
fi
AM_CONDITIONAL(BUILD_SCRIPT_NEKO, test "x$have_neko" = "xyes" && test "x$have_neko_bin" = "xyes")
AC_SUBST(NEKO_LIBS)

ESCEN_CFLAGS="$ENDER_CFLAGS"
ESCEN_LIBS="$ENDER_LIBS $NEKO_LIBS"
AC_SUBST(ESCEN_CFLAGS)
AC_SUBST(ESCEN_LIBS)

## Make the debug preprocessor configurable

AC_OUTPUT([
Makefile
src/Makefile
src/lib/Makefile
],[
])


#####################################################################
## Info

echo
echo
echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE $VERSION"
echo "------------------------------------------------------------------------"
echo
echo
echo "Configuration Options Summary:"
echo
echo "Installation Path.........: $prefix"
echo
echo "Now type 'make' ('gmake' on some systems) to compile $PACKAGE,"
echo "and then afterwards as root (or the user who will install this), type"
echo "'make install'. Change users with 'su' or 'sudo' appropriately."
echo
