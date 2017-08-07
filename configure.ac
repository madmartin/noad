dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright (c) by theNoad <theNoad@SoftHome.net>

AC_PREREQ(2.57)
AC_INIT(noad, 0.6.0, theNoad@SoftHome.net)
AM_INIT_AUTOMAKE(dist-bzip2)
AC_CONFIG_SRCDIR([cchecklogo.cpp])
AC_CONFIG_HEADER([config.h])
AC_DEFINE(_GNU_SOURCE, 1, [Define for setting a GNU environment])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

AC_LANG(C++)

dnl always set _GNU_SOURCE
CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
AC_GNU_SOURCE

AC_SUBST(defprefix,$prefix)
test "$defprefix" = NONE && defprefix=$ac_default_prefix

dnl setup for libmpeg
dnl look for an existing pkg-config for libmpeg2
dnl else use $prefix/include/mpeg2dec and $prefix/lib
dnl   if no header is in $prefix/include/mpeg2dec
dnl     use /usr/inlcude/mpeg2dec and /usr/lib
AC_SUBST(pkgmpeginc)
AC_SUBST(pkgmpeglibdir)
AC_SUBST(pkgmpeglib)
AC_SUBST(pkgconfigured,0)
AC_MSG_CHECKING(for libmpeg with pkg-config)
if ( ( pkg-config --exists libmpeg2 ) > /dev/null 2>&1 ); then
  AC_MSG_RESULT(configure from pkg-config)
  pkgmpeginc="`pkg-config --cflags libmpeg2`"
  pkgmpeglibdir="`pkg-config --libs-only-L libmpeg2`"
  pkgmpeglib="`pkg-config --libs-only-l libmpeg2`"
  AC_SUBST(pkgconfigured,1)
else
  AC_MSG_RESULT(no, configuring with standard-values)
  AC_CHECK_FILE(${defprefix}/include/mpeg2dec/mpeg2.h,[AC_SUBST(pkgmpeginc,"-I${defprefix}/include/mpeg2dec")],[AC_SUBST(pkgmpeginc,"-I/usr/include/mpeg2dec")])
  AC_CHECK_FILE(${defprefix}/lib/libmpeg2.a,[AC_SUBST(pkgmpeglibdir,"-L${defprefix}/lib")],[AC_SUBST(pkgmpeglibdir,"-L/usr/lib")])
  AC_SUBST(pkgmpeglib,"-lmpeg2")
fi
  
dnl override include-dir for libmpeg2 if user has given a different dir
AC_SUBST(mpeginc,$pkgmpeginc)
AC_SUBST(mpegincoverridden,0)
AC_ARG_WITH(mpeginclude,
   AC_HELP_STRING([--with-mpeginclude=DIR], [directory with mpeg2 header-files [[pkg-config|PREFIX/mpeg2dec|/usr/include/mpeg2dec]]]),
   [AC_SUBST(mpeginc,"-I$with_mpeginclude") AC_SUBST(mpegincoverridden,1)])
CPPFLAGS="$mpeginc $CPPFLAGS"

dnl override lib-dir for libmpeg2 if user has given a different dir
AC_SUBST(mpegliboverridden,0)
AC_ARG_WITH(mpeglibdir,
   AC_HELP_STRING([--with-mpeglibdir=DIR], [directory with mpeg2 library-files [[pkg-config|PREFIX/lib|/usr/lib]]]),
   [AC_SUBST(mpeglibdir,"-L$with_mpeglibdir") AC_SUBST(mpegliboverridden,1)],
   [AC_SUBST(mpeglibdir,"$pkgmpeglibdir")])
LDFLAGS="$mpeglibdir $LDFLAGS"
   
AC_MSG_NOTICE(taking mpeg-includes from $mpeginc)
AC_MSG_NOTICE(using mpeg-library from $mpeglibdir)
AC_MSG_NOTICE(LDFLAGS=$LDFLAGS)








dnl check for libavcodec(ffmpeg)
AC_SUBST(libavcodecinc)
AC_SUBST(libavcodecdir)
AC_SUBST(libavcodeclib)
AC_SUBST(HAVE_AVCHEADER,0)
AC_ARG_WITH(ffmpeg,
   AC_HELP_STRING([--with-ffmpeg],[audio-tools einbinden]),
   withffmpeg="1",
   withffmpeg="0")

AC_SUBST(HAVE_AVCHEADER,0)
if test "$withffmpeg" == "1"; then
  AC_MSG_CHECKING(for libavcodec(ffmpeg))
  AC_CHECK_FILE(${defprefix}/include/ffmpeg/avcodec.h,[AC_SUBST(libavcodecinc,"-I${defprefix}/include/ffmpeg")],[AC_SUBST(libavcodecinc,"-I/usr/include/ffmpeg")])
  AC_CHECK_FILE(${defprefix}/lib/libavcodec.so,[AC_SUBST(libavcodecdir,"-L${defprefix}/lib")],[AC_SUBST(libavcodecdir,"-L/usr/local/lib")])
fi

dnl override include-dir for ffmpeg if user has given a different dir
AC_SUBST(ffmpegincoverridden,0)
AC_ARG_WITH(ffmpeginclude,
   AC_HELP_STRING([--with-ffmpeginclude=DIR], [directory with ffmpeg header-files [[PREFIX/ffmpeg|/usr/include/ffmpeg]]]),
   [AC_SUBST(libavcodecinc,"-I$with_ffmpeginclude") AC_SUBST(ffmpegincoverridden,1)])
CPPFLAGS="$libavcodecinc $CPPFLAGS"

dnl override lib-dir for libavcodec if user has given a different dir
AC_SUBST(ffmpegliboverridden,0)
AC_ARG_WITH(ffmpeglibdir,
   AC_HELP_STRING([--with-ffmpeglibdir=DIR], [directory with ffmpeg library-files [[PREFIX/lib|/usr/lib]]]),
   [AC_SUBST(ffmpeglibdir,"-L$with_ffmpeglibdir") AC_SUBST(ffmpegliboverridden,1)],
   [AC_SUBST(ffmpeglibdir,"$libavcodecdir")])
LDFLAGS="$ffmpeglibdir $LDFLAGS"

dnl check that libavcodec is usable   
AC_SUBST(LIBAVCODEC,"-lavcodec")
AC_CHECK_LIB([avcodec],[avcodec_init],[AC_SUBST(HAVE_LIBAVCODEC,1) AC_SUBST(LIBS,"$LIBS $LIBAVCODEC ") ])





   
dnl check for feature ImageMagick++
AC_ARG_WITH(magick,
   AC_HELP_STRING([--with-magick],[ImageMagick++ vorhanden]),
   havemagick="1",
   havemagick="0")

AC_SUBST(PICSRC,"")
AC_MSG_CHECKING(for Magick++(2))
CFLAGS=$ac_save_CFLAGS
ac_save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -lMagick++"
AC_COMPILE_IFELSE([
AC_LANG_PROGRAM([#include <Magick++.h>
using namespace Magick;
],[Image image;])
],[AC_MSG_RESULT("yes");mpp_defined="1"],[AC_MSG_RESULT("no");mpp_defined="0"]
)
CXXFLAGS=$ac_save_CXXFLAGS
test $mpp_defined -eq 1 && test $havemagick -eq 1 && PICSRC="markpics"

dnl check for request to compile toolies
AC_SUBST(TOOLSRC,"")
AC_ARG_WITH(tools,
   AC_HELP_STRING([--with-tools],[tools compilieren]),
   withtools="1",
   withtools="0")
test $withtools -eq 1 && TOOLSRC="showindex"


# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/vfs.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([gettimeofday localtime_r memmove memset mkdir select socket strchr strdup strerror strrchr strstr])
AC_CHECK_FUNCS([strsignal])
#CXXFLAGS='-g -O3 -pg'
CXXFLAGS='-g -O3'

if test "$withffmpeg" == "1"; then 
  dnl check for a usable avcodec.h 
  dnl have to strip the "-I" from libavcodecinc for doing this
  AC_SUBST(avcincpath,`echo $libavcodecinc|sed -e s*-I**`)
  AC_CHECK_HEADER([$avcincpath/avcodec.h],[AC_SUBST(HAVE_AVCHEADER,1)],[AC_SUBST(HAVE_AVCHEADER,0)])
fi #endif
if test "$HAVE_LIBAVCODEC" == "1" -a "$HAVE_AVCHEADER" == "1";
then
  AC_DEFINE(HAVE_LIBAVCODEC,1,[have libavcodec for audio])
fi

dnl check for a usable mpeg2.h 
dnl have to strip the "-I" from mpeginc for doing this
AC_SUBST(HAVE_MPEGHEADER,0)
AC_SUBST(mpegincpath,`echo $mpeginc|sed -e s*-I**`)
AC_CHECK_HEADER([$mpegincpath/mpeg2.h],[AC_SUBST(HAVE_MPEGHEADER,1)])

dnl setup for forcing static linking of libmpeg
AC_SUBST(LIBMPEGX,$pkgmpeglib)
AC_ARG_WITH(libmpeg2-static,
   AC_HELP_STRING([--with-libmpeg2-static],[force linking of static version of libmpeg]),
   [AC_SUBST(LIBMPEGX,"-Xlinker -Bstatic $pkgmpeglib -Xlinker -Bdynamic")])

dnl check for libmpeg2
AC_SUBST(HAVE_LIBMPEG2,0) 
AC_SUBST(LIBMPEG_VERSION,"unknown")
AC_CHECK_LIB([mpeg2],[mpeg2_reset],[AC_SUBST(HAVE_LIBMPEG2,1) AC_SUBST(LIBS,"$LIBS $LIBMPEGX ") AC_SUBST(LIBMPEG_VERSION,"0.4.0 or newer")])
if test "$HAVE_LIBMPEG2" != "1"; 
then
  AC_CHECK_LIB([mpeg2],[mpeg2_accel],[AC_SUBST(HAVE_LIBMPEG2,1) AC_SUBST(LIBS,"$LIBS $LIBMPEGX ") AC_SUBST(LIBMPEG_VERSION,"0.3.1")])
fi
if test "$HAVE_LIBMPEG2" != "1"; 
then
  AC_CHECK_LIB([mpeg2],[mpeg2_pts],[AC_SUBST(LIBMPEG_VERSION,"0.3.0 or older")])
fi
if test "$HAVE_LIBMPEG2" != "1"; 
then
  AC_MSG_ERROR("your libmpeg (Version is $LIBMPEG_VERSION) is to old or could not be detected")
fi


if test "$HAVE_LIBMPEG2" != "1"; 
then
  AC_MSG_ERROR("can't find useable libmpeg2.")
else
  if test "$HAVE_MPEGHEADER" != 1;
  then
    AC_MSG_ERROR("can't find useable mpeg2.h")
  else
    AC_CONFIG_FILES([Makefile])
    AC_OUTPUT
    AC_MSG_NOTICE(this is noad Version $VERSION)
    AC_MSG_NOTICE(configured for libmpeg2 Version $LIBMPEG_VERSION)
    test $pkgconfigured -eq 1 && AC_MSG_NOTICE(--configuration taken from pkg-config)
    test $mpegincoverridden -eq 0 && AC_MSG_NOTICE(--taking mpeg-includes from $mpeginc)
    test $mpegincoverridden -eq 1 && AC_MSG_NOTICE(--taking mpeg-includes from user-defined dir $mpeginc)
    test $mpegliboverridden -eq 0 && AC_MSG_NOTICE(--using mpeg-library from $mpeglibdir)
    test $mpegliboverridden -eq 1 && AC_MSG_NOTICE(--using mpeg-library from user-defined dir $mpeglibdir)
  fi
fi

