dnl ---------------------------------------------------------------------------
dnl Process this file with autoconf to produce a configure script.
dnl
dnl ---------------------------------------------------------------------------
dnl top-level configure.ac for noad
dnl Copyright (c) by theNoad <theNoad@ulmail.net>
dnl ---------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl initialisation
dnl ---------------------------------------------------------------------------
AC_PREREQ(2.57)
AC_INIT(noad, 0.8.8, theNoad@ulmail.net)
AM_INIT_AUTOMAKE(dist-bzip2)
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([cchecklogo.cpp])
AC_CONFIG_HEADER([config.h])
AC_DEFINE(_GNU_SOURCE, 1, [Define for setting a GNU environment])

AC_DEFINE(FFMPEGDECODING,1,[Define for ffmpeg_using])
AC_USE_SYSTEM_EXTENSIONS
CPPFLAGS="-D__STDC_CONSTANT_MACROS $CPPFLAGS"

dnl ---------------------------------------------------------------------------
# Checks for programs.
dnl ---------------------------------------------------------------------------
AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG()
AC_LANG(C++)


dnl ---------------------------------------------------------------------------
dnl always set _GNU_SOURCE
dnl because it's not set automatic on all systems
dnl ---------------------------------------------------------------------------
CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
AC_GNU_SOURCE

AC_SUBST(defprefix,$prefix)
test "$defprefix" = NONE && defprefix=$ac_default_prefix

dnl ---------------------------------------------------------------------------
# Checks for header files.
dnl ---------------------------------------------------------------------------
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/vfs.h syslog.h unistd.h])

dnl ---------------------------------------------------------------------------
# Checks for typedefs, structures, and compiler characteristics.
dnl ---------------------------------------------------------------------------
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl ---------------------------------------------------------------------------
# Checks for library functions.
dnl ---------------------------------------------------------------------------
AC_FUNC_CLOSEDIR_VOID
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([gettimeofday localtime_r memmove memset mkdir select socket strchr strdup strerror strrchr strstr])
AC_CHECK_FUNCS([strsignal])
#CXXFLAGS='-g -O3 -pg'
dnl CXXFLAGS='-v -g -O3'

dnl ---------------------------------------------------------------------------
dnl check for posix_fadvise 
dnl can posix_fadvise be used
dnl ---------------------------------------------------------------------------
AC_CHECK_DECLS(posix_fadvise, [], [], [
#include <fcntl.h>])
AC_CHECK_FUNCS([posix_fadvise])


AC_SUBST(decoderok,0)
dnl ---------------------------------------------------------------------------
dnl setup for libmpeg2
dnl look for an existing pkg-config for libmpeg2
dnl ---------------------------------------------------------------------------
AC_SUBST(libmpeg2ok,0)
AC_ARG_WITH([libmpeg2],
            [AS_HELP_STRING([--without-libmpeg2],
              [disable support for libmpeg2])],
            [],
            [with_libmpeg2=yes])
          
if test "x$with_libmpeg2" != "xno" ; then
  PKG_CHECK_MODULES(LIBMPEG2, [libmpeg2 >= 0.3.1], PKG_LIBMPEG2=1, PKG_LIBMPEG2=0)
  if test "$PKG_LIBMPEG2" = "1"; then
    LIBMPEG_VERSION="`pkg-config --modversion libmpeg2`"
    AC_MSG_NOTICE([found libmpeg2 Version $LIBMPEG_VERSION])
    AC_SUBST(LIBMPEGX,$LIBMPEG2_LIBS)
    AC_MSG_NOTICE([LIBMPEG2_LIBS=$LIBMPEG2_LIBS])
    
    dnl ---------------------------------------------------------------------------
    dnl setup for forcing static linking of libmpeg2
    dnl ---------------------------------------------------------------------------
    AC_ARG_WITH(libmpeg2-static,
	AC_HELP_STRING([--with-libmpeg2-static],[force linking of static version of libmpeg2]),
	[AC_SUBST(LIBMPEGX,["-Wl,--trace -Xlinker -Bstatic $LIBMPEG2_LIBS -lmpeg2convert -Xlinker -Bdynamic"])])
    CPPFLAGS=" $CPPFLAGS -DUSE_LIBMPGE2 $LIBMPEG2_CFLAGS"
    AC_SUBST(LIBS,"$LIBS $LIBMPEGX ")
    AC_SUBST(HAVE_LIBMPEG2,1) 
    AC_SUBST(libmpeg2ok,1)
    AC_SUBST(decoderok,1)
  fi
fi


dnl ---------------------------------------------------------------------------
dnl check for libavcodec(ffmpeg)
dnl ---------------------------------------------------------------------------
# Need libavcodec 52.26.0 for avcodec_decode_video2
AC_SUBST(ffmpegok,0)
AC_ARG_WITH([ffmpeg],
            [AS_HELP_STRING([--without-ffmpeg],
              [disable support for ffmpeg])],
            [],
            [with_ffmpeg=yes])
          
if test "x$with_ffmpeg" != "xno" ; then
  PKG_CHECK_MODULES(FFMPEG, libavutil libavformat [libavcodec >= 52.0.0], HAVE_FFMPEG=1, HAVE_FFMPEG=0)
  if test "$HAVE_FFMPEG" = "1"; then
    LIBAVCODEC_VERSION="`pkg-config --modversion libavcodec`"
    AC_MSG_NOTICE([found libavcodec Version $LIBAVCODEC_VERSION])
    OLDLDFLAGS="$LDFLAGS"
    OLDCXXFLAGS="$CXXFLAGS"
    OLDLIBS="$LIBS"
    LIBS="$LIBS $FFMPEG_LIBS"
    dnl LDFLAGS="$LDFLAGS $FFMPEG_LIBS"
    CXXFLAGS="$FFMPEG_CFLAGS $CXXFLAGS"
    AC_CHECK_MEMBER(AVIOContext.read_packet,HAVE_AVIOCONTEXT=1, HAVE_AVIOCONTEXT=0,[#include "libavformat/avformat.h"])
    if test "$HAVE_AVIOCONTEXT" = "1"; then
	CPPFLAGS="-DUSE_AVIOCONTEXT $CPPFLAGS"
    fi
    if test "$HAVE_AVIOCONTEXT" = "0" ; then
	echo ffmpeg not usable
	HAVE_FFMPEG=0
	LDFLAGS="$OLDLDFLAGS"
	CXXFLAGS="$OLDCXXFLAGS"
	LIBS="$OLDLIBS"
    else
	AC_SUBST(ffmpegok,1)
	AC_SUBST(decoderok,1)
	CXXFLAGS="-DUSE_FFMPEG $CXXFLAGS"
	AC_CHECK_MEMBER(AVFrame.width,HAVE_AVFRAMEWIDTH=1, HAVE_AVFRAMEWIDTH=0,[#include "libavcodec/avcodec.h"])
	if test "$HAVE_AVFRAMEWIDTH" = "0" ; then
	   CXXFLAGS="-DFORCE_CODECWIDTH $CXXFLAGS"
	fi
	dnl AC_SUBST(LIBS,"$LIBS $FFMPEG_LIBS")
	AC_DEFINE(HAVE_LIBAVCODEC,1,[have libavcodec for audio])
    fi
  fi
fi   

dnl ---------------------------------------------------------------------------
dnl check for feature ImageMagick++
dnl ---------------------------------------------------------------------------
AC_ARG_WITH([magick],
   AC_HELP_STRING([--with-magick],[compile with ImageMagick++ support]),
   [],
   [with_magick=no])

AC_SUBST(libmagickok,0)
if test "x$with_magick" != "xno" ; then
  PKG_CHECK_MODULES(IMAGEMAGICK, Magick++, havemagick=1, havemagick=0)
  AC_SUBST(PICSRC,"")
  AC_SUBST(markpiccxxflags)
  AC_SUBST(markpics_ldadd)
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS $IMAGEMAGICK_CFLAGS $IMAGEMAGICK_LIBS"
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([#include <Magick++.h>
    using namespace Magick;
    ],[Image image;])
    ],[mpp_defined="1"],[mpp_defined="0"]
  )
  CXXFLAGS=$ac_save_CXXFLAGS
  if test $mpp_defined -eq 1 && test $havemagick -eq 1 ; then
    PICSRC="markpics"
    markpiccxxflags="$IMAGEMAGICK_CFLAGS"
    markpics_ldadd="$IMAGEMAGICK_LIBS"
    AC_SUBST(libmagickok,1)
    LIBMAGICK_VERSION="`pkg-config --modversion Magick++`"
  fi
fi


dnl ---------------------------------------------------------------------------
dnl check for request to compile toolies
dnl ---------------------------------------------------------------------------
AC_SUBST(TOOLSRC,"")
AC_ARG_WITH([tools],
   AC_HELP_STRING([--with-tools],[compile with tools]),
   [],
   [with_tools=no])
test "x$with_tools" != xno && TOOLSRC="showindex"

LDFLAGS="$LDFLAGS -rdynamic"

dnl CXXFLAGS="-v -g -O3 $CXXFLAGS"
CFLAGS="$CFLAGS -DFFMPEGDECODING"
CPPFLAGS="$CPPFLAGS -DFFMPEGDECODING "

if test "$decoderok" != "1" ; then
  echo
  echo
  echo "can't find useable video-decoder"
  echo "please install libmpeg2 or ffmpeg or both"
  AC_MSG_ERROR("no video-decoder")
else
    AC_CONFIG_FILES([Makefile])
    AC_OUTPUT
    dnl !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    dnl don't use "test" in combination with "&&" after this point
    dnl it will set the exit-value of configure to 1
    dnl which means that configure has failed!
    dnl !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    echo
    echo
    echo " this is noad Version $VERSION"
    if test "$libmpeg2ok" = "1" ; then
       echo " configured with libmpeg2 Version $LIBMPEG_VERSION"
    else
       echo " libmpeg2-support not configured"
    fi
    if test "$ffmpegok" = "1" ; then
       echo " configured with libavcodec Version $LIBAVCODEC_VERSION"
    else
       echo " ffmpeg-support not configured"
    fi
    if test "$libmagickok" = "1" ; then
       echo " configured with Magick++ Version $LIBMAGICK_VERSION"
    fi
    if test "$with_tools" = "yes" ; then
      echo " compile with tools: $TOOLSRC"
    fi
    echo 
    echo "now start \"make\" to build noad"
    echo " and \"make install\" to install noad"
fi

