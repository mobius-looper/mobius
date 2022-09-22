/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Platform independent MIDI tests.
 *
 */

#include <stdio.h>
#include <string.h>

#include "Thread.h"

#include "MidiInterface.h"
#include "MidiListener.h"
#include "MidiPort.h"
#include "MidiEvent.h"
//#include "MidiSequence.h"

/****************************************************************************
 *                                                                          *
 *                                 DATA MODEL                               *
 *                                                                          *
 ****************************************************************************/

// can't include this test till we sort out WinMidiEnv
void testModel(int argc, char *argv[])
{
#if 0
	MidiSequence* seq = new MidiSequence();
	seq->readXml("sequencer/seqtest.xml");

	char* xml = seq->toXml();
	printf("%s\n", xml);
	delete xml;

	delete seq;
#endif
}

//////////////////////////////////////////////////////////////////////
//
// Devices
//
//////////////////////////////////////////////////////////////////////

void testDevices()
{
	MidiInterface* midi = MidiInterface::getInterface("testDevices");

	midi->printEnvironment();
	printf("************************************************\n");

	printf("Input Ports:\n");
	MidiPort* devs = midi->getInputPorts();
	if (devs == NULL)
	  printf("  No input ports\n");
	else {
		for (MidiPort* dev = devs ; dev != NULL ; dev = dev->getNext()) {
			printf("  %d %s\n", dev->getId(), dev->getName());
		}
	}

	printf("Output Ports:\n");
	devs = midi->getOutputPorts();
	if (devs == NULL)
	  printf("  No output devices\n");
	else {
		for (MidiPort* dev = devs ; dev != NULL ; dev = dev->getNext()) {
			printf("  %d %s\n", dev->getId(), dev->getName());
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

class TestListener : public MidiEventListener
{
  public:

  	TestListener(MidiInterface* midi);
	~TestListener();

	void midiEvent(MidiEvent* e);

  private:

	MidiInterface* mMidi;

};

TestListener::TestListener(MidiInterface* midi)
{
	mMidi = midi;
}

TestListener::~TestListener()
{
}

void TestListener::midiEvent(MidiEvent* e)
{
	e->dump(false);
    fflush(stdout);
	//mMidi->send(e);
}

void testEvents()
{
#ifdef _WIN32
	const char* indev = "4- ReMOTE";
	const char* outdev = "Ultralite";
#else
	const char* indev = "USB Trigger Finger";
	const char* outdev = "micro lite Port 1";
#endif

	MidiInterface* midi = MidiInterface::getInterface("testEvents");

	midi->setInput(indev);
	midi->setOutput(outdev);
	midi->setListener(new TestListener(midi));

	bool waitForKey = true;

	if (waitForKey) {
		printf("Press a key....");
		fflush(stdout);
		getchar();
	}
	else {
		while (true) {
			SleepMillis(1000);
			printf(".\n");
			fflush(stdout);
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                    MAIN                                  *
 *                                                                          *
 ****************************************************************************/

int main(int argc, char *argv[])
{
    if (argc < 2)
      printf("miditest [model | devices | events]\n");
    else {
        char* test = argv[1];

        if (!strcmp(test, "model"))
          testModel(argc, argv);

        else if (!strcmp(test, "devices"))
		  testDevices();

        else if (!strcmp(test, "events"))
		  testEvents();

        /*
        else if (!strcmp(test, "timer"))
          testTimer(argc, argv);

        else if (!strcmp(test, "in"))
          testIn();

        else if (!strcmp(test, "out"))
          testOut();
        */

		else 
		  printf("Unknown test: %s\n", test);
    }
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
