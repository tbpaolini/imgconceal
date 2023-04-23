# Process this file with autoconf to produce a configure script.
AC_INIT([dummy], [0])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# For autobuild.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

m4_pattern_forbid([^gl_[A-Z]])dnl the gnulib macro namespace
m4_pattern_allow([^gl_ES$])dnl a valid locale name
m4_pattern_allow([^gl_LIBOBJS$])dnl a variable
m4_pattern_allow([^gl_LTLIBOBJS$])dnl a variable

gl_PROG_AR_RANLIB

gl_USE_SYSTEM_EXTENSIONS
LT_INIT([win32-dll])
LT_LANG([C++])
AM_CONDITIONAL([GL_COND_LIBTOOL], [true])
gl_cond_libtool=true
AC_DEFUN([gl_INIT], [
gl_m4_base='glm4'
  m4_pushdef([AC_LIBOBJ], m4_defn([gl_LIBOBJ]))
  m4_pushdef([AC_REPLACE_FUNCS], m4_defn([gl_REPLACE_FUNCS]))
  m4_pushdef([AC_LIBSOURCES], m4_defn([gl_LIBSOURCES]))
  m4_pushdef([gl_LIBSOURCES_LIST], [])
  m4_pushdef([gl_LIBSOURCES_DIR], [])
  gl_COMMON
gl_source_base='gllib'
changequote(,)dnl
LTALLOCA=`echo "$ALLOCA" | sed -e 's/\.[^.]* /.lo /g;s/\.[^.]*$/.lo/'`
changequote([, ])dnl
AC_SUBST([LTALLOCA])
gl_FUNC_ALLOCA
gl_ARGP
m4_ifdef([AM_XGETTEXT_OPTION],
  [AM_][XGETTEXT_OPTION([--flag=argp_error:2:c-format])
   AM_][XGETTEXT_OPTION([--flag=argp_failure:4:c-format])])
gl_DIRNAME_LGPL
gl_DOUBLE_SLASH_ROOT
gl_HEADER_ERRNO_H
gl_FLOAT_H
if test $REPLACE_FLOAT_LDBL = 1; then
  AC_LIBOBJ([float])
fi
gl_FUNC_GETOPT_GNU
if test $REPLACE_GETOPT = 1; then
  AC_LIBOBJ([getopt])
  AC_LIBOBJ([getopt1])
  gl_PREREQ_GETOPT
fi
gl_MODULE_INDICATOR_FOR_TESTS([getopt-gnu])
gl_FUNC_GETOPT_POSIX
if test $REPLACE_GETOPT = 1; then
  AC_LIBOBJ([getopt])
  AC_LIBOBJ([getopt1])
  gl_PREREQ_GETOPT
fi
AC_SUBST([LIBINTL])
AC_SUBST([LTLIBINTL])
gl_FUNC_MALLOC_GNU
if test $REPLACE_MALLOC = 1; then
  AC_LIBOBJ([malloc])
fi
gl_MODULE_INDICATOR([malloc-gnu])
gl_FUNC_MALLOC_POSIX
if test $REPLACE_MALLOC = 1; then
  AC_LIBOBJ([malloc])
fi
gl_STDLIB_MODULE_INDICATOR([malloc-posix])
gl_FUNC_MEMCHR
if test $HAVE_MEMCHR = 0 || test $REPLACE_MEMCHR = 1; then
  AC_LIBOBJ([memchr])
  gl_PREREQ_MEMCHR
fi
gl_STRING_MODULE_INDICATOR([memchr])
gl_FUNC_MEMPCPY
if test $HAVE_MEMPCPY = 0; then
  AC_LIBOBJ([mempcpy])
  gl_PREREQ_MEMPCPY
fi
gl_STRING_MODULE_INDICATOR([mempcpy])
gl_MULTIARCH
gl_FUNC_RAWMEMCHR
if test $HAVE_RAWMEMCHR = 0; then
  AC_LIBOBJ([rawmemchr])
  gl_PREREQ_RAWMEMCHR
fi
gl_STRING_MODULE_INDICATOR([rawmemchr])
gl_SIZE_MAX
gl_FUNC_SLEEP
if test $HAVE_SLEEP = 0 || test $REPLACE_SLEEP = 1; then
  AC_LIBOBJ([sleep])
fi
gl_UNISTD_MODULE_INDICATOR([sleep])
gt_TYPE_SSIZE_T
AM_STDBOOL_H
gl_STDDEF_H
gl_STDINT_H
gl_STDIO_H
gl_STDLIB_H
gl_STRCASE
if test $HAVE_STRCASECMP = 0; then
  AC_LIBOBJ([strcasecmp])
  gl_PREREQ_STRCASECMP
fi
if test $HAVE_STRNCASECMP = 0; then
  AC_LIBOBJ([strncasecmp])
  gl_PREREQ_STRNCASECMP
fi
gl_FUNC_STRCHRNUL
if test $HAVE_STRCHRNUL = 0 || test $REPLACE_STRCHRNUL = 1; then
  AC_LIBOBJ([strchrnul])
  gl_PREREQ_STRCHRNUL
fi
gl_STRING_MODULE_INDICATOR([strchrnul])
gl_FUNC_STRERROR
if test $REPLACE_STRERROR = 1; then
  AC_LIBOBJ([strerror])
fi
gl_MODULE_INDICATOR([strerror])
gl_STRING_MODULE_INDICATOR([strerror])
AC_REQUIRE([gl_HEADER_ERRNO_H])
AC_REQUIRE([gl_FUNC_STRERROR_0])
if test -n "$ERRNO_H" || test $REPLACE_STRERROR_0 = 1; then
  AC_LIBOBJ([strerror-override])
  gl_PREREQ_SYS_H_WINSOCK2
fi
gl_HEADER_STRING_H
gl_HEADER_STRINGS_H
gl_FUNC_STRNDUP
if test $HAVE_STRNDUP = 0 || test $REPLACE_STRNDUP = 1; then
  AC_LIBOBJ([strndup])
fi
gl_STRING_MODULE_INDICATOR([strndup])
gl_FUNC_STRNLEN
if test $HAVE_DECL_STRNLEN = 0 || test $REPLACE_STRNLEN = 1; then
  AC_LIBOBJ([strnlen])
  gl_PREREQ_STRNLEN
fi
gl_STRING_MODULE_INDICATOR([strnlen])
gl_SYS_TYPES_H
AC_PROG_MKDIR_P
gl_SYSEXITS
gl_UNISTD_H
gl_FUNC_VASNPRINTF
gl_FUNC_VSNPRINTF
gl_STDIO_MODULE_INDICATOR([vsnprintf])
gl_WCHAR_H
gl_XSIZE
  m4_ifval(gl_LIBSOURCES_LIST, [
    m4_syscmd([test ! -d ]m4_defn([gl_LIBSOURCES_DIR])[ ||
      for gl_file in ]gl_LIBSOURCES_LIST[ ; do
        if test ! -r ]m4_defn([gl_LIBSOURCES_DIR])[/$gl_file ; then
          echo "missing file ]m4_defn([gl_LIBSOURCES_DIR])[/$gl_file" >&2
          exit 1
        fi
      done])dnl
      m4_if(m4_sysval, [0], [],
        [AC_FATAL([expected source file, required through AC_LIBSOURCES, not found])])
  ])
  m4_popdef([gl_LIBSOURCES_DIR])
  m4_popdef([gl_LIBSOURCES_LIST])
  m4_popdef([AC_LIBSOURCES])
  m4_popdef([AC_REPLACE_FUNCS])
  m4_popdef([AC_LIBOBJ])
  AC_CONFIG_COMMANDS_PRE([
    gl_libobjs=
    gl_ltlibobjs=
    if test -n "$gl_LIBOBJS"; then
      # Remove the extension.
      sed_drop_objext='s/\.o$//;s/\.obj$//'
      for i in `for i in $gl_LIBOBJS; do echo "$i"; done | sed -e "$sed_drop_objext" | sort | uniq`; do
        gl_libobjs="$gl_libobjs $i.$ac_objext"
        gl_ltlibobjs="$gl_ltlibobjs $i.lo"
      done
    fi
    AC_SUBST([gl_LIBOBJS], [$gl_libobjs])
    AC_SUBST([gl_LTLIBOBJS], [$gl_ltlibobjs])
  ])
])

# Like AC_LIBOBJ, except that the module name goes
# into gl_LIBOBJS instead of into LIBOBJS.
AC_DEFUN([gl_LIBOBJ], [
  AS_LITERAL_IF([$1], [gl_LIBSOURCES([$1.c])])dnl
  gl_LIBOBJS="$gl_LIBOBJS $1.$ac_objext"
])

# Like AC_REPLACE_FUNCS, except that the module name goes
# into gl_LIBOBJS instead of into LIBOBJS.
AC_DEFUN([gl_REPLACE_FUNCS], [
  m4_foreach_w([gl_NAME], [$1], [AC_LIBSOURCES(gl_NAME[.c])])dnl
  AC_CHECK_FUNCS([$1], , [gl_LIBOBJ($ac_func)])
])

# Like AC_LIBSOURCES, except the directory where the source file is
# expected is derived from the gnulib-tool parameterization,
# and alloca is special cased (for the alloca-opt module).
# We could also entirely rely on EXTRA_lib..._SOURCES.
AC_DEFUN([gl_LIBSOURCES], [
  m4_foreach([_gl_NAME], [$1], [
    m4_if(_gl_NAME, [alloca.c], [], [
      m4_define([gl_LIBSOURCES_DIR], [gllib])
      m4_append([gl_LIBSOURCES_LIST], _gl_NAME, [ ])
    ])
  ])
])

gl_INIT

AC_CONFIG_FILES([Makefile gllib/Makefile glm4/Makefile])
AC_OUTPUT
