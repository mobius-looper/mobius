Sun Feb 07 12:01:01 2010

An interface providing basic OSC connectivity.
Currently built around Ross Bencina's oscpack, adding a thread 
and message abstraction.

The oscpack subdirectory has Ross' distribution.  I don't think
I made any changes to the source code, but I added my own makefiles
for Windows and Mac.  

----------------------------------------------------------------------
Build Notes
----------------------------------------------------------------------

My usual compiler options are:

gcc+ -g -c -DOSX -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4    vbuf.cpp -o vbuf.o

Ross uses:

g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o tests/OscUnitTests.o tests/OscUnitTests.cpp

Difference:

Ross doesn't use -g (debugging) and uses -Wall (warnings) and -O3 (optimization)

I use -arch for universal binaries, -mmacosx-version-min for version checking,
my own OSX declaration, and -isysroot for some OSX specific shit, may
not be necessary here?


g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o osc/OscOutboundPacketStream.o osc/OscOutboundPacketStream.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o osc/OscTypes.o osc/OscTypes.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o osc/OscReceivedElements.o osc/OscReceivedElements.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o osc/OscPrintReceivedElements.o osc/OscPrintReceivedElements.cpp
g++ -o bin/OscUnitTests tests/OscUnitTests.o osc/OscOutboundPacketStream.o osc/OscTypes.o osc/OscReceivedElements.o osc/OscPrintReceivedElements.o  
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o tests/OscSendTests.o tests/OscSendTests.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o ip/posix/NetworkingUtils.o ip/posix/NetworkingUtils.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o ip/posix/UdpSocket.o ip/posix/UdpSocket.cpp
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o ip/IpEndpointName.o ip/IpEndpointName.cpp
g++ -o bin/OscSendTests tests/OscSendTests.o osc/OscOutboundPacketStream.o osc/OscTypes.o ip/posix/NetworkingUtils.o ip/posix/UdpSocket.o ip/IpEndpointName.o  
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o tests/OscReceiveTest.o tests/OscReceiveTest.cpp
g++ -o bin/OscReceiveTest tests/OscReceiveTest.o osc/OscTypes.o osc/OscReceivedElements.o osc/OscPrintReceivedElements.o ip/posix/NetworkingUtils.o ip/posix/UdpSocket.o  
g++ -Wall -O3 -I./ -DOSC_HOST_LITTLE_ENDIAN   -c -o examples/OscDump.o examples/OscDump.cpp
g++ -o bin/OscDump examples/OscDump.o osc/OscTypes.o osc/OscReceivedElements.o osc/OscPrintReceivedElements.o ip/posix/NetworkingUtils.o ip/posix/UdpSocket.o  
