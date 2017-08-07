#SUBDIRS = libvo

noinst_LIBRARIES=libnoad.a
libnoad_a_SOURCES=tools.cpp vdr_cl.cpp svdrpc.cpp tools.h vdr_cl.h svdrpc.h mpeg2wrap.cpp mpeg2wrap.h

bin_PROGRAMS=noad
noad_SOURCES=cchecklogo.cpp ccontrol.cpp cgetlogo.cpp ctoolbox.cpp noad.cpp tnoad.cpp noaddata.cpp\
	           audiotools.cpp main.cpp \
	    	audiotools.h cchecklogo.h ccontrol.h cgetlogo.h ctoolbox.h noad.h tnoad.h noaddata.h videodir.h

noad_LDADD=libnoad.a

noinst_PROGRAMS=@PICSRC@ @TOOLSRC@
EXTRA_PROGRAMS=markpics showindex

markpics_SOURCES=markpics.cpp
markpics_LDADD=libnoad.a -lMagick++
markpics_CXXFLAGS=@markpiccxxflags@
showindex_SOURCES=showindex.cpp
showindex_LDADD=libnoad.a

bin_SCRIPTS=allnewnoad allnoad allnoadnice allnoaduncut clearlogos noadifnew stat2html statupd noadcall.sh

EXTRA_DIST=$(bin_SCRIPTS) README.en mystat.HTML noad_scripts.zip
