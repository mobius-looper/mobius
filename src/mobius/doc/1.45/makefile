#
# Makefile for the documentation
#

DOC2HTML = ..\..\..\util\Doc2Html.exe

default: inst start explore tech sync ref edp script

#
# Installation
#

inst: installation.htm

installation.htm: installation.xml
	$(DOC2HTML) $*.xml $*.htm

#
# Gettign Started
#

start: starting.htm

starting.htm: starting.xml
	$(DOC2HTML) $*.xml $*.htm
#
# Exploring
#

explore: exploring.htm

exploring.htm: exploring.xml
	$(DOC2HTML) $*.xml $*.htm

#
# Synchronization
#

sync: synchronization.htm

synchronization.htm: synchronization.xml
	$(DOC2HTML) $*.xml $*.htm

#
# Techniques
#

tech: techniques.htm

techniques.htm: techniques.xml
	$(DOC2HTML) $*.xml $*.htm

#
# Reference
#

ref: reference.htm

reference.htm: reference.xml
	$(DOC2HTML) $*.xml $*.htm

#
# EDP
#

edp: edp.htm

edp.htm: edp.xml
	$(DOC2HTML) $*.xml $*.htm

#
# Scripting
#

script: scripts.htm

scripts.htm: scripts.xml
	$(DOC2HTML) $*.xml $*.htm

# doesn't work for some reason...
.xml.htm:
	$(DOC2HTML) $*.xml $*.htm

clean:
	@-rm *.htm

#
# Create a directory containing only the html and image files so we
# can ftp them all in one shot
#

publish:
	-del -rf docs
	mkdir docs
	mkdir docs/images
	copy *.htm docs
	copy images/*.gif docs/images
