#
# Makefile for the audio abstraction.  Requires both PortAudio and MidiEnv.
# MidiEnv is needed just to get the Timer which is used for some
# metering.  Would be better to split timer services out into an
# independent library.
#
#

######################################################################
#
# targets
#
######################################################################

default: lib devices test

!include ..\make\common.mak

#PORTAUDIO = ../../tools/portaudio
PORTAUDIO = ../PortAudio/20071207/win

PA_INCLUDES = /I$(PORTAUDIO)
# ole32 needed for COM, user32 for GetDesktopWindow and CharLowerBuf
PA_LIBS = /LIBPATH:$(PORTAUDIO) portaudio.lib ole32.lib user32.lib

INCLUDES = /I../util /I../midi $(PA_INCLUDES)

LIBS = /LIBPATH:../util util.lib /LIBPATH:../midi jmidi.lib $(PA_LIBS)

######################################################################
#
# audio.lib
#
######################################################################

AUDIO_HFILES	= AudioInterface.h
AUDIO_OFILES	= AudioInterface.obj WinAudioInterface.obj

AUDIO_NAME	= audio
AUDIO_LIB	= $(AUDIO_NAME).lib
AUDIO_DLL	= $(AUDIO_NAME).dll
AUDIO_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)
AUDIO_LIBS	= $(LIBS) $(DLL_SYSLIBS)

$(AUDIO_DLL): $(AUDIO_OFILES)
	$(link) $(DLL_LFLAGS) $(AUDIO_LIBS) \
	-out:$(AUDIO_DLL) @<<
	$(AUDIO_OFILES)
<<

$(AUDIO_LIB): $(AUDIO_OFILES)
	lib -out:$(AUDIO_LIB) @<<
	$(AUDIO_OFILES)
<<

$(AUDIO_OFILES):
	$(cc) $(AUDIO_CFLAGS) /c $(@:.obj=.cpp)

lib: $(AUDIO_LIB)

######################################################################
#
# devices.exe
#
######################################################################

DEV_OBJS	= devices.obj
DEV_LIBS	= $(AUDIO_LIB) $(LIBS) $(EXE_SYSLIBS)
DEV_EXE		= devices.exe

$(DEV_EXE) : $(DEV_OBJS) $(AUDIO_LIB)
	$(link) $(EXE_LFLAGS) $(AUDIO_LIB) $(DEV_LIBS) -out:$(DEV_EXE) @<<
	$(DEV_OBJS)
<<KEEP

devices: $(DEV_EXE)

######################################################################
#
# audiotest.exe
#
######################################################################

TST_OFILES	= audiotest.obj
TST_LIBS	= $(AUDIO_LIB) $(LIBS) $(EXE_SYSLIBS)
TST_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)

audiotest.exe: $(TST_OFILES)
	$(link) $(EXE_LFLAGS) /out:$@ $(TST_OFILES) $(TST_LIBS)

test: audiotest.exe

# default c rules are for DLL's, override them for exe's
# update: this is old and I don't remember why this was necessary
# devices.exe doesn't do this and it works fine...
 
$(TST_OFILES): $(AUDIO_OFILES)
	cl $(TST_CFLAGS) /c $*.cpp

######################################################################
######################################################################
######################################################################
