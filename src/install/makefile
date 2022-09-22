#
# Makefile for the Mobius installer, based on the NullSoft installer
# An installation of NSIS is required
#
# Before making an intaller, change the VERSION definition
# in this file and the version constant used in AboutDialog.cpp
#

 
######################################################################
#
# Targets
#
######################################################################

all: install
	
!include ../make/common.mak
	 
# 32 bit
#NSIS = "c:\Program Files\NSIS\makensis.exe"
# 64 bit
NSIS = "c:\Program Files (x86)\NSIS\makensis.exe"


MAJOR=2
MINOR=5

######################################################################
#
# setup.exe
#
######################################################################

SCRIPTS = mobius.nsi
#SCRIPTS = test.nsi

mobius-$(MAJOR)-$(MINOR).exe : $(SCRIPTS)
	  $(NSIS) /V4 $(SCRIPTS)
	  mv setup.exe mobius-$(MAJOR)-$(MINOR).exe

install: mobius-$(MAJOR)-$(MINOR).exe

######################################################################
# Emacs stuff
#;;; Local Variables:
#;;; mode: text
#;;; indent-tabs-mode: t
#;;; tab-width: 8
#;;; End:
