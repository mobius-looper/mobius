#
# Multitrack Sequencer
#

default: lib testseq install

!include ..\make\common.mak

INCLUDES = /I../util /I../midi /I../mmdev /I../midifile
LIBS = /LIBPATH:../util /LIBPATH:../midi /LIBPATH:../mmdev /LIBPATH:../midifile \
	util.lib jmidi.lib mmdev.lib smf.lib

######################################################################
#
# seq.lib
#
######################################################################

SEQ_HFILES	= seq.h
SEQ_OFILES	= seq.obj seqint.obj track.obj recording.obj recmerge.obj \
			metro.obj api.obj

SEQ_NAME	= seq
SEQ_LIB		= $(SEQ_NAME).lib
SEQ_DLL		= $(SEQ_NAME).dll
SEQ_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)
SEQ_LIBS	= $(LIBS)

$(SEQ_DLL): $(SEQ_OFILES)
	$(link) $(DLL_LFLAGS) $(DLL_SYSLIBS) $(SEQ_LIBS) \
	-out:$(SEQ_NAME).dll @<<
	$(SEQ_OFILES)
<<

$(SEQ_LIB): $(SEQ_OFILES)
	lib -out:$(SEQ_LIB) @<<
	$(SEQ_OFILES) $(LIBS)
<<

$(SEQ_OFILES):
	cl $(SEQ_CFLAGS) /c $*.cxx

lib: $(SEQ_LIB)

######################################################################
#
# testseq.exe
#
# Basic play test.
#
######################################################################

TEST_OFILES 	= testseq.obj
TEST_LIBS	= seq.lib $(EXE_SYSLIBS)
TEST_CFLAGS	= $(INCLUDES) $(EXE_CFLAGS)

testseq.exe: $(TEST_OFILES) seq.lib
	$(link) $(EXE_LFLAGS) /out:$@ $(TEST_OFILES) $(TEST_LIBS)

$(TEST_OFILES):
	cl $(TEST_CFLAGS) /c $*.cxx

testseq: testseq.exe

######################################################################
#
# install
#
######################################################################

install:
	cp $(SEQ_HFILES) $(INCDIR)
	cp $(SEQ_LIB) $(LIBDIR)

######################################################################
######################################################################
######################################################################
