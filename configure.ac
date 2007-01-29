dnl autoconf script for litmus

AC_INIT(litmus, 0.11, litmus@webdav.org)

AC_PREREQ(2.59)

AC_COPYRIGHT([Copyright (c) 2001-2005 Joe Orton and others
This configure script may be copied, distributed and modified under the 
terms of the GNU General Public license; see COPYING for more details])

AC_CONFIG_SRCDIR(litmus.in)
AC_CONFIG_HEADER(config.h)

AC_DEFINE([_GNU_SOURCE], 1, [Define to enable GNU extensions])

AC_PROG_CC
AC_SET_MAKE
AC_PROG_INSTALL

dnl List of tests
AC_SUBST([TESTS], ["basic copymove props locks http"])

NE_REQUIRE_VERSIONS([0], [25 26])
NEON_WITHOUT_ZLIB
NEON_WITHOUT_ACL
NEON_TEST
NEON_VPATH_BUNDLED(['$(top_srcdir)/lib/neon'], [lib/neon], [NEON_NORMAL_BUILD()
NEON_XML_PARSER([\$(top_srcdir)/lib/expat], [\$(top_builddir)/lib/expat])
])

dnl Check for getopt_long
AC_CHECK_FUNC(getopt_long,,[AC_LIBOBJ(lib/getopt)
AC_LIBOBJ(lib/getopt1)])

NEON_FORMAT(long long)
NEON_DEBUG
NEON_WARNINGS

CPPFLAGS="$CPPFLAGS -I\${top_builddir} -I\$(top_srcdir)/lib"

AC_CONFIG_FILES([litmus], [chmod +x litmus])
AC_CONFIG_FILES([Makefile lib/neon/Makefile])

AC_OUTPUT

AC_MSG_NOTICE([Configured to build AC_PACKAGE_STRING:

  Install prefix:  ${prefix}
  Compiler:        ${CC}
  neon library:    ${neon_library_message}
  XML parser:      ${neon_xml_parser_message}
  SSL library:     ${ne_SSL_message}
])

# For VPATH builds
for d in lib lib/expat lib/neon test-common src; do
    test -d $d || mkdir $d
done
