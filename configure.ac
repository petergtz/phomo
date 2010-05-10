#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([phomo], [0.3], [p3rg3z@users.sourceforge.com])
AM_INIT_AUTOMAKE([1.11 foreign])


# Checks for programs.
AC_PROG_CXX
#gcc_version=`gcc -dumpversion`
#AC_MSG_ERROR(`gcc -dumpversion`)
#if test `gcc -dumpversion` = "4.4.3" ; then
#AC_MSG_ERROR(wrong version AX_GCC_VERSION)
#fi

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

# Checks for library functions.



AC_OUTPUT(Makefile)
