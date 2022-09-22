#
# Custom makefile for an older version of V19
# This is no longer used
#
 
 
######################################################################
#
# Targets
#
######################################################################

all: lib
	
NO_COMMON_CLEAN=true
!include ../../make/common.mak

#
# Common build settings
#

LIBS = $(EXE_SYSLIBS) winmm.lib

ASIO_INCLUDES = /Iasio

######################################################################
#
# portaudio19.lib
#
######################################################################

# Need a "DirectX SDK" to get some include files for pa_win_wdmks.c
#		pa_win_wdmks.obj \

# Dsound.h seems to be missing in VC8, don't really need it anyway
#dsound_wrapper.obj \
#pa_win_ds.obj \

LIB_NAME	= portaudio19
PA_LIB		= $(LIB_NAME).lib
PA_OBJS	= \
		pa_allocation.obj \
		pa_converters.obj \
		pa_cpuload.obj \
		pa_dither.obj \
		pa_front.obj \
		pa_process.obj \
		pa_skeleton.obj \
		pa_stream.obj \
		pa_trace.obj \
		pa_win_hostapis.obj \
		pa_win_util.obj \
		pa_win_wmme.obj \
		pa_x86_plain_converters.obj \
		pa_asio.obj

ASIO_OBJS = \
	  ASIOConvertSamples.obj \
	  asio.obj \
	  asiodrivers.obj \
	  asiolist.obj \
	  combase.obj \
	  debugmessage.obj \
	  register.obj

$(PA_LIB) : $(PA_OBJS) $(ASIO_OBJS)
	lib -out:$(PA_LIB) $(PA_OBJS) $(ASIO_OBJS)

lib: $(PA_LIB)

######################################################################
#
# sine.exe
#
######################################################################

SINE_NAME	= patest_sine
SINE_EXE	= $(SINE_NAME).exe
SINE_OBJS	= patest_sine.obj

$(SINE_EXE) : $(SINE_OBJS)
	$(link) $(EXE_LFLAGS) $(PA_LIB) $(LIBS) -out:$(SINE_EXE) @<<
	$(SINE_OBJS)
<<

sine: $(SINE_EXE)

######################################################################
#
# multi_sine.exe
#
######################################################################

MSINE_NAME	= patest_multi_sine
MSINE_EXE	= $(MSINE_NAME).exe
MSINE_OBJS	= patest_sine.obj

$(MSINE_EXE) : $(MSINE_OBJS)
	$(link) $(EXE_LFLAGS) $(PA_LIB) $(LIBS) -out:$(MSINE_EXE) @<<
	$(MSINE_OBJS)
<<

msine: $(MSINE_EXE)

######################################################################
#
# Housekeeping targets
#
######################################################################

.c.obj:
	$(cc) $(ASIO_INCLUDES) $(EXE_CFLAGS) /c $(@:.obj=.c)

.cpp.obj:
	$(cc) $(ASIO_INCLUDES) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

clean:
	-del /q *.exp *.lib *.dll *.exe
	-del /q *.obj *.res
	-del /q *.sbr
	-del /q *.pdb
	-del /q *.opt

rebuild: clean all

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
