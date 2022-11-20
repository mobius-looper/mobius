#
# Windows Mobius makefile
#
 
######################################################################
#
# Targets
#
######################################################################

all: funclib lib uilib mobius vst expr

!include ../make/common.mak
	 
PORTAUDIO = ../portaudio/20071207/win
SOUNDTOUCH = ../soundtouch
DIRAC = ../../tools/DspDimension/dirac/Windows
VST = ../vst/2.4

PA_INCLUDES = /I$(PORTAUDIO)
# ole32 needed for COM, user32 for GetDesktopWindow and CharLowerBuf
PA_LIBS = /LIBPATH:$(PORTAUDIO) portaudio.lib ole32.lib user32.lib

VST_INCLUDES = /I$(VST)
VST_LIBS = $(VST)/vst.lib

ST_INCLUDES = /I$(SOUNDTOUCH)
ST_LIBS = /LIBPATH:$(SOUNDTOUCH) SoundTouch.lib

DIRAC_INCLUDES = /I$(DIRAC)/Source
DIRAC_LIBS = /LIBPATH:$(DIRAC) Dirac.lib

INCLUDES = /I../util /I../midi /I../audio /I../osc /I../qwin $(PA_INCLUDES) $(VST_INCLUDES) $(ST_INCLUDES)

# !! try to statically link PA with audio.lib...

LIBS = /LIBPATH:../util util.lib /LIBPATH:../midi jmidi.lib /LIBPATH:../audio audio.lib /LIBPATH:../osc osc.lib \
      $(PA_LIBS) $(ST_LIBS) $(EXE_SYSLIBS) winmm.lib

UILIBS = /LIBPATH:../util util.lib /LIBPATH:../midi jmidi.lib /LIBPATH:../audio audio.lib /LIBPATH:../osc osc.lib \
       /LIBPATH:../qwin qwin.lib \
       $(PA_LIBS) $(ST_LIBS) $(EXE_SYSLIBS) winmm.lib

PLUGIN_LIBS = /LIBPATH:../util util.lib /LIBPATH:../midi jmidi.lib /LIBPATH:../audio audio.lib /LIBPATH:../osc osc.lib \
       /LIBPATH:../qwin qwin.lib \
       $(PA_LIBS) $(VST_LIBS) $(ST_LIBS) $(GUILIBS) $(DLL_SYSLIBS) winmm.lib

######################################################################
#
# mobius.lib
#
######################################################################

MOB_LIB = mobiuscore.lib
                                                 
MOB_OBJS = \
	 Action.obj Audio.obj AudioCursor.obj \
	 Binding.obj BindingResolver.obj \
	 Components.obj ControlSurface.obj \
	 Event.obj EventManager.obj Export.obj Expr.obj \
	 FadeTail.obj FadeWindow.obj Function.obj \
	 HostConfig.obj HostInterface.obj Launchpad.obj Layer.obj Loop.obj \
	 MidiExporter.obj MidiQueue.obj MidiTransport.obj \
	 Mobius.obj MobiusConfig.obj MobiusPlugin.obj MobiusPools.obj \
	 MobiusState.obj MobiusThread.obj \
	 Mode.obj ObjectPool.obj OldBinding.obj OscConfig.obj \
	 Parameter.obj ParameterGlobal.obj ParameterSetup.obj ParameterTrack.obj \
	 ParameterPreset.obj \
	 PitchPlugin.obj Preset.obj Project.obj \
	 Recorder.obj Resampler.obj \
	 Sample.obj Script.obj Segment.obj Setup.obj \
	 Stream.obj StreamPlugin.obj SyncState.obj SyncTracker.obj \
	 Synchronizer.obj SystemConstant.obj \
	 Track.obj TriggerState.obj UserVariable.obj Variable.obj \
	 WatchPoint.obj WinInit.obj


funclib:
	@cd functions
	@$(MAKE) /nologo
	@cd ..

$(MOB_LIB) : $(MOB_OBJS) functions/mobiusfunc.lib
	lib -out:$(MOB_LIB) @<<
	$(MOB_OBJS) functions/mobiusfunc.lib
<<

lib: $(MOB_LIB) 

######################################################################
#
# ui.lib
#
######################################################################

UI_LIB = uicore.lib

UI_OBJS	= \
	AboutDialog.obj AudioDialog.obj BindingDialog.obj ButtonBindingDialog.obj \
	CalibrationDialog.obj CalibrationResultDialog.obj \
	DisplayDialog.obj GlobalDialog.obj \
	KeyBindingDialog.obj KeyHelpDialog.obj \
	MidiBindingDialog.obj MidiDialog.obj MidiHelpDialog.obj \
	PluginBindingDialog.obj PresetDialog.obj PromptDialog.obj \
	RenameDialog.obj \
	SampleDialog.obj SaveDialog.obj ScriptDialog.obj SetupDialog.obj \
	UI.obj UIConfig.obj UITypes.obj
 
$(UI_LIB) : $(UI_OBJS)
	lib -out:$(UI_LIB) @<<
	$(UI_OBJS)
<<

$(UI_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) $(INCLUDES) /c $(@:.obj=.cpp)

uilib: $(UI_LIB)

######################################################################
#
# mobius.exe
#
######################################################################

UI_EXE	= mobius.exe
UI_RC	= win/UIResources.rc
UI_RES	= UIResources.res
UI_LIBS	= $(UILIBS) $(GUILIBS) $(DLL_SYSLIBS)
UI_MAIN_OBJS = WinMain.obj

$(UI_EXE) : lib uilib $(UI_MAIN_OBJS) $(UI_RES)
	$(link) $(WIN_LFLAGS) $(UI_LIBS) $(UI_LIB) $(MOB_LIB) \
	-out:$(UI_EXE) @<<
	$(UI_MAIN_OBJS) $(UI_RES)
<<

$(UI_RES) : $(UI_RC)
	rc /fo $(UI_RES) $(UI_RC)

$(UI_MAIN_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) $(INCLUDES) /c $(@:.obj=.cpp)

mobius: $(UI_EXE)

uiclean:
	del -f $(UI_OBJS) $(UI_MAIN_OBJS)

######################################################################
#
# MobiusVst2.dll
#
# Adding a "2" suffix so we can have both the 1.45 and 2.0 versions
# installed until we're sure 2.0 works.
# 
######################################################################

# Orignally this was Mobius.dll but under VC8 this creates a 
# conflicting .pdb file, probably mobius.pdb which is also created
# for mobius.exe.  Have to give the dll a different name than the exe.
# Specifying  /Fdmobiusvst alone isn't enough.

VST_DLL	= MobiusVst2.dll
VST_OBJS = VstPlugin.obj VstMobius.obj VstMain.obj VstConfig.obj

$(VST_DLL) : $(VST_OBJS) $(UI_RES) $(UI_LIB) $(MOB_LIB)
	$(link) $(DLL_LFLAGS) $(PLUGIN_LIBS) \
	-out:$(VST_DLL) @<<
	$(VST_OBJS) $(UI_RES) $(UI_LIB) $(MOB_LIB) $(PLUGIN_LIBS)
<<

$(VST_OBJS) : $(@:.obj=.cpp)
	$(cc) $(EXE_CFLAGS) $(INCLUDES) /c $(@:.obj=.cpp)

vst: $(VST_DLL)

######################################################################
#
# exprtest.exe
#
# Tests the Expr expression evaluator, burried in Mobius.dll.
#
######################################################################

EXP_EXE		= exprtest.exe
EXP_OBJS	= exprtest.obj

$(EXP_EXE) : $(EXP_OBJS) $(MOB_LIB)
	$(link) $(EXE_LFLAGS) $(MOB_LIB) $(LIBS) -out:$(EXP_EXE) @<<
	$(EXP_OBJS)
<<

expr: $(EXP_EXE)

######################################################################
#
# Config Files
#
######################################################################

PROGRAM_FILES=c:/Program Files (x86)
OUTPUT_DIR=$(PROGRAM_FILES)/Mobius 2

# copy the initial install config files to the installation directory
initconfig:
	IF NOT EXIST "$(OUTPUT_DIR)" (mkdir "$(OUTPUT_DIR)")
	copy install/config/mobius.xml "$(PROGRAM_FILES)/Mobius 2"
	copy install/config/ui.xml "$(PROGRAM_FILES)/Mobius 2"
	copy install/config/host.xml "$(PROGRAM_FILES)/Mobius 2"

# restore the development configration
devconfig:
	IF NOT EXIST "$(OUTPUT_DIR)" (mkdir "$(OUTPUT_DIR)")
	copy config/mobius.xml "$(PROGRAM_FILES)/Mobius 2"
	copy config/ui.xml "$(PROGRAM_FILES)/Mobius 2"
	copy install/config/host.xml "$(PROGRAM_FILES)/Mobius 2"

# save the current config files as the development config
saveconfig:
	copy "$(PROGRAM_FILES)/Mobius 2/mobius.xml" config
	copy "$(PROGRAM_FILES)/Mobius 2/ui.xml" config

# install the unit test configuration
unitconfig:
	IF NOT EXIST "$(OUTPUT_DIR)" (mkdir "$(OUTPUT_DIR)")
	copy config/unitmobius.xml "$(PROGRAM_FILES)/Mobius 2/mobius.xml"
	copy config/unitui.xml "$(PROGRAM_FILES)/Mobius 2/ui.xml"
	copy install/config/host.xml "$(PROGRAM_FILES)/Mobius 2"

saveunitconfig:
	copy "$(PROGRAM_FILES)/Mobius 2/mobius.xml" config/unitmobius.xml
	copy "$(PROGRAM_FILES)/Mobius 2/ui.xml" config/unitui.xml

restoreconfig:
	copy saveui.xml "$(PROGRAM_FILES)/Mobius 2/ui.xml" 
	copy savemobius.xml "$(PROGRAM_FILES)/Mobius 2/mobius.xml" 

######################################################################
#
# Housekeeping targets
#
######################################################################

.cpp.obj:
	$(cc) $(INCLUDES) $(EXE_CFLAGS) /c $(@:.obj=.cpp)

rebuild: clean all

clean: standardclean
	@cd functions
	@$(MAKE) /nologo clean
	@cd ..

execlean:
	del -f *.exe
	del -f *.dll
	del -f *.lib

vstclean:
	del -f $(VST_DLL)
	del -f $(VST2_DLL)
	del -f $(VST_OBJS)
	del -f $(VST1_OBJS)
	del -f $(VST2_OBJS)

# Refresh the installation directory with new versions of the executables

vst: export

export:
	IF NOT EXIST "$(OUTPUT_DIR)" (mkdir "$(OUTPUT_DIR)")
	copy $(VST_DLL) "$(PROGRAM_FILES)/VstPlugins"
	copy MobiusVst2.pdb "$(PROGRAM_FILES)/VstPlugins"
	copy $(UI_EXE) "$(PROGRAM_FILES)/Mobius 2"
	copy Catalog_* "$(PROGRAM_FILES)/Mobius 2"
	copy $(VST_DLL) "$(PROGRAM_FILES)/Mobius 2"
	copy "install\config\host.xml" "$(PROGRAM_FILES)/Mobius 2"

# have to start distributing this!!
#msvc:
#	copy "$(PROGRAM_FILES)/Microsoft Visual Studio 8/VC/redist/x86/Microsoft.VC80.CRT/msvcr80.dll" c:/windows/system32
#

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
