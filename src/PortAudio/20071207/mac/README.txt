Sun Sep 07 20:32:46 2008

This is PortAudio 19 posted December 7, 2007.

It is used in MacMobius.  It was build from source with
just the necessary header and library files left behind.
The full sources are in release.tar.gz if you need to rebuild it.

See this page for information on Darwin building:

    http://portaudio.com/trac/wiki/TutorialDir/Compile/MacintoshCoreAudio

I used the default build process:

    $ ./configure && make

By default this builds universal binaries.  If you want debugging
symbols you have to ask for them:

    $ ./configure --enable-mac-debug && make

Building PA on OSX was easy, but I had problems linking with the static 
library it leaves in lib/.libs/libportaudio.a.  If you try to link this
using the usual convention of -L.../lib/.libs -lportaudio it links but 
when the app is run it complains about not being able to laod dynamic
libraaries:

  dyld: Library not loaded: /usr/local/lib/libportaudio.2.dylib
    Referenced from: /Users/jeff/dev/mobius/src/audio/./audiotest
 
So it seems to be linking to one of the dynamic libraries instead of
libportaudio.a.  I copied libportaudio.a to the local direcotry and it works.

The first one I built was:

  -rw-r--r--    1 jeff  jeff   583148 Aug 21 23:50 libportaudio.a

I wasn't sure if this was universal so I tried it again and got:

  -rw-r--r--    1 jeff  jeff  583404 Oct  1 00:00 libportaudio.a

Pretty close but not exact, hmm.....

