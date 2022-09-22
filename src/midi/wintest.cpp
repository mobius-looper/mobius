/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simple test of the midi memory model
 *
 */

#include <stdio.h>
#include <windows.h>

#include "WinMidiEnv.h"
#include "MidiSequence.h"

/****************************************************************************
 *                                                                          *
 *                                 DATA MODEL                               *
 *                                                                          *
 ****************************************************************************/

void testModel(int argc, char *argv[])
{
	MidiSequence* seq = new MidiSequence();
	seq->readXml("seqtest.xml");

	char* xml = seq->toXml();
	printf("%s\n", xml);
	delete xml;

	delete seq;
}

/****************************************************************************
 *                                                                          *
 *                                   TIMER                                  *
 *                                                                          *
 ****************************************************************************/

static int MaxBeats;

void TimerCallback(MidiTimer *timer, void *args)
{
	int next;

	printf(".");
    fflush(stdout);

	// return the next clock we're interested in
	MaxBeats--;
	if (MaxBeats <= 0) {
		// can we do this from an interrupt handler?
		timer->transStop();
		next = 0;
	}
	else {
		next = timer->getClock() + timer->getResolution();
	}

	timer->setNextSignalClock(next);
}

void testTimer(int argc, char *argv[])
{
	MaxBeats = 10;

    MidiEnv* env = MidiEnv::getEnv();
    MidiTimer* timer = env->getTimer();
	timer->setCallback(TimerCallback, NULL);
	timer->setTempo(60);
	timer->start();

	printf("You should see 10 beats...\n");
    fflush(stdout);

	while (timer->isRunning())
	  Sleep(100);

    env->exit();
}

/****************************************************************************
 *                                                                          *
 *                                   INPUT                                  *
 *                                                                          *
 ****************************************************************************/

static int count = 0;

class InputCallback : public MidiInputListener {


	void midiInputEvent(MidiInput *in)
	{
		MidiEvent *events, *e;
		
		// get the current event(s)
		events = in->getEvents();

		for (e = events ; e != NULL ; e = e->getNext()) {
			printf("%xd\n", e->getStatus());
			if (e->getStatus() == MS_NOTEON) {
				printf("Another note on\n");
				count++;
			}
		}
		
		events->free();
	}
};

void testIn()
{
    MidiEnv* env = MidiEnv::getEnv();

    const char* portname = "Remote 25";
    MidiPort* port = env->getInputPort(portname);
    if (port == NULL) {
        printf("Invalid port: %s\n", portname);
        return;
    }

	// allocate a default MIDI input port 
    MidiInput* in = env->openInput(port);
	in->setListener(new InputCallback());

	if (in->connect())
	  printf("Unable to connect to input port\n");
	else {
		// loop
		printf("Enter 5 notes...");

		// global updated by input_callback
		count = 0;

		while (count < 5)
		  Sleep(1000);
	}

    env->exit();
}

/****************************************************************************
 *                                                                          *
 *                                   OUTPUT                                 *
 *                                                                          *
 ****************************************************************************/

// also use MaxBeats from the timer test
int NoteStatus;

void OutputTimerCallback(MidiTimer *timer, void *args)
{
	MidiOutput *out = (MidiOutput *)args;
	int next;

	// return the next clock we're interested in
	MaxBeats--;
	if (MaxBeats <= 0) {
		// can we do this from an interrupt handler?
		timer->transStop();
		next = 0;

		if (NoteStatus) {
			out->sendNoteOff(0, 40);
			NoteStatus = 0;
		}
	}
	else {
		next = timer->getClock() + timer->getResolution();

		if (NoteStatus) {
			out->sendNoteOff(0, 40);
			NoteStatus = 0;
		}
		else {
			out->sendNoteOn(0, 40, 90);
 			NoteStatus = 1;
		}
	}

	timer->setNextSignalClock(next);
}

void testOut()
{
	MaxBeats = 10;
    NoteStatus = 0;

	// build an environment
    MidiEnv* env = MidiEnv::getEnv();

    // allocate the timer
    MidiTimer* timer = env->getTimer();
	timer->setTempo(120);

	// allocate the default MIDI output port
    MidiPort* port = env->getDefaultOutputPort();
    MidiOutput* out = env->openOutput(port);

	if (out->connect())
	  printf("Unable to connect to output port\n");
	else {
		// pass output device to our timer callback
		timer->setCallback(OutputTimerCallback, (void *)out);

		// loop
		printf("You should hear 5 notes...\n");
		timer->transStart();
		while (timer->isRunning())
		  Sleep(1);
	}

    env->exit();
}

/****************************************************************************
 *                                                                          *
 *                                    OPEN                                  *
 *                                                                          *
 ****************************************************************************/

void testOpen()
{
    WinMidiOutput::testOpen();
}

/****************************************************************************
 *                                                                          *
 *                                    MAIN                                  *
 *                                                                          *
 ****************************************************************************/


void main(int argc, char *argv[])
{
    if (argc < 2)
      printf("miditest [model | timer | in | out | open]\n");
    else {
        char* test = argv[1];

        if (!strcmp(test, "model"))
          testModel(argc, argv);

        else if (!strcmp(test, "timer"))
          testTimer(argc, argv);

        else if (!strcmp(test, "in"))
          testIn();

        else if (!strcmp(test, "out"))
          testOut();

        else if (!strcmp(test, "open"))
		  testOpen();

		else 
		  printf("Unknown test: %s\n", test);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
