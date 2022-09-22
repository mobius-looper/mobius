# Copyright (C) 2005 Jeffrey S. Larson.  All rights reserved.

######################################################################
#
# targets
#
######################################################################

default: lib install

!include ..\make\common.mak

INCLUDES = /I../util
#LIBS = /LIBPATH:../util util.lib

######################################################################
#
# soundtouch.lib
#
# Not supposed to be static linking this, but it's such a pain in the
# ass for VST plugins.  Will provide object modules and link instructions
# on the web site instead.
#
######################################################################

# can't get this to compile, it's AMD anyway
# 3dnow_win.obj

ST_OFILES = cpu_detect_x86_win.obj mmx_optimized.obj \
	FIFOSampleBuffer.obj AAFilter.obj FIRFilter.obj \
	RateTransposer.obj TDStretch.obj SoundTouch.obj

ST_NAME		= SoundTouch
ST_LIB		= $(ST_NAME).lib
ST_DLL		= $(ST_NAME).dll
ST_CFLAGS	= /DFLOAT_SAMPLES=1 $(INCLUDES) $(DLL_CFLAGS)
ST_LIBS	= $(LIBS) $(DLL_SYSLIBS)

$(ST_DLL): $(ST_OFILES)
	$(link) $(DLL_LFLAGS) $(ST_LIBS) \
	-out:$(ST_DLL) @<<
	$(ST_OFILES)
<<

$(ST_LIB): $(ST_OFILES)
	lib -out:$(ST_LIB) @<<
	$(ST_OFILES)
<<

$(ST_OFILES):
	$(cc) $(ST_CFLAGS) /c $(@:.obj=.cpp)

lib: $(ST_LIB)

######################################################################
#
# install
#
######################################################################

install:

######################################################################
######################################################################
######################################################################

