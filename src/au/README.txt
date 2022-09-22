Sun Jan 11 23:01:56 2009

This is a copy of the AU framework from the CoreAudio SDK 1.4.3
with a simple example effect, all built using my build framework
rather than Xcode.

qwin has a simplified version of the MultiTapDelayAU example
with a Qwin view.

For reasons I don't remember this builds a "MultitapAU" example 
and leaves the artifacts in the top-level directory through the
sources come from CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.o.
I guess this saves having the bundle packaging stuff buried deep, 
but it would be nice if this could be pushed down into a subdirectory.



----------------------------------------------------------------------
Files
----------------------------------------------------------------------

View

MultitapDelayAU/MultitapAUView.cpp 
AUPublic/AUCarbonViewBase/CarbonEventHandler.cpp
AUPublic/AUCarbonViewBase/AUControlGroup.cpp
AUPublic/AUCarbonViewBase/AUCarbonViewDispatch.cpp
AUPublic/AUCarbonViewBase/AUCarbonViewControl.cpp
AUPublic/AUCarbonViewBase/AUCarbonViewBase.cpp
AUPublic/AUBase/ComponentBase.cpp
PublicUtility/CAAUParameter.cpp

Component

MultitapDelayAU/MultitapAU.cpp
AUPublic/AUBase/AUBase.cpp
AUPublic/AUBase/AUDispatch.cpp
AUPublic/AUBase/AUInputElement.cpp
AUPublic/AUBase/AUOutputElement.cpp
AUPublic/AUBase/AUScopeElement.cpp
AUPublic/AUBase/ComponentBase.cpp
AUPublic/OtherBases/AUEffectBase.cpp
AUPublic/Utility/AUBuffer.cpp
PublicUtility/CAAudioChannelLayout.cpp
PublicUtility/CAStreamBasicDescription.cpp
MultitapDelayAU/ViewComponentShim.cpp
PublicUtility/CAVectorUnit.cpp

----------------------------------------------------------------------
Compiler settings
----------------------------------------------------------------------

View Compiler

/Developer/usr/bin/gcc-4.0
-x c++				set language, same as g++?
-arch ppc 
-pipe			use pipes rather than temp files
-Wno-trigraphs 		suppress trigraph warnings?
-fpascal-strings	allow Pascal string literals, not required?
-fasm-blocks 		enable blocks of assembly code, not required?
-g			debugging
-O0			disable optimization, this is the default
-fmessage-length=0      disable wrapping of error messages
-mtune=G4        	set instruction scheduling params for machine type
			not sure what this does, and I don't think we need it
			it is listed under PowerPC options
-fvisibility-inlines-hidden    not documented?
-mfix-and-continue 	       not documented
-mmacosx-version-min=10.4      minimum version
-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap  
   precompiled header file, not using
-Wmost 
    same as -Wall -Wno-parentheses
-Wno-four-char-constants 
    unknown
-Wno-unknown-pragmas 
   disable warnings about unknown pragmas

-F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development 
   additional directory to search for frameworks, NEED???

-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include
   common generated incluce directory, don't need
-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources
   generated include directory, don't need
-isysroot /Developer/SDKs/MacOSX10.4u.sdk 
   the usual sysroot, need
-c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAUView.cpp
   file to compile
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/MultitapAUView.o
   where it goes

Component Compiler (same settings as view)

/Developer/usr/bin/gcc-4.0
-x c++
-arch ppc
-pipe
-Wno-trigraphs
-fpascal-strings
-fasm-blocks
-g
-O0
-fmessage-length=0
-mtune=G4
-fvisibility-inlines-hidden
-mfix-and-continue
-mmacosx-version-min=10.4
-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap
-Wmost
-Wno-four-char-constants
-Wno-unknown-pragmas
-F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include
-I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources
-isysroot /Developer/SDKs/MacOSX10.4u.sdk
-c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.cpp
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/MultitapAU-C8F74757.o

----------------------------------------------------------------------
Linker settings
----------------------------------------------------------------------

View Link

/Developer/usr/bin/g++-4.0 
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAUView
-L/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
  where to find libraries
-F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
   where to find frameworks
-filelist /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/MultitapAUView.LinkFileList
   alternative to command line files
-framework Carbon
-framework CoreServices
-framework AudioUnit
-framework AudioToolbox
-arch ppc

-exported_symbols_list MultitapAUView.exp
  needed, could probably avoid this with declarations like __private extern__ but
  this is more convenient if we have few symbols to export
  Can also use: -exported_symbol symbol

-prebind
  used in 10.3 and before to improve launch performance, no longer used

-Wl,-single_module
  docs say this is now the default and does not need to be specified

-compatibility_version 1
-current_version 1
  numbers used to verify that an application and a dynlib have the same version,
  if zero no check is made, not sure if this is required
  
-install_name /Library/Audio/Plug-Ins/Components/MultitapAU.component/Contents/MacOS/MultitapAUView
    Sets an internal "install path" (LC_ID_DYLIB) in a dynamic library. Any clients linked
    against the library will record that path as the way dyld should locate this library.  If
    this option is not specified, then the -o path will be used.  This option is also called
    -dylib_install_name for compatibility

    !!! what the hell is this, can't this be relative

-Wl,-Y,1455
    Used to control how many occurances of each symbol specifed with -y would be shown.  This
    option is obsolete.
-dynamiclib
    the default, implied by -dynamic, -bundle, or -execute

-mmacosx-version-min=10.4
-isysroot /Developer/SDKs/MacOSX10.4u.sdk
ld: warning -prebind ignored because MACOSX_DEPLOYMENT_TARGET environment variable greater or equal to 10.4

Component Link

/Developer/usr/bin/g++-4.0 
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS/MultitapAU 
-L/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
-F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
-filelist /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/MultitapAU.LinkFileList
-framework CoreFoundation
-framework CoreServices
-framework Carbon
-framework AudioUnit
-arch ppc
-exported_symbols_list MultitapAU.exp
-Wl,-Y,1455 
-bundle 
-mmacosx-version-min=10.4 
-isysroot /Developer/SDKs/MacOSX10.4u.sdk

----------------------------------------------------------------------
Rez
----------------------------------------------------------------------

First we compile the fork with Rez:

/Developer/Tools/Rez
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects/MultitapAU-C63E5A7E.rsrc
    output file for the resource fork

-d SystemSevenOrLater=1
  defines a macro, not sure what it means but use it
  
-useDF
    Reads and writes resource information from the files' data forks, instead of their resource forks.
-script Roman
    Enables the recognition of any of several 2-byte character script systems to use when
    compiling and decompiling files. This option insures that 2-byte characters in strings are
    handled as indivisible entities. The default language is Roman and specifies 1-byte character
    sets.-d ppc_YES
-d i386_$i386
  another define, no idea
-I / 
-I /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
  include directories
-arch ppc
-i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
-i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase
-i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility
-i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases
-i /Developer/Examples/CoreAudio/PublicUtility
-i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase
-i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUViewBase
-i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
-i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include
    include files
-isysroot /Developer/SDKs/MacOSX10.4u.sdk
  the usual
/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.r
  the input file


Next we merge resource fork and files into one file:

/Developer/Tools/ResMerger
-dstIs DF
    The fork in which to write resources in the output file.  
    The default is the data fork (DF).

/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects/MultitapAU-C63E5A7E.rsrc
    the input file
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/MultitapAU.rsrc
    Specifies the output file.  If the fork designated by -dstIs exists, it is overwritten
    unless the -a flag is provided; if it does not exist, it is created.

And again, using the shit we just built.  All I can see is that this moves it 
to another file...WTF is this doing?

/Developer/Tools/ResMerger
/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/MultitapAU.rsrc
-dstIs DF
-o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Resources/MultitapAU.rsrc


/usr/bin/touch
-c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component
   update mod date

This is a private Xcode utility used to copy things from the project to the 
final bundle:

/Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp
-exclude .DS_Store
-exclude CVS
-exclude .svn
-resolve-src-symlinks
/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAUView /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS


----------------------------------------------------------------------
Raw Xcode output for the MultitapAUView project...
----------------------------------------------------------------------

Building target “MultitapAUView” of project “MultitapAU” with configuration “Development”


Checking Dependencies
CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/MultitapAUView.o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAUView.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAUView.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/MultitapAUView.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/CarbonEventHandler.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/CarbonEventHandler.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/CarbonEventHandler.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/CarbonEventHandler.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUControlGroup.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUControlGroup.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUControlGroup.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUControlGroup.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewDispatch.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewDispatch.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewDispatch.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewDispatch.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewControl.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewControl.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewControl.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewControl.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/AUCarbonViewBase.o
xxx
CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/ComponentBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/ComponentBase.o

CompileC build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/CAAUParameter.o /Developer/Examples/CoreAudio/PublicUtility/CAAUParameter.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/MultitapAUView.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAAUParameter.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/CAAUParameter.o

Ld /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAUView normal ppc
    mkdir /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/g++-4.0 -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAUView -L/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -filelist /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAUView.build/Objects-normal/ppc/MultitapAUView.LinkFileList -framework Carbon -framework CoreServices -framework AudioUnit -framework AudioToolbox -arch ppc -exported_symbols_list MultitapAUView.exp -prebind -Wl,-single_module -compatibility_version 1 -current_version 1 -install_name /Library/Audio/Plug-Ins/Components/MultitapAU.component/Contents/MacOS/MultitapAUView -Wl,-Y,1455 -dynamiclib -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk
ld: warning -prebind ignored because MACOSX_DEPLOYMENT_TARGET environment variable greater or equal to 10.4

Building target “MultitapAU” of project “MultitapAU” with configuration “Development”


Checking Dependencies
Processing /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Info.plist Info-MultitapAU.plist
    mkdir /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    com.apple.tools.info-plist-utility Info-MultitapAU.plist -expandbuildsettings -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Info.plist

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/MultitapAU-C8F74757.o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/MultitapAU-C8F74757.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUBase.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUDispatch.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUDispatch.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUDispatch.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUDispatch.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUInputElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUInputElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUInputElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUInputElement.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUOutputElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUOutputElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUOutputElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUOutputElement.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUScopeElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUScopeElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUScopeElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUScopeElement.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/ComponentBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/ComponentBase.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUEffectBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases/AUEffectBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases/AUEffectBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUEffectBase.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUBuffer.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUBuffer.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUBuffer.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/AUBuffer.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAAudioChannelLayout.o /Developer/Examples/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAAudioChannelLayout.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAStreamBasicDescription.o /Developer/Examples/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAStreamBasicDescription.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/ViewComponentShim.o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/ViewComponentShim.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/ViewComponentShim.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/ViewComponentShim.o

CompileC build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAVectorUnit.o /Developer/Examples/CoreAudio/PublicUtility/CAVectorUnit.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/MultitapAU.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAVectorUnit.cpp -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/CAVectorUnit.o

Ld /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS/MultitapAU normal ppc
    mkdir /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/usr/bin/g++-4.0 -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS/MultitapAU -L/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -F/Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -filelist /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/Objects-normal/ppc/MultitapAU.LinkFileList -framework CoreFoundation -framework CoreServices -framework Carbon -framework AudioUnit -arch ppc -exported_symbols_list MultitapAU.exp -Wl,-Y,1455 -bundle -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk

Rez /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects/MultitapAU-C63E5A7E.rsrc MultitapAU.r
    mkdir /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/Tools/Rez -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects/MultitapAU-C63E5A7E.rsrc -d SystemSevenOrLater=1 -useDF -script Roman -d ppc_YES -d i386_$i386 -I / -I /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers -arch ppc -i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases -i /Developer/Examples/CoreAudio/PublicUtility -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUViewBase -i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development -i /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/include -isysroot /Developer/SDKs/MacOSX10.4u.sdk /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/MultitapAU.r

ResMergerCollector /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/MultitapAU.rsrc
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/Tools/ResMerger -dstIs DF /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/Objects/MultitapAU-C63E5A7E.rsrc -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/MultitapAU.rsrc

ResMergerProduct /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Resources/MultitapAU.rsrc
    mkdir /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Resources
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/Tools/ResMerger /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/MultitapAU.build/Development/MultitapAU.build/ResourceManagerResources/MultitapAU.rsrc -dstIs DF -o /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/Resources/MultitapAU.rsrc

Touch /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /usr/bin/touch -c /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component

Building target “Multitap AU + View” of project “MultitapAU” with configuration “Development”


Checking Dependencies
PBXCp build/Development/MultitapAU.component/Contents/MacOS/MultitapAUView build/Development/MultitapAUView
    cd /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU
    /Developer/Library/PrivateFrameworks/DevToolsCore.framework/Resources/pbxcp -exclude .DS_Store -exclude CVS -exclude .svn -resolve-src-symlinks /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAUView /Developer/Examples/CoreAudio/AudioUnits/MultitapDelayAU/build/Development/MultitapAU.component/Contents/MacOS

----------------------------------------------------------------------
DIAG BUILD NOTES
----------------------------------------------------------------------

Building target “AUPulseDetector” of project “DiagnosticAUs” with configuration “Development”


Checking Dependencies
Processing /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/Info.plist Info-AUPulseDetector.plist
    mkdir /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    com.apple.tools.info-plist-utility Info-AUPulseDetector.plist -expandbuildsettings -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/Info.plist

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUBase.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDispatch.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUDispatch.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUDispatch.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDispatch.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUInputElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUInputElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUInputElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUInputElement.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUOutputElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUOutputElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUOutputElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUOutputElement.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUScopeElement.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUScopeElement.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/AUScopeElement.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUScopeElement.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/ComponentBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase/ComponentBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/ComponentBase.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUBuffer.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUBuffer.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUBuffer.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUBuffer.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAStreamBasicDescription.o /Developer/Examples/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAStreamBasicDescription.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDebugDispatcher.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUDebugDispatcher.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility/AUDebugDispatcher.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDebugDispatcher.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAAudioChannelLayout.o /Developer/Examples/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAAudioChannelLayout.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAHostTimeBase.o /Developer/Examples/CoreAudio/PublicUtility/CAHostTimeBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAHostTimeBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAHostTimeBase.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAAUParameter.o /Developer/Examples/CoreAudio/PublicUtility/CAAUParameter.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAAUParameter.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAAUParameter.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CarbonEventHandler.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/CarbonEventHandler.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/CarbonEventHandler.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CarbonEventHandler.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUControlGroup.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUControlGroup.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUControlGroup.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUControlGroup.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewDispatch.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewDispatch.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewDispatch.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewDispatch.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewControl.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewControl.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewControl.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewControl.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase/AUCarbonViewBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUCarbonViewBase.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDiskStreamingCheckbox.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AUDiskStreamingCheckbox.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AUDiskStreamingCheckbox.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUDiskStreamingCheckbox.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AULoadCPU.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AULoadCPU.o
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp: In member function 'bool AULoadCPU::HandleMouseEvent(OpaqueEventRef*)':
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:342: warning: 'GetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1862)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:342: warning: 'GetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1862)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:343: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:343: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:349: warning: 'GlobalToLocal' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:3720)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:349: warning: 'GlobalToLocal' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:3720)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:362: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AULoadCPU.cpp:362: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AURenderQualityPopup.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AURenderQualityPopup.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/AURenderQualityPopup.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AURenderQualityPopup.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/GenericAUView.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/GenericAUView.o
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp: In member function 'virtual void GenericAUView::RespondToEventTimer(__EventLoopTimer*)':
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:810: warning: 'GetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1862)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:810: warning: 'GetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1862)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:811: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:811: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:818: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:818: warning: 'SetPort' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:1847)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp: In member function 'void MeterView::Draw()':
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:957: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:957: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:959: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:959: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:961: warning: 'PaintRect' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:2550)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:961: warning: 'PaintRect' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:2550)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:969: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:969: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:971: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:971: warning: 'RGBForeColor' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:4520)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:973: warning: 'PaintRect' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:2550)
/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView/GenericAUView.cpp:973: warning: 'PaintRect' is deprecated (declared at /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/ApplicationServices.framework/Frameworks/QD.framework/Headers/Quickdraw.h:2550)

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUParamInfo.o /Developer/Examples/CoreAudio/PublicUtility/AUParamInfo.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/AUParamInfo.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUParamInfo.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUPulseDetector-E9B2EC95.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/AUPulseDetector.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/AUPulseDetector.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUPulseDetector-E9B2EC95.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUPulseDetectorView.o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/AUPulseDetectorView.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/AUPulseDetectorView.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUPulseDetectorView.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUEffectBase.o /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases/AUEffectBase.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases/AUEffectBase.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUEffectBase.o

CompileC build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAVectorUnit.o /Developer/Examples/CoreAudio/PublicUtility/CAVectorUnit.cpp normal ppc c++ com.apple.compilers.gcc.4_0
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/gcc-4.0 -x c++ -arch ppc -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -g -O0 -fmessage-length=0 -mtune=G4 -fvisibility-inlines-hidden -mfix-and-continue -mmacosx-version-min=10.4 -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/AUPulseDetector.hmap -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -O0 -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -I/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/DerivedSources -isysroot /Developer/SDKs/MacOSX10.4u.sdk -c /Developer/Examples/CoreAudio/PublicUtility/CAVectorUnit.cpp -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/CAVectorUnit.o

Ld /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/MacOS/AUPulseDetector normal ppc
    mkdir /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/MacOS
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/usr/bin/g++-4.0 -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/MacOS/AUPulseDetector -L/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -F/Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -filelist /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/Objects-normal/ppc/AUPulseDetector.LinkFileList -framework CoreServices -framework CoreAudio -framework Carbon -framework AudioToolbox -framework AudioUnit -arch ppc -exported_symbols_list AUPulseDetector.exp -Wl,-Y,1455 -bundle -mmacosx-version-min=10.4 -bundle -isysroot /Developer/SDKs/MacOSX10.4u.sdk

Rez /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/Objects/AUPulseDetector-E69B675D.rsrc AUPulseDetector.r
    mkdir /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/Objects
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/Tools/Rez -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/Objects/AUPulseDetector-E69B675D.rsrc -d SystemSevenOrLater=1 -useDF -script Roman -d ppc_YES -d i386_$i386 -I / -I /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers -arch ppc -i /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUViewBase -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUBase -i /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/../CarbonGenericView -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/OtherBases -i /Developer/Examples/CoreAudio/PublicUtility -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/AUCarbonViewBase -i /Developer/Examples/CoreAudio/AudioUnits/AUPublic/Utility -i /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development -i /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/include -isysroot /Developer/SDKs/MacOSX10.4u.sdk /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/AUPulseDetector.r

ResMergerCollector /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/AUPulseDetector.rsrc
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/Tools/ResMerger -dstIs DF /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/Objects/AUPulseDetector-E69B675D.rsrc -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/AUPulseDetector.rsrc

ResMergerProduct /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/Resources/AUPulseDetector.rsrc
    mkdir /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/Resources
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /Developer/Tools/ResMerger /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/DiagnosticAUs.build/Development/AUPulseDetector.build/ResourceManagerResources/AUPulseDetector.rsrc -dstIs DF -o /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component/Contents/Resources/AUPulseDetector.rsrc

Touch /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component
    cd /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs
    /usr/bin/touch -c /Developer/Examples/CoreAudio/AudioUnits/DiagnosticAUs/build/Development/AUPulseDetector.component

