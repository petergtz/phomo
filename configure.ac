#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([phomo], [0.3.1], [p3rg3z@users.sourceforge.com])
AM_INIT_AUTOMAKE([1.11 -Wall foreign])


# Checks for programs.
AC_PROG_CXX
##GCC_VERSION=`gcc -dumpversion`

## Commenting the following completely out, problems might only occur with the combination boost 1.40 and gcc 4.4

##AC_CHECK_PROG(CXX_4_3, g++-4.3, yes, no) # checks if g++-4.3 exists and if yes, sets CXX_4_3=yes.
##if test "$CXX_4_3" = "no" ; then
##     AC_MSG_ERROR(You need to have g++ 4.3 installed. Boost GIL/MPL 1.40 does not compile with newer versions.)
##fi


# Checks for libraries.

AC_LANG([C++])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(boost/gil/image.hpp, , exit)
AC_CHECK_HEADERS(jpeglib.h, , exit)
AC_CHECK_HEADERS(boost/gil/extension/io/jpeg_dynamic_io.hpp, , exit)
AC_CHECK_HEADERS(boost/gil/extension/numeric/sampler.hpp, , AC_MSG_ERROR(boost GIL numeric-extension: http://opensource.adobe.com/wiki/display/gil/Downloads))
AC_CHECK_HEADERS(boost/gil/extension/numeric/resample.hpp, , exit)
AC_CHECK_HEADERS(boost/program_options.hpp, , exit)
AC_CHECK_HEADERS(boost/lambda/lambda.hpp, , exit)
AC_CHECK_HEADERS(boost/filesystem.hpp, , exit)
AC_CHECK_HEADERS(boost/algorithm/string.hpp, , exit)
AC_CHECK_HEADERS(boost/lexical_cast.hpp, , exit)
AC_CHECK_HEADERS(boost/thread.hpp, , exit)
AC_CHECK_HEADERS(boost/timer.hpp, , exit)
AC_CHECK_HEADERS(boost/foreach.hpp, , exit)
AC_CHECK_HEADERS(exiv2/image.hpp, , exit)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

AC_MSG_CHECKING([whether to build and install fspot extension])
AC_ARG_WITH(fspot,
    [  --with-fspot           also build f-spot extension],
    with_fspot=$withval,
    with_fspot=yes)

AC_MSG_RESULT($with_fspot)
AM_CONDITIONAL(WITH_FSPOT, test "x$with_fspot" = "xyes")

if test "$with_fspot" = "yes"; then

	PKG_CHECK_MODULES(GTKSHARP2, gtk-sharp-2.0)
	PKG_CHECK_MODULES(FSPOT, f-spot)

	AC_PATH_PROG(GMCS, gmcs, no) # checks if gmcs exists and if yes, sets GMCS to its path, otherwise "no".
	if test "$GMCS" = "no" ; then
		 AC_MSG_ERROR(You need to have mono gmcs installed.)
	fi

	AC_SUBST(GMCS) # this will replace GMCS in Makefile.am by prefix/gmcs
fi

AC_CONFIG_FILES([Makefile src/Makefile f-spot-extension/Makefile]) # list all files that should be created by configure
AC_OUTPUT # any form with parameters is obsolete.
