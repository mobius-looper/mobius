/* Copyright (c) 2009 Jeffrey S. Larson.  All rights reserved. */
/*
 * An interface provising basic OSC connectivity.
 * Currently built around Ross Bencina's oscpack.
 *
 */

#ifndef OSC_INTERFACE_H
#define OSC_INTERFACE_H

///////////////////////////////////////////////////////////////////////////////
//
// OscMessage
//
///////////////////////////////////////////////////////////////////////////////

#define OSC_MAX_STRING 256
#define OSC_MAX_ARGS 4
#define OSC_MAX_OUTPUT 1024

/**
 * We're going to simplify the message structure by only allowing a fixed
 * number of float arguments.  Need to extend this eventually to at least
 * allow strings!!
 */
class OscMessage {
  public:

	OscMessage();

    // if this is an incomming messsage you should call free() instead
    // so they can be pooled
	~OscMessage();

	const char* getAddress();
	void setAddress(const char* s);

	int getNumArgs();
	void setNumArgs(int i);

	float getArg(int i);
	void setArg(int i, float f);

	void free();

  private:

	OscMessage* mNext;
	char mAddress[OSC_MAX_STRING];
	float mArgs[OSC_MAX_ARGS];
	int mNumArgs;

};


///////////////////////////////////////////////////////////////////////////////
//
// OscListener
//
///////////////////////////////////////////////////////////////////////////////

/**
 * Interface to be implemented by something that wants to receive
 * incomming OSC messages.
 */
class OscListener {
  public:

	virtual void oscMessage(OscMessage* msg) = 0;

};

//////////////////////////////////////////////////////////////////////
//
// OscDevice
//
//////////////////////////////////////////////////////////////////////

/**
 * An object describing an OSC device that messages may be sent to.
 * These are created and owned by the OscInterface.
 */
class OscDevice {
  public:

	virtual ~OscDevice() {}
    virtual const char* getHost() = 0;
    virtual int getPort() = 0;

};

///////////////////////////////////////////////////////////////////////////////
//
// OscInterface
//
///////////////////////////////////////////////////////////////////////////////

class OscInterface {

  public:

	/** 
	 * Return an implementation of the interface.
	 */
	static OscInterface* getInterface();

	/**
	 * The interface is not a singleton, it must be deleted.
	 * Note that we have to have a virtual defined here
	 * to get the subclass destructor to run.
	 */
	virtual ~OscInterface() {}

	/**
	 * Set the UDP port to receive on.
	 */
	virtual void setReceivePort(int p) = 0;

	/**
	 * Register a listener for incomming messages.
	 */
	virtual void setListener(OscListener* l) = 0;

	/**
	 * Start listening for messages.
	 */
	virtual void start() = 0;

	/**
	 * Stop listening for messages.
	 * This may wait up to a second for the listener thread 
	 * to terminate.
	 */
	virtual void stop() = 0;

    /**
     * Register an OSC device.
     */
    virtual OscDevice* registerDevice(const char* host, int port) = 0;

    /**
     * Send a messages to a registered device.
     */
    virtual void send(OscDevice* dev, OscMessage* m) = 0;

	/**
	 * Send a message to an unregistered device.
	 */
	virtual void send(const char* host, int port, OscMessage* m) = 0;

  protected:

};

#endif
