#
# Makefile for the OSC interface library
#
# This is an abstraction built around Ross Bencina's oscpack.
# 

default: lib oscdump

!include ..\..\make\common.mak

######################################################################
#
# liboscpack
#
######################################################################

LIBNAME = oscpack.lib

LIB_O = \
	./osc/OscTypes.obj \
	./osc/OscReceivedElements.obj \
	./osc/OscPrintReceivedElements.obj \
	./osc/OscOutboundPacketStream.obj \
	./ip/IpEndpointName.obj \
	./ip/win32/NetworkingUtils.obj \
	./ip/win32/UdpSocket.obj


$(LIBNAME): $(LIB_O)
	lib -out:$(LIBNAME) @<<
	$(LIB_O)
<<

$(LIB_O):
	$(cc) /I. $(DLL_CFLAGS) /c /Fo$(@) $(@:.obj=.cpp)

lib: $(LIBNAME)

######################################################################
#
# oscdump
#
######################################################################

DUMP_O = ./examples/OscDump.obj

oscdump.exe: $(DUMP_O)
	$(link) $(EXE_LFLAGS) /out:$@ $(DUMP_O) $(LIBNAME) $(LIBS) $(EXE_SYSLIBS)


$(DUMP_O):
	$(cc) /I. $(DLL_CFLAGS) /c /Fo$(@) $(@:.obj=.cpp)

oscdump: oscdump.exe

######################################################################
#
# clean
#
######################################################################

clean: standardclean
	@-rm -f osc/*.obj
	@-rm -f ip/*.obj
	@-rm -f ip/win32/*.obj
	@-rm -f examples/*.obj

######################################################################
######################################################################
######################################################################
