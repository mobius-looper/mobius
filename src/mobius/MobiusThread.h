/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Thread spawned by the Mobius engine to perform various 
 * tasks required by the interrupt handler, but which can't be
 * performed in the interrupt thread.
 *
 */

#ifndef MOBIUS_THREAD_H
#define MOBIUS_THREAD_H

#include "Trace.h"
#include "Thread.h"

/****************************************************************************
 *                                                                          *
 *                             THREAD EVENT TYPE                            *
 *                                                                          *
 ****************************************************************************/

/**
 * The types of thread events we can have.
 */
typedef enum {

	TE_NONE,
	TE_WAIT,
    TE_SAVE_LOOP,
    TE_SAVE_AUDIO,
    TE_SAVE_PROJECT,
    TE_SAVE_CONFIG,
    TE_LOAD,
    TE_DIFF,
    TE_DIFF_AUDIO,
	TE_TIME_BOUNDARY,
	TE_ECHO,
	TE_PROMPT,
	TE_GLOBAL_RESET

} ThreadEventType;

/****************************************************************************
 *                                                                          *
 *                                THREAD EVENT                              *
 *                                                                          *
 ****************************************************************************/

#define MAX_THREAD_ARG 1024

/**
 * Class used to represent an operation that the interrupt thread wants to 
 * have the Mobius thread perform.  Not time sequenced like Event,
 * though we might be able to share something?
 */
class ThreadEvent {

  public:

	ThreadEvent();
	ThreadEvent(ThreadEventType type);
	ThreadEvent(ThreadEventType type, const char* file);
	~ThreadEvent();

	ThreadEventType getType();
	void setType(ThreadEventType type);

	ThreadEvent* getNext();
	void setNext(ThreadEvent* te);

    void setArg(int psn, const char* value);
    const char* getArg(int psn);

	void setReturnCode(int i);
	int getReturnCode();

	void setProject(class Project* p);
	class Project* getProject();

  private:

	void init();
    void setArg(const char* file, char* dest);

	ThreadEvent* mNext;
	ThreadEventType mType;

    char mArg1[MAX_THREAD_ARG];
    char mArg2[MAX_THREAD_ARG];
    char mArg3[MAX_THREAD_ARG];

	int mReturnCode;

	// for TE_SAVE_PROJECT
	class Project* mProject;

};

/****************************************************************************
 *                                                                          *
 *                                   THREAD                                 *
 *                                                                          *
 ****************************************************************************/

class MobiusThread : public Thread, public TraceListener
{
  public:

    MobiusThread(Mobius* mob);
    ~MobiusThread();

	void setCheckInterrupt(bool b);
	void setTraceListener(bool b);
    void eventTimeout();
	void processEvent();
	void finishPrompt(class Prompt* p);

	void addEvent(ThreadEvent* e);
	void addEvent(ThreadEventType type);
	void traceEvent();
	void threadEnding();

  private:

	void flushEvents();
	ThreadEvent* popEvent();
	const char* getHomeDirectory();
	const char* getFullPath(ThreadEvent* e, const char* dflt, const char* ext);
	const char* getQuickPath();
	const char* getRecordingPath();
	const char* getQualifiedPath(const char* base, const char* extension,
								 int* counter);
	const char* getTestPath(const char* name, const char* extension);
    void diff(int type, bool reverse, const char* file1, const char* file2);
	void prompt(ThreadEvent* e);
    void finishEvent(ThreadEvent* e);

    class Mobius* mMobius;
	ThreadEvent* mEvents;
	ThreadEventType mOneShot;
	long mInterrupts;
    long mCycles;
    int mStatusCycles;
	bool mCheckInterrupt;
	int mQuickSaveCounter;

	char mPathBuffer[1024 * 8];
	char mMessage[1024 * 8];

	int mPrompts;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
