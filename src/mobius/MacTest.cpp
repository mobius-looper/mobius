/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * OSX Main routine for the tests
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"
#include "MacUtil.h"

#include "Context.h"

#include "MacInstall.h"
#include "AudioInterface.h"
#include "HostInterface.h"

//////////////////////////////////////////////////////////////////////
//
// HostInterface Simulator
//
//////////////////////////////////////////////////////////////////////

class TestHost : public HostInterface, public AudioInterface, public AudioStream {
  public:

	TestHost(Context* con) {
		mContext = con;
	}

	//
	// HostInterface
	//
	
	Context* getContext();
	const char* getHostName();
	const char* getHostVersion();
	AudioInterface* getAudioInterface();
	void notifyParameter(int id, float value);

	// 
	// AudioInterface
	//

	void terminate();
	AudioDevice** getDevices();
	AudioDevice* getDevice(int id);
	AudioDevice* getDevice(const char* name, bool output);
	void printDevices();

	// only thing interesting
	AudioStream* getStream();

	//
	// AudioStream
	//

	AudioInterface* getInterface();

	bool setInputDevice(int id);
	bool setInputDevice(const char* name);
	AudioDevice* getInputDevice();

	bool setOutputDevice(int id);
	bool setOutputDevice(const char* name);
	AudioDevice* getOutputDevice();

    int getInputChannels();
	int getInputPorts();

    int getOutputChannels();
	int getOutputPorts();

	void setSampleRate(int i);
	int getSampleRate();

	void setHandler(AudioHandler* h);
	AudioHandler* getHandler();

	bool open();
	void close();

	// this is PortAudio specific, weed this out!!
	const char* getLastError();
	void setSuggestedLatencyMsec(int msec);
    int getInputLatencyFrames();
    void setInputLatencyFrames(int frames);
    int getOutputLatencyFrames();
    void setOutputLatencyFrames(int frames);
	void printStatistics();

	// AudioHandler callbacks
	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** inbuf, 
							 int outport, float** outbuf);
	AudioTime* getTime();
	double getStreamTime();
	double getLastInterruptStreamTime();

	//
	// Our extra stuff
	//
	
	PluginInterface* getPlugin();

  private:

	Context* mContext;
	AudioTime mTime;

};

PluginInterface* TestHost::getPlugin()
{
	return NULL;
}

/**
 * Build an application context for the plugin.
 * This relies on the fact that MacContext is now defined
 * in Context.h and doesn't drag in any of the other qwin
 * stuff which conflicts with various things in CoreAudio
 * and Carbon.
 */
Context* TestHost::getContext()
{
	return mContext;
}

AudioInterface* TestHost::getAudioInterface()
{
	return this;
}

const char* TestHost::getHostName()
{
	return NULL;
}

const char* TestHost::getHostVersion()
{
	return NULL;
}

void TestHost::notifyParameter(int id, float value)
{
}

//////////////////////////////////////////////////////////////////////
//
// AudioInterface
//
//////////////////////////////////////////////////////////////////////

void TestHost::terminate()
{
}

AudioDevice** TestHost::getDevices()
{
	return NULL;
}

AudioDevice* TestHost::getDevice(int id)
{
	return NULL;
}

AudioDevice* TestHost::getDevice(const char* name, bool output)
{
	return NULL;
}

void TestHost::printDevices()
{
}

AudioStream* TestHost::getStream()
{
	return this;
}

//////////////////////////////////////////////////////////////////////
//
// AudioStream
//
//////////////////////////////////////////////////////////////////////

AudioInterface* TestHost::getInterface()
{
	return this;
}

int TestHost::getInputChannels()
{
	return 2;
}

int TestHost::getInputPorts()
{
	return 1;
}

int TestHost::getOutputChannels()
{
	return 2;
}

int TestHost::getOutputPorts()
{
	return 1;
}

bool TestHost::setInputDevice(int id)
{
	return true;
}

bool TestHost::setInputDevice(const char* name)
{
	return true;
}

bool TestHost::setOutputDevice(int id)
{
	return true;
}

bool TestHost::setOutputDevice(const char* name)
{
	return true;
}

void TestHost::setSuggestedLatencyMsec(int i)
{
}

AudioDevice* TestHost::getInputDevice() 
{
	return NULL;
}

AudioDevice* TestHost::getOutputDevice() 
{
	return NULL;
}

int TestHost::getSampleRate() 
{
	return 44100;
}

void TestHost::setSampleRate(int rate)
{
}

AudioHandler* TestHost::getHandler()
{
	return NULL;
}

void TestHost::setHandler(AudioHandler* h)
{
}

const char* TestHost::getLastError()
{
	return NULL;
}

bool TestHost::open()
{
	return true;
}

int TestHost::getInputLatencyFrames()
{
	return 0;
}

void TestHost::setInputLatencyFrames(int frames)
{
}

int TestHost::getOutputLatencyFrames()
{
    return 0;
}

void TestHost::setOutputLatencyFrames(int frames)
{
}

void TestHost::close()
{
}

void TestHost::printStatistics()
{
}

PUBLIC long TestHost::getInterruptFrames()
{
	return 0;
}

PUBLIC AudioTime* TestHost::getTime()
{
	return &mTime;
}

PUBLIC double TestHost::getStreamTime()
{
	return 0.0;
}

PUBLIC double TestHost::getLastInterruptStreamTime()
{
	return 0.0;
}

PUBLIC void TestHost::getInterruptBuffers(int inport, float** inbuf,
										  int outport, float** outbuf)
{
}

//////////////////////////////////////////////////////////////////////
//
// Test Driver
//
//////////////////////////////////////////////////////////////////////

void simulatePlugin(Context* con)
{
	printf("MacTest: Creating plugin\n");
	fflush(stdout);

	HostInterface* host = new TestHost(con);

	PluginInterface* p = PluginInterface::newPlugin(host);

	int nPorts = p->getPluginPorts();

 	PluginParameter* params = p->getParameters();
	for (PluginParameter* p = params ; p != NULL ; p = p->getNext()) {
		printf("MacTest: declareParameters %d %s %f\n", 
			   p->getId(), p->getName(), p->getDefault());
	}

	printf("MacTest: Starting plugin\n");
	fflush(stdout);
	p->start();

	SleepMillis(1000);

	printf("MacTest: Deleting plugin\n");
	fflush(stdout);
	delete p;
	delete host;
}

int main(int argc, char *argv[])
{
	int result = 0;
	
	Context* con = Context::getContext(argc, argv);

	// we're going to skip MacInstall here since we're not really
	// an installed bundle, and just set the names we normally use
	con->setInstallationDirectory("/Applications/Mobius 2/Mobius.app/Contents/Resources");
	con->setConfigurationDirectory("/Library/Application Support/Mobius 2");

	simulatePlugin(con);
	SleepMillis(1000);
	simulatePlugin(con);

    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
