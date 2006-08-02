#!/bin/sh
set -e
echo -n "aclocal... "
${ACLOCAL:-aclocal} -I macros
echo -n "autoheader... "
${AUTOHEADER:-autoheader}
echo -n "autoconf... "
${AUTOCONF:-autoconf}
echo okay.
rm -rf autom4te*.cache
