# Copyright (C) 2004 Jeff Larson.  All rights reserved.
# $Id: makefile,v 1.3 2009/01/06 04:06:36 jeff Exp $
 
######################################################################
#
# Targets
#
######################################################################

all: host delay cgui
	
!include ../../../make/common.mak

VSTSDK = ..

# although we shouldn't use midi or PA in plugins,
# share the same variable so we can use a default rule

INCLUDES = /I$(VSTSDK)

PLUGIN_LIBS = $(VSTSDK)/vst.lib Shell32.lib

HOST_LIBS = $(VSTSDK)/vst.lib $(EXE_SYSLIBS) winmm.lib

######################################################################
#
# host.exe
#
######################################################################

HOST_NAME	= Host
HOST_EXE	= $(HOST_NAME).exe
HOST_OBJS	= $(HOST_NAME).obj

$(HOST_EXE) : $(HOST_OBJS)
	$(link) $(EXE_LFLAGS) $(HOST_LIBS) -out:$(HOST_EXE) @<<
	$(HOST_OBJS)
<<

host: $(HOST_EXE)

######################################################################
#
# ADelay.dll
#
######################################################################

DELAY_DLL	= Delay.dll
DELAY_OBJS	= ADelay.obj ADelayMain.obj

$(DELAY_DLL) : $(DELAY_OBJS)
	$(link) $(DLL_LFLAGS) $(PLUGIN_LIBS) \
	-out:$(DELAY_DLL) @<<
	$(DELAY_OBJS) $(DLL_SYSLIBS)
<<

delay: $(DELAY_DLL)

######################################################################
#
# controlsgui.dll
#
######################################################################

CGUI_DLL	= controlsgui.dll
CGUI_OBJS	= controlsgui.obj controlsguieditor.obj controlsguimain.obj

$(CGUI_DLL) : $(CGUI_OBJS)
	$(link) $(DLL_LFLAGS) $(PLUGIN_LIBS) \
	-out:$(CGUI_DLL) @<<
	$(CGUI_OBJS) $(DLL_SYSLIBS)
<<

cgui: $(CGUI_DLL)
      cp $(CGUI_DLL) "c:/Program Files/VstPlugins"

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(INCLUDES) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

rebuild: clean all

install:


######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
