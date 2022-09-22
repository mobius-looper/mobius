Wed Feb 10 22:19:30 2010

Various files to flesh out the installation image.  Note that this is
not the product README.txt, this just describes what is in this
directory.  The README files included with the installation are under
the "text" directory.

config
  - configuration files: mobius.xml and ui.xml

loops
  - example loops

samples
  - example samples

scripts
  - example scripts

text
  - readmes, announcements, other docs


The files and some of the directories are copied into an executable
installer (NullSoft NSIS) by the the makefile the src/install
directory.

The loops, samples, and scripts directories are copied in their
entirety and left as directories.  Files inside the config and text
directories are copied into the root of the installation.

