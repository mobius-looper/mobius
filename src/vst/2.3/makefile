# Copyright (C) 2004 Jeff Larson.  All rights reserved.
# $Id: makefile,v 1.2 2009/01/06 04:06:36 jeff Exp $
 
######################################################################
#
# Targets
#
######################################################################

#all: vstsdk vstgui
all: vstsdk
	
!include ../../make/common.mak

######################################################################
#
# vstsdk.lib
#
######################################################################

VST_LIB	  = vst.lib
VST_OBJS  = AudioEffect.obj audioeffectx.obj aeffguieditor.obj \
	  vstgui.obj vstcontrols.obj

$(VST_LIB) : $(VST_OBJS)
	lib -out:$(VST_LIB) @<<
	$(VST_OBJS)
<<

vstsdk: $(VST_LIB)

######################################################################
#
# vstgui.exe
#
######################################################################

GUI_EXE	  = vstgui.exe
GUI_OBJS  = vstgui.obj aeffguieditor.obj vstcontrols.obj
GUI_LIBS  = vstsdk.lib $(DLL_SYSLIBS) winmm.lib

$(GUI_EXE) : $(HOST_OBJS)
	$(link) $(EXE_LFLAGS) $(GUI_LIBS) Shell32.lib -out:$(GUI_EXE) @<<
	$(GUI_OBJS)
<<

vstgui: $(GUI_EXE)

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(INCLUDES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)

rebuild: clean all

install:
	cp $(LIBSNDFILE)/libsndfile.dll ../../../../bin


######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
