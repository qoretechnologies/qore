# Process this file with autoconf to produce a configure script.

# AC_PREREQ(2.59)
# NOTE: Qore should always use semantic versioning: https://semver.org/
AC_INIT([qore], [2.3.0],
        [David Nichols <david@qore.org>],
        [qore])
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2 tar-pax subdir-objects])
AC_CONFIG_HEADER(include/qore/intern/unix-config.h)
AC_CONFIG_HEADER(modules/astparser/config.h)
AC_CONFIG_HEADER(modules/mongodb/config.h)

# fix default include dir
DEFAULT_INCLUDES="-I\${top_builddir}/include/qore"
AC_SUBST([DEFAULT_INCLUDES])

##AC_PROG_MAKE_SET
AC_C_BIGENDIAN

AC_CANONICAL_HOST
if test "${host_os}" = "mingw32"; then
    AC_LIBTOOL_WIN32_DLL
    a=1
fi

AC_LANG(C++)
AC_PROG_CXX
AC_PROG_CC
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL
AC_PROG_INSTALL
#LT_INIT
AM_PROG_AS

if test ! -d m4; then mkdir m4; fi
AC_CONFIG_MACRO_DIR([m4])

# set system variables
OS=`uname -s`
if test "$OS" = "HP-UX"; then
   if test "$host_cpu" = "ia64"; then
      ARCH=itanium
   else
      ARCH=PA-RISC
   fi
else
   ARCH=`uname -p`
   if test $ARCH = unknown ; then
      ARCH=`uname -m`
   fi
   if test $ARCH = i686 -o $ARCH = i586 -o $ARCH = athlon ; then
      ARCH=i386
   fi
fi

# set find prefix
if test -n "$prefix" -a "$prefix" != "NONE"; then
   find_prefix="$prefix"
fi

# get CPU family
case "${host_cpu}" in
   i[[3456]]86) cpu_family=i386 ;;
   hppa*)       cpu_family=parisc ;;
   amd64)   	cpu_family=x86_64 ;;
   *)           cpu_family=${host_cpu} ;;
esac

# set OS-specific variables
SHLIB_SUFFIX=so
#echo host_os=${host_os}
case "${host_os}" in
    *linux*)
        AC_DEFINE(NEED_ICONV_TRANSLIT, 1, Define to 1 if //TRANSLIT should be added to the destination character set code)
        SHLIB_SUFFIX=so
        QORE_BUILD_HOST=`uname -smpo`
		;;
     *solaris*)
        # set compile flags for Solaris
        CPPFLAGS="${CPPFLAGS} -D_XPG6 -D_XOPEN_SOURCE=600"
		# for checking for some network functions on Solaris
		QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -lnsl -lsocket"
		# assume we are using CC if not using g++, add -norunpath and -mt to link flags in this case
		if test "$GXX" != "yes"; then
		   CXXFLAGS="${CXXFLAGS} -mt"
		   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -norunpath -mt"
		else
		   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -latomic"
		fi
		AC_DEFINE(SOLARIS, 1, if compiling on Solaris)
		AC_DEFINE(ZONEINFO_LOCATION, "/usr/share/lib/zoneinfo", zoneinfo location)
		# add -ldl and -lthread to libs if on Solaris 8
		if test "${host_os}" = "solaris2.8"; then
		   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -ldl -lthread -lpthread"
		fi
		solaris=yes
        QORE_BUILD_HOST=`uname -spirv`
        ;;
     *darwin*)
        # set compile flags for Darwin/OSX
		CPPFLAGS="${CPPFLAGS} -D_APPLE_C_SOURCE -D_DARWIN_C_SOURCE"
		SHLIB_SUFFIX=dylib
		# libtool 1.5.* is creating modules on Darwin 8.* (OS/X 10.4.*) with a .so suffix for some reason
		MODULE_SUFFIX=so
		AC_DEFINE(DARWIN, 1, [if compiling on Darwin])
		# use gnutar on darwin to avoid getting those pesky resource files
		AMTAR=gnutar
		darwin=yes
        QORE_BUILD_HOST=`uname -smr`
		;;
     *hpux*)
        if test "$host_cpu" = "ia64"; then
            SHLIB_SUFFIX=so
		else  # assume hppa (PA-RISC)
            SHLIB_SUFFIX=sl
            AC_DEFINE(SHLIB_SUFFIX, "sl", shared library suffix)
		fi
		# add "-AA" and -D__STDC_EXT__ to aCC commandline to enable a clean compile
		if test -n "`echo $CXX|grep aCC`"; then
		   CXXFLAGS="$CXXFLAGS -AA"
		   CPPFLAGS="$CPPFLAGS -D_HPUX_SOURCE -D__STDC_EXT__ -D_RWSTD_MULTI_THREAD"
		fi
		CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE_EXTENDED"
		AC_DEFINE(HPUX, 1, if compiling on HP-UX)
		AC_DEFINE(NEED_ENVIRON_LOCK, 1, [if the environment operations need explicit locking])
		hpux=yes
        QORE_BUILD_HOST=`uname -a`
		;;
     *cygwin*)
        SHLIB_SUFFIX=dll
        CPPFLAGS=-mwin32
		windows=yes
		# turn off signal handling for cygwin; cygwin supports it, but it's emulated/slow/non-native
		no_sig_handling=yes
        QORE_BUILD_HOST=`uname -a`
        ;;
     *mingw32*)
        SHLIB_SUFFIX=dll
        windows=yes
		mingw=yes
		need_user_dl=yes # set to make sure there's a user-provided dlfcn.h and libdl.(a|dll) available
		# DEBUG
		#make_dll=yes
		# libraries to find
		need_libs="$need_libs wsock32 ws2_32 gdi32 iconv pthread"
		# DEBUG: cannot create dll without these libraries for some reason
		#need_libs="$need_libs user32 gdi32 iberty msvcrt advapi32 mingwex moldname kernel32"
		# turn off signal handling for windows
		no_sig_handling=yes
		# have to set _WIN32_WINNT level so that some networking APIs are available
		CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501 -fstack-protector"
        QPP_CPPFLAGS="${QPP_CPPFLAGS} -fstack-protector"
        LDFLAGS="${LDFLAGS} -fstack-protector -lssp"
		MODULE_LDFLAGS="-no-undefined"
		# have to set a flag when building the library so symbols are exported correctly
		QORE_LIB_CPPFLAGS="-DBUILDING_DLL=1"
		# make sure OS is set to Windows when cross-compiling
		OS=Windows
		# set LIB_PREFIX if not already set
		if test -z "$LIB_PREFIX"; then
            AC_MSG_CHECKING([for mingw32 library prefix])
            cxx=`echo $CXX|cut -f1 -d\ `
            cc_loc=`which $cxx|head -1`
            mingw_base=`dirname $cc_loc|sed 's/\/@<:@^/@:>@*$//'`
            cc_targ=`echo $cxx|sed s/-g++$//`
            if test -d $mingw_base/$cc_targ; then
                LIB_PREFIX=$mingw_base/$cc_targ
                AC_MSG_RESULT([$LIB_PREFIX])
            else
                AC_MSG_RESULT([not found])
            fi
		fi
		#LDFLAGS="${LDFLAGS} -L${LIB_PREFIX}/lib"
		#CPPFLAGS="-I${LIB_PREFIX}/include"
		# need a native g++ compiler for native targets (ie qpp)
		if test -z "$NATIVE_CXX"; then
            NATIVE_CXX=`which g++|head -1 2>/dev/null`
            if test -z "$NATIVE_CXX"; then
                AC_MSG_ERROR(unable to find a native c++ compiler; please set NATIVE_CXX to the path to the native c++ compiler for building qpp)
            else
                NATIVE_CXX="$NATIVE_CXX -std=c++17"
            fi
		fi
		if test -z "$NATIVE_CXX"; then
            NATIVE_CXX=$CXX
		fi
        QORE_BUILD_HOST=`uname -a`
		AC_SUBST(NATIVE_CXX)
		AC_SUBST(NATIVE_CPPFLAGS)
		AC_SUBST(NATIVE_LDDFLAGS)
		;;
     freebsd*)
        # add /usr/local/lib to LDFLAGS if not already there
        if test -d /usr/local/lib -a -z "`echo $LDFLAGS|grep /usr/local/lib`"; then
            LDFLAGS="$LDFLAGS -L/usr/local/lib"
        fi
		# add /usr/local/include to CPPFLAGS if not already there
        if test -d /usr/local/include -a -z "`echo $CPPFLAGS|grep /usr/local/include`" -a -z "`echo $CFLAGS|grep /usr/local/include`" -a -z "`echo $CXXFLAGS|grep /usr/local/include`"; then
            CPPFLAGS="$CPPFLAGS -I/usr/local/include"
        fi
		# set LEX=/usr/local/bin/flex if not already set and the target exists
		if test -z "$LEX" -a -f /usr/bin/flex -a -z "`/usr/bin/flex --version | grep -e ' 2\.5\.3[[1-9]]' -e ' 2\.5\.[[4-9]][[0-9]]' -e ' 2\.\[[6-9]]' -e ' [[3-9]]'`" -a -f /usr/local/bin/flex; then
            LEX=/usr/local/bin/flex
        fi
		AC_WARN([LDFLAGS=$LDFLAGS LEX=$LEX])
        QORE_BUILD_HOST=`uname -a`
        ;;
     *)
        AC_WARN(Unknown platform ${host_os})
        QORE_BUILD_HOST=`uname -a`
        ;;
esac
AC_SUBST(SHLIB_SUFFIX)
if test -z "$MODULE_SUFFIX" ; then
    MODULE_SUFFIX=$SHLIB_SUFFIX
fi
AC_SUBST(MODULE_SUFFIX)

CPPFLAGS="$CPPFLAGS -DHAVE_UNIX_CONFIG_H"
QPP_CPPFLAGS="$QPP_CPPFLAGS -DHAVE_UNIX_CONFIG_H"
AC_SUBST(QPP_CPPFLAGS)

AC_PROG_LEX

# test for C++17
AX_CXX_COMPILE_STDCXX_17()
if test "$HAVE_CXX17" = "1"; then
    AM_CXXFLAGS=-std=gnu++17
fi

# test for flex & flex version
AC_MSG_CHECKING([for flex 2.5.31 or greater])
if test -z "`echo $LEX | grep flex`"; then
    AC_MSG_ERROR([no flex version found])
elif test -z "`$LEX --version | grep -e ' 2\.5\.3[[1-9]]' -e ' 2\.5\.[[4-9]][[0-9]]' -e ' 2\.[[6-9]]' -e ' [[3-9]]'`" ; then
    flexver=`$LEX --version`
	AC_MSG_ERROR([incompatible $flexver found - you can download flex 2.5.35 at http://sourceforge.net/projects/flex])
else
    flexver=`$LEX --version`
	AC_MSG_RESULT([yes ($flexver)])
fi
# turn off warnings in LEX
LEX="$LEX --nowarn"
AC_SUBST(YACC)

AC_PROG_YACC

# test for bison & bison version
AC_MSG_CHECKING([for bison 1.85 or greater])
if test -z "`echo $YACC | grep bison`"; then
	AC_MSG_ERROR([bison not found])
elif test -z "`$YACC --version | head -n 1 | grep -e ' [[2-9]]\.' -e ' 1\.8[[5-9]]*' -e ' 1\.9'`"; then
    bisonver=`$YACC --version | head -n 1`
	AC_MSG_ERROR([incompatible $bisonver found])
else
	AC_MSG_RESULT([yes])
fi

if test "$no_sig_handling" != "yes"; then
    AC_DEFINE(HAVE_SIGNAL_HANDLING, 1, [if signal handling is supported])
fi

AC_ARG_ENABLE([single-compilation-unit],
  [AS_HELP_STRING([--enable-single-compilation-unit],
		  [enable compilation as a single unit (default: on)])],
  [case "${enable_single_compilation_unit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_single_compilation_unit} for --enable-single-compilation-unit) ;;
      esac],
  [enable_single_compilation_unit=yes])

AC_ARG_ENABLE([scu],
  [AS_HELP_STRING([--enable-scu],
		  [short for --enable-single-compilation-unit: enable compilation as a single unit])],
  [case "${enable_scu}" in
       yes|no) enable_single_compilation_unit=$enable_scu;;
       *)      AC_MSG_ERROR(bad value ${enable_scu} for --enable-scu) ;;
      esac])

AC_ARG_ENABLE([64bit],
  [AS_HELP_STRING([--enable-64bit],
		  [enable 64bit support (default: auto)])],
  [case "${enable_64bit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_64bit} for --enable-64bit) ;;
      esac],
  [enable_64bit=auto])

if test "$enable_64bit" = "auto"; then
   # turn on 64-bit by default if compiling on a 64-bit platform
   case "${host_cpu}" in
        *64*)
            enable_64bit=yes
            ;;
        s390x)
            enable_64bit=yes
            ;;
        *)
            case "${host_os}" in
                darwin*)
                    mv=`uname -r|cut -f1 -d.`
                    if test $mv -ge 10 -a "${host_cpu}" = "i386"; then
                        enable_64bit=yes
                    fi
                    ;;
                solaris*)
                    if test "${host_cpu}" = "i386"; then
                        # if we are running on a 64-bit machine, then enable a 64-bit build
                        if test -n "`isainfo -v 2>/dev/null| grep 64-bit`"; then
                            enable_64bit=yes
                        fi
                    fi
                    ;;
            esac
            if test "$enable_64bit" = "auto"; then
                enable_64bit=no
            fi
            ;;
    esac
fi

# allow users to explicitly set libsuffix
AC_ARG_WITH([libsuffix],
  [AS_HELP_STRING([--with-libsuffix@<:@=LIBSUFFIX@:>@],
                  [explitily set libsuffix])],
  [LIBSUFFIX=$with_libsuffix; LIBSUFFIX_SET=yes;AC_SUBST(LIBSUFFIX)],
  [])

# see if we know how to set flags properly for different hosts and architectures
# FIXME: add test on HP-UX if the compiler can generate the appropriate binaries
if test "$enable_64bit" = "yes"; then
    case "${host_os}" in
        *linux*)
            if test "$GXX" = "yes"; then
                case "$host_cpu" in
                    x86_64|ppc64)
                        CXXFLAGS="$CXXFLAGS -m64"
                        ;;
                esac
            fi
            # multiarch
            if test "$LIBSUFFIX_SET" != "yes"; then
                if test -d /usr/lib64; then
                    LIBSUFFIX=64
                fi
                if test "$GXX" = "yes"; then
                    multiarch=`${CXX} --print-multiarch 2> /dev/null`
                    if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                        LIBSUFFIX=/${multiarch}
                    fi
                fi
                AC_SUBST(LIBSUFFIX)
            fi
            ;;
        *hpux*)
            if test "$host_cpu" = "ia64"; then
                if test -n "`echo $CXX|grep aCC`"; then
                    CXXFLAGS="$CXXFLAGS +DD64"
                elif test "$GXX" = "yes"; then
                    CXXFLAGS="$CXXFLAGS -mlp64"
                    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -lCsup"
                fi
            else
                if test -n "`echo $CXX|grep aCC`"; then
                    CXXFLAGS="$CXXFLAGS +DA2.0W"
                elif test "$GXX" = "yes"; then
                    CXXFLAGS="$CXXFLAGS -march=2.0"
                fi
            fi
            ;;
        *solaris*)
            if test "$GXX" = "yes"; then
                if test "$host_cpu" = "sparc"; then
                    CXXFLAGS="$CXXFLAGS -mcpu=ultrasparc3 -m64"  # NOTE we compile for minimum ultrasparc 3 in 64-bit mode
                else
                    CXXFLAGS="$CXXFLAGS -m64"
                fi
            else
                CC_MAJOR=`$CXX -V 2>&1 | head -1| cut -f1 -d.|cut -d\  -f4`
                CC_MINOR=`$CXX -V 2>&1 | head -1| cut -f2 -d.|cut -d\  -f1`
                if test "$CC_MAJOR" -gt 5 -o \( "$CC_MAJOR" -eq 5 -a "$CC_MINOR" -ge 10 \); then
                    CXXFLAGS="$CXXFLAGS -m64"
                else
                    CXXFLAGS="$CXXFLAGS -xarch=generic64"
                fi
            fi
            ;;
        *darwin*)
            # set ARCH to x86_64 to be consistent with other intel 64-bit builds
            if test "$host_cpu" = i386 -o "$host_cpu" = x86_64; then
                ARCH=x86_64
            fi
            ;;
    esac
    bits=64
else
   case "${host_os}" in
     *linux*)
        if test "$GXX" = "yes"; then
            case "$host_cpu" in
                arm*) ;;
                *) CXXFLAGS="$CXXFLAGS -m32" ;;
            esac
        fi
        # multiarch
        if test "$LIBSUFFIX_SET" != "yes"; then
            if test "$host_cpu" = "x86_64" -a -d /usr/lib32; then
                LIBSUFFIX=32
            fi
            if test "$GXX" = "yes"; then
                multiarch=`${CXX} --print-multiarch 2> /dev/null`
                if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                    LIBSUFFIX=/${multiarch}
                fi
            fi
            AC_SUBST(LIBSUFFIX)
        fi
        ;;
     *darwin*)
        if test "$GXX" = "yes"; then
            case "$host_cpu" in
                arm*) ;;
                *) CXXFLAGS="$CXXFLAGS -m32" ;;
            esac
        fi
        ;;
     *hpux*)
        if test "$host_cpu" != "ia64"; then
            if test "$GXX" = "yes"; then
                CXXFLAGS="$CXXFLAGS -march=2.0"
            elif test -n "`echo $CXX|grep aCC`" -a "$host_cpu" != "ia64"; then
                CXXFLAGS="$CXXFLAGS +DA2.0N"   # NOTE we compile for PA-RISC 2.0 32-bit, not PA-RISC 1.1
            fi
        fi
        ;;
     *solaris*)
        if test "$GXX" = "yes"; then
            if test "$host_cpu" = "sparc"; then
                CXXFLAGS="$CXXFLAGS -mv8plus -m32"
            else
                CXXFLAGS="$CXXFLAGS -m32"
            fi
        else
            CXXFLAGS="$CXXFLAGS -xarch=generic"
        fi
        ;;
    esac
    if test "$host_cpu" = "x86_64"; then
        cpu_family=i386
    fi
    bits=32
fi

# set CPU_X86_64 if we are targeting it
if test "${enable_64bit}" = yes; then
    if test "${cpu_family}" = i386 -o "${cpu_family}" = x86_64; then
        AC_DEFINE([CPU_X86_64], 1, [define if we are targeting a x86_64 CPU])
    fi
fi

if test "$host_cpu" = "sparc"; then
   AC_DEFINE([SPARC], 1, [define if we are targeting the SPARC CPU])
fi

AC_ARG_ENABLE([static-openssl],
     [AS_HELP_STRING([--enable-static-openssl],
		     [force static linkage of openssl libraries (default=auto)])],
     [case "${enable_static_openssl}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_static_openssl} for --enable-static-openssl) ;;
      esac],
     [enable_static_openssl=no])

AC_ARG_ENABLE([static-zlib],
     [AS_HELP_STRING([--enable-static-zlib],
		     [force static linkage of zlib libraries (default=auto)])],
     [case "${enable_static_zlib}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_static_zlib} for --enable-static-zlib) ;;
      esac],
     [enable_static_zlib=no])

AC_ARG_ENABLE([static-bz2],
     [AS_HELP_STRING([--enable-static-bz2],
		     [force static linkage of bz2 libraries (default=auto)])],
     [case "${enable_static_bz2}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_static_bz2} for --enable-static-bz2) ;;
      esac],
     [enable_static_bz2=no])

AC_ARG_ENABLE([static-pcre2],
     [AS_HELP_STRING([--enable-static-pcre2],
		     [force static linkage of pcre2 libraries (default=auto)])],
     [case "${enable_static_pcre2}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_static_pcre2} for --enable-static-pcre2) ;;
      esac],
     [enable_static_pcre2=no])

AC_ARG_ENABLE([static-common],
     [AS_HELP_STRING([--enable-static-common],
		     [force static linkage of openssl and pcre2 libraries (default=auto)])],
     [case "${enable_static_common}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_static_common} for --enable-static-common) ;;
      esac],
     [enable_static_common=no])

if test "$enable_static_openssl" = "yes"; then
   openssl_static=yes
fi

if test "$enable_static_zlib" = "yes"; then
   zlib_static=yes
fi

if test "$enable_static_bz2" = "yes"; then
   bz2lib_static=yes
elif test "$enable_static_bz2" != "no" -a "$BZ2_STATIC" = "yes"; then
   bz2lib_static=yes
fi

if test "$enable_static_pcre2" = "yes"; then
   pcre2_static=yes
fi

if test "$enable_static_common" = "yes"; then
   zlib_static=yes
   openssl_static=yes
   pcre2_static=yes
   bz2lib_static=yes
fi

set_zlib_cppflags() {
    if test "$1" != "/usr/include"; then
        ZLIB_CPPFLAGS=-I$1
    fi
    with_zlib_includes="$1"
}

find_zlib() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
            dir=/
            lib=/lib${LIBSUFFIX}
            inc=/usr/include
        fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi

    # if we only want the static zlib libraries
    if test -n "$zlib_static"; then
    	if test -f "$lib/libz.a"; then
            ZLIB_LDFLAGS="$lib/libz.a"
            with_zlib_libs="$lib"
            zlib_static=1
    	fi
    else
        if test -f "$lib/libz.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                ZLIB_LDFLAGS="-L$lib"
            fi
            ZLIB_LDFLAGS="$ZLIB_LDFLAGS -lz"
            with_zlib_libs="$lib"
    	elif test -f "$lib/libz.a"; then
            if test "$mingw" = yes -a "$make_dll" = yes; then
                ZLIB_LDFLAGS="-Wl,$lib/libz.a"
            else
                if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                    ZLIB_LDFLAGS="-L$lib"
                fi
                ZLIB_LDFLAGS="$ZLIB_LDFLAGS -lz"
            fi
            with_zlib_libs="$lib"
            zlib_static=1
    	fi
    fi
    if test -z "$ZLIB_LDFLAGS"; then
        return
    fi
    # try to find include files
    if test -f "$inc/zlib.h"; then
        set_zlib_cppflags $inc
    elif test -f "$inc/zlib/zlib.h"; then
        set_zlib_cppflags $inc/zlib
    else
        unset ZLIB_LDFLAGS
        unset with_zlib_libs
        return
    fi
    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $ZLIB_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $ZLIB_CPPFLAGS"
}

set_mpfr_cppflags() {
    if test "$1" != "/usr/include"; then
        MPFR_CPPFLAGS=-I$1
    fi
    with_mpfr_includes="$1"
}

find_mpfr() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
            dir=/
            lib=/lib${LIBSUFFIX}
            inc=/usr/include
        fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi

    echo find_mpfr dir: $dir lib: $lib inc: $inc

    # if we only want the static mpfr libraries
    if test -n "$mpfr_static"; then
    	if test -f "$lib/libmpfr.a"; then
            MPFR_LDFLAGS="$lib/libmpfr.a"
            with_mpfr_libs="$lib"
            mpfr_static=1
    	fi
    else
        if test -f "$lib/libmpfr.${SHLIB_SUFFIX}"; then
    	    if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
       	        MPFR_LDFLAGS="-L$lib"
    	    fi
            MPFR_LDFLAGS="$MPFR_LDFLAGS -lmpfr"
            with_mpfr_libs="$lib"
        elif test -f "$dir/bin/libmpfr-6.${SHLIB_SUFFIX}"; then
    	    if test "$dir" != "/" -a "$dir" != "/usr"; then
       	        MPFR_LDFLAGS="-L$dir/bin"
    	    fi
            MPFR_LDFLAGS="$MPFR_LDFLAGS -lmpfr-6"
            with_mpfr_libs="$dir/bin"
    	elif test -f "$lib/libmpfr.a"; then
            if test "$mingw" = yes -a "$make_dll" = yes; then
                MPFR_LDFLAGS="-Wl,$lib/libmpfr.a"
            else
                if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                    MPFR_LDFLAGS="-L$lib"
                fi
                MPFR_LDFLAGS="$MPFR_LDFLAGS -lmpfr"
            fi
            with_mpfr_libs="$lib"
            mpfr_static=1
    	fi
    fi
    if test -z "$MPFR_LDFLAGS"; then
        return
    fi
    # try to find include files
    if test -f "$inc/mpfr.h"; then
        set_mpfr_cppflags $inc
    elif test -f "$inc/mpfr/mpfr.h"; then
        set_mpfr_cppflags $inc/mpfr
    else
        unset MPFR_LDFLAGS
        unset with_mpfr_libs
        return
    fi
    # see if mpfr_divby0_p is supported
    mpfr_db0=`grep mpfr_divby0_p "$with_mpfr_includes/mpfr.h" 2>/dev/null`
    mpfr_exp_t=`grep mpfr_exp_t "$with_mpfr_includes/mpfr.h" 2>/dev/null`
    mpfr_regular=`grep mpfr_regular_p "$with_mpfr_includes/mpfr.h" 2>/dev/null`
    mpfr_rndn=`grep MPFR_RNDN "$with_mpfr_includes/mpfr.h" 2>/dev/null`
    mpfr_sprintf=`grep mpfr_sprintf "$with_mpfr_includes/mpfr.h" 2>/dev/null`
    mpfr_buildopt_tls_p=`grep mpfr_buildopt_tls_p "$with_mpfr_includes/mpfr.h" 2>/dev/null`

    QORE_LIB_LDFLAGS="$MPFR_LDFLAGS $QORE_LIB_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $MPFR_CPPFLAGS"
}

# allow the user to set a generic search prefix for libraries, includes, etc
AC_ARG_WITH([lib-prefix],
  [AS_HELP_STRING([--with-lib-prefix@<:@=DIR@:>@],
                  [generic search prefix for libraries, includes, etc])],
  [if test ! -d "${with_lib_prefix}"; then AC_MSG_ERROR([bad value ${with_lib_prefix} for --with-lib-prefix]); unset with_lib_prefix; fi],
  [with_lib_prefix="$LIB_PREFIX"])

# see if we can figure out where the zlib library is
AC_ARG_WITH([zlib-dir],
  [AS_HELP_STRING([--with-zlib-dir@<:@=DIR@:>@],
                  [ZLIB directory])],
  [if test ! -d "${with_zlib_dir}"; then AC_MSG_ERROR([bad value ${with_zlib_dir} for --with-zlib-dir]); unset with_zlib_dir; fi],
  [with_zlib_dir="$ZLIB_DIR"])

AC_MSG_CHECKING([for zlib libraries and header files])
for dir in "${with_zlib_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/zlib /usr/local/zlib /opt/local /sw /usr/sfw /opt/sfw; do
    if test -z "$dir"; then
        continue
    fi
    find_zlib $dir
    if test -n "$ZLIB_LDFLAGS"; then
        break
    fi
done
if test -z "$ZLIB_LDFLAGS"; then
    AC_MSG_ERROR([no zlib library found])
fi
if test -n "$zlib_static"; then
   AC_MSG_RESULT([includes $with_zlib_includes libs $with_zlib_libs (static)])
else
   AC_MSG_RESULT([includes $with_zlib_includes libs $with_zlib_libs (shared)])
fi

# see if we can figure out where the mpfr library is
AC_ARG_WITH([mpfr-dir],
  [AS_HELP_STRING([--with-mpfr-dir@<:@=DIR@:>@],
                  [MPFR directory])],
  [if test ! -d "${with_mpfr_dir}"; then AC_MSG_ERROR([bad value ${with_mpfr_dir} for --with-mpfr-dir]); unset with_mpfr_dir; fi],
  [with_mpfr_dir="$MPFR_DIR"])

AC_MSG_CHECKING([for mpfr libraries and header files])
for dir in "${with_mpfr_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/mpfr /usr/local/mpfr /opt/local /sw /usr/sfw /opt/sfw; do
    if test -z "$dir"; then
        continue
    fi
    find_mpfr $dir
    if test -n "$MPFR_LDFLAGS"; then
        break
    fi
done
if test -z "$MPFR_LDFLAGS"; then
    AC_MSG_ERROR([no mpfr library found])
fi
if test -n "$mpfr_static"; then
   AC_MSG_RESULT([includes $with_mpfr_includes libs $with_mpfr_libs (static)])
else
   AC_MSG_RESULT([includes $with_mpfr_includes libs $with_mpfr_libs (shared)])
fi

AC_MSG_CHECKING([for mpfr_divby0_p])
if test -n "$mpfr_db0"; then
   AC_DEFINE([HAVE_MPFR_DIVBY0], 1, [define if the mpfr library supports mpfr_divby0_p and associated functions])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for mpfr_exp_t])
if test -n "$mpfr_exp_t"; then
   AC_DEFINE([HAVE_MPFR_EXP_T], 1, [define if the mpfr library supports mpfr_exp_t])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for mpfr_regular_p])
if test -n "$mpfr_regular"; then
   AC_DEFINE([HAVE_MPFR_REGULAR], 1, [define if the mpfr library supports mpfr_regular_p])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for MPFR_RNDN])
if test -n "$mpfr_rndn"; then
   AC_DEFINE([HAVE_MPFR_RNDN], 1, [define if the mpfr library defines MPFR_RNDN])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for mpfr_sprintf])
if test -n "$mpfr_sprintf"; then
   AC_DEFINE([HAVE_MPFR_SPRINTF], 1, [define if the mpfr library supports mpfr_sprintf])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for mpfr_buildopt_tls_p])
if test -n "$mpfr_buildopt_tls_p"; then
   AC_DEFINE([HAVE_MPFR_BUILDOPT_TLS_P], 1, [define if the mpfr library supports mpfr_buildopt_tls_p])
   AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([not found])
fi

set_pcre2_cppflags() {
    if test "$1" != "/usr/include"; then
        PCRE2_CPPFLAGS=-I$1
    fi
    if test -n "$pcre2_static"; then
        PCRE2_CPPFLAGS="${PCRE2_CPPFLAGS} -DPCRE2_STATIC=1"
    fi
    with_pcre2_includes="$1"
}

find_pcre2() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
            dir="/"
            lib=/lib${LIBSUFFIX}
            inc=/usr/include
        fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi
    # if we only want the static pcre2 libraries
    if test -n "$pcre2_static"; then
    	if test -f "$lib/libpcre2-8.a"; then
            PCRE2_LDFLAGS="$lib/libpcre2-8.a"
            with_pcre2_libs="$lib"
            pcre2_static=1
    	fi
    else
        if test -f "$lib/libpcre2-8.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                PCRE2_LDFLAGS="-L$lib"
            fi
            PCRE2_LDFLAGS="$PCRE2_LDFLAGS -lpcre2-8"
            with_pcre2_libs="$lib"
        elif test -f "$lib/libpcre2-8.a"; then
            if test "$mingw" = yes -a "$make_dll" = yes; then
                PCRE2_LDFLAGS="-Wl,$lib/libpcre2-8.a"
            else
                if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                    PCRE2_LDFLAGS="-L$lib"
                fi
                PCRE2_LDFLAGS="$PCRE2_LDFLAGS -lpcre2-8"
            fi
            with_pcre2_libs="$lib"
            pcre2_static=1
        elif test "$windows" = yes -a -f "$dir/bin/libpcre2-8.${SHLIB_SUFFIX}"; then
            PCRE2_LDFLAGS="-L$dir/bin -lpcre2-8"
            with_pre_libs="$dir"
        fi
    fi
    if test -z "$PCRE2_LDFLAGS"; then
        return
    fi
    # try to find include files
    if test -f "$inc/pcre2.h"; then
        set_pcre2_cppflags $inc
    elif test -f "$inc/pcre2/pcre2.h"; then
        set_pcre2_cppflags $inc/pcre2
    else
        unset PCRE2_LDFLAGS
        unset with_pcre2_libs
        return
    fi
    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $PCRE2_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $PCRE2_CPPFLAGS"
}

# see if we can figure out where the pcre2 library is
AC_ARG_WITH([pcre2-dir],
  [AS_HELP_STRING([--with-pcre2-dir@<:@=DIR@:>@],
                  [PCRE2 directory])],
  [if test ! -d "${with_pcre2_dir}"; then AC_MSG_ERROR([bad value ${with_pcre2_dir} for --with-pcre2-dir]); unset with_pcre2_dir; fi],
  [with_pcre2_dir="$PCRE2_DIR"])

AC_MSG_CHECKING([for pcre2 libraries and header files])
for dir in "${with_pcre2_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/pcre2 /usr/local/pcre2 /opt/local /sw /usr/sfw /opt/sfw; do
    if test -z "$dir"; then
        continue
    fi
    find_pcre2 $dir
    if test -n "$PCRE2_LDFLAGS"; then
        break
    fi
done
if test -z "$PCRE2_LDFLAGS"; then
    AC_MSG_ERROR([no pcre2 library found])
fi
if test -n "$pcre2_static"; then
   AC_MSG_RESULT([includes $with_pcre2_includes libs $with_pcre2_libs (static)])
else
   AC_MSG_RESULT([includes $with_pcre2_includes libs $with_pcre2_libs (shared)])
fi

set_bz2lib_cppflags() {
    if test "$1" != "/usr/include"; then
        BZ2_CPPFLAGS=-I$1
    fi
}

find_bz2lib() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
	    dir="/"
	    lib=/lib${LIBSUFFIX}
	    inc=/usr/include
	fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi
    # if we only want the static bz2lib libraries
    if test -n "$bz2lib_static"; then
    	if test -f "$lib/libbz2.a"; then
	    BZ2_LDFLAGS="$lib/libbz2.a"
	    bzlib_type="forced static"
    	fi
    else
        if test -f "$lib/libbz2.${SHLIB_SUFFIX}"; then
	    if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
       	        BZ2_LDFLAGS="-L$lib"
    	    fi
	    BZ2_LDFLAGS="$BZ2_LDFLAGS -lbz2"
	    bzlib_type="shared"
    	elif test -f "$lib/libbz2.a"; then
            if test "$mingw" = yes -a "$make_dll" = yes; then
	    	BZ2_LDFLAGS="-Wl,$lib/libbz2.a"
	    else
	        BZ2_LDFLAGS="$lib/libbz2.a"
            fi
	    bzlib_type="static"
    	fi
    fi
    if test -z "$BZ2_LDFLAGS"; then
        return
    fi

    # try to find include files
    if test -f "$inc/bzlib.h"; then
        set_bz2lib_cppflags $inc
    else
        unset bzlib_type
        unset BZ2_LDFLAGS
	return
    fi

    AC_MSG_RESULT([$lib ($bzlib_type) $inc])

    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $BZ2_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $BZ2_CPPFLAGS"
}

# see if we can figure out where the bz2lib library is
AC_ARG_WITH([bz2lib-dir],
  [AS_HELP_STRING([--with-bz2lib-dir@<:@=DIR@:>@],
                  [bz2lib directory])],
  [if test ! -d "${with_bz2lib_dir}"; then AC_MSG_ERROR(bad value ${with_bz2lib_dir} for --with-bz2lib-dir); unset with_bz2lib_dir; fi],
  [with_bz2lib_dir="$BZ2_DIR"])

AC_MSG_CHECKING([for bz2lib libraries and header files])
for dir in "${with_bz2lib_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/bzlib /usr/local/bzlib /opt/local /sw /usr/sfw /opt/sfw; do
    if test -z "$dir"; then
        continue
    fi
    find_bz2lib $dir
    if test -n "$BZ2_LDFLAGS"; then
        break
    fi
done
if test -z "$BZ2_LDFLAGS"; then
    AC_MSG_ERROR([no bzlib library found])
fi

# Check for libmongoc (MongoDB C driver) - optional
AC_MSG_CHECKING([for libmongoc libraries and header files])
if pkg-config --exists libmongoc-1.0 2>/dev/null; then
    MONGODB_CPPFLAGS="`pkg-config --cflags libmongoc-1.0`"
    MONGODB_LIBS="`pkg-config --libs libmongoc-1.0`"
    have_mongodb=yes
    AC_MSG_RESULT([yes (pkg-config)])
else
    have_mongodb=no
    AC_MSG_RESULT([no])
fi
AC_SUBST([MONGODB_CPPFLAGS])
AC_SUBST([MONGODB_LIBS])

set_openssl_cppflags() {
    if test "$1" != "/usr/include"; then
        OPENSSL_CPPFLAGS=-I$1
    fi
}

find_openssl() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
	    dir="/"
	    lib=/lib${LIBSUFFIX}
	    inc=/usr/include
 	fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi
    # if we only want the static openssl libraries
    if test -n "$openssl_static"; then
    	if test -f "$lib/libssl.a" -a -f "$lib/libcrypto.a"; then
            OPENSSL_LDFLAGS="$lib/libssl.a $lib/libcrypto.a"
            openssl_type="forced static"
            #AC_MSG_RESULT([$lib (forced static)])
    	elif test "$bits" = 64 -a -f "$lib/64/libssl.a" -a -f "$lib/64/libcrypto.a"; then
            OPENSSL_LDFLAGS="$lib/64/libssl.a $lib/64/libcrypto.a"
            lib=$lib/64
            openssl_type="forced static"
            #AC_MSG_RESULT([$lib/64 (forced static)])
    	fi
    else
        if test -f "$lib/libssl.${SHLIB_SUFFIX}" -a -f "$lib/libcrypto.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                OPENSSL_LDFLAGS="-L$lib"
            fi
            OPENSSL_LDFLAGS="$OPENSSL_LDFLAGS -lssl -lcrypto"
            openssl_type="shared"
            #AC_MSG_RESULT([$lib (shared)])
            elif test -f "$lib/libssl.a" -a -f "$lib/libcrypto.a"; then
            if test "$mingw" = yes -a "$make_dll" = yes; then
                OPENSSL_LDFLAGS="-Wl,$lib/libssl.a -Wl,$lib/libcrypto.a"
                #OPENSSL_LDFLAGS="-L$lib -lssl -lcrypto"
            else
                if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                    OPENSSL_LDFLAGS="-L$lib"
                fi
                OPENSSL_LDFLAGS="$OPENSSL_LDFLAGS -lssl -lcrypto"
            fi
            openssl_type="static"
            #AC_MSG_RESULT([$lib (static)])
        elif test "$bits" = 64 -a -f "$lib/64/libssl.${SHLIB_SUFFIX}" -a -f "$lib/64/libcrypto.${SHLIB_SUFFIX}"; then
            OPENSSL_LDFLAGS="-L$lib/64 -lssl -lcrypto"
            lib=$lib/64
            openssl_type="shared"
            #AC_MSG_RESULT([$lib/64 (shared)])
        elif test "$bits" = 64 -a -f "$lib/64/libssl.a" -a -f "$lib/64/libcrypto.a"; then
            OPENSSL_LDFLAGS="-L$lib -lssl -lcrypto"
            openssl_type="static"
       	    #AC_MSG_RESULT([$lib (static)])
    	fi
    fi
    if test -z "$OPENSSL_LDFLAGS"; then
        return
    fi
    #AC_MSG_CHECKING([for openssl include files])
    # try to find include files
    if test -f "$inc/openssl/ssl.h"; then
        #AC_MSG_RESULT([found: $inc/openssl])
        set_openssl_cppflags $inc
    else
        unset OPENSSL_LDFLAGS
        unset openssl_type
        return;
        #AC_MSG_ERROR([not found])
    fi


    if test -f "$inc/openssl/engine.h"; then
        AC_DEFINE(HAVE_OPNESSL_ENGINE_H, 1, [define if openssl/engine.h is available])
    fi

    AC_MSG_RESULT([$lib ($openssl_type) $inc])

    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $OPENSSL_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $OPENSSL_CPPFLAGS"
}

if test "$mingw" = yes; then
    LIBS="$LIBS -lgdi32 -lz -lpsapi"
    # if compiling for mingw32, binary modules delivered with qore have to link with libqore
    MODULE_LDFLAGS="$MODULE_LDFLAGS -L\${top_builddir}/lib -lqore"
    AC_SUBST(MODULE_LDFLAGS)
fi

# see if we can figure out where the openssl library is
AC_ARG_WITH([openssl-dir],
  [AS_HELP_STRING([--with-openssl-dir@<:@=DIR@:>@],
                  [openssl directory])],
  [if test ! -d "${with_openssl_dir}"; then AC_MSG_ERROR(bad value ${with_openssl_dir} for --with-openssl-dir); unset with_openssl_dir; fi],
  [with_openssl_dir="$OPENSSL_DIR"])

AC_MSG_CHECKING([for openssl libraries and header files])
for dir in "${with_openssl_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/openssl /usr/local/openssl /opt/local /sw /usr/sfw /opt/sfw; do
    if test -z "$dir"; then
        continue
    fi
    find_openssl $dir
    if test -n "$OPENSSL_LDFLAGS"; then
        break
    fi
done
if test -z "$OPENSSL_LDFLAGS"; then
    AC_MSG_ERROR([no openssl library found])
else
    SAVE_CPPFLAGS="$CPPFLAGS"
    SAVE_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $QORE_LIB_CPPFLAGS $OPENSSL_CPPFLAGS"
    LIBS="$OPENSSL_LDFLAGS $QORE_LIB_LDFLAGS $LIBS"

    AC_MSG_CHECKING([for MDC2 in openssl])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/evp.h>]],
    [[const EVP_MD *evp = EVP_mdc2();]])],
       [AC_MSG_RESULT([found])
        AC_DEFINE([HAVE_OPENSSL_MDC2], 1, [define if the MDC2 algorithm is available in openssl])],
       [AC_MSG_RESULT([not found])])

    # check for SHA-0 algorithm
    AC_MSG_CHECKING([for SHA in openssl])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/evp.h>]],
    [[const EVP_MD *evp = EVP_sha();]])],
       [AC_MSG_RESULT([found])
        AC_DEFINE([HAVE_OPENSSL_SHA], 1, [define if the sha0 algorithm is available in openssl])],
       [AC_MSG_RESULT([not found])])

    # check for SHA512 algorithm
    AC_MSG_CHECKING([for SHA224, SHA256, SHA384, SHA512 in openssl])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/evp.h>]],
    [[const EVP_MD *evp = EVP_sha512();]])],
       [AC_MSG_RESULT([found])
        AC_DEFINE([HAVE_OPENSSL_SHA512], 1, [define if the sha512 algorithm is available in openssl])],
       [AC_MSG_RESULT([not found])])

    # check for openssl const correctness (later versions have it)
    AC_MSG_CHECKING([const correctness in openssl headers])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/x509.h>]],
    [[const unsigned char *p; d2i_X509(0, &p, 0l);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_OPENSSL_CONST], 1, [Define if openssl headers correctly use const])],
       [AC_MSG_RESULT([old style, not const correct])])

    # check for const required with SSL_CTX_new
    AC_MSG_CHECKING([const required with SSL_CTX_new in openssl headers])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[const SSL_METHOD *meth; SSL_CTX_new(meth);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([NEED_SSL_CTX_NEW_CONST], 1, [Define if SSL_CTX_new() requires a const argument])],
       [AC_MSG_RESULT([no const required])])

    # check for return value with HMAC_Update and friends
    AC_MSG_CHECKING([return value supported for HMAC_Update() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/hmac.h>]],
    [[int rc = HMAC_Update(0, 0, 0);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_OPENSSL_HMAC_RV], 1, [Define if HMAC_Update() and friends support a return value])],
       [AC_MSG_RESULT([no return values are supported])])

    # check for X509_get_signature_nid()
    AC_MSG_CHECKING([X509_get_signature_nid() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/x509.h>]],
    [[int rc = X509_get_signature_nid(0);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_X509_GET_SIGNATURE_NID], 1, [Define if X509_get_signature_nid() is present])],
       [AC_MSG_RESULT([no X509_get_signature_nid() present])])

    # check for X509_get0_signature() const correctness
    AC_MSG_CHECKING([X509_get0_signature() const correctness in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/x509.h>]],
    [[const ASN1_BIT_STRING* sig; const X509_ALGOR* alg; X509_get0_signature(&sig, &alg, 0);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_X509_GET0_SIGNATURE_CONST], 1, [Define if X509_get0_signature() is present and accepts const args])],
       [AC_MSG_RESULT([no X509_get0_signature() present or does not accept const args])])

    # check for X509_get0_pubkey()
    AC_MSG_CHECKING([X509_get0_pubkey() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/x509.h>]],
    [[X509_get0_pubkey(0);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_X509_GET0_PUBKEY], 1, [Define if X509_get0_pubkey() is present])],
       [AC_MSG_RESULT([no X509_get0_pubkey() present])])

    # check for OPENSSL_init_crypto()
    AC_MSG_CHECKING([OPENSSL_init_crypto() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/crypto.h>]],
    [[int rc = OPENSSL_init_crypto(0, 0);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_OPENSSL_INIT_CRYPTO], 1, [Define if OPENSSL_init_crypto() is present])],
       [AC_MSG_RESULT([no OPENSSL_init_crypto() present])])

    # check for SSL_set_max_proto_version()
    AC_MSG_CHECKING([SSL_set_max_proto_version() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[SSL_set_max_proto_version(nullptr, 0x0303);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_SSL_SET_MAX_PROTO_VERSION], 1, [Define if SSL_set_max_proto_version() is present])],
       [AC_MSG_RESULT([no SSL_set_max_proto_version() present])])

    # check for SSL_set_hostflags()
    AC_MSG_CHECKING([SSL_set_hostflags() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[SSL_set_hostflags(nullptr, 0x4);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_SSL_SET_HOSTFLAGS], 1, [Define if SSL_set_hostflags() is present])],
       [AC_MSG_RESULT([no SSL_set_hostflags() present])])

    # check for SSL_set1_hosts()
    AC_MSG_CHECKING([SSL_set1_host() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[SSL_set1_host(nullptr, nullptr);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_SSL_SET1_HOST], 1, [Define if SSL_set1_host() is present])],
       [AC_MSG_RESULT([no SSL_set1_host() present])])

    # check for EVP_MAC_CTX_new()
    AC_MSG_CHECKING([EVP_MAC_CTX_new() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/evp.h>]],
    [[EVP_MAC_CTX_new(nullptr);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_EVP_MAC_CTX_NEW], 1, [Define if EVP_MAC_CTX_new() is present])],
       [AC_MSG_RESULT([no EVP_MAC_CTX_new() present])])

    # check for SSL_read_ex()
    AC_MSG_CHECKING([SSL_read_ex() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[size_t num; SSL_read_ex(nullptr, nullptr, 0, &num);]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_SSL_READ_EX], 1, [Define if SSL_read_ex() is present])],
       [AC_MSG_RESULT([no HAVE_SSL_READ_EX() present])])

    # check for TLS_server_method()
    AC_MSG_CHECKING([TLS_server_method() in openssl])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <openssl/ssl.h>]],
    [[TLS_server_method();]])],
       [AC_MSG_RESULT([ok])
        AC_DEFINE([HAVE_TLS_SERVER_METHOD], 1, [Define if TLS_server_method() is present])],
       [AC_MSG_RESULT([no HAVE_TLS_SERVER_METHOD() present])])

    CPPFLAGS="$SAVE_CPPFLAGS"
    LIBS="$SAVE_LIBS"
fi

set_dl_cppflags() {
    if test "$1" != "/usr/include"; then
        DL_CPPFLAGS=-I$1
    fi
}

find_dl() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
            dir="/"
            lib=/lib${LIBSUFFIX}
            inc=/usr/include
        fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi

    if test "$mingw" = yes -a -f "$lib/libdl.a"; then
        if test "$make_dll" = yes; then
    	    DL_LDFLAGS="-Wl,$lib/libdl.a"
	    else
	        DL_LDFLAGS="$DL_LDFLAGS $lib/libdl.a"
    	fi
       	AC_MSG_RESULT([$lib (static)])
    elif test -f "$lib/libdl.${SHLIB_SUFFIX}" -o -f "$lib/libdl.${SHLIB_SUFFIX}.a"; then
        if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
     	    DL_LDFLAGS="-L$lib"
        fi
    	DL_LDFLAGS="$DL_LDFLAGS -ldl"
       	AC_MSG_RESULT([$lib (shared)])
    elif test -f "$lib/libdl.a"; then
        DL_LDFLAGS="$DL_LDFLAGS $lib/libdl.a"
       	AC_MSG_RESULT([$lib (static)])
    fi

    if test -z "$DL_LDFLAGS"; then
        return
    fi
    AC_MSG_CHECKING([for dllib include files])
    # try to find include files
    if test -f "$inc/dlfcn.h"; then
        AC_MSG_RESULT([found: $inc/dlfcn.h])
        set_dl_cppflags $inc
    else
	AC_MSG_ERROR([not found])
    fi

    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $DL_LDFLAGS"
    QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $DL_CPPFLAGS"
}

# see if we can figure out where libdl is
AC_ARG_WITH([dl-dir],
  [AS_HELP_STRING([--with-dl-dir@<:@=DIR@:>@],
                  [dl directory])],
  [if test ! -d "${with_dl_dir}"; then AC_MSG_ERROR(bad value ${with_dl_dir} for --with-dl-dir); unset with_dl_dir; fi],
  [with_dl_dir="$DL_DIR"])

find_lib() {
    libname="$1"
    dir="$2"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
            dir=/usr
            lib=/usr/lib${LIBSUFFIX}
            inc=/usr/include
        else
	    dir="/"
	    lib=/lib${LIBSUFFIX}
	    inc=/usr/include
	fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi

    if test -f "$lib/lib${libname}.${SHLIB_SUFFIX}"; then
        if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
     	    LIB_LDFLAGS="-L$lib"
        fi
	LIB_LDFLAGS="$LIB_LDFLAGS -l${libname}"
       	AC_MSG_RESULT([$lib (shared)])
    elif test -f "$lib/lib${libname}.a"; then
        if test "$mingw" = yes -a "$make_dll" = yes; then
	    LIB_LDFLAGS="-Wl,$lib/lib${libname}.a"
	    #LIB_LDFLAGS="$LIB_LDFLAGS -l${libname}"
	else
	    LIB_LDFLAGS="$lib/lib${libname}.a"
	fi
       	AC_MSG_RESULT([$lib (static)])
    fi

    if test -z "$LIB_LDFLAGS"; then
        return
    fi

    QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $LIB_LDFLAGS"
}

if test "$need_user_dl" = yes; then
    AC_MSG_CHECKING([for dl libraries and header files])
    for dir in "${with_dl_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/mingw /opt/mingw32 /opt/local /sw /usr/sfw /opt/sfw; do
        if test -z "$dir"; then
            continue
        fi
        find_dl $dir
        if test -n "$DL_LDFLAGS"; then
            break
        fi
    done
    if test -z "$DL_LDFLAGS"; then
        if test "$windows" = yes; then
            AC_MSG_ERROR([cannot find dlfcn.h and libdl; set DL_DIR=... if available or use the --with-dl-dir=... option; a version for Windows is available at http://code.google.com/p/dlfcn-win32/downloads/list])
        else
            AC_MSG_ERROR([cannot find dlfcn.h and libdl; set DL_DIR=... if available or use the --with-dl-dir=... option])
        fi
    fi
fi

for clib in $need_libs; do
    AC_MSG_CHECKING([for the $clib library])

    #QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -l$clib"

    for dir in "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/mingw /opt/mingw32 /opt/local /sw /usr/sfw /opt/sfw; do
        if test -z "$dir"; then
            continue
        fi
        LIB_LDFLAGS=
        find_lib "$clib" "$dir"
        if test -n "$LIB_LDFLAGS"; then
            break
        fi
    done
    if test -z "$LIB_LDFLAGS"; then
        AC_MSG_ERROR([cannot find the $clib library which is needed to compile Qore for the target platform ($OS)])
    fi
done

AC_ARG_ENABLE([profile],
     [AS_HELP_STRING([--enable-profile],
		     [turn on profiling support (default=no)])],
     [case "${enable_profile}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_profile} for --enable-profile) ;;
      esac],
     [enable_profile=no])

AC_ARG_ENABLE([debug],
     [AS_HELP_STRING([--enable-debug],
		     [turn on debugging (default=yes)])],
     [case "${enable_debug}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_debug} for --enable-debug) ;;
      esac],
     [enable_debug=yes])

if test "${enable_debug}" = yes; then
   AC_DEFINE(DEBUG, 1, Define if debugging support should be included)
   # remove -O2 from CXXFLAGS so valgrind can report properly
   CXXFLAGS="`echo $CXXFLAGS | sed 's/\-O2//'`"
else
   QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS -DNDEBUG=1"
fi

if test "${enable_profile}" = yes; then
   AC_DEFINE(PROFILE, 1, Define if profiling support should be included)
fi

AC_ARG_ENABLE([optimization],
     [AS_HELP_STRING([--enable-optimization],
		     [turn on optimization (default=auto (yes unless debugging is enabled)])],
     [case "${enable_optimization}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_optimization} for --enable-optimization) ;;
      esac],
     [enable_optimization=auto])

if test "$enable_optimization" = "auto"; then
   if test "${enable_debug}" = yes; then
      enable_optimization=no
   else
      enable_optimization=yes
   fi
fi

# check for gcc visibility support
AC_MSG_CHECKING([for gcc visibility support])
if test "$GXX" = "yes"; then
   gcc_version=`$CXX -dumpversion|cut -f1 -d.`
   if test "$gcc_version" -ge "4"; then
      AC_DEFINE(HAVE_GCC_VISIBILITY, 1, if compiling with g++ and visibility support is available)
      AC_MSG_RESULT([yes, $CXX $gcc_version])
      # turn off strict aliasing for g++
      CXXFLAGS="$CXXFLAGS -fno-strict-aliasing"
   else
      AC_MSG_RESULT([no, gcc $gcc_version])
   fi
else
   AC_MSG_RESULT([no, $CXX])
fi

# checks for how to build threads
if test "$mingw" != yes; then
   ACX_PTHREAD
   if test "$acx_pthread_ok" = "no"; then
      AC_MSG_ERROR(POSIX threads do not seem to be supported on this platform, aborting)
   fi
   CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $PTHREAD_LIBS"
fi

AC_ARG_WITH([doxygen],
    [AS_HELP_STRING([--with-doxygen@<:@=PATH@:>@],
                    [path to doxygen binary])],
    [if test "${with_doxygen}" != "no" -a ! -x "${with_doxygen}"; then AC_MSG_ERROR(${with_doxygen} is not an executable in --with-doxygen); unset with_doxygen; fi ],
    [with_doxygen=auto])

if test "${with_doxygen}" = "auto"; then
   AC_MSG_CHECKING([for doxygen to build API documentation])
   # check for doxygen
   with_doxygen=`which doxygen 2>/dev/null | grep -v "no doxygen in.*"`
   if test -n "$with_doxygen"; then
      doxy_version=`$with_doxygen --version`
      AC_MSG_RESULT([found $with_doxygen $doxy_version])
   else
      AC_MSG_RESULT([not found])
   fi
fi

if test -n "${with_doxygen}"; then
   AC_SUBST(DOXYGEN_CMD, $with_doxygen)
fi

# check for LLVM bug 22050: http://llvm.org/bugs/show_bug.cgi?id=22050 (clang++ 3.5.0 generates invalid code for the conditional/ternary operator with -O1 or above in some cases)
if test "$cross_compiling" != "yes" -a -n "`echo $CXX|grep clang`"; then
   CXXFLAGS_SAVE="$CXXFLAGS"
   CXXFLAGS="$CXXFLAGS -O2"
   AC_MSG_CHECKING([for llvm bug 22050 with the conditional/ternary operator code generation])
   AC_RUN_IFELSE([AC_LANG_PROGRAM(
   [[#include <sys/types.h>
#include <stdlib.h>]],
   [[
   union u {
      size_t _refptr;
      void setPtr(int* refptr, bool readonly = false) {
         _refptr = (size_t)refptr;
         if (readonly)
            _refptr |= 1;
      }
      int* getPtr() const {
         // the line below is compiled incorrectly with clang++ 3.5.0 -O1 or above
         return (int*)((_refptr & 1L) ? (_refptr ^ 1L) : _refptr);
      }
   };
   u u;
   int i = 100;
   u.setPtr(&i);
   exit(&i == u.getPtr());
   ]])],
      [AC_MSG_RESULT([found; enabling workaround])
       AC_DEFINE([HAVE_LLVM_BUG_22050], 1, [define if compiling with clang with llvm bug 22050])],
      [AC_MSG_RESULT([not found])])

   CXXFLAGS="$CXXFLAGS_SAVE"
fi

# check if socklen_t is needed
if test "$mingw" != yes; then
   TYPE_SOCKLEN_T
fi

# check how to include and link iconv
LIBICONV_AC_FUNC_ICONV
if test "$func_iconv" = "yes"; then
   QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS $INCICONV"
   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS $LIBICONV"
else
   AC_MSG_ERROR([iconv not found])
fi

# see if the socket library is required
AC_SEARCH_LIBS(setsockopt, socket)

# see if the nsl library is required
AC_SEARCH_LIBS(gethostbyname, nsl)

# look for librt (needed by clock_gettime() on some platforms)
AC_SEARCH_LIBS(clock_gettime, rt)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h inttypes.h netdb.h netinet/in.h stddef.h stdlib.h string.h strings.h sys/socket.h sys/time.h unistd.h execinfo.h cxxabi.h arpa/inet.h sys/socket.h sys/statvfs.h winsock2.h ws2tcpip.h glob.h sys/un.h termios.h netinet/tcp.h pwd.h sys/wait.h getopt.h stdint.h poll.h grp.h net/if_dl.h linux/if_packet.h])

# check for umem.h
AC_CHECK_HEADER([umem.h], have_umem_h=yes, have_umem_h=no)
if test "$have_umem_h" = "yes"; then
   AC_DEFINE(HAVE_UMEM_H, 1, [define if umem.h is available])
   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -lumem"
fi

# see if struct flock is declared
if test "$ac_cv_header_fcntl_h" = yes; then
   AC_MSG_CHECKING([for struct flock in fcntl.h])
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
   [[#include <fcntl.h>]], [[struct flock fl;]])],
      [AC_MSG_RESULT(yes)
       AC_DEFINE(HAVE_STRUCT_FLOCK, 1, [Define to 1 if you have struct flock in fcntl.h])],
      [AC_MSG_RESULT(no)])
fi

if test "$need_user_dl" = yes; then
    SAVE_CPPFLAGS="$CPPFLAGS"
    SAVE_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$CPPFLAGS $DL_CPPFLAGS"
    LDFLAGS="$LDFLAGS $DL_LDFLAGS"

    # make sure libdl is available
    AC_MSG_CHECKING([if libdl is available])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[#include <dlfcn.h>]],
            [[int a = 1;]])],
        [AC_MSG_RESULT([yes])],
        [AC_MSG_ERROR([no; cannot link with -ldl])])

    # make sure dlfcn.h header file can be parsed without 'extern "C" {}'
    AC_MSG_CHECKING([if dlfcn.h is c++ compatible])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[#include <dlfcn.h>]],
            [[dlopen("tmp.dll", RTLD_LAZY|RTLD_GLOBAL);]])],
        [AC_MSG_RESULT([yes])],
        [AC_MSG_RESULT([no; needs wrapper])
        AC_DEFINE([NEED_DLFCN_WRAPPER], [1], [define if dlfcn.h is not c++ compatible])])

   CPPFLAGS="$SAVE_CPPFLAGS"
   LDFLAGS="$SAVE_LDFLAGS"
fi

# pthread_setname_np is not portable; there are at least 4 different implementations
AC_MSG_CHECKING([if pthread_setname_np() exists and takes 1 argument])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <pthread.h>]],
                [[pthread_setname_np("foo"); return 0;]])
        ],[
                AC_MSG_RESULT([yes])
                AC_DEFINE(QORE_HAVE_PTHREAD_SETNAME_NP_1, [1],
                    [pthread_setname_np() takes 1 argument])
        ],[
                AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([if pthread_setname_np() exists and takes 2 arguments])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <pthread.h>]],
                [[pthread_setname_np(pthread_self (), "foo"); return 0;]])
        ],[
                AC_MSG_RESULT([yes])
                AC_DEFINE(QORE_HAVE_PTHREAD_SETNAME_NP_2, [1],
                    [pthread_setname_np() takes 2 arguments])
        ],[
                AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([if pthread_setname_np() exists and takes 3 arguments])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <pthread.h>]],
                [[pthread_setname_np(pthread_self(), "foo", (void*)0); return 0;]])
        ],[
                AC_MSG_RESULT([yes])
                AC_DEFINE(QORE_HAVE_PTHREAD_SETNAME_NP_3, [1],
                    [pthread_setname_np() takes 3 arguments])
        ],[
                AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([if pthread_set_name_np() exists])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <pthread.h>]],
                [[pthread_set_name_np(pthread_self(), "foo"); return 0;]])
        ],[
                AC_MSG_RESULT([yes])
                AC_DEFINE(QORE_HAVE_PTHREAD_SET_NAME_NP, [1],
                    [pthread_set_name_np() exists])
        ],[
                AC_MSG_RESULT([no])
])

# pthread_getattr_np() is non-portable
AC_MSG_CHECKING([if pthread_getattr_np() exists])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <pthread.h>]],
                [[pthread_attr_t attr; pthread_getattr_np(pthread_self(), &attr); return 0;]])
        ],[
                AC_MSG_RESULT([yes])
                AC_DEFINE(QORE_HAVE_PTHREAD_GETATTR_NP, [1],
                    [have pthread_getattr_np()])
        ],[
                AC_MSG_RESULT([no])
])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# checks for an STL hash_map template
AC_CXX_MAKE_HASH_MAP_H([include/qore/hash_map_include.h])

# checks for an STL slist template
AC_CXX_MAKE_SLIST_H([include/qore/slist_include.h])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
##AC_FUNC_MALLOC
AC_FUNC_MKTIME
##AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([bzero floor getaddrinfo gethostbyaddr gethostbyname gethostname getnameinfo getrlimit \
    gettimeofday memmove memset mkfifo putenv regcomp select socket setsockopt getsockopt strcasecmp \
    strchr strdup strerror strspn strstr atoll strtol strtoll isblank localtime_r gmtime_r exp2 \
    clock_gettime realloc timegm seteuid setegid setenv unsetenv round \
    pthread_attr_getstacksize getpwuid_r getpwnam_r \
    getgrgid_r getgrnam_r backtrace glob system inet_ntop inet_pton lstat fsync lchown chown setsid \
    setuid mkfifo random kill getppid getgid getegid getuid geteuid setuid seteuid setgid setegid sleep \
    usleep nanosleep readlink symlink access strcasestr strncasecmp setgroups getgroups poll realpath \
    memmem getifaddrs pthread_get_stacksize_np pthread_get_name_np pthread_getname_np link_ntoa])

# some systems have internal gethostby*_r in libc but don't hide the
# symbols, so we look if they are declared before checking in the libraries
AC_CHECK_DECLS([gethostbyaddr_r, gethostbyname_r],[],[],[[#include <netdb.h>]])
if test "$ac_cv_have_decl_gethostbyaddr_r" = "yes"; then
AC_CHECK_FUNCS([gethostbyaddr_r])
fi
if test "$ac_cv_have_decl_gethostbyname_r" = "yes"; then
AC_CHECK_FUNCS([gethostbyname_r])
fi

# try to find strtoimax on HPUX
AC_MSG_CHECKING([for strtoimax])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#include <inttypes.h>]], [[strtoimax("10", NULL, 10);]])],
   [AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_STRTOIMAX, 1, [Define to 1 if you have strtoimax])],
   [AC_MSG_RESULT(no)])

# Check definition of gethostbyaddr_r (glibc2 defines this with 8 arguments)
ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([style of gethost* routines], qore_cv_gethost_style,
AC_LANG_PUSH([C++])

# Do not treat warnings as errors if we are linking against other libc
# this is to work around gcc not being permissive on non-system includes
# with respect to ANSI C++
# We also remove the -fbranch-probabilities option as this will give warnings
# about not profiled code, which confuses configure
if test "$GXX" = "yes" -a "$with_other_libc" = "no"
then
  CXXFLAGS="`echo $CXXFLAGS -Werror | sed 's/-fbranch-probabilities//'`"
fi

AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>]],
[[int skr;
 struct hostent *foo = gethostbyaddr_r((const char *) 0,
  0, 0, (struct hostent *) 0, (char *) NULL,  0, &skr); return (foo == 0);]])],
qore_cv_gethost_style=solaris, qore_cv_gethost_style=other))
AC_LANG_POP([C++])
CXXFLAGS="$ac_save_CXXFLAGS"
if test "$qore_cv_gethost_style" = "solaris"
then
  AC_DEFINE([HAVE_SOLARIS_STYLE_GETHOST], [1],
            [Solaris defines gethostbyaddr_r with 7 arguments. glibc2 defines this with 8 arguments])
fi

# Check definition of gethostbyname_r (glibc2.0.100 is different from Solaris)
ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([style of gethostname_r routines], qore_cv_gethostname_style,
AC_LANG_PUSH([C++])
if test "$GXX" = "yes" -a "$with_other_libc" = "no"
then
  CXXFLAGS="`echo $CXXFLAGS -Werror | sed 's/-fbranch-probabilities//'`"
fi
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>]],
[[int skr;

 skr = gethostbyname_r((const char *) 0,
  (struct hostent*) 0, (char*) 0, 0, (struct hostent **) 0, &skr);]])],
qore_cv_gethostname_style=glibc2, qore_cv_gethostname_style=other))
AC_LANG_POP([C++])
CXXFLAGS="$ac_save_CXXFLAGS"
if test "$qore_cv_gethostname_style" = "glibc2"
then
  AC_DEFINE([HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE], [1],
            [Solaris define gethostbyname_r with 5 arguments. glibc2 defines this with 6 arguments])
fi

# Check 3rd argument of getthostbyname_r
ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([3 argument to gethostname_r routines], qore_cv_gethostname_arg,
AC_LANG_PUSH([C++])
if test "$GXX" = "yes" -a "$with_other_libc" = "no"
then
  CXXFLAGS="`echo $CXXFLAGS -Werror | sed 's/-fbranch-probabilities//'`"
fi
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>]],
[[int skr;
 skr = gethostbyname_r((const char *) 0, (struct hostent*) 0, (struct hostent_data*) 0);]])],
qore_cv_gethostname_arg=hostent_data, qore_cv_gethostname_arg=char))
AC_LANG_POP([C++])
CXXFLAGS="$ac_save_CXXFLAGS"
if test "$qore_cv_gethostname_arg" = "hostent_data"
then
  AC_DEFINE([HAVE_GETHOSTBYNAME_R_RETURN_INT], [1],
            [In OSF 4.0f the 3'd argument to gethostname_r is hostent_data *])
fi

# check if the compiler supports variadic local arrays
AC_MSG_CHECKING([c++ compiler supports local variadic arrays])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
  [[]],
  [[int x = 2; int y[x];]])],
     [AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_LOCAL_VARIADIC_ARRAYS], 1, [Define if the c++ compiler supports local variadic arrays])],
     [AC_MSG_RESULT([no])])

# turn on optimizations if we know how
if test "$enable_optimization" = "yes"; then
    if test "$GXX" = "yes"; then
        # only add optimizations if not there already
    	if test -z "`echo $CXXFLAGS|grep -e -O`"; then
            if test "$host_os" = "mingw32"; then
                # for some reason mingw32 segfaults with -O3, -O2 is OK
                    CXXFLAGS="$CXXFLAGS -O2"
            else
                    CXXFLAGS="$CXXFLAGS -O3"
            fi
        fi
    elif test "$CXX" = "CC" -a "`echo $host_os|cut -b-7`" = "solaris"; then
        # only add optimizations if not there already
        if test -z "`echo $CXXFLAGS|grep -e -xO`"; then
            # remove -g with Solaris CC as it reduces optimization levels
            CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` -xO5" # Solaris CC
        fi
    elif test -n "`echo $CXX|grep aCC`"; then
        # only add optimizations if not there already
        if test -z "`echo $CXXFLAGS|grep -e +O`"; then
            # remove -g with HP-UX aCC as it reduces optimization levels
            CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` +O4" # HP-UX aCC
        fi
    fi
else
    if test "$GXX" = "yes"; then
        CXXFLAGS="`echo $CXXFLAGS| sed s/-O2//`"  # gcc
    fi
fi

# hack to add C++ libs with oracle sun studio on solaris
if test "$CXX" = "CC" -a "`echo $host_os|cut -b-7`" = "solaris"; then
    LIBS="$LIBS -lCrun -lCstd"
fi

# try to find libtbbmalloc on Darwin and link with it if present
if test "$darwin" = "yes"; then
    SAVE_LIBS="$LIBS"
    LIBS="$LIBS $QORE_LIB_LDFLAGS $OPENSSL_LDFLAGS -ltbbmalloc"

    # check for libtbbmalloc
    AC_MSG_CHECKING([for libtbbmalloc])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [[#include <stdlib.h>]],
    [[void* p = malloc(2);]])],
       [LIBTBB_FOUND=1
        AC_MSG_RESULT([found])
        AC_DEFINE([HAVE_TBBMALLOC], 1, [define if libtbbmalloc])],
       [AC_MSG_RESULT([not found])])

    if test "$LIBTBB_FOUND" = "1"; then
      QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -ltbbmalloc"
    fi

    LIBS="$SAVE_LIBS"
fi

# do we need -latomic on this platform
AC_MSG_CHECKING([for pure compile-time atomic functionality])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[#include <atomic>]],
[[bool b = std::atomic_int{}.is_lock_free();]])],
   [ATOMIC_COMPILE_ONLY=1
    AC_MSG_RESULT([found])
    AC_DEFINE([HAVE_ATOMIC_COMPILE_ONLY], 1, [define if there is no need to link with libatomic])],
   [AC_MSG_RESULT([needs libatomic])])
if test "$ATOMIC_COMPILE_ONLY" != "1"; then
    SAVE_LIBS="$LIBS"
    LIBS="$LIBS $QORE_LIB_LDFLAGS $OPENSSL_LDFLAGS -latomic"
    AC_MSG_CHECKING([for libatomic])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [[#include <atomic>]],
    [[bool b = std::atomic_int{}.is_lock_free();]])],
       [LIBATOMIC_FOUND=1
        AC_MSG_RESULT([found])
        AC_DEFINE([HAVE_LIBATOMIC], 1, [define if libatomic])],
       [AC_MSG_RESULT([not found])])
    if test "$LIBATOMIC_FOUND" = "1"; then
      QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -latomic"
    fi
    LIBS="$SAVE_LIBS"
fi

# turn on some build flags for non-debugging, non-profiling builds
#if test "$enable_debug" = "no" -a "$enable_profile" = "no" -a "$enable_optimization" = "yes" ; then
    # we can no longer strip binaries and libraries with non-debugging, non-profiling builds on Darwin with -Wl,-x because it breaks linking :(
#fi

# check compiler settings
# 1: turn on all warnings if we know how
# 2: make sure compiler settings are correct for assembly if we need it
if test "$GXX" = "yes"; then
    CXXFLAGS="$CXXFLAGS -Wall"  # gcc
else
    if test "$solaris" = yes; then
        # turn on all warnings for the oracle sun studio compiler
        #CXXFLAGS="$CXXFLAGS +w"
        if test "$enable_64bit" = yes; then
            if test "$build_cpu" = sparc; then
                solaris_cc_sparc64=yes
            else
                solaris_cc_x86_64=yes
            fi
        else
            if test "$build_cpu" = sparc; then
                solaris_cc_sparc32=yes
            else
                solaris_cc_i386=yes
            fi
        fi
        need_assembly=yes
    elif test "$hpux" = yes; then
        if test "$cpu_family" = parisc; then
            if test "$enable_64bit" = yes; then
                hpux_acc_parisc64=yes
            else
                hpux_acc_parisc32=yes
            fi
            need_assembly=yes
        elif test "$cpu_family" = "ia64"; then
            if test "$enable_64bit" = yes; then
                hpux_acc_64bit_ia64=yes
            else
                hpux_acc_32bit_ia64=yes
            fi
            need_assembly=yes
        fi
    fi
fi

if test "$need_assembly" = yes; then
    CCAS="$CXX"
    CCASFLAGS="$CCASFLAGS $CXXFLAGS"
fi

# do a symlink for the correct spec file
if test ! -f qore.spec; then
    if test -f /etc/redhat-release -o -f /etc/fedora-release; then
        ln -s qore.spec-fedora qore.spec
    elif test -f /etc/SuSE-release; then
        ln -s qore.spec-opensuse qore.spec
        ln -s qore.changes-opensuse qore.changes
    else
        ln -s qore.spec-multi qore.spec
    fi
fi

# get module API version to set pkglibdir
MODULE_API_MAJOR=`grep QORE_MODULE_API_MAJOR include/qore/ModuleManager.h|cut -f3 -d\ |head -1`
MODULE_API_MINOR=`grep QORE_MODULE_API_MINOR include/qore/ModuleManager.h|cut -f3 -d\ |head -1`
MODULE_API=$MODULE_API_MAJOR.$MODULE_API_MINOR

mypkglibdir=qore-modules
AC_SUBST([MODULE_API_MAJOR])
AC_SUBST([MODULE_API_MINOR])
AC_SUBST(MODULE_API)
AC_SUBST([mypkglibdir])

# save more version information in config.h
VERSION_MAJOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f1 -d.`
VERSION_MINOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f2 -d.`
VERSION_SUB=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f3 -d.`
VERSION_PATCH=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f4 -d.`
if test -z "$VERSION_PATCH"; then
   VERSION_PATCH=0
fi

# set version information
AC_DEFINE_UNQUOTED([VERSION_MAJOR], $VERSION_MAJOR, [major version number])
AC_DEFINE_UNQUOTED([VERSION_MINOR], $VERSION_MINOR, [minor version number])
AC_DEFINE_UNQUOTED([VERSION_SUB], $VERSION_SUB, [sub version number])
AC_DEFINE_UNQUOTED([VERSION_PATCH], $VERSION_PATCH, [patch version number])
AC_DEFINE_UNQUOTED([TARGET_ARCH], "$ARCH", [host type])
AC_DEFINE_UNQUOTED([TARGET_OS], "$OS", [host type])
AC_DEFINE_UNQUOTED([TARGET_BITS], $bits, [32 or 64 bit build])

AM_CONDITIONAL([COND_DEBUG],                   [test "$enable_debug" = yes])
AM_CONDITIONAL([COND_PROFILE],                 [test "$enable_profile" = yes])
AM_CONDITIONAL([COND_SINGLE_COMPILATION_UNIT], [test "$enable_single_compilation_unit" = yes])
AM_CONDITIONAL([COND_MACOSX],                  [test "$darwin" = yes])
AM_CONDITIONAL([COND_DOXYGEN],                 [test -n "$with_doxygen"])
AM_CONDITIONAL([COND_SOLARIS_CC_X86_64],       [test "$solaris_cc_x86_64" = yes])
AM_CONDITIONAL([COND_SOLARIS_CC_I386],         [test "$solaris_cc_i386" = yes])
AM_CONDITIONAL([COND_SOLARIS_CC_SPARC32],      [test "$solaris_cc_sparc32" = yes])
#AM_CONDITIONAL([COND_SOLARIS_CC_SPARC64],      [test "$solaris_cc_sparc64" = yes])
AM_CONDITIONAL([COND_HPUX_ACC_PARISC32],       [test "$hpux_acc_parisc32" = yes])
#AM_CONDITIONAL([COND_HPUX_ACC_PARISC64],       [test "$hpux_acc_parisc64" = yes])
AM_CONDITIONAL([COND_HPUX_ACC_64BIT_IA64],     [test "$hpux_acc_64bit_ia64" = yes])
#AM_CONDITIONAL([COND_HPUX_ACC_32BIT_IA64],     [test "$hpux_acc_3b2it_ia64" = yes])
AM_CONDITIONAL([COND_SIGNAL_HANDLING],         [test "$no_sig_handling" != yes])
AM_CONDITIONAL([COND_NEED_GLOB],               [test "$ac_cv_func_glob" != yes])
AM_CONDITIONAL([COND_HAVE_TERMIOS],            [test "$ac_cv_header_termios_h" = yes])
AM_CONDITIONAL([COND_NEED_INET_NTOP],          [test "$ac_cv_func_inet_ntop" != yes])
AM_CONDITIONAL([COND_NEED_INET_PTON],          [test "$ac_cv_func_inet_pton" != yes])
AM_CONDITIONAL([COND_NEED_GETOPT_LONG],        [test "$ac_cv_header_getopt_h" != yes])
AM_CONDITIONAL([COND_HAVE_MPFR],               [test "$have_mpfr_h" = yes])
AM_CONDITIONAL([COND_MINGWCC],                 [test "$mingw" = yes])
AM_CONDITIONAL([COND_MONGODB],                 [test "$have_mongodb" = yes])

# find prefix
if test -n "${exec_prefix}" -a "${exec_prefix}" != NONE; then
    myprefix="${exec_prefix}"
elif test -n "${prefix}" -a "${prefix}" != NONE; then
    myprefix="${prefix}"
else
    myprefix=${ac_default_prefix}
fi
AC_SUBST([myprefix])

if test "${prefix}" = "NONE"; then
    prefix=${myprefix}
fi

# libdir that is set for Makefile in Makefile.am, but not set for configure in
# configure.ac. This way (for example) config.status generates qore.pc from qore.pc.in
# that points to incorrect libdir (using .../lib instead of .../lib64 on x64 machines,
# for example). --PQ 03-Aug-2016
libdir="\${exec_prefix}/lib${LIBSUFFIX}"
AC_SUBST([libdir])

# set module path
if test "$windows" = yes; then
    winprefix="z:${myprefix}"
    winlibdir=`echo ${mypkglibdir}|sed 's/\/$//'`
    # we don't know the real final install location, but at least we can set a valid windows path
    MODULE_DIR="${winprefix}/lib${LIBSUFFIX}/${winlibdir}"
    USER_MODULE_DIR="${winprefix}/share/${winlibdir}"
else
    MODULE_DIR="${myprefix}/lib${LIBSUFFIX}/${mypkglibdir}"
    if test -z "${datarootdir}"; then
        datarootdir="${prefix}/share"
        AC_SUBST([datarootdir])
        USER_MODULE_DIR="${datarootdir}/${mypkglibdir}"
    elif test -n "echo ${datarootdir}|grep \$"; then
        USER_MODULE_DIR="`eval echo ${datarootdir}`/${mypkglibdir}"
    else
        USER_MODULE_DIR="${datarootdir}/${mypkglibdir}"
    fi
fi
AC_SUBST([MODULE_DIR])
AC_SUBST([USER_MODULE_DIR])

if test "${VERSION_PATCH}" != "0"; then
    mymodverdir=qore-modules/${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}.${VERSION_PATCH}
else
    mymodverdir=qore-modules/${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}
fi
AC_SUBST([mymodverdir])

# get version-dependent module path
if test "$windows" = yes; then
    # we don't know the real final install location, but at least we can set a valid windows path
    MODULE_VER_DIR="${MODULE_DIR}/${mymodverdir}"
    USER_MODULE_VER_DIR="${USER_MODULE_DIR}/${mymodverdir}"
else
    MODULE_VER_DIR="${myprefix}/lib${LIBSUFFIX}/${mymodverdir}"
    if test -n "echo ${datarootdir}|grep \$"; then
        USER_MODULE_VER_DIR="`eval echo ${datarootdir}`/${mymodverdir}"
    else
        USER_MODULE_VER_DIR="${datarootdir}/${mymodverdir}"
    fi
fi
AC_SUBST([MODULE_VER_DIR])
AC_SUBST([USER_MODULE_VER_DIR])

modexeclibdir="${MODULE_VER_DIR}"
AC_SUBST([modexeclibdir])

# cmake variables used in the cmake build, needed to make the cmake build configure QoreConfig.cmake correctly
CMAKE_INSTALL_FULL_BINDIR="`eval echo ${myprefix}/bin`"
CMAKE_INSTALL_FULL_INCLUDEDIR="`eval echo ${myprefix}/include`"
CMAKE_INSTALL_FULL_LIBDIR="`eval echo ${myprefix}/lib${LIBSUFFIX}`"
CMAKE_INSTALL_FULL_DATADIR="`eval echo ${datarootdir}`"
AC_SUBST([CMAKE_INSTALL_FULL_BINDIR])
AC_SUBST([CMAKE_INSTALL_FULL_INCLUDEDIR])
AC_SUBST([CMAKE_INSTALL_FULL_LIBDIR])
AC_SUBST([CMAKE_INSTALL_FULL_DATADIR])

if test "$enable_profile" = yes; then
   QORE_LIB_CXXFLAGS="$QORE_LIB_CXXFLAGS -g -pg"
   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -g -pg"
   BUILD_TYPE=dev
elif test "$enable_debug" = yes; then
   QORE_LIB_CXXFLAGS="$QORE_LIB_CXXFLAGS -g"
   QORE_LIB_LDFLAGS="$QORE_LIB_LDFLAGS -g"
   BUILD_TYPE=dev
else
   BUILD_TYPE=prod
fi
AC_SUBST([BUILD_TYPE])

# set up flags
QORE_LIB_CXXFLAGS="`echo $QORE_LIB_CXXFLAGS $CXXFLAGS -lm|sed 's/ +/ /g'`"
AC_SUBST([QORE_LIB_CXXFLAGS])

QORE_LIB_LDFLAGS="`echo $QORE_LIB_LDFLAGS $LDFLAGS $LIBS -lm|sed 's/ +/ /g'`"
AC_SUBST([QORE_LIB_LDFLAGS])

QORE_LIB_CPPFLAGS="`echo $QORE_LIB_CPPFLAGS $CPPFLAGS -D_QORE_LIB_INTERN|sed 's/ +/ /g'`"
QORE_LIB_CPPFLAGS="$QORE_LIB_CPPFLAGS -DMODULE_DIR=\\\"${MODULE_DIR}\\\" -DMODULE_VER_DIR=\\\"${MODULE_VER_DIR}\\\" -DUSER_MODULE_DIR=\\\"${USER_MODULE_DIR}\\\" -DUSER_MODULE_VER_DIR=\\\"${USER_MODULE_VER_DIR}\\\""
AC_SUBST([QORE_LIB_CPPFLAGS])
if test -n "$QORE_LIB_CXXFLAGS"; then
   QORE_LIB_CFLAGS="$QORE_LIB_CPPFLAGS $QORE_LIB_CXXFLAGS"
else
   QORE_LIB_CFLAGS="$QORE_LIB_CPPFLAGS"
fi

# save compiler settings in config file
AC_DEFINE_UNQUOTED([QORE_LIB_CXX], "$CXX", [c++ compiler used])
AC_DEFINE_UNQUOTED([QORE_LIB_CPPFLAGS], "$QORE_LIB_CPPFLAGS", [preprocessor flags])
AC_DEFINE_UNQUOTED([QORE_LIB_CXXFLAGS], "$QORE_LIB_CXXFLAGS", [c++ compiler flags])
AC_DEFINE_UNQUOTED([QORE_LIB_CFLAGS],   "$QORE_LIB_CFLAGS", [c++ compiler flags])
AC_DEFINE_UNQUOTED([QORE_LIB_LDFLAGS],  "$QORE_LIB_LDFLAGS", [linker flags])
AC_DEFINE_UNQUOTED([QORE_BUILD_HOST],   "$QORE_BUILD_HOST", [Qore build host info])

AC_CONFIG_FILES([Makefile lib/Makefile])
AC_CONFIG_FILES([modules/astparser/Makefile modules/astparser/src/Makefile])
AC_CONFIG_FILES([modules/reflection/Makefile modules/reflection/src/Makefile])
AC_CONFIG_FILES([modules/logger_bin/Makefile modules/logger_bin/src/Makefile])
AC_CONFIG_FILES([modules/mongodb/Makefile modules/mongodb/src/Makefile])
AC_CONFIG_FILES([qore.pc])
AC_CONFIG_FILES([cmake/QoreConfig.cmake cmake/QoreConfigVersion.cmake])
AC_OUTPUT

echo "*** BUILTIN FEATURES ***"
printf "%-35s: %s %s" library "$OS" "$cpu_family"
if test "$enable_64bit" = "yes"; then
   printf " (64-bit)\n"
else
   printf " (32-bit)\n"
fi

printf "%-35s: %s %s\n" "binary module directories" "${MODULE_DIR}" "${MODULE_VER_DIR}"
printf "%-35s: %s %s\n" "user module directories" "${USER_MODULE_DIR}" "${USER_MODULE_VER_DIR}"

show_library_feature() {
   printf "%-35s: " "$1"
   if test "$2" = "yes"; then
      printf "%-4s" yes
   else
      printf "%-4s" no
   fi
   if test -n "$3"; then
      printf " (%s)" $3
   fi
   echo
}

show_library_feature optimizations $enable_optimization
show_library_feature "single compilation unit" $enable_single_compilation_unit
echo "*** DEBUG OPTIONS (i.e. performance penalty) ***"
show_library_feature profiling $enable_profile
show_library_feature debug $enable_debug
echo "*** OPTIONAL BUILTIN MODULES ***"
show_library_feature "mongodb module" $have_mongodb

echo
echo "*** MODULES DELIVERED SEPARATELY - SEE 'README-MODULES' FOR MORE INFO ***"

# hack to use our libtool if on mingw to enable static libs to be linked in to the qore DLL
if test "$mingw" = yes; then
   # backup generated libtool
   if test ! -e libtool.orig ; then
      cp libtool libtool.orig
   fi
   sed -e 's/deplibs_check_method=.*/deplibs_check_method=pass_all/g' -e 's/libext=.*/libext=a/g' libtool > libtool.tmp
   mv libtool.tmp libtool
fi
