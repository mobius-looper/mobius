Sun Jan  4 22:05:49 2009

The 2.3 and 2.4 directories are simplifications of the standard
directory structures with my makefiles.  They both build a static
library that will be linked into Mobius.

2.3 has only been used on Windows.  

2.4 was used during Mac porting and then by Windows.

2.3 includes vstgui which was never used by Mobius.  It also has an
examples subdirectory containing some of the original examples, I'm
not sure how much reorganization this had.  There is a makefile here
for Windows.

2.4 does not include vstgui but it does have the "again" example and
build notes for Mac.  


2.4 has been used long enough that we don't need to be dragging
2.3 around any more, but it does have the vstgui sources which
I occasionally look at.
