#
# Windows makefile for the Qwin library
#
 
######################################################################
#
# Targets
#
######################################################################

all: qwin qwintest wintest colors
	
!include ..\make\common.mak

INCLUDES = /I../util
LIBS = /LIBPATH:../util util.lib

######################################################################
#
# qwin.lib
#
# The core UI library.
#
######################################################################

QWIN_OBJS 	= KeyCode.obj \
		Context.obj qwin.obj Character.obj Color.obj Graphics.obj Border.obj \
		Component.obj Container.obj Strut.obj Custom.obj \
		LayoutManager.obj Panel.obj Timer.obj Font.obj \
		Listeners.obj MouseEvent.obj KeyEvent.obj \
		Static.obj Label.obj Text.obj TextArea.obj GroupBox.obj \
		ListBox.obj ComboBox.obj TabbedPane.obj \
		Button.obj RadioButton.obj Checkbox.obj ScrollBar.obj \
		Radios.obj ToolBar.obj StatusBar.obj Slider.obj \
		Menu.obj Window.obj Frame.obj Dialog.obj \
		SimpleDialog.obj FormPanel.obj NumberField.obj Divider.obj \
		LabeledText.obj Table.obj Tree.obj OpenDialog.obj MessageDialog.obj \
		Dragable.obj ColorDialog.obj MultiSelect.obj Palette.obj \
		FontConfig.obj \
		UIManager.obj UIWindows.obj
		
QWIN_NAME	= qwin
QWIN_LIB	= $(QWIN_NAME).lib
QWIN_DLL	= $(QWIN_NAME).dll
QWIN_RES	= 
QWIN_CFLAGS	= $(INCLUDES) $(DLL_CFLAGS)
QWIN_LIBS	= $(DLL_SYSLIBS)

$(QWIN_DLL) : $(QWIN_OBJS) $(QWIN_RES)
	$(link) $(DLL_LFLAGS) $(QWIN_LIBS) $(QWIN_BASEOPT) \
	-out:$(QWIN_DLL) @<<
	$(QWIN_OBJS) $(QWIN_RES)
<<
	cp $(QWIN_LIB) $(LIBDIR)

$(QWIN_LIB) : $(QWIN_OBJS)
	lib -out:$(QWIN_LIB) @<<
	$(QWIN_OBJS)
<<

$(QWIN_OBJS) : $(@:.obj=.cpp)
	$(cc) $(QWIN_CFLAGS) /c $(@:.obj=.cpp)

qwin: $(QWIN_LIB)

######################################################################
#
# qwintest.exe
#
# Simple test driver for the QWIN system.
#
# !! some artifact of the link options we're using causes us
# to generate a .lib file too, doesn't seem to hurt but would like
# to know why.
#
######################################################################

TEST_NAME	= qwintest
TEST_EXE	= $(TEST_NAME).exe
TEST_OBJS	= $(TEST_NAME).obj
TEST_RC		= $(TEST_NAME).rc
TEST_LIBS	= $(LIBS) $(QWIN_LIB) $(DLL_SYSLIBS)
TEST_CFLAGS	= $(INCLUDES) $(EXE_CFLAGS)

$(TEST_EXE) : $(TEST_OBJS) $(QWIN_LIB) $(TEST_NAME).res
	$(link) $(WIN_LFLAGS) $(TEST_LIBS) \
	-out:$(TEST_EXE) @<<
	$(TEST_OBJS) $(TEST_NAME).res
<<

$(TEST_OBJS) : $(@:.obj=.cpp)
	$(cc) $(TEST_CFLAGS) /c $(@:.obj=.cpp)

qwintest: $(TEST_EXE)

######################################################################
#
# wintest.exe
#
# Another test driver for the QWIN system with only Windows specific
# tests.  This one uses resource files for some things, an interesting
# example but not something we would use any more.
#
######################################################################

TEST2_NAME	= wintest
TEST2_EXE	= $(TEST2_NAME).exe
TEST2_OBJS	= $(TEST2_NAME).obj
TEST2_LIBS	= $(LIBS) $(QWIN_LIB) $(DLL_SYSLIBS)
TEST2_RC	= $(TEST2_NAME).rc
TEST2_CFLAGS	= $(INCLUDES) $(EXE_CFLAGS)

$(TEST2_EXE) : $(TEST2_OBJS) $(QWIN_LIB) $(TEST2_NAME).res
	$(link) $(WIN_LFLAGS) $(TEST2_LIBS) \
	-out:$(TEST2_EXE) @<<
	$(TEST2_OBJS) $(TEST2_NAME).res
<<

$(TEST2_OBJS) : $(@:.obj=.cpp)
	$(cc) $(TEST2_CFLAGS) /c $(@:.obj=.cpp)

wintest: $(TEST2_EXE)

######################################################################
#
# colors.exe
#
# An old test application showing a color chooser dialog.
#
######################################################################

COLORS_NAME	= colors
COLORS_EXE	= $(COLORS_NAME).exe
COLORS_OBJS	= $(COLORS_NAME).obj
COLORS_LIBS	= $(LIBS) $(QWIN_LIB) $(DLL_SYSLIBS)

$(COLORS_EXE) : $(COLORS_OBJS) $(QWIN_LIB)
	$(link) $(WIN_LFLAGS) $(COLORS_LIBS) \
	-out:$(COLORS_EXE) @<<
	$(COLORS_OBJS)
<<

$(COLORS_OBJS) : $(@:.obj=.cpp)
	$(cc) $(TEST_CFLAGS) /c $(@:.obj=.cpp)

colors: $(COLORS_EXE)

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
