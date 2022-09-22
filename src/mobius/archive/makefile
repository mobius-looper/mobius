######################################################################
#
# pitch.exe
#
######################################################################

# bring over some of the mobius.lib files so we don't have to rebuild
# the entire library every time during hacking

# SHIT! the dependnecy on FadeWindow drug all sorts of crap in through Layer
# until we can sort this out, can't build this

PITCH_OBJS	= pitch.obj Plugin2.obj \
		Audio.obj AudioCursor.obj ObjectPool.obj FadeWindow.obj FadeTail.obj
#PITCH_OBJS	= pitch.obj
PITCH_LIBS	= $(LIBS) $(DIRAC_LIBS) $(EXE_SYSLIBS)
PITCH_RC	= 
PITCH_EXE	= pitch.exe

#$(PITCH_EXE) : $(PITCH_OBJS) $(MOB_LIB)

$(PITCH_EXE) : $(PITCH_OBJS)
	$(link) $(EXE_LFLAGS) $(PITCH_LIBS) \
	-out:$(PITCH_EXE) @<<
	$(PITCH_OBJS)
<<

$(PITCH_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) $(INCLUDES) $(DIRAC_INCLUDES) /c $(@:.obj=.cpp)

pitch: $(PITCH_EXE)

######################################################################
#
# resample.exe
#
######################################################################

RES_EXE		= resample.exe
RES_OBJS	= Resample.obj

$(RES_EXE) : $(RES_OBJS) $(MOB_LIB)
	$(link) $(EXE_LFLAGS) $(MOB_LIB) $(LIBS) -out:$(RES_EXE) @<<
	$(RES_OBJS)
<<

resample: $(RES_EXE)
