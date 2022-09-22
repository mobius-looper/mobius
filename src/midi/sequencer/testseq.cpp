/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Sequencer "play" test
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

// for _beginthread
#include <process.h>

// not sure where Sleep is
#include <windows.h>


#include "port.h"
#include "seq.h"

#define CPB 96

/****************************************************************************
 *                                                                          *
 *   							  ARG PARSER                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Stupid little command line argument tokenizer for the command loop.
 *
 */

char *getArg(char *src, char *dest)
{

	// trim whitespace
	while (*src && isspace(*src)) src++;
	
	// check for quotes
	if (*src != '"') {
		while (*src && !isspace(*src))
		  *dest++ = *src++;
	}
	else {
		src++;
		while (*src && *src != '"')
		  *dest++ = *src++;

		if (*src)
		  src++;
	}
	*dest = 0;

	return src;
}

void parseArgs(char *line, char *cmd, char *arg1, char *arg2, char *arg3)
{
	char *src;

	strcpy(cmd, "");
	strcpy(arg1, "");
	strcpy(arg2, "");
	strcpy(arg3, "");

	src = getArg(line, cmd);
	src = getArg(src, arg1);
	src = getArg(src, arg2);
	src = getArg(src, arg3);
}

/****************************************************************************
 *                                                                          *
 *   						  SEQUENCE COMPILER                             *
 *                                                                          *
 ****************************************************************************/
/* 
 * A very simple tool for building test sequences, without having
 * to use external files.  
 * Nice for testing but probably not very usefull for anything else.
 */

MidiSequence *compileSequence(Sequencer *seq, int *tmpl, int start_clock)
{
	MidiSequence 	*s;
	MidiEvent		*e;
	int				time, i, channel, duration, velocity;

	time     = start_clock;
	channel  = 0;
	duration = CPB;
	velocity = 80;
	s	   	 = seq->newSequence();

	for (i = 0 ; tmpl[i] != 0 ; i++) {
		e = seq->newEvent(MS_NOTEON, 0, tmpl[i], velocity);
		e->setClock(time);
		e->setDuration(duration);
		s->insert(e);
		time += CPB;
	}

	return s;
}

/****************************************************************************
 *                                                                          *
 *   						  SEQUENCER CALLBACKS                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Send things to the debug window, to avoid cluttering up the prompt
 */

#if 0
void commandCallback(Sequencer *s, int start, int events)
{
	BasicEnvironment *env = s->getEnv();

	if (start)
	  env->debug("start\n");
	else
	  env->debug("stop\n");
}

void loopCallback(Sequencer *s, MidiSequence *ms, int events)
{
	BasicEnvironment *env = s->getEnv();
	env->debug("loop\n");
}

void noteCallback(Sequencer *s, MidiEvent *e, int on)
{
	BasicEnvironment *env = s->getEnv();
	if (on)
	  env->debug("%d on\n", e->getKey());
	else
	  env->debug("%d off\n", e->getKey());
}

int beatCallback(Sequencer *s)
{
	BasicEnvironment *env = s->getEnv();
	env->debug(".\n");
	return 0;
}
#endif

/****************************************************************************
 *                                                                          *
 *   							EVENT HANDLERS                              *
 *                                                                          *
 ****************************************************************************/

void checkEvents(Sequencer *s) 
{
	BasicEnvironment *env = s->getEnv();
	SeqEvent *e, *events;

	events = s->getEvents();

	for (e = events ; e ; e = e->getNext()) {

		if (e->getType() == SeqEventStart)
		  printf("start\n");
		
		else if (e->getType() == SeqEventStop)
		  printf("stop\n");

		else if (e->getType() == SeqEventLoop)
		  printf("loop\n");

		else if (e->getType() == SeqEventBeat)
		  printf(".\n");

		else if (e->getType() == SeqEventNoteOn)
		  printf("%d on\n", e->getValue());

		else if (e->getType() == SeqEventNoteOff)
		  printf("%d off\n", e->getValue());
		
		else
		  printf("???\n");
	}

	if (events)
	  events->free();
}

/****************************************************************************
 *                                                                          *
 *   							  PLAY TEST                                 *
 *                                                                          *
 ****************************************************************************/

// note templates for the test sequences
int testseq[] = {60, 61, 62, 63, 0};
int testseq2[] = {70, 71, 72, 73, 0};

void setupPlayTest(Sequencer *s)
{
	MidiSequence *s1, *s2;

	// clear existing sequences (and free them)
	s->clearTracks();

	// build some test sequences
	s1 = compileSequence(s, testseq, 0);
	s2 = compileSequence(s, testseq2, CPB * 4);

	// install em in the sequencer
	s->addSequence(s1);
	s->addSequence(s2);
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP TEST                                 *
 *                                                                          *
 ****************************************************************************/
/*
 * Formerly used a file to contain loop info, now we maintain it 
 * in a static structure array.
 *
 */

// note templates for the test sequences
int ltestseq[] = {61, 62, 63, 64, 0};

typedef struct {
	
	int start;
	int end;
	int count;

} LOOP_INFO;

// just repeat the sequence twice
// 1234 1234

LOOP_INFO testloops1[] = {
	{0, CPB * 4, 2},
	{-1, -1, -1}
};

// harder test
// outer loop the whole thing twice
// inner loop the first two notes three times
// second inner loop the last two notes twice
// 
// 121212123434 121212123434

LOOP_INFO testloops[] = {
	{0, CPB * 4, 1},
	{0, CPB * 2, 3},
	{CPB * 2, CPB * 4, 1},
	{-1, -1, -1}
};

void setupLoopTest(Sequencer *s)
{
	MidiSequence *s1;
	int i;

	// clearn out existing sequences
	s->clearTracks();

	// compile a new one
	s1 = compileSequence(s, ltestseq, 0);

	// add loops
	for (i = 0 ; testloops[i].start != -1 ; i++) {
		LOOP_INFO *l = &testloops[i];
		s1->addLoop(l->start, l->end, l->count);
	}

	// install it
	s->addSequence(s1);
}

/****************************************************************************
 *                                                                          *
 *   							 RECORD TEST                                *
 *                                                                          *
 ****************************************************************************/

void setupRecordTest(Sequencer *s)
{
	MidiSequence *ms;
	SeqTrack *tr;

	// clear current sequences
	s->clearTracks();

	// initialize a new one
	ms = s->newSequence();
	tr = s->addSequence(ms);
	tr->startRecording();

	// setup a record loop
	s->setLoopEndMeasure(2);
	s->setLoopEndEnable(1);
}

/****************************************************************************
 *                                                                          *
 *   								 MAIN                                   *
 *                                                                          *
 ****************************************************************************/

void usage(void)
{
	printf("Sequencer test driver:\n");
	printf("    ?          help\n");
	printf("    q          quit\n");
	printf("    p          play\n");
	printf("    s          stop\n");
	printf("    r          record\n");
	printf("    rq         quit recording\n");
	printf("    rc         clear recording\n");
	printf("    ra         accept recording\n");
	printf("    rr         revert recording\n");
	printf("    tp         setup play test\n");
	printf("    tl         setup loop test\n");
	printf("    tr         setup record test\n");
	printf("\n");
}

int gMonitorRun = 1;

void monitorSequencer(void *arg)
{
	Sequencer *seq = (Sequencer *)arg;

	printf("monitor thread starting\n");

	while (gMonitorRun) {
		Sleep(100);
		if (gMonitorRun)
		  checkEvents(seq);
	}
	printf("monitor thread exiting\n");
}

int main(int argc, char *argv[])
{
	SeqEnvironment 	*env;
	Sequencer		*seq;
	char			line[80], cmd[80], arg[80], arg2[80], arg3[80];
	int				stop;

	// build an environment
	env = SeqEnvironment::create();
	if (env == NULL)
	  return 1;

	// make a sequencer
	seq = env->newSequencer();
	if (seq == NULL)
	  return 1;

	// assign MIDI ports, and various parameters
	// sequencer will open default ports, we don't have to tell it
	//seq->openInputPort(MIDI_IN_LYNX_1);
	//seq->openOutputPort(MIDI_OUT_LYNX_1);
	seq->setTempo(120);

#if 0
	seq->setCallbackCommand(commandCallback);
	seq->setCallbackLoop(loopCallback);
	seq->setCallbackNote(noteCallback);
	seq->setCallbackBeat(beatCallback);
	seq->setRecordEcho(1);
#endif

	seq->enableEvents(SeqEventStart | SeqEventStop | 
					  SeqEventLoop | SeqEventBeat |
					  SeqEventNoteOn | SeqEventNoteOff);

	// launch the monitor thread
	gMonitorRun = 1;
	_beginthread(monitorSequencer, 0, (void *)seq);

	// command loop
	usage();
	stop = 0;
	while (!stop) {

		printf("> ");
		gets(line);

		// isolate arguments
		parseArgs(line, cmd, arg, arg2, arg3);

		if (!strcmp(cmd, "q"))
		  stop = 1;

		else if (!strcmp(cmd, "?"))
		  usage();

		else if (!strcmp(cmd, "p")) {
			seq->setClock(0);
			seq->start();
		}

		else if (!strcmp(cmd, "s")) {
			seq->stop();
		}

		else if (!strcmp(cmd, "r")) {
			// start recording
 			seq->setClock(0);
			seq->start();
		}

		else if (!strcmp(cmd, "rq")) {
			// quit recording
			seq->stop();
			seq->clearRecording();
		}

		else if (!strcmp(cmd, "rc")) {
			// clear the recording sequence
			SeqTrack *tr = seq->getTrack(0);
			seq->stop();
			tr->clear();
		}

		else if (!strcmp(cmd, "ra")) {
			// accept the recording
			seq->stop();
			seq->acceptRecording();
		}

		else if (!strcmp(cmd, "rr")) {
			// revert the recording
			seq->stop();
			seq->revertRecording();
		}

		else if (!strcmp(cmd, "tp"))
		  setupPlayTest(seq);

		else if (!strcmp(cmd, "tl"))
		  setupLoopTest(seq);

		else if (!strcmp(cmd, "tr"))
		  setupRecordTest(seq);

		else
		  usage();
	}

	// stop the monitor thread, need to wait nicely
	gMonitorRun = 0;
	Sleep(500);

	// clean up everything
	delete env;
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/





