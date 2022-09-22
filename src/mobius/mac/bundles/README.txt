Tue Jan 27 00:01:01 2009

This directory contains support files for building the OS X bundles
for the Mobius application and plugins.

  app
     - support files for Mobius.app

  vst
     - support files for Mobius.vst

  component
     - support files for Mobius.component


In addition we'll also create these fleshed out bundle
directories as an artificat of the build process.  These
are not under source control.  

!! Reconsider putting these in a "build" directory
or something else that is obviously for transient stuff.

  Mobius.app
    - The bundle directory for standalone Mobius, created
      from scratch every build.

  Mobius.vst
  Mobius.component
    - The bundle directories for the VST and AU plugins.  

----------------------------------------------------------------------
Naming plugins in Bidule

The name of the /Library/Audio/Plug-Ins/Components directory doesn't matter.
CFBundleName doesn't work.

Changing the name of the executable to "Mobius 2" made it not load!
Mobius2 without spaces didn't work either.    Maybe mismatch with the
.rsrc file? Yes!

The name needs to be in AUMobius.r for Bidule to display it.
After changing AUMobius.r it shows "Mobius 2" but also "Mobius".
Changing CFBundleName to "Mobius 2" fixed the duplicate entry.
Not sure why having AUMobius.r have "Mobius" and CFBundleName have "Mobius 2"
didn't also show a duplicate entry.


