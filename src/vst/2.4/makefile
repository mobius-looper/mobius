#
# Windows makefile for the VST SDK
#
 
######################################################################
#
# Targets
#
######################################################################

all: libvst
	
!include ../../make/common.mak

######################################################################
#
# libvst.lib
#
######################################################################

VST_LIB	  = vst.lib

VST_OBJS = audioeffect.obj audioeffectx.obj vstplugmain.obj 

$(VST_OBJS) : $(@:.obj=.cpp)
	$(cc) $(INCLUDES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)

$(VST_LIB) : $(VST_OBJS)
	lib -out:$(VST_LIB) @<<
	$(VST_OBJS)
<<

libvst: $(VST_LIB)

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(INCLUDES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)


######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
