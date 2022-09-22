#
# Makefile for the MIDI interface abstraction
#

######################################################################
#
# targets
#
######################################################################

default: lib miditest wintest midicaps

!include ..\make\common.mak

INCLUDES = /I../util
LIBS = /LIBPATH:../util util.lib

######################################################################
#
# midi.lib
#
# note that the name of the DLL has been changed, having midi.dll 
# on the path seemed to screw up the "technotoys" applications, this
# may be a standard dll name?
#
######################################################################

MIDI_OFILES = \
	MidiUtil.obj MidiEvent.obj MidiSequence.obj MidiMap.obj MidiPort.obj \
	MidiEnv.obj MidiTimer.obj MidiInput.obj MidiOutput.obj \
	MidiInterface.obj \
	WinMidiEnv.obj WinMidiTimer.obj WinMidiInput.obj WinMidiOutput.obj \
	WinSysexBuffer.obj


MIDI_NAME	= jmidi
MIDI_LIB	= $(MIDI_NAME).lib
MIDI_DLL	= $(MIDI_NAME).dll
MIDI_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)
MIDI_LIBS	= $(LIBS) $(DLL_SYSLIBS)

$(MIDI_DLL): $(MIDI_OFILES)
	$(link) $(DLL_LFLAGS) $(MIDI_LIBS) \
	-out:$(MIDI_DLL) @<<
	$(MIDI_OFILES)
<<

$(MIDI_LIB): $(MIDI_OFILES)
	lib -out:$(MIDI_LIB) @<<
	$(MIDI_OFILES)
<<

$(MIDI_OFILES):
	$(cc) $(MIDI_CFLAGS) /c $(@:.obj=.cpp)

lib: $(MIDI_LIB)

######################################################################
#
# miditest
#
######################################################################

MT_OFILES	= miditest.obj
MT_LIBS		= $(MIDI_LIB) $(LIBS) $(EXE_SYSLIBS)
MT_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)

miditest.exe: $(MT_OFILES)
	$(link) $(EXE_LFLAGS) /out:$@ $(MT_OFILES) $(MT_LIBS)

miditest: miditest.exe

# default c rules are for DLL's, override them for exe's

$(MT_OFILES): $(MIDI_OFILES)
	cl $(MT_CFLAGS) /c $*.cpp

######################################################################
#
# wintest
#
######################################################################

WT_OFILES	= wintest.obj
WT_LIBS		= $(MIDI_LIB) $(LIBS) $(EXE_SYSLIBS)
WT_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)

wintest.exe: $(WT_OFILES)
	$(link) $(EXE_LFLAGS) /out:$@ $(WT_OFILES) $(WT_LIBS)

wintest: wintest.exe

# default c rules are for DLL's, override them for exe's

$(WT_OFILES): $(MIDI_OFILES)
	cl $(WT_CFLAGS) /c $*.cpp

######################################################################
#
# midicaps.exe
#
######################################################################

MD_OFILES	= midicaps.obj
MD_LIBS		= $(MIDI_LIB) $(LIBS) $(EXE_SYSLIBS)
MD_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)

midicaps.exe: $(MD_OFILES)
	$(link) $(EXE_LFLAGS) /out:$@ $(MD_OFILES) $(MD_LIBS)

midicaps: midicaps.exe

# default c rules are for DLL's, override them for exe's

$(MD_OFILES): $(MIDI_OFILES)
	cl $(MD_CFLAGS) /c $*.cpp

######################################################################
######################################################################
######################################################################

