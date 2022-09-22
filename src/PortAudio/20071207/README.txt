Tue Jan 06 15:35:37 2009

This is PortAudio v19 20071207 (December 7, 2007)

pa_stable_v19_20071207.tar.gz has the standard download, this
decompressses to the "portaudio" directory which is not under source
control.

For Mac we build using the standard instructions and leave just the
necessary artifacts behind in the "mac" directory.

For Windows we have historically used a custom makefile rather than
the provided VStudio solution to get batch builds and more control
over the compiler options.  In particular we make a static library
rather than a DLL.  The sources have also been moved to a common
directory to simplify the build and make it easier to debug.

The win diretcory contains files copied from these locations:

   portaudio/include/pa_asio.h
   portaudio/include/pa_win_ds.h
   portaudio/include/pa_win_waveformat.h
   portaudio/include/pa_win_mme.h
   portaudio/include/portaudio.h

   portaudio/src/common/*
   portaudio/src/os/win/*

   portaudio/src/hostapi/asio/pa_asio.cpp
     (but not iasiothiscallresolver.cpp)
   
   portaudio/src/hostapi/wmme/*
   portaudio/src/hostapi/dsound/*
     (consider wdmks someday)

Compiling dsound interfaces required Dsound.h which seems to have
changed locations in VC8.  

pa_ringbuffer.c uses #warning which is not supported by MSVC, had
to comment these lines out.

pa_win_wdmks_utils.obj required <ksmedia.h> which is proably
in the newer PlatformSdk.  

----------------------------------------------------------------------
Mac Notes
----------------------------------------------------------------------

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

Summary:

  To build PortAudio, simply use the traditional Unix "./configure &&
  make" style build to compile. You do not need to do "make install",
  and we don't recommend it; however, you may be using software that
  instructs you to do so, in which case, obviously, you should follow
  those instructions. There is no X Code project for PortAudio.

     $ ./configure && make

By default it compiles universal binaries.  This can be disabled with configure
options or with "lipo" but it is best not to.

Files are compiled with -g but the build notes claim that debugging is off by default:

  By default, PortAudio on the mac is built without any debugging
  options. This is because asserts are generally inappropriate for a
  production environment and debugging information has been suspected,
  though not proven, to cause trouble with some interfaces. If you would
  like to compile with debugging, you must run configure with the
  appropriate flags. For example:

     $ ./configure --enable-mac-debug && make


The library is libportaudio.a, to link with it you also need these frameworks:

  CoreAudio.framework
  AudioToolbox.framework
  AudioUnit.framework
  CoreServices.framework

Other build notes:

  For gcc/Make style projects, include "include/portaudio.h" and link
  "libportaudio.a", and use the frameworks listed in the previous
  section. How you do so depends on your build.

  For additional, Mac-only extensions to the PortAudio interface, you
  may also want to grab "include/pa_mac_core.h". This file contains some
  special, mac-only features relating to sample-rate conversion, channel
  mapping, performance and device hogging. See
  "src/hostapi/coreaudio/notes.txt" for more details on these features.

----------------------------------------------------------------------
