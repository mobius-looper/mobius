/* Copyright (c) 2009 Jeffrey S. Larson.  All rights reserved. */
/*
 * An interface providing basic OSC connectivity.
 * Currently built around Ross Bencina's oscpack, adding a thread 
 * and message abstraction.
 *
 */

#include <stdexcept>
#include <iostream>

#include "oscpack/osc/OscPacketListener.h"
#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/ip/UdpSocket.h"
#include "oscpack/ip/PacketListener.h"

#include "port.h"
#include "util.h"
#include "Trace.h"
#include "Thread.h"
#include "OscInterface.h"

//////////////////////////////////////////////////////////////////////
//
// OscMessage
//
//////////////////////////////////////////////////////////////////////

OscMessage::OscMessage()
{
	mNext = NULL;
	mNumArgs = 0;
	strcpy(mAddress, "");
}

OscMessage::~OscMessage()
{
}

/**
 * This is what apps should call in case we want to pool them.
 */
PUBLIC void OscMessage::free()
{
	delete this;
}

PUBLIC const char* OscMessage::getAddress()
{
	return mAddress;
}

/**
 * Assume the caller has already checked length overflow and raised an error.
 * If not truncate.
 */
PUBLIC void OscMessage::setAddress(const char* s)
{
	CopyString(s, mAddress, sizeof(mAddress));
}

PUBLIC int OscMessage::getNumArgs()
{
	return mNumArgs;
}

PUBLIC void OscMessage::setNumArgs(int i)
{
	mNumArgs = i;
}

PUBLIC float OscMessage::getArg(int i)
{
	float f = 0.0f;
	if (i >= 0 && i < OSC_MAX_ARGS)
	  f = mArgs[i];
	return f;
}

PUBLIC void OscMessage::setArg(int i, float f)
{
	if (i >= 0 && i < OSC_MAX_ARGS)
	  mArgs[i] = f;

    // it is common to forget this, assume caller is setting these
    // without gaps
    int nargs = i + 1;
    if (mNumArgs < nargs)
      mNumArgs = nargs;
}

//////////////////////////////////////////////////////////////////////
//
// OscThread
//
//////////////////////////////////////////////////////////////////////

/**
 * A thread to listen for incomming OSC messages.
 */
class OscThread : public Thread, public osc::OscPacketListener
{
  public:
	OscThread(int port, OscListener* l);
	~OscThread();

	void run();
	void stop();

	void ProcessMessage(const osc::ReceivedMessage& m, 
						const IpEndpointName& remoteEndpoint);

  private:

	int mPort;
	OscListener* mListener;
    UdpListeningReceiveSocket* mSocket;

};

OscThread::OscThread(int port, OscListener* l)
{
    setName("OSC");
	mPort = port;
	mListener = l;
}

OscThread::~OscThread()
{
	if (mSocket != NULL) {
		// must have been shut down cleanly by now, deleting will
		// most likely cause an exception
		Trace(1, "ERROR: OscThread socket still open in destructor\n");

		// I guess we could try to stop here, but the app should be doing this
		stopAndWait();
	}
}

/**
 * Eventually called by the Thread::start method.
 * Thread::isRunning returns true until this method exits.
 */
PUBLIC void OscThread::run()
{
	Trace(2, "OscThread::run starting\n");

	try {
		// setup a UDP socket with this thread as the listener
		mSocket = new UdpListeningReceiveSocket(IpEndpointName(IpEndpointName::ANY_ADDRESS, mPort),
												this);

		// run until break
		mSocket->Run();

		mSocket = NULL;
	}
	catch (std::runtime_error& e) {
		Trace(1, "ERROR: OscThread::run runtime_error thrown by oscpack\n");
        // avoid a warning
        e = e;
	}

	Trace(2, "OscThread::run stopped\n");
}
	
/**
 * Called by another thread to async break this one.
 */
PUBLIC void OscThread::stop()
{
	if (mSocket != NULL) {
		Trace(2, "OscThread::stop requesting asynchronous break\n");
		mSocket->AsynchronousBreak();
	}
}

/**
 * The thread is a oscpack OscPacketListener.
 * Bundles have already been traversed.
 * Some fundamental exceptions may have been thrown by now, not sure
 * where they end up.
 */
void OscThread::ProcessMessage(const osc::ReceivedMessage& msg, 
							   const IpEndpointName& remoteEndpoint )
{
	if (mListener != NULL) {
		try {
			const char* address = msg.AddressPattern();
			if (address == NULL) {
				// assume oscpack would have handled this
				Trace(1, "OscThread: missing address\n");
			}
			else if (strlen(address) >= OSC_MAX_STRING) {
				Trace(1, "OscThread: address too long %s\n", address);
			}
			else {
				long nargs = msg.ArgumentCount();
				if (nargs > OSC_MAX_ARGS) {
					Trace(1, "OscThread: too many arguments %ld\n", nargs);
				}
				else {
					// convert it to our message
					// !! pool these or require that ownership not transfer?
					OscMessage* m = new OscMessage();
					m->setAddress(address);
					m->setNumArgs((int)nargs);

					// example #2 -- argument iterator interface, supports
					// reflection for overloaded messages (eg you can call 
					// (*arg)->IsBool() to check if a bool was passed etc).
					osc::ReceivedMessage::const_iterator arg = msg.ArgumentsBegin();

					for (int i = 0 ; i < nargs ;i++) {
						float fval = 0.0f;

						if (arg->IsFloat()) {
							fval = arg->AsFloat();
						}
						else if (arg->IsString()) {
							// need to support these eventually!!
							Trace(1, "OscThread: string argument encountered\n");
						}
						else {
							// I guess bool can be 0/1 and int could be scaled
							// from 0-127 to 0.0-1.0
							Trace(1, "OscThread: unsupported argument encountered\n");
						}
						
						m->setArg(i, fval);
						arg++;
					}

					mListener->oscMessage(m);
				}
			}
		}
		catch (osc::Exception& e) {
            // any parsing errors such as unexpected argument types, or 
            // missing arguments get thrown as exceptions.
			Trace(1, "ERROR: OscThread::ProcessPacket exception during packet decoding\n");
			/*
            std::cout << "error while parsing message: "
					  << m.AddressPattern() << ": " << e.what() << "\n";
			*/
            // avoid a warning
            e = e;
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// OscpackDevice
//
//////////////////////////////////////////////////////////////////////

/**
 * An implementation of OscDevice that uses oscpack.
 */
class OscpackDevice : public OscDevice
{
  public:

    OscpackDevice(const char* host, int port, UdpTransmitSocket* socket);
    ~OscpackDevice();

    OscpackDevice* getNext();
    void setNext(OscpackDevice* d);

    UdpTransmitSocket* getSocket();

    // these are required by the OscDevice interface
    const char* getHost();
    int getPort();

  private:

    OscpackDevice* mNext;
    char* mHost;
    int mPort;
    UdpTransmitSocket* mSocket;

};

PUBLIC OscpackDevice::OscpackDevice(const char* host, int port,
                                    UdpTransmitSocket* socket)
{
    mNext = NULL;
    mHost = CopyString(host);
    mPort = port;
    mSocket = socket;
}

PUBLIC OscpackDevice::~OscpackDevice()
{
	OscpackDevice *el, *next;

    delete mHost;
    delete mSocket;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC OscpackDevice* OscpackDevice::getNext()
{
    return mNext;
}

PUBLIC void OscpackDevice::setNext(OscpackDevice* d)
{
    mNext = d;
}

PUBLIC const char* OscpackDevice::getHost()
{
    return mHost;
}

PUBLIC int OscpackDevice::getPort()
{
    return mPort;
}

PUBLIC UdpTransmitSocket* OscpackDevice::getSocket()
{
    return mSocket;
}

//////////////////////////////////////////////////////////////////////
//
// OscpackInterface
//
//////////////////////////////////////////////////////////////////////

/**
 * An implementation of OscInterface that uses oscpack.
 */
class OscpackInterface : public OscInterface 
{
  public:

	OscpackInterface();
	~OscpackInterface();

	void setReceivePort(int p);
	void setListener(OscListener* l);
	void start();
	void stop();

    OscDevice* registerDevice(const char* host, int port);
    void send(OscDevice* dev, OscMessage* m);
	void send(const char* host, int port, OscMessage* m);

  private:
   
    UdpTransmitSocket* openSocket(const char* host, int port);
    void send(UdpTransmitSocket* socket, OscMessage* msg);

	OscThread* mThread;
	OscListener* mListener;
    OscpackDevice* mDevices;
	int mInPort;
    bool mTrace;

};

OscpackInterface::OscpackInterface()
{
	mThread = NULL;
	mListener = NULL;
    mDevices = NULL;
	mInPort = 0;

    mTrace = false;
}

OscpackInterface::~OscpackInterface()
{
	stop();
    delete mDevices;
}

//
// Receiving
//

void OscpackInterface::setReceivePort(int p)
{
	if (p != mInPort) {
		if (mThread == NULL)
		  mInPort = p;
		else {
			// if the thread was already started, restart it
			stop();
			mInPort = p;
			start();
		}
	}
}

/**
 * You must call this before starting.
 * You can change the port later but not the listener.
 */
void OscpackInterface::setListener(OscListener* l)
{
	mListener = l;
}

void OscpackInterface::start()
{
	if (mThread == NULL) {
		if (mInPort <= 0) {
			Trace(1, "ERROR: OscpackInterface::start invalid input port\n");
		}
		else if (mListener == NULL) {
			Trace(1, "ERROR: OscpackInterface::start no listener\n");
		}
		else {
			mThread = new OscThread(mInPort, mListener);
			mThread->start();
			// start may fail, it will have traced
			if (!mThread->isRunning()) {
				delete mThread;
				mThread = NULL;	
			}
		}
	}
}

/**
 * We'll wait up to a second for the thread to stop.
 * If it doesn't just ignore it and hope it will stop eventually.
 * Do *not* delete it in case it wakes up.
 */
void OscpackInterface::stop()
{
	if (mThread != NULL) {
		mThread->stopAndWait();
		if (!mThread->isRunning())
		  delete mThread;
		mThread = NULL;
	}
}

//
// Sending
//

/**
 * Open a UDP socket for a given host/port.
 * Send errors to the trace log.
 */
PRIVATE UdpTransmitSocket* OscpackInterface::openSocket(const char* host, int port)
{
    UdpTransmitSocket* socket = NULL;

    if (host == NULL) {
        Trace(1, "OscInterface: invalid output host\n");
    }
    else if (port <= 0) {
        Trace(1, "OscInterface: invalid output port\n");
    }
    else {
        try {
            socket = new UdpTransmitSocket(IpEndpointName(host, port));
        }
        catch (std::runtime_error& e) {
            Trace(1, "ERROR: OscInterface:openSocket runtime_error thrown by oscpack\n");
            // avoid a warning
            e = e;
        }
    }

    return socket;
}

/**
 * Register an OSC device.  If the host/port are invalid NULL is returned.
 * The caller may assume that these live as long as the OscInterface
 * and may be passed to send().
 *
 * It is permissible for the host/port to be already registered.
 */
PUBLIC OscDevice* OscpackInterface::registerDevice(const char* host, int port)
{
    OscDevice* device = NULL;

    // make sure there are no duplicates
    for (OscpackDevice* d = mDevices ; d != NULL ; d = d->getNext()) {
        if (StringEqual(d->getHost(), host) && 
            d->getPort() == port) {
            device = d;
            break;
        }
    }

    if (device == NULL) {
        UdpTransmitSocket* socket = openSocket(host, port);
        if (socket != NULL) {
            OscpackDevice* d = new OscpackDevice(host, port, socket);
            d->setNext(mDevices);
            mDevices = d;
            device = d;
        }
    }

    return device;
}

PUBLIC void OscpackInterface::send(OscDevice* dev, OscMessage* msg)
{
	if (dev != NULL && msg != NULL) {

        OscpackDevice* packdev = (OscpackDevice*)dev;
        send(packdev->getSocket(), msg);

		// should we be allowed to take ownership?
        // NO! common for the caller to have stack allocated this
		//msg->free();
	}
}


PUBLIC void OscpackInterface::send(const char* host, int port, OscMessage* msg)
{
	if (msg != NULL) {

        // TODO: auto-register these?
        UdpTransmitSocket* socket = openSocket(host, port);

        send(socket, msg);

        delete socket;

		// should we be allowed to take ownership?
		msg->free();
	}
}

PRIVATE void OscpackInterface::send(UdpTransmitSocket* socket, OscMessage* msg)
{
	if (socket != NULL && msg != NULL) {

        char buffer[OSC_MAX_OUTPUT];
        osc::OutboundPacketStream p(buffer, OSC_MAX_OUTPUT);
    
        // god I fucking hate C++
        p << osc::BeginMessage(msg->getAddress());
        for (int i = 0 ; i < msg->getNumArgs() ; i++) {
            p << msg->getArg(i);
        }
        p << osc::EndMessage;


        if (mTrace) {
            printf("OSC sending: %s %f\n", msg->getAddress(), msg->getArg(0));
            fflush(stdout);
        }

        socket->Send(p.Data(), p.Size());
	}
}

//////////////////////////////////////////////////////////////////////
//
// Factory
//
//////////////////////////////////////////////////////////////////////

OscInterface* OscInterface::getInterface()
{
	return new OscpackInterface();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
