/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Event management tools for tracks.
 *
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "Preset.h"

/**
 * A class encapsulating Event management code for a Track.
 */
class EventManager {

  public:

    EventManager(class Track* t);
    ~EventManager();

    // allocation

    Event* newEvent();
    Event* newEvent(EventType* type, long frame);
    Event* newEvent(Function* f, long frame);
    Event* newEvent(Function* f, EventType* type, long frame);

    // Misc control and status

    void reset();
    void resetLastSyncEventFrame();
    long getLastSyncEventFrame();
    void setLastSyncEventFrame(long frame);
    Event* getEvents();
    bool hasEvents();
    Event* getSwitchEvent();
    void setSwitchEvent(Event* e);
    bool isSwitching();
    bool isSwitchConfirmed();
    Event* findEvent(long frame);
    Event* findEvent(EventType* type);
    Event* findEvent(class Function* func);
    bool isValidationSuppressed(Event* finished);
    EventList* stealEvents();

    // General Scheduling

    void flushEventsExceptScripts();
    
    void addEvent(Event* event);
    bool isEventScheduled(Event* e) ;
    void removeEvent(Event* e);
    void removeScriptReferences(class ScriptInterpreter* si);
    Event* getFunctionEvent(Action* action, Loop* loop, Function* func);

    // Adjustments

    void shiftEvents(long frames);
    void reorderEvent(Event* e);
    void advanceScriptWaits(long frames);
    void loopSwitchScriptWaits(Loop* current, long nextFrame);
    void moveEventHierarchy(Loop* loop, Event* e, long newFrame);
    void moveEvent(Loop* loop, Event* e, long newFrame);
    void reverseEvents(long originalFrame, long newFrame);

    // Undo

    void freeEvent(Event* event);
    bool undoLastEvent();
    void undoEvent(Event* event);

    // Switch Scheduling

    void scheduleSwitchStack(Event* event);
    Event* getUncomittedSwitch();
    bool undoSwitchStack();
    void cancelSwitchStack(Event* e);
    void switchEventUndo(Event* e);
    void cancelSwitch();

    // Play Jump Scheduling

    void getEffectiveLatencies(Loop* loop, Event* parent, long frame, 
                               int* retInput, int* retOutput);

    Event* schedulePlayJump(Loop* loop, Event* parent);
    Event* schedulePlayJumpType(Loop* loop, Event* parent, EventType* type);
    Event* schedulePlayJumpAt(Loop* loop, Event* parent, long frame);

    // Return Scheduling

    Event* scheduleReturnEvent(Loop* loop, Event* trigger, 
                               Loop* prev, bool sustain);
    void finishReturnEvent(Loop* loop);
    void returnEventUndo(Event* e);
    bool cancelReturn();
    void cleanReturnEvents();

    // Summary

    void getEventSummary(class LoopState* s);

    // Selection

	Event* getNextEvent();

    // Processing

    void processEvent(Event* e);

    // Utilities

    long getQuantizedFrame(Loop* loop, long frame, Preset::QuantizeMode q, 
                           bool after);

    long getPrevQuantizedFrame(Loop* loop, long frame, Preset::QuantizeMode q,
                               bool before);

    long wrapFrame(long frame, long loopFrames);

  private:

    void flushAllEvents();
    void free(Event* event, bool flush);
    void release(Event* e);
    void releaseAll(Event* e);
    Event* removeUndoEvent();
    void removeAll(Event* e);
    void undoAndFree(Event* e);
    void undoProcessedEvents(Event* event);
    long reverseFrame(long origin, long newOrigin, long frame);
    void finishReturnEvent(Loop* loop, Event* re);

    void getEventSummary(class LoopState* s, Event* e, bool stacked);
    bool isEventVisible(Event* e, bool stacked);
    long reflectFrame(Loop* loop, long frame);

    Event* getNextScheduledEvent(int availFrames, Event* syncEvent);

    void rescheduleEvents(Loop* loop, Event* previous);
    Event* getRescheduleEvents(Loop* loop, Event* previous);

    // the track that owns us
    class Track* mTrack;

    // the event list
    class EventList* mEvents;

    // a pending switch "stacking" event
    class Event* mSwitch;

    // special sync event we can inject
	Event* mSyncEvent;
	long mLastSyncEventFrame;

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
