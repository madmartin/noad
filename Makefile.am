#SUBDIRS = libvo

noinst_LIBRARIES=libnoad.a
libnoad_a_SOURCES=tools.cpp vdr_cl.cpp svdrpc.cpp  \
	tools.h vdr_cl.h svdrpc.h 

bin_PROGRAMS=noad checkMarks
noad_SOURCES=audiotools.cpp cchecklogo.cpp ccontrol.cpp cgetlogo.cpp ctoolbox.cpp \
		noad.cpp noaddata.cpp tnoad.cpp ffmpeg_decoder.cpp libmpeg2_decoder.cpp mpeg_decoder.cpp \
	        main.cpp \
	    	audiotools.h cchecklogo.h ccontrol.h cgetlogo.h ctoolbox.h channels.h \
		noad.h tnoad.h noaddata.h videodir.h \
		ffmpeg_decoder.h libmpeg2_decoder.h mpeg2wrap_ffmpeg.h mpeg_decoder.h yuvbuf.h

checkMarks_SOURCES=checkmarks.cpp
noad_LDADD=libnoad.a
checkMarks_LDADD=libnoad.a

noinst_PROGRAMS=@PICSRC@ @TOOLSRC@
EXTRA_PROGRAMS=markpics showindex

markpics_SOURCES=markpics.cpp
markpics_LDADD=libnoad.a @markpics_ldadd@
markpics_CXXFLAGS=@markpiccxxflags@
showindex_SOURCES=showindex.cpp
showindex_LDADD=libnoad.a

bin_SCRIPTS=allnewnoad allnoad allnoadnice allnoaduncut clearlogos noadifnew stat2html statupd noadcall.sh checkAllMarks

EXTRA_DIST=$(bin_SCRIPTS) README.en mystat.HTML noad_scripts.zip
