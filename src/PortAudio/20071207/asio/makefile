#
# Makefile for ASIO
#
# This does not build a .lib, it just makes the .obj files
# which are then included in portaudio.lib.
#
 
######################################################################
#
# Targets
#
######################################################################

all: lib
	
!include ../../../make/common.mak

######################################################################
#
# asio.lib
#
######################################################################

ASIO_LIB = asio.lib

ASIO_OBJS = \
	  ASIOConvertSamples.obj \
	  asio.obj \
	  asiodrivers.obj \
	  asiolist.obj \
	  combase.obj \
	  debugmessage.obj \
	  register.obj

objs: $(ASIO_OBJS)

lib: $(ASIO_LIB)

$(ASIO_OBJS) : $(@:.obj=.cpp)
	$(cc) $(INCLUDES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)

$(ASIO_LIB) : $(ASIO_OBJS)
	lib -out:$(ASIO_LIB) $(ASIO_OBJS)

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
