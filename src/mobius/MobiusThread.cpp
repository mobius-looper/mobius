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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"

#include "Action.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "MobiusThread.h"
#include "Project.h"
#include "Script.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Timeout once every 1/10 second so the UI can refresh and we can check
 * for memory allocations. 
 *
 * This timeout also controls the granularity of the MIDI export.
 */
#define DEFAULT_TIMEOUT 100

/**
 * The number of cycles between tracing periodic memory status.
 * If a cycle is 1/10 second, there are 10 a second and 600 a minute.
 */
#define STATUS_CYCLES 600

/****************************************************************************
 *                                                                          *
 *   							 THREAD EVENT                               *
 *                                                                          *
 ****************************************************************************/

ThreadEvent::ThreadEvent()
{
	init();
}

ThreadEvent::ThreadEvent(ThreadEventType type)
{
	init();
	mType = type;
}

ThreadEvent::ThreadEvent(ThreadEventType type, const char* file)
{
	init();
	mType = type;
    setArg(0, file);
}

void ThreadEvent::init()
{
	mNext = NULL;
	mType = TE_NONE;
	mProject = NULL;
	mReturnCode = 0;
    strcpy(mArg1, "");
    strcpy(mArg2, "");
    strcpy(mArg3, "");
}

ThreadEvent::~ThreadEvent()
{
	// we get to own this
	delete mProject;
}

ThreadEventType ThreadEvent::getType()
{
	return mType;
}

void ThreadEvent::setType(ThreadEventType type)
{
	mType = type;
}

ThreadEvent* ThreadEvent::getNext()
{
	return mNext;
}

void ThreadEvent::setNext(ThreadEvent* te)
{
	mNext = te;
}

Project* ThreadEvent::getProject()
{
	return mProject;
}

void ThreadEvent::setProject(Project* p)
{
	// do we get to own this?
	mProject = p;
}

void ThreadEvent::setArg(int psn, const char* value)
{
	switch (psn) {
		case 0:
			setArg(value, mArg1);
			break;
		case 1:
			setArg(value, mArg2);
			break;
		case 2:
			setArg(value, mArg3);
			break;
	}
}

void ThreadEvent::setArg(const char* src, char* dest)
{
	strcpy(dest, "");
	if (src != NULL) {
		size_t len = strlen(src) + 1;
		if (len < sizeof(mArg1))
		  strcpy(dest, src);
		else
		  Trace(1, "ThreadEvent::setArg value to long %s\n", src);
	}
}

/**
 * Get an argument, returning NULL if arg buffer is empty.
 * This saves the caller having to check for strlen() == 0.
 */
const char* ThreadEvent::getArg(int psn)
{
	const char* value = NULL;
	switch (psn) {
		case 0:
			if (mArg1[0]) value = mArg1;
			break;
		case 1:
			if (mArg2[0]) value = mArg2;
			break;
		case 2:
			if (mArg3[0]) value = mArg3;
			break;
	}
	return value;
}

void ThreadEvent::setReturnCode(int i)
{
	mReturnCode = i;
}

int ThreadEvent::getReturnCode()
{
	return mReturnCode;
}

/****************************************************************************
 *                                                                          *
 *   								PROMPT                                  *
 *                                                                          *
 ****************************************************************************/

Prompt::Prompt()
{
	mNext = NULL;
	mEvent = NULL;
	mText = NULL;
	mOk = false;
}

Prompt::~Prompt()
{
	delete mText;
	delete mEvent;
}

Prompt* Prompt::getNext()
{
	return mNext;
}

void Prompt::setNext(Prompt* p)
{
	mNext = p;
}

ThreadEvent* Prompt::getEvent() 
{
	return mEvent;
}

void Prompt::setEvent(ThreadEvent* e)
{
	mEvent = e;
}

const char* Prompt::getText()
{
	return mText;
}

void Prompt::setText(const char* text)
{
	mText = CopyString(text);
}

bool Prompt::isOk()
{
	return mOk;
}

void Prompt::setOk(bool b)
{
	mOk = b;
}

/**
 * This is called during shutdown, we're supposed to release
 * resources.
 */

/****************************************************************************
 *                                                                          *
 *   								THREAD                                  *
 *                                                                          *
 ****************************************************************************/

MobiusThread::MobiusThread(Mobius* m)
{
	setTimeout(DEFAULT_TIMEOUT);
    mMobius = m;
	mEvents = NULL;
	mOneShot = TE_NONE;
	mInterrupts = 0;
    mCycles = 0;
    mStatusCycles = 0;
	mQuickSaveCounter = 1;
	mPrompts = 0;

	// normally this is on but disable during the Mac port until
	// we can work out a way to pass this in as an option
	mCheckInterrupt = false;

    setName("Mobius");
}

MobiusThread::~MobiusThread()
{
	flushEvents();

	// TODO: What to do about lingering prompts?
	// There is some ownership confusion since they've been
	// given to the UI so assume it will clean them up
	if (mPrompts != 0)
	  Trace(1, "MobiusThread destructing with %ld lingering prompts!\n",
			(long)mPrompts);
}

void MobiusThread::setCheckInterrupt(bool b) 
{
	mCheckInterrupt = b;
	mInterrupts = 0;
}

/**
 * Control whether the thread is registered as the trace listener.
 */
void MobiusThread::setTraceListener(bool b)
{
	// ugh, don't really like the interface here, but not really worth
	// a more complicated encapsulation
	if (b) {
		if (NewTraceListener != this) {
			Trace(2, "Replacing trace listener with MobiusThread\n");
			NewTraceListener = this;
		}
	}
	else if (NewTraceListener == this) {
		Trace(2, "Removing MobiusThread as trace listener\n");
		NewTraceListener = NULL;
	}
	else {
		Trace(1, "MobiusThread was not the trace listener!\n");
	}
}
		

/**
 * Called by Thread as it closes, possibly after catching
 * an exception.  If we're still registered as the trace listener,
 * cancel it.
 */
void MobiusThread::threadEnding()
{
	if (NewTraceListener == this)
	  NewTraceListener = NULL;
}

void MobiusThread::flushEvents()
{
    ThreadEvent* events;

    enterCriticalSection();
    events = mEvents;
    mEvents = NULL;
    leaveCriticalSection();

	ThreadEvent* next = NULL;
	for (ThreadEvent* e = events ; e != NULL ; e = next) {
		next = e->getNext();
		delete e;
	}
}

PUBLIC void MobiusThread::addEvent(ThreadEvent* e)
{
	enterCriticalSection();
    // these are often order dependent!
    ThreadEvent* last = mEvents;
    while (last != NULL && last->getNext() != NULL)
      last = last->getNext();
    if (last == NULL)
      mEvents = e;
    else 
      last->setNext(e);
    leaveCriticalSection();

	// this will signal the inherited Thread::run loop and we should
	// shortly end up in processEvent
	signal();
}

/**
 * Added for the one-shot TE_TIME_BOUNDARY event which can happen
 * a lot so avoid allocating a ThreadEvent.  
 *
 * NOTE: Since TE_TIME_BOUNDARY is important to make the UI 
 * flashers look synchronized we effectively can only use the one shot
 * event for TE_TIME_BOUNDARY, I suppose if we miss a few it won't matter
 * but we can't use it for things that must have guaranteed delivery 
 * like TE_GLOBAL_RESET.
 */
PUBLIC void MobiusThread::addEvent(ThreadEventType tet)
{
	mOneShot = tet;
	signal();
}

/**
 * We implement the util/TraceListener interface and will be registered
 * as the listener.  This method is called whenever a new trace
 * record is added.  Wake up and flush trace messages.
 */
PUBLIC void MobiusThread::traceEvent()
{
	signal();
}

/****************************************************************************
 *                                                                          *
 *   						   EVENT PROCESSING                             *
 *                                                                          *
 ****************************************************************************/

ThreadEvent* MobiusThread::popEvent()
{
    enterCriticalSection();
	ThreadEvent* e = mEvents;
	if (e != NULL)
	  mEvents = e->getNext();
    leaveCriticalSection();
	return e;
}

/**
 * Called by the run loop when an event wait times out, default
 * of once every 1/10 second.  This is where we call the MobiusRefresh
 * handler to let the UI refresh itself.
 *
 * Also make sure the interrupt counter is advancing, if it isn't there
 * is probably a loop in the handler which can lock up the machine.
 *
 * UPDATE: This is NOT a good timer!!  We use it to dump trace
 * messages so it is signal()'d regularly which resets the amount
 * of time before a timeout.  If we're doing a lot of trace we may
 * not get a timeout for a long time.  We'll have to look at the
 * system clock or something in processEvent to see if we're ready
 * to fire a MobiusRefresh there too...
 */
void MobiusThread::eventTimeout()
{
	/*
	static int counter = 0;
	counter++;
	if (counter == 10) {
		counter = 0;
		printf("tick\n");
		fflush(stdout);
	}
	*/

    mCycles++;
    mStatusCycles++;
    
    if (mStatusCycles >= STATUS_CYCLES) {
        MobiusConfig* config = mMobius->getConfiguration();
        if (config->isLogStatus())
          mMobius->logStatus();
        mStatusCycles = 0;
    }

    // this is typically the UI
	MobiusListener* ml = mMobius->getListener();
	if (ml != NULL)
	  ml->MobiusRefresh();
    
    // this exports changes to parameters/controls to MIDI control surfaces
    mMobius->exportStatus(true);

	if (mCheckInterrupt) {
		long interrupts = mMobius->getInterrupts();
		if (mInterrupts > 0 && mInterrupts == interrupts) {
			if (mMobius->isInInterrupt()) {
				// we appear stuck
				Trace(1, "Interrupt handler looks stuck, emergency exit!\n");
				mMobius->emergencyExit();
				stop();
			}
		}
		mInterrupts = interrupts;
	}
}

void MobiusThread::processEvent()
{
	// always flush any pending trace messages
	if (NewTraceListener == this) FlushTrace();

	ThreadEvent* e = popEvent();
	while (e != NULL) {
        ThreadEventType type = e->getType();
		bool freeEvent = true;

		switch (type) {

			case TE_SAVE_CONFIG: {
                mMobius->writeConfiguration();
			}
			break;

			case TE_SAVE_LOOP: {
				Audio* a = mMobius->getPlaybackAudio();
				if (a != NULL) {
					const char* path = getFullPath(e, NULL, ".wav");
					if (path == NULL)
					  path = getQuickPath();

					a->write(path);
					// this is a flattened copy we now own
					delete a;
					Trace(2, "Saved loop to %s\n", path);
				}
			}
			break;

			case TE_SAVE_AUDIO: {
				// unlike captured scripts we do NOT own this
				Audio* a = mMobius->getCapture();
				if (a != NULL) {
					const char* path = getFullPath(e, NULL, ".wav");
					if (path == NULL)
					   path = getRecordingPath();

					a->write(path);
					Trace(2, "Saved recording to %s\n", path);
				}
			}
			break;

			case TE_SAVE_PROJECT: {
				const char* path = getFullPath(e, NULL, ".mob");
				if (path != NULL) {
					Project* p = mMobius->saveProject();
					p->setPath(path);
					p->write();
					delete p;
					Trace(2, "Saved project to %s\n", path);
				}
			}
			break;

			case TE_LOAD: {
				const char* path = getFullPath(e, NULL, ".mob");
				if (path != NULL) {
					if (EndsWithNoCase(path, ".mob")) {

						Project* p = new Project(path);
                        p->read(mMobius->getAudioPool());

						if (!p->isError()) {
							mMobius->loadProject(p);
							
							
							// #023 SetSetup in mConfig, so update the view!
							Setup* s = mMobius->mConfig->getSetup(p->getSetup());
            				if (s != NULL)
              					mMobius->setSetupInternal(s);  //#023
							
							Trace(3, "[script] Loaded project from %s | setSetup %s", path,p->getSetup());
							Trace(2, "Loaded project from %s\n", path);
						}
						else {
							// localize!!
							sprintf(mMessage, "Invalid project file: %s", path);
							Trace(1, "%s\n", mMessage);
							MobiusListener* ml = mMobius->getListener();
							if (ml != NULL)
							  ml->MobiusAlert(mMessage);
							delete p;
						}
					}
					else if (EndsWithNoCase(path, ".wav")) {
						if (IsFile(path)) {
							// other possible errors, should have
							// something like Project::isError
                            AudioPool* pool = mMobius->getAudioPool();
							Audio* au = pool->newAudio(path);
							// TODO: need to pass the desired target
							// track in the event
							mMobius->loadLoop(au);
						}
						else {
							// localize!!
							sprintf(mMessage, "Invalid file: %s", path);
							Trace(1, "%s\n", mMessage);
							MobiusListener* ml = mMobius->getListener();
							if (ml != NULL)
							  ml->MobiusAlert(mMessage);
						}
					}
					else {
						// guess?
					}
				}
			}
			break;

			case TE_DIFF:
			case TE_DIFF_AUDIO: {
				const char* file1 = e->getArg(0);
				const char* file2 = e->getArg(1);
				bool reverse = StringEqualNoCase(e->getArg(2), "reverse");

				if (file1 != NULL && file2 != NULL) {
					// just assume these are both relative
					diff(type, reverse, file1, file2);
				}
				else if (file1 != NULL) {
					const char* extension = ".wav";
					const char* master = getTestPath(file1, extension);
					char newpath[1025];
					strcpy(newpath, file1);
					if (!EndsWith(newpath, extension))
					  strcat(newpath, extension);
					diff(type, reverse, newpath, master);
				}
			}
			break;

			case TE_WAIT: {
				// will be handled by ScriptInterpreter below
				// I don't think we really need this any more now
				// that we have "Wave last"
			}	
			break;

			case TE_ECHO: {
				const char* msg = e->getArg(0);
#ifdef _WIN32
				OutputDebugString(msg);
#endif
				printf("%s", msg);
				fflush(stdout);
			}	
			break;

			case TE_PROMPT: {
				// we free these later
				freeEvent = false;
				prompt(e);
			}	
			break;

			case TE_GLOBAL_RESET: {
				// Let the UI know so it can clear any lingering messages.
				// This is kludgey, once we have a better state objet for
				// conveying state we may not need this.  Still events
				// like this are closer to the OSC model so we might
				// want to expand these too.
				mMobius->notifyGlobalReset();
			}
			break;

			// need these to prevent xcode 5 from whining
			case TE_NONE: break;
			case TE_TIME_BOUNDARY: break;
		}

		if (freeEvent) {
            if (type == TE_NONE)
              delete e;
            else
              finishEvent(e);
		}

		e = popEvent();
	}

	// also catch the one-shot events that don't allocate event objects
	if (mOneShot == TE_TIME_BOUNDARY) {
		// we crossed a beat/cycle/loop boundary, tell the  UI
		// so it can refresn immediately
		MobiusListener* ml = mMobius->getListener();
		if (ml != NULL)
		  ml->MobiusTimeBoundary();
		mOneShot = TE_NONE;
	}

	// and flush trace messages again
	if (NewTraceListener == this) FlushTrace();
}

/**
 * Determine the root of the directory containing the files to 
 * read and write when using relative paths.  
 *
 * Mobius::getHomeDirectory will return the configuration directory
 * or the installation directory.   These are the same on Windows, 
 * on Mac config will be /Library/Application Support/Mobius.
 *
 * For unit testing we don't want to copy everything out of the 
 * source direcory to the config/install directory so recognize the
 * MOBIUS_HOME environment variable.
 *
 * !! I don't like this, need to add TestDirectory or something to 
 * MobiusConfig?
 */
PRIVATE const char* MobiusThread::getHomeDirectory()
{
	const char* home = getenv("MOBIUS_HOME");
	if (home == NULL) {
		home = mMobius->getHomeDirectory();
	}
	return home;
}

/**
 * Calculate the path of an input or output file.
 */
PRIVATE const char* MobiusThread::getFullPath(ThreadEvent* e, 
											  const char* dflt,
											  const char* extension)
{
	const char* fullpath = NULL;

	const char* path = e->getArg(0);
	if (path == NULL)
	  path = dflt;

	if (path != NULL) {
		if (IsAbsolute(path)) {
		  strcpy(mPathBuffer, path);
		}
		else if (StartsWith(path, "./")) {
            // force relative to PWD
			strcpy(mPathBuffer, &path[2]);
		}
		else {
            // releaitve to homedir
			MergePaths(getHomeDirectory(), path, 
					   mPathBuffer, sizeof(mPathBuffer));
		}

		// assume if there is any extension that we shouldn't replace it
		// makes it possible for TE_LOAD to look for both .wav and .mob files
		//if (extension != NULL && !EndsWith(mPathBuffer, extension))

		if (extension != NULL && !HasExtension(mPathBuffer))
		  strcat(mPathBuffer, extension);

		fullpath = mPathBuffer;
	}

	return fullpath;
}

/**
 * Given a test output file name and extension, derive the full path 
 * to the expected test file.
 * 
 * The global parameter unitTestRoot must be set to the directory
 * containing the test files.  
 * 
 * Formerly this would just assume that the files were under
 * the test/expected directory relative to the current working directory.
 * This broke when we moved the test files out of the main buid directory.
 */
PRIVATE const char* MobiusThread::getTestPath(const char* name, const 
											  char* extension)
{
	MobiusConfig* config = mMobius->getConfiguration();
	const char* root = config->getUnitTests();
	if (root == NULL) {
		// guess
		root = "../../../mobiustest";
	}

	sprintf(mPathBuffer, "%s/expected/%s", root, name);

	if (!EndsWith(mPathBuffer, extension))
	  strcat(mPathBuffer, extension);

	return mPathBuffer;
}

/**
 * Calculate a QuickSave path.
 */
PRIVATE const char* MobiusThread::getQuickPath()
{
	MobiusConfig* config = mMobius->getConfiguration();
	const char* file = config->getQuickSave();
	if (file == NULL)
	  file = "mobiusloop";

	return getQualifiedPath(file, ".wav", &mQuickSaveCounter);
}

/**
 * Calculate the default path for a captured audio recording.
 * !! Unlike the QuickSave handler, we don't have a static
 * counter to help pick a qualifier.  Not sure if this makes
 * much difference since SaveCapture is uncommon.
 */
PRIVATE const char* MobiusThread::getRecordingPath()
{
	char buffer[1024 * 8];
	const char* recpath = "recording";

	MobiusConfig* config = mMobius->getConfiguration();
	const char* qfile = config->getQuickSave();
	if (qfile != NULL) {
		// this normally contains the quicksave file name, strip it off
		// !! need to make the quickSave parameter be just the directory name

		GetDirectoryPath(qfile, buffer);

		if (strlen(buffer) > 0) {
			size_t last = strlen(buffer) - 1;
			char lastchar = buffer[last];
			if (lastchar == '/' || lastchar == '\\') {
				// looks like a path
				strcat(buffer, "recording");
				recpath = buffer;
			}
		}
	}

	return getQualifiedPath(recpath, ".wav", NULL);
}

/**
 * Generate a unique file name.
 * In theory this could take awhile if the directory is already full
 * of previously saved loops.  Looks like a good thing for the utilities
 * library.
 */
PRIVATE const char* MobiusThread::getQualifiedPath(const char* base,
												   const char* extension,
												   int* counter)
{
	int qualifier = 1;

	if (counter != NULL)
	  qualifier = *counter;

	while (true) {
		char qualfile[1024];
		sprintf(qualfile, "%s%d%s", base, qualifier++, extension);
		// note that if the qualfile is itself an absolute path, 
		// the home directory will be ignored
		MergePaths(getHomeDirectory(), qualfile, mPathBuffer, sizeof(mPathBuffer));
		
		if (!IsFile(mPathBuffer))
		  break;
	}

	if (counter != NULL)
	  *counter = qualifier;

	return mPathBuffer;
}

/**
 * Diff two files.  Assume they are binary.
 * Could be smarter about printing differences if these turn
 * out to be non-binary project files.
 */
PRIVATE void MobiusThread::diff(int type, bool reverse,
								const char* file1, const char* file2)
{
	int size1 = GetFileSize(file1);
	int size2 = GetFileSize(file2);

	if (size1 < 0) {
		printf("ERROR: File does not exist: %s\n", file1);
		fflush(stdout);
	}
	else if (size2 < 0) {
		printf("ERROR: File does not exist: %s\n", file2);
		fflush(stdout);
	}
	else if (size1 != size2) {
		printf("ERROR: Files differ in size: %s, %s\n", file1, file2);
		fflush(stdout);
	}
	else {
		FILE* fp1 = fopen(file1, "rb");
		if (fp1 == NULL) {
			printf("Unable to open file: %s\n", file1);
			fflush(stdout);
		}
		else {
			FILE* fp2 = fopen(file2, "rb");
			if (fp2 == NULL) {
				printf("Unable to open file: %s\n", file2);
				fflush(stdout);
			}
			else {
				bool different = false;
				if (type == TE_DIFF_AUDIO) {

					bool checkFloats = false;
                    AudioPool* pool = mMobius->getAudioPool();

					Audio* a1 = pool->newAudio(file1);
					Audio* a2 = pool->newAudio(file2);
					int channels = a1->getChannels();

					if (a1->getFrames() != a2->getFrames()) {
						printf("Frame counts differ %s, %s\n", file1, file2);
						fflush(stdout);
					}
					else if (channels != a2->getChannels()) {
						printf("Channel counts differ %s, %s\n", file1, file2);
						fflush(stdout);
					}
					else {
						AudioBuffer b1;
						float f1[AUDIO_MAX_CHANNELS];
						b1.buffer = f1;
						b1.frames = 1;
						b1.channels = channels;

						AudioBuffer b2;
						float f2[AUDIO_MAX_CHANNELS];
						b2.buffer = f2;
						b2.frames = 1;
						b2.channels = channels;

						bool stop = false;
						int psn2 = (reverse) ? a2->getFrames() - 1 : 0;

						for (int i = 0 ; i < a1->getFrames() && !different ; 
							 i++) {

							memset(f1, 0, sizeof(f1));
							memset(f2, 0, sizeof(f2));
							a1->get(&b1, i);
							a2->get(&b2, psn2);
			
							for (int j = 0 ; j < channels && !different ; j++) {
								// sigh, due to rounding errors it is 
								// impossible to reliably assume that
								// x + y - y = x with floats, so coerce
								// to an integer to do the comparion

								if (checkFloats) {
									if (f1[j] != f2[j]) {
										printf("WARNING: files differ at frame %d: %f %f: %s, %s\n", 
											   i, f1[j], f2[j], file1, file2);
										fflush(stdout);
									}
								}

								// 24 bit is too much, but 16 is too small
								// 16 bit signed (2^15)
								//float precision = 32767.0f;
								// 24 bit signed (2^23)
								//float precision = 8388608.0f;
								// 20 bit
								float precision = 524288.0f;
								
								int i1 = (int)(f1[j] * precision);
								int i2 = (int)(f2[j] * precision);

								if (i1 != i2) {
									printf("Files differ at frame %d: %d %d: %s, %s\n", 
										   i, i1, i2, file1, file2);
									fflush(stdout);
									different = true;
								}
							}

							if (reverse)
							  psn2--;
							else
							  psn2++;
						}
					}

					delete a1;
					delete a2;
				}
				else {
					unsigned char byte1;
					unsigned char byte2;

					for (int i = 0 ; i < size1 && !different ; i++) {
						if (fread(&byte1, 1, 1, fp1) < 1) {
							printf("Unable to read file: %s\n", file1);
							fflush(stdout);
							different = true;
						}
						else if (fread(&byte2, 1, 1, fp2) < 1) {
							printf("Unable to read file: %s\n", file2);
							fflush(stdout);
							different = true;
						}
						else if (byte1 != byte2) {
							printf("Files differ at byte %d: %s, %s\n",
								   i, file1, file2);
							fflush(stdout);
							different = true;
						}
					}
				}
				fclose(fp2);
				if (!different) {
				  printf("%s - ok\n", file1);
				  fflush(stdout);
				}
			}
			fclose(fp1);
		}
	}

	fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *   								ALERTS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Create a Prompt object containing the message we want to display
 * and send it to the listener for processing.   
 *
 * The listener is considered the owner of the Prompt and must call
 * Mobius::finishPrompt when it is done.  The Prompt contains a 
 * ThreadEvent the script interpreter may be waiting on.  The listener
 * *must not* delete the Prompt which will also delete the ThreadEvent
 * out from under the interpreter.  It *must* call Mobius::finishPrompt.
 *
 */
PRIVATE void MobiusThread::prompt(ThreadEvent* e)
{
	MobiusListener* l = mMobius->getListener();
	if (l != NULL) {
		Prompt* p = new Prompt();
		// keep a counter for sanity checks
		mPrompts++;

		p->setEvent(e);
		p->setText(e->getArg(0));

		l->MobiusPrompt(p);
	}
}

/**
 * Called by Mobius when it gets a prompt back from the listener.
 */
PUBLIC void MobiusThread::finishPrompt(Prompt* p)
{
	if (mPrompts == 0)
	  Trace(1, "Unbalanced call to finishPrompt!\n");
	else
	  mPrompts--;

	// we saved the event in the prompt, complete it now
	ThreadEvent* e = p->getEvent();
	if (e != NULL) {
        ThreadEventType type = e->getType();
		if (type != TE_NONE) {

			// This is one of the few (only) events with a return code
			// it is used to convey the prompt button selection into
			// the ScriptInterpter that is waiting for this event.
			// Since a single "Ok" button is the simplest case, we'll
			// use 0 to mean normal completion, 1 to mean cancel
			int code = (p->isOk()) ? 0 : 1;
			e->setReturnCode(code);

            finishEvent(e);
		}
	}

	// event will be deleted by the Prompt
	delete p;
}

/**
 * When we're done processing an event, send it back to Mobius
 * so it can notify any ScriptInterpreters that might be waiting on it.
 *
 * This creates an action with a special trigger and target so it can
 * be deferred until the next interrupt.
 */
PRIVATE void MobiusThread::finishEvent(ThreadEvent* e)
{
    Action* a = mMobius->newAction();
    a->trigger = TriggerThread;
    a->setTarget(TargetScript);

    // this is a little unusual because we use this
    // for an input to the action and it's usually a return
    a->setThreadEvent(e);

    mMobius->doAction(a);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
