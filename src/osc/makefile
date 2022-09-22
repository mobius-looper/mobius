#
# Makefile for the OSC interface library
#
# This is an abstraction built around Ross Bencina's oscpack.
# 

default: liboscpack lib osctest

!include ..\make\common.mak

INCLUDES = -I../util -I./oscpack/osc

######################################################################
#
# liboscpack
#
######################################################################

liboscpack:
	@echo oscpack
	@cd oscpack
	@$(MAKE) /nologo
	@echo osc
	@cd ..

######################################################################
#
# libosc
#
######################################################################

LIBNAME = osc.lib
LIB_O = OscInterface.obj

$(LIBNAME): $(LIB_O)
	lib -out:$(LIBNAME) @<<
	$(LIB_O) oscpack\oscpack.lib
<<

$(LIB_O):
	$(cc) /I. /I../util $(DLL_CFLAGS) /c /Fo$(@) $(@:.obj=.cpp)

lib: $(LIBNAME)

######################################################################
#
# osctest
#
######################################################################

TEST_O = OscTest.obj
TEST_LIBS = $(LIBNAME) /LIBPATH:../util util.lib $(EXE_SYSLIBS)

osctest.exe: $(LIBNAME) $(TEST_O)
	$(link) $(EXE_LFLAGS) /out:$@ $(TEST_O) $(TEST_LIBS)

$(TEST_O):
	$(cc) /I. /I../util $(DLL_CFLAGS) /c /Fo$(@) $(@:.obj=.cpp)

osctest: osctest.exe

######################################################################
#
# osctest
#
######################################################################

clean: standardclean
	@echo oscpack
	@cd oscpack
	@$(MAKE) /nologo clean
	@cd ..

######################################################################
######################################################################
######################################################################
