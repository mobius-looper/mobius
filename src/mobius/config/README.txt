Mon Feb 08 00:38:41 2010

These are the Mobius configuration files used during development.
Note that these are *not* the standard config files used in the
installation, those are kept under install/config.

There are three sets of these:

    mobius.xml
    ui.xml
        - used for normal Windows development, these are not stable

    macmobius.xml
    macui.xml
        - used for normal Mac development, not stable

    unitmobius.xml
    unitui.xml
        - used for Windows unit tests, these are stable meaning they
          will have what is expected to run the unit tests.  Tests will
          initialize their own preset but these files have MIDI and audio
          devices selected, and the test script directory configured

    macunitmobius.xml
    maxunitui.xml
        - used for Mac unit tests, these are stable

The Windows and Mac files have audio and MIDI devices selected
from my devlopment environment.

The unit tests configs should be close to the base install but
with devices, buttons, bindings, and samples used for testing.
You can change the devices and the test script location if necessary but
you should leave the suggest latency values at 5.  The scripts require
specific values for inputLatencyFrames and outputLatencyFrames to match
what was in use when the files were captured.  The tests should set
these but it's a good idea to have the selected audio device be close
to the same.   It may not matter, but I'm not sure what the side effects
would be if Mobius thought it was using an ASIO device with a 256 block
size but you were actually using an MME device with a large block size.

The development configs are more random and may contain UI and binding
configs quite different from the base install.

demo1.mob and demo1-1-1.wav is an example project, I'm not sure if 
I used these in any tests and we don't include them in the install any more.

There are makefile targets to maintain these files

    initconfig
      - copy the base install config files to the system directory
        this is c:\Program Files\Mobius on Windows or
         /Library/Application Support/Mobius on Mac

    devconfig
      - copy the development config files to the system directory
      
    saveconfig
      - save the system config files to the development direcotry

    unitconfig
      - copy the files from the config directory to the system directory

    saveunitconfig
      - copy the config files from the system directory to the config directory


    
