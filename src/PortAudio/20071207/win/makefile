#
# $Id: makefile,v 1.3 2009/07/25 18:11:19 jeff Exp $
#
# Makefile for PortAudio 19
#
# Tried to include wdmks awhile ago but it needed "DirectX SDK".
#
# Dsound.h seems to be missing in VC8.  I'm sure it's there somewhere
# but leaving Dsound out since we don't really need it.
#

 
######################################################################
#
# Targets
#
######################################################################

all: lib test
	
!include ../../../make/common.mak

ASIO_INCLUDES = /I../asio

# Need ALLOW_SMP_DANGERS to get pa_ringbuffer.c to compile
# Need PA_NO_DS to prevent pa_win_hostapis from including DirectSound

SPECIAL_DEFINES = -DALLOW_SMP_DANGERS=1 -DPA_NO_DS=1

######################################################################
#
# portaudio.lib
#
######################################################################

PA_LIB = portaudio.lib

# leave these out until we can find Dsound.h in VC8
#	pa_win_ds.obj \
#	pa_win_ds_dynlink.obj \

# this requires ksmedia.h which is probably part of the PlatformSdk,
# that we don't have in the Ancient C++ 6.0 install
#	pa_win_wdmks_utils.obj \


PA_OBJS	= \
	pa_allocation.obj \
	pa_asio.obj \
	pa_converters.obj \
	pa_cpuload.obj \
	pa_debugprint.obj \
	pa_dither.obj \
	pa_front.obj \
	pa_process.obj \
	pa_ringbuffer.obj \
	pa_skeleton.obj \
	pa_stream.obj \
	pa_trace.obj \
	pa_win_hostapis.obj \
	pa_win_util.obj \
	pa_win_waveformat.obj \
	pa_win_wmme.obj \
	pa_x86_plain_converters.obj

#$(PA_OBJS) : $(@:.obj=.c)
#	$(cc) $(ASIO_INCLUDES) $(SPECIAL_DEFINES) $(DLL_CFLAGS) /c $(@:.obj=.c)

#$(PA_OBJS) : $(@:.obj=.cpp)
#	$(cc) $(ASIO_INCLUDES) $(SPECIAL_DEFINES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)

$(PA_LIB) : $(PA_OBJS)
	lib -out:$(PA_LIB) $(PA_OBJS) ../asio/asio.lib

lib: $(PA_LIB)

######################################################################
#
# sine.exe
#
######################################################################

SINE_EXE	= patest_sine.exe
SINE_OBJS	= patest_sine.obj

# ole32 needed for COM, user32 for GetDesktopWindow and CharLowerBuf
LIBS = $(EXE_SYSLIBS) ole32.lib user32.lib

$(SINE_EXE) : $(SINE_OBJS) $(PA_LIB)
	$(link) $(EXE_LFLAGS) $(PA_LIB) $(LIBS) -out:$(SINE_EXE) @<<
	$(SINE_OBJS)
<<

test: $(SINE_EXE)

######################################################################
#
# Housekeeping targets
#
######################################################################

.c.obj:
	$(cc) $(ASIO_INCLUDES) $(SPECIAL_DEFINES) $(DLL_CFLAGS) /c $(@:.obj=.c)

.cpp.obj:
	$(cc) $(ASIO_INCLUDES) $(SPECIAL_DEFINES) $(DLL_CFLAGS) /c $(@:.obj=.cpp)

rebuild: clean all

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
