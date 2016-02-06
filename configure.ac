dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

AC_INIT(ezinstall, 0.0.25)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

## Determine MySQL library path
# try and find libs without any additional libraries
AC_CHECK_LIB(mysqlclient, mysql_init,
	MYSQL_LIBS=" ",
	MYSQL_LIBS=
)

# hunt through several directories trying to find the libraries
if test "x$MYSQL_LIBS" = "x"; then
	old_LDFLAGS="$LDFLAGS"
	for try in /usr/lib /usr/lib/mysql /usr/local/lib /usr/local/mysql/lib /usr/local/lib/mysql ; do
		LDFLAGS="$old_LDFLAGS -L$try"
		unset ac_cv_lib_mysqlclient_mysql_init
		AC_CHECK_LIB(mysqlclient, mysql_init,
			MYSQL_LIBS="-L$try",
			MYSQL_LIBS=
		)
		if test "x$MYSQL_LIBS" != "x"; then
			break;
		fi
	done
	LDFLAGS="$old_LDFLAGS $MYSQL_LIBS"
fi

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lmysqlclient':
AC_CHECK_LIB([mysqlclient], [main])
# FIXME: Replace `main' with a function in `-lxml2':
AC_CHECK_LIB([xml2], [main])
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])
# FIXME: Replace `main' with a function in `-lz':
AC_CHECK_LIB([z], [main])
# FIXME: Replace `main' with a function in `-lbz2':
AC_CHECK_LIB([bz2], [main])
# FIXME: Replace `main' with a function in `-lcurl':
AC_CHECK_LIB([curl], [main])

AC_CHECK_HEADERS([fcntl.h limits.h memory.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h unistd.h])
xml2_config=''
AC_CHECK_PROGS(xml2_config,xml2-config,)dnl
if test -n "$xml2_config"; then
    # Debian installs libxml headers under /usr/include/libxml2/libxml with
    # the shared library installed under /usr/lib, whereas the package
    # installs itself under $prefix/libxml and $prefix/lib.
    xml2_prefix=`xml2-config --prefix`
    if test -d "${xml2_prefix}/include/libxml2"; then
        CPPFLAGS="$CPPFLAGS -I${xml2_prefix}/include/libxml2"
    fi
    if test "${xml2_prefix}" != '/usr'; then
        LDFLAGS="$LDFLAGS -L${xml2_prefix}/lib"
    fi
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_STDBOOL
AC_TYPE_MODE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([getcwd gethostbyaddr gethostbyname memset memmem mkdir socket strcasecmp stricmp strchr strdup strerror strncasecmp strnicmp strrchr strstr strtol])

EXEEXT=".cgi"

#PKG_CHECK_MODULES(XML2, libxml-2.0)
#AC_SUBST(XML2_CFLAGS)
#AC_SUBST(XML2_LIBS)

IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=ezinstall
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
])
