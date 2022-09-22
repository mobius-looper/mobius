#
# Makefile for the core utilities
#
 
######################################################################
#
# Targets
#
######################################################################

all: util wavtest wavdiff xmltest xomtest doc2html threadtest
	
!include ..\make\common.mak

######################################################################
#
# util.lib
#
# A library of common utilities.
#
######################################################################

UTIL_OBJS = \
	  Trace.obj Util.obj Vbuf.obj List.obj Map.obj Thread.obj \
	  TcpConnection.obj MessageCatalog.obj \
	  XmlBuffer.obj XmlParser.obj XmlModel.obj XomParser.obj \
	  WaveFile.obj

UTIL_NAME	= util
UTIL_LIB	= $(UTIL_NAME).lib
UTIL_DLL	= $(UTIL_NAME).dll
UTIL_RES	= 
UTIL_CFLAGS	= $(DLL_CFLAGS)
UTIL_LIBS	= $(DLL_SYSLIBS)

# proper offsets will depend on what we're being combined with,
# need to adjust later
# jsl - I forget why this is necessary we don't do it in any
# of the other libraries
UTIL_BASE	= 0x60a60000
#UTIL_BASEOPT	= /BASE:$(UTIL_BASE)

$(UTIL_DLL) : $(UTIL_OBJS) $(UTIL_RES)
	$(link) $(DLL_LFLAGS) $(UTIL_LIBS) $(UTIL_BASEOPT) \
	-out:$(UTIL_DLL) @<<
	$(UTIL_OBJS) $(UTIL_RES)
<<
	cp $(UTIL_LIB) $(LIBDIR)

$(UTIL_LIB) : $(UTIL_OBJS)
	lib -out:$(UTIL_LIB) @<<
	$(UTIL_OBJS)
<<

$(UTIL_OBJS) : $(@:.obj=.cpp)
	$(cc) $(UTIL_CFLAGS) /c $(@:.obj=.cpp)

util: $(UTIL_LIB)

######################################################################
#
# wavtest.exe
#
# Simple test driver for the WaveFile IO classes.
#
######################################################################

WAV_NAME	= wavtest
WAV_EXE		= $(WAV_NAME).exe
WAV_OBJS	= $(WAV_NAME).obj
WAV_LIBS	= $(UTIL_LIB) $(EXE_SYSLIBS)

$(WAV_EXE) : $(WAV_OBJS) $(UTIL_LIB)
	$(link) $(EXE_LFLAGS) $(WAV_LIBS) \
	-out:$(WAV_EXE) @<<
	$(WAV_OBJS)
<<

$(WAV_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

wavtest: $(WAV_EXE)

######################################################################
#
# wavdiff.exe
#
# Simple test driver for the WaveFile IO classes.
#
######################################################################

WDIFF_NAME	= wavdiff
WDIFF_EXE	= $(WDIFF_NAME).exe
WDIFF_OBJS	= $(WDIFF_NAME).obj
WDIFF_LIBS	= $(UTIL_LIB) $(EXE_SYSLIBS)

$(WDIFF_EXE) : $(WDIFF_OBJS) $(UTIL_LIB)
	$(link) $(EXE_LFLAGS) $(WDIFF_LIBS) \
	-out:$(WDIFF_EXE) @<<
	$(WDIFF_OBJS)
<<

$(WDIFF_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

wavdiff: $(WDIFF_EXE)

######################################################################
#
# xmltest.exe
#
# Simple test driver for the XML mini parser.
#
######################################################################

XMLT_NAME	= xmltest
XMLT_EXE	= $(XMLT_NAME).exe
XMLT_OBJS	= $(XMLT_NAME).obj
XMLT_LIBS	= $(UTIL_LIB) $(EXE_SYSLIBS)

$(XMLT_EXE) : $(XMLT_OBJS) $(UTIL_LIB)
	$(link) $(EXE_LFLAGS) $(XMLT_LIBS) \
	-out:$(XMLT_EXE) @<<
	$(XMLT_OBJS)
<<

$(XMLT_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

xmltest: $(XMLT_EXE)

######################################################################
#
# xomtest.exe
#
# Simple test driver for the XML mini parser with XOM model.
#
######################################################################

XOM_NAME	= xomtest
XOM_EXE		= $(XOM_NAME).exe
XOM_OBJS	= $(XOM_NAME).obj
XOM_LIBS	= $(UTIL_LIB) $(EXE_SYSLIBS)

$(XOM_EXE) : $(XOM_OBJS) $(UTIL_LIB)
	$(link) $(EXE_LFLAGS) $(XOM_LIBS) \
	-out:$(XOM_EXE) @<<
	$(XOM_OBJS)
<<

$(XOM_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

xomtest: $(XOM_EXE)

######################################################################
#
# threadtest.exe
#
# Thead and csect tests
#
######################################################################

THREAD_LIBS = $(UTIL_LIB) $(EXE_SYSLIBS)
THREAD_OBJS = threadtest.obj

$(THREAD_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

threadtest.exe : $(UTIL_LIB) threadtest.obj
	$(link) $(EXE_LFLAGS) $(THREAD_LIBS) \
	-out:threadtest.exe @<<
	threadtest.obj
<<

threadtest: threadtest.exe

######################################################################
#
# doc2html.exe
#
######################################################################

DOC_NAME	= Doc2Html
DOC_EXE		= $(DOC_NAME).exe
DOC_OBJS	= $(DOC_NAME).obj
DOC_LIBS	= $(UTIL_LIB) $(EXE_SYSLIBS)

$(DOC_EXE) : $(DOC_OBJS) $(UTIL_LIB)
	$(link) $(EXE_LFLAGS) $(DOC_LIBS) \
	-out:$(DOC_EXE) @<<
	$(DOC_OBJS)
<<

$(DOC_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

doc2html: $(DOC_EXE)

######################################################################
#
# rules
#
######################################################################

%.obj : %.cpp
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

%.obj : %.cxx
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
