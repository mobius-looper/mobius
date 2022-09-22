#
# Windows makefile for the function library
#

######################################################################
#
# Targets
#
######################################################################

all: lib

!include ../../make/common.mak
	 
# need midibytes.h for Midi.cpp, AudioInterface.h is unfortunately
# necessary for lots of stuff included indirectly

INCLUDES = /I.. /I../../util /I../../midi /I../../audio


######################################################################
#
# mobiusfunc.lib
#
######################################################################

FUNC_LIB = mobiusfunc.lib
                                                 
FUNC_OBJS = \
	  Bounce.obj \
	  Capture.obj \
	  Checkpoint.obj \
	  Clear.obj \
	  Confirm.obj \
	  Coverage.obj \
	  Debug.obj \
	  Divide.obj \
	  FocusLock.obj \
	  FunctionUtil.obj \
	  Insert.obj \
	  InstantMultiply.obj \
	  LoopSwitch.obj \
	  Midi.obj \
	  Move.obj \
	  Multiply.obj \
	  Mute.obj \
	  Overdub.obj \
	  Pitch.obj \
	  Play.obj \
	  Realign.obj \
	  Record.obj \
	  Replace.obj \
	  Reverse.obj \
	  Reset.obj \
	  RunScript.obj \
	  SampleTrigger.obj \
	  Save.obj \
	  Shuffle.obj \
	  Slip.obj \
	  Solo.obj \
	  Speed.obj \
	  StartPoint.obj \
	  Stutter.obj \
	  Substitute.obj \
	  Surface.obj \
	  Sync.obj \
	  TrackCopy.obj \
	  TrackGroup.obj \
	  TrackSelect.obj \
	  Trim.obj \
	  UIFunctions.obj \
	  UndoRedo.obj \
	  Window.obj

$(FUNC_LIB) : $(FUNC_OBJS)
	lib -out:$(FUNC_LIB) @<<
	$(FUNC_OBJS)
<<

lib: $(FUNC_LIB)

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(INCLUDES) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

rebuild: clean all


######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
