Wed Oct 01 20:49:48 2008

This is SoundTouch 1.3.1 with a reorganized directory.
I merged the source/SoundTouch directory with the include directory
to make it simpler.  The nmake file builds a static library.

soundtouch-1.3.1.zip has the original distribution.  If you diff these
you may notice changes to the CVS checkin ids, this is because we both
use CVS and each time this is modified it rolls the version and
modifier.

CHANGES

- SoundTouch.cpp: added SoundTouch::reset, used when changing rates
  
  This was done in version 1.3.0, it may not be necessary in 1.3.1?

- Changed "BOOL" to just "bool" to avoid a conflict
  with windows.h that resulted in signature differences
  between the referencing application and ST code.  Probably
  a way to avoid this, but it was easier.

  This I didn't retrofit when I upgraded to 1.3.1 !

- Compiling FIRFilter under MSSVC8 gave this error

  FIRFilter.cpp(180) : error C2668: 'pow' : ambiguous call to overloaded function
          x:\msvc8\VC\include\math.h(575): could be 'long double pow(long double,int)'
          x:\msvc8\VC\include\math.h(573): or 'long double pow(long double,long double)'
          x:\msvc8\VC\include\math.h(527): or 'float pow(float,int)'
          x:\msvc8\VC\include\math.h(525): or 'float pow(float,float)'
          x:\msvc8\VC\include\math.h(489): or 'double pow(double,int)'
          x:\msvc8\VC\include\math.h(123): or 'double pow(double,double)'
          while trying to match the argument list '(int, uint)'

  The result was going to a float so I cast both args to float.



  
----------------------------------------------------------------------
Mac Notes
----------------------------------------------------------------------

If you build this on PPC the x86 optimizations will be disabled.
I guess if you want a universal binary with x86 optimizations it has
to be built on an x86.

From the README.html file:

2. Building in Gnu platforms

The SoundTouch library can be compiled in practically any platform
supporting GNU compiler (GCC) tools. SoundTouch have been tested with
gcc version 3.3.4., but it shouldn't be very specific about the gcc
version. Assembler-level performance optimizations for GNU platform
are currently available in x86 platforms only, they are automatically
disabled and replaced with standard C routines in other processor
platforms.

To build and install the binaries, run the following commands in
SoundTouch/ directory:

./configure
  Configures the SoundTouch package for the local environment.

make
  Builds the SoundTouch library & SoundStretch utility.

make install
  Installs the SoundTouch & BPM libraries to /usr/local/lib and
  SoundStretch utility to /usr/local/bin. Please notice that 'root'
  privileges may be required to install the binaries to the destination
  locations.

NOTE: At the time of release the SoundTouch package has been tested to
compile in GNU/Linux platform. However, in past it's happened that new
gcc versions aren't necessarily compatible with the assembler
setttings used in the optimized routines. If you have problems getting
the SoundTouch library compiled, try the workaround of disabling the
optimizations by editing the file "include/STTypes.h" and removing the
following definition there:

  #define ALLOW_OPTIMIZATIONS 1

--------------------

Running ./configure checked a bunch of stuff and generated these files:

configure: creating ./config.status
config.status: creating Makefile
config.status: creating source/Makefile
config.status: creating source/SoundTouch/Makefile
config.status: creating source/example/Makefile
config.status: creating source/example/SoundStretch/Makefile
config.status: creating source/example/bpm/Makefile
config.status: creating include/Makefile
config.status: creating soundtouch-1.0.pc
config.status: creating include/soundtouch_config.h
config.status: executing depfiles commands

soundtouch_config.h is the important one!

The generated Makefile is godawful though, so it scares me to 
try to write my own!?

Doing the standard make leaves a static library here:

   /soundtouch-1.3.1/source/SoundTouch/.libs/libSoundTouch.a

The g++ command line is:

 g++ -DHAVE_CONFIG_H -I. -I. -I../../include -I../../include -O3 -msse -fcheck-new -I../../include -MT AAFilter.lo -MD -MP -MF .deps/AAFilter.Tpo -c AAFilter.cpp -o AAFilter.o

It is NOT building a universal binary...

When building with my usual makefiles I get...

g++ -g -c -DOSX -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386  -DFLOAT_SAMPLES=1  FIRFilter.cpp -o FIRFilter.o
FIRFilter.cpp: In static member function 'static void* soundtouch::FIRFilter::operator new(size_t)':
FIRFilter.cpp:223: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
FIRFilter.cpp: In static member function 'static void* soundtouch::FIRFilter::operator new(size_t)':
FIRFilter.cpp:223: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
g++ -g -c -DOSX -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386  -DFLOAT_SAMPLES=1  RateTransposer.cpp -o RateTransposer.o
RateTransposer.cpp: In static member function 'static void* soundtouch::RateTransposer::operator new(size_t)':
RateTransposer.cpp:118: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
RateTransposer.cpp: In static member function 'static void* soundtouch::RateTransposer::operator new(size_t)':
RateTransposer.cpp:118: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
g++ -g -c -DOSX -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386  -DFLOAT_SAMPLES=1  TDStretch.cpp -o TDStretch.o
TDStretch.cpp: In static member function 'static void* soundtouch::TDStretch::operator new(size_t)':
TDStretch.cpp:666: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)
TDStretch.cpp: In static member function 'static void* soundtouch::TDStretch::operator new(size_t)':
TDStretch.cpp:666: warning: 'operator new' must not return NULL unless it is declared 'throw()' (or -fcheck-new is in effect)

I think these are harmless....?
