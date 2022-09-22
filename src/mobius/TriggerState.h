/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A small utility class used to monitor the up/down transitions
 * of a trigger in order to detect "long presses".
 *
 */

#ifndef TRIGGER_STATE_H
#define TRIGGER_STATE_H

/**
 * Utility class used to detect when a trigger is held down long enough
 * to cause "long press" behavior.
 *
 * Currently we maintain one of these in each track, this allows multiple
 * controllers to be sending function down/up transitions to different tracks
 * at the same time.  But within one track we only allow one function to be
 * considered down at a time.  If we get another down transition before 
 * receiving an up transition, the previous long press is canceled.
 *
 * We could allow multiple long presses in each track but this would require
 * a dynamic list which in practice I think is overkill.
 */
class TriggerWatcher {

  public:

    TriggerWatcher();
    ~TriggerWatcher();

    void init(Action* a);

    /** 
     * Link for the pool or active list.
     */
    TriggerWatcher* next;

    /**
     * The trigger that went down.
     */
    Trigger* trigger;

    /**
     * The unique id of the trigger.
     */
    int triggerId;

    /**
     * The function that is being held down.
     */
    Function* function;

    /**
     * Target track (zero for current).
     */
    int track;

    /**
     * Target group.
     */
    int group;

    /**
     * The time in frames this function has been held down.
     */
    int frames;

    /**
     * Set true if we decide this was a long press.
     * This is used on the up transition to adjust how the function
     * ends.
     */
    bool longPress;
    

  private:

    void init();

};

/**
 * A collection of TriggerWatchers.
 *
 * This maintains a list of sustaining triggers.  There is a maximum
 * number of triggers we will track, if this limit is exceeded we stop
 * tracking the oldest triggers.  This is to prevent watcher explosion
 * if for example you have a misconfigured MIDI footswtitch that sends
 * MIDI note on but never note off.
 *
 * In practice there will be a small number uf sustaining triggers, usuallyu
 * only one.  If there are multiple performers controlling different tracks
 * there may be one sustaining trigger per track.  In rare situations
 * there may be more than one sustaining trigger per track, but usually
 * they cancel each other.  Some that could be supported are SUSOverdub
 * combined with SUSReverse.
 * 
 */
class TriggerState {

  public:

    TriggerState();
    ~TriggerState();

    void setLongPressFrames(int frames);
    void setLongPressTime(int msecs, int sampleRate);

    void assimilate(Action* action);
    void advance(Mobius* mobius, int frames);

  private:
    
    TriggerWatcher* remove(Action* action);

    TriggerWatcher* mPool;
    TriggerWatcher* mWatchers;
    TriggerWatcher* mLastWatcher;

    int mLongPressFrames;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
