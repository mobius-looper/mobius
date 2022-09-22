/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Speed shift.
 *
 * In 2.2 we merged the former "rate shift" functions with the "half speed"
 * functions into a more general set of speed functions and parameters.
 * We don't really need the half speed functions but EDP users expect them
 * so they're kept for backward compatibility.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "MidiByte.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "FunctionUtil.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Mode.h"
#include "Resampler.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// Minor Modes
//
//////////////////////////////////////////////////////////////////////

/**
 * Minor mode when a speed octave is active.
 */
class SpeedOctaveModeType : public MobiusMode {
  public:
	SpeedOctaveModeType();
};

SpeedOctaveModeType::SpeedOctaveModeType() :
    MobiusMode("speedOctave", "Speed Octave")
{
	minor = true;
}

MobiusMode* SpeedOctaveMode = new SpeedOctaveModeType();

/**
 * Minor mode when a speed step is active.
 */
class SpeedStepModeType : public MobiusMode {
  public:
	SpeedStepModeType();
};

SpeedStepModeType::SpeedStepModeType() :
    MobiusMode("speedStep", "Speed Step")
{
	minor = true;
}

MobiusMode* SpeedStepMode = new SpeedStepModeType();

/**
 * Minor mode when a speed bend is active.
 */
class SpeedBendModeType : public MobiusMode {
  public:
	SpeedBendModeType();
};

SpeedBendModeType::SpeedBendModeType() :
    MobiusMode("speedBend", "Speed Bend")
{
	minor = true;
}

MobiusMode* SpeedBendMode = new SpeedBendModeType();

/**
 * Minor mode when a speed toggle is active.
 */
class SpeedToggleModeType : public MobiusMode {
  public:
	SpeedToggleModeType();
};

SpeedToggleModeType::SpeedToggleModeType() :
    MobiusMode("speedToggle", "Speed Toggle")
{
	minor = true;
}

MobiusMode* SpeedToggleMode = new SpeedToggleModeType();

/**
 * Minor mode when a time stretch is active.
 */
class TimeStretchModeType : public MobiusMode {
  public:
	TimeStretchModeType();
};

TimeStretchModeType::TimeStretchModeType() :
    MobiusMode("timeStretch", "Time Stretch")
{
	minor = true;
}

MobiusMode* TimeStretchMode = new TimeStretchModeType();

//////////////////////////////////////////////////////////////////////
//
// SpeedEvent
//
//////////////////////////////////////////////////////////////////////

class SpeedEventType : public EventType {
  public:
	SpeedEventType();
};

PUBLIC SpeedEventType::SpeedEventType()
{
	name = "Speed";
    // !! had to do this when we could have overlapping
    // Speed and RateShift events, do we still need it?
	reschedules = true;
}

PUBLIC EventType* SpeedEvent = new SpeedEventType();

//////////////////////////////////////////////////////////////////////
//
// SpeedFunctionType
//
//////////////////////////////////////////////////////////////////////

/**
 * We've got a bunch of speed functions that all behave similarly
 * except for how they calculate the next speed.  This enumeration
 * is used to set an internal type code we use to select behavior.
 * Easier than having a boatload of subclasses or boolean options.
 *
 * The first set used to be the "rate" functions prior to 2.2.
 * The second set used to be the "half speed" functions.  Specific
 * functions for going in and out of halfspeed is a holdover
 * from the EDP that we don't need now that we have more general
 * speed control but people expect it so it is built in rather
 * than having to use binding arguments.
 *
 * Binding arguments: (these don't work yet)
 *
 *    sustain - make the change temporary 
 *    toggle  - make the change temporary and
 *              revert on next press
 *    octave  - change in octave up or down
 *    step    - change in steps up or down
 *    bend    - change in bend up or down
 *
 * With those we can accomplish all speed behaviors
 * with a single function.
 *
 */
typedef enum {

	SPEED_CANCEL,        // cancel all speed changes
	SPEED_OCTAVE,        // octave steps, not spread
	SPEED_STEP,          // chromatic steps, spread, or arguments
	SPEED_BEND,          // continuous bend degree
	SPEED_UP,            // up one step
	SPEED_DOWN,          // down one step
	SPEED_NEXT,          // next speed in sequence
	SPEED_PREV,          // previous speed in sequence
    SPEED_TOGGLE,        // toggle a semitone step, default -12
    SPEED_SUS_TOGGLE,    // sustained step toggle

    SPEED_STRETCH,       // SPEED_BEND combined with pitch bend 

    // legacy halfspeed function
    SPEED_HALF,          // one octave down, not toggled

    SPEED_RESTORE

} SpeedFunctionType;

//////////////////////////////////////////////////////////////////////
//
// SpeedChange
//
//////////////////////////////////////////////////////////////////////

/**
 * Enumeration of the possible change units for speed.
 */
typedef enum {

    SPEED_UNIT_OCTAVE,
    SPEED_UNIT_STEP,
    SPEED_UNIT_BEND,
    SPEED_UNIT_STRETCH,
    SPEED_UNIT_TOGGLE

} SpeedUnit;

/**
 * Little structure used to assist in calculating speed changes.
 * 
 * There are two parts to this, the first part contains values
 * copied from an Action or an Event.  It represents one type
 * of speed change.
 *
 * The second section represents the current state of the stream
 * or the new state to be applied.
 */
typedef struct {

    //
    // This part is calculated from an Action or Event
    //

    // true if there is no change
    bool ignore;

    // the unit of change
    SpeedUnit unit;

    // amount of change in positive or negative steps
    int value;

    // 
    // This part is calculated from the desired change
    // above combined with current stream state
    //

    int newToggle;
    int newOctave;
    int newStep;
    int newBend;
    int newStretch;

} SpeedChange;

//////////////////////////////////////////////////////////////////////
//
// SpeedFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * SpeedStep triggers may be spread over a range of keys.
 * Should we have two variants one that restarts and one that doesn't
 * or make restart be a preset parameter?  Currently it is a parameter.
 *
 * !! Need to overload invoke() so we can add some semantics.
 * When bound to a keyboard, it is common to get rapid triggers before
 * we've finished processing the last one.  Rather than the usual
 * "escape quantization" semantics, we need to find the previous event
 * and change the step amount.
 */
class SpeedFunction : public Function {
  public:
	SpeedFunction(SpeedFunctionType type);
	bool isRecordable(Preset* p);
    Event* invoke(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* scheduleSwitchStack(Action* action, Loop* l);
    Event* scheduleTransfer(Loop* l);
    void prepareJump(Loop* l, Event* e, JumpContext* next);
    void doEvent(Loop* l, Event* e);
  private:
	void convertAction(Action* action, Loop* l, SpeedChange* change);
    void convertEvent(Event* e, SpeedChange* change);
    bool isIneffective(Action* a, Loop* l, SpeedChange* change);
    void annotateEvent(Event* event, SpeedChange* change);
    void applySpeedChange(Loop* l, SpeedChange* change, bool both);
    void applySpeedChange(SpeedChange* change, Stream* s);
    void calculateNewSpeed(SpeedChange* change);
	SpeedFunctionType mType;
    bool mCanRestart;
};

PUBLIC Function* SpeedCancel = new SpeedFunction(SPEED_CANCEL);
PUBLIC Function* SpeedOctave = new SpeedFunction(SPEED_OCTAVE);
PUBLIC Function* SpeedStep = new SpeedFunction(SPEED_STEP);
PUBLIC Function* SpeedBend = new SpeedFunction(SPEED_BEND);
PUBLIC Function* SpeedUp = new SpeedFunction(SPEED_UP);
PUBLIC Function* SpeedDown = new SpeedFunction(SPEED_DOWN);
PUBLIC Function* SpeedNext = new SpeedFunction(SPEED_NEXT);
PUBLIC Function* SpeedPrev = new SpeedFunction(SPEED_PREV);
PUBLIC Function* SpeedToggle = new SpeedFunction(SPEED_TOGGLE);
PUBLIC Function* SUSSpeedToggle = new SpeedFunction(SPEED_SUS_TOGGLE);
PUBLIC Function* Halfspeed = new SpeedFunction(SPEED_HALF);
PUBLIC Function* SpeedRestore = new SpeedFunction(SPEED_RESTORE);
PUBLIC Function* TimeStretch = new SpeedFunction(SPEED_STRETCH);

PUBLIC SpeedFunction::SpeedFunction(SpeedFunctionType type)
{
	mType = type;

	eventType = SpeedEvent;
	minorMode = true;
	mayCancelMute = true;
	resetEnabled = true;
	thresholdEnabled = true;
	switchStack = true;
	cancelReturn = true;

    // Does quantization ever make sense for the "bend" functions?
    // These will almost always be assigned to a CC or PB, quantizing
    // these is just confusing.  If you really need quant you can
    // use scripts.  Put SPEED_OCTAVE in here too since it is only
    // accessible from a control.
    if (type != SPEED_STRETCH && type != SPEED_BEND && type != SPEED_OCTAVE) {
        quantized = true;
        quantizeStack = true;
    }
    else {
        // I don't think these should be stackable, it's possible
        // but since you can't hear it why?
        switchStack = false;
        // what about cancelReturn?
    }

    // Originally only SpeedStep would obey SpeedShiftRestart but
    // it seels like the others should too.  Do NOT include
    // what used to be the HalfStep functions, that isn't expected.
    // Also don't do this for toggles.  Could have more options 
    // here but you can always use scripts.
    mCanRestart = false;

	switch (mType) {
		case SPEED_CANCEL:
			setName("SpeedCancel");
            alias1 = "RateNormal";
            alias2 = "Fullspeed";
			setKey(MSG_FUNC_SPEED_CANCEL);
            mCanRestart = true;
			break;

        // these are access as parameters or controls
		case SPEED_OCTAVE:
			setName("SpeedOctave");
			setKey(MSG_PARAM_SPEED_OCTAVE);
            // keep this out of the binding list, we'll get here via
            // the Parameter
            scriptOnly = true;
			break;
		case SPEED_STEP:
			setName("SpeedStep");
            alias1 = "RateShift";
			setKey(MSG_PARAM_SPEED_STEP);
			spread = true;
            mCanRestart = true;
            // Since these can be "played" rapidly keep them out of 
            // trace.  Should we disable quantization too?
            silent = true;
			break;
		case SPEED_BEND:
            // could be spread but this is intended more for CC bindings
			setName("SpeedBend");
			setKey(MSG_PARAM_SPEED_BEND);
            // keep this out of the binding list, we'll get here via
            // the Parameter
            scriptOnly = true;
            silent = true;
			break;
		case SPEED_STRETCH:
			setName("TimeStretch");
			setKey(MSG_PARAM_TIME_STRETCH);
            // keep this out of the binding list, we'll get here via
            // the Parameter
            scriptOnly = true;
            silent = true;
			break;

        // these are access as functions
		case SPEED_NEXT:
			setName("SpeedNext");
			alias1 = "RateNext";
			setKey(MSG_FUNC_SPEED_NEXT);
            mCanRestart = true;
			break;
		case SPEED_PREV:
			setName("SpeedPrev");
			alias1 = "RatePrev";
			setKey(MSG_FUNC_SPEED_PREV);
            mCanRestart = true;
			break;
		case SPEED_UP:
			setName("SpeedUp");
			alias1 = "RateUp";
			setKey(MSG_FUNC_SPEED_UP);
            mCanRestart = true;
			break;
		case SPEED_DOWN:
			setName("SpeedDown");
			alias1 = "RateDown";
			setKey(MSG_FUNC_SPEED_DOWN);
            mCanRestart = true;
			break;
		case SPEED_TOGGLE:
			setName("SpeedToggle");
			setKey(MSG_FUNC_SPEED_TOGGLE);
            alias1 = "Speed";
            longFunction = SUSSpeedToggle;
            maySustain = true;
            mayConfirm = true;
			break;
        case SPEED_SUS_TOGGLE:
            sustain = true;
            setName("SUSSpeedToggle");
            alias1 = "SUSSpeed";
            setKey(MSG_FUNC_SPEED_SUS_TOGGLE);
            break;
        case SPEED_HALF:
			setName("Halfspeed");
			setKey(MSG_FUNC_SPEED_HALF);
            break;
        case SPEED_RESTORE:
            setName("SpeedRestore");
            // not really for scripts either, but this keeps it out
            // of the binding list
            scriptOnly = true;
            break;
	}
}

/**
 * This is true if the function is can be used during recording.
 * Since we didn't do this originally, it is controlled by a preset parameter.
 */
PUBLIC bool SpeedFunction::isRecordable(Preset* p)
{
	return p->isSpeedRecord();
}

/**
 * Calculate the speed changes that will be done by this function.
 * 
 * Note that this will advance the speed sequence even if we end
 * up undoing the event.  May want to revisit that...
 *
 * !! It is unreliable to compare the current speed with the new speed?
 * There can be events in the queue that would change the speed by the
 * time the one we're about to schedule is reached.  Technically we
 * should examing the pending events and calculate the effective speed.
 * Or we could defer SPEED_UP and SPEED_DOWN and evaluate them when the 
 * event is evaluated.  Nice to see the number in the event though.
 * Instead the Event could be annotated with the former sequence index
 * so we restore the index.
 *
 * RANGE CONSTRAINTS
 *
 * There are a number of parameters that can influence the bounds of
 * these functions.  
 *
 *    spreadRange 
 *      - Global parameter used for MIDI note and program bindings
 *        for SpeedStep and PitchStep.  This spreads the binding
 *        over a range of notes and effectively limits the step range.
 *
 *    speedStepRange
 *      - Preset parameter used for MIDI CC and PITCHBEND bindings
 *        to limit SpeedStep.  Also used for Host and OSC bindings.
 *        The action will be created using the default parameter
 *        range of -48  to 48.  We'll ignore that and rescale the
 *        original value.
 *
 *    speedBendRange
 *      - Preset parameter used for MIDI CC and PITCHBEND bindings
 *        to limit SpeedBend.  Also used for Host and OSC bindings.
 *        Like speedStepRange this will ignore the action value
 *        and rescale the original value.
 *
 *    timeStretchRange
 *      - Like speedBendRange but for the SPEED_STRETCH function.
 *
 */
PRIVATE void SpeedFunction::convertAction(Action* action, Loop* l, SpeedChange* change)
{
    InputStream* istream = l->getInputStream();

    // If we end up with a SPEED_UNIT_STEP change,
    // these are usually constrained by the global
    // parameter spreadRange.  There are some special
    // cases though...
    bool checkSpreadRange = true;

    // set up the defaults
    change->ignore = false;
    change->unit = SPEED_UNIT_STEP;
    change->value = 0;

    if (mType == SPEED_CANCEL) {
        // Pre 2.2 this canceled RateShift but left Halfspeed
        // In 2.2 this cancels all speed effects which
        // shouldn't be a problem except maybe for those
        // those that think of Halfspeed as an independent mode.
        // See who screams...

        // We don't have a way to convey a reset of both
        // octave and step in SpeedChange, just reset
        // the step and handle the octave in the event handler
        change->value = 0;
    }
    else if (mType == SPEED_OCTAVE) {
        int value = action->arg.getInt();
        if (value >= -MAX_RATE_OCTAVE && value <= MAX_RATE_OCTAVE) {
            change->unit = SPEED_UNIT_OCTAVE;
            change->value = value;
        }
        else {
            // should have limited this by now
            Trace(l, 1, "SpeedOctave value out of range %ld\n", (long)value);
            change->ignore = true;
        }
    }
	else if (mType == SPEED_STEP) {
        // Before the rate/speed merger this was "RateShift"
        // which could be both a ranged function and used
        // with an argument in scripts.  arg.getInt figures it out
        change->value = action->arg.getInt();

        // support rescaling for some triggers
        Preset* p = l->getPreset();
        int scaledRange = p->getSpeedStepRange();

        if (RescaleActionValue(action, l, scaledRange, false, &change->value))
          checkSpreadRange = false;
	}
	else if (mType == SPEED_BEND || mType == SPEED_STRETCH) {

        change->value = action->arg.getInt();

        Preset* p = l->getPreset();
        int scaledRange;

        if (mType == SPEED_BEND) {
            change->unit = SPEED_UNIT_BEND;
            scaledRange = p->getSpeedBendRange();
        }
        else {
            change->unit = SPEED_UNIT_STRETCH;
            scaledRange = p->getTimeStretchRange();
        }

        RescaleActionValue(action, l, scaledRange, true, &change->value);
    }
    else if (mType == SPEED_UP || mType == SPEED_DOWN) {

        // can be used in scripts with an argument
        // should also allow binding args!!
		int increment = 1;
		if (action->arg.getType() == EX_INT) {
			int ival = action->arg.getInt();
			if (ival != 0)
			  increment = ival;
		}

        int current = istream->getSpeedStep();

        if (mType == SPEED_UP)
          change->value = current + increment;
        else
          change->value = current - increment;

        // will check spread range below
    }
    else if (mType == SPEED_NEXT || mType == SPEED_PREV) {

        Track* t = l->getTrack();
        int step = t->getSpeedSequenceIndex();
        Preset* p = l->getPreset();
        StepSequence* seq = p->getSpeedSequence();
        bool next = (mType == SPEED_NEXT);

        // stay here if we have no sequence
        int speed = istream->getSpeedStep();

        // !! If the event is undone we will still have
        // advanced the sequence.  It feels like we should
        // defer advancing until the event is processed,
        // but this would not allow the scheduling
        // of successive quantized events with different
        // steps.
        step = seq->advance(step, next, speed, &speed);
        t->setSpeedSequenceIndex(step);

        change->value = speed;
        // will check spread range below
    }
    else if (mType == SPEED_TOGGLE || mType == SPEED_SUS_TOGGLE) {
        // An argument may be used to specify the step, the default is -12
        change->unit = SPEED_UNIT_TOGGLE;
        
        if (action->arg.getType() == EX_INT)
          change->value = action->arg.getInt();

        if (change->value == 0)
          change->value = -12;
    }
    else if (mType == SPEED_HALF) {
        // Here is where the new toggle concept doesn't quite work.
        // We're doing a non toggling move to -12, if you then press
        // ToggleHalfspeed it will go down another -12 whereas before 2.2
        // it would have gone back up to -12.  Try to catch this
        // in the event handler.
        change->value = -12;
    }
    else {
        Trace(l, 1, "SpeedFunction: Unknown speed type!\n");
        change->ignore = true;
    }

    if (!change->ignore && 
        change->unit == SPEED_UNIT_STEP
        && checkSpreadRange) {
        
		int max = l->getMobius()->getConfiguration()->getSpreadRange();
		int min = -max;
		if (change->value < min)
		  change->value = min;
		if (change->value > max)
		  change->value = max;
    }
}

/**
 * Invocation intercept.  Usually we just forward this to the
 * default logic in Function but we have a few special cases.
 *
 * If we don't have a quantized action then this must be one of the
 * speed functions bound to a controller.  If we
 * find a previous event, modify it rather than warn about things
 * "comming in too fast".
 *
 * Hmm, actually checking quant isn't entirely accurate since quantization
 * may just be turned off.  It 
 */
PUBLIC Event* SpeedFunction::invoke(Action* action, Loop* loop)
{
	Event* event = NULL;
    bool standard = true;

    // Octave, bend and stretch always unquantized controls
    // step may be a function or a control.
    if (mType == SPEED_OCTAVE ||
        mType == SPEED_STEP ||
        mType == SPEED_BEND ||
        mType == SPEED_STRETCH) {

        // look for a specific function rather than EventType
        // since we have subtypes
        EventManager* em = loop->getTrack()->getEventManager();
        Event* prev = em->findEvent(this);
        if (prev != NULL && !prev->quantized) {
            Event* jump = prev->findEvent(JumpPlayEvent);
            if (jump == NULL || !jump->processed) {
                
                SpeedChange change;
                convertAction(action, loop, &change);
                if (!change.ignore) {
                    // since we searched by Function we shouldn't
                    // need to check the unit
                    if (prev->fields.speed.unit == change.unit) {
                        prev->number = change.value;
                        standard = false;
                    }
                }
            }
        }
    }

    if (standard)
      event = Function::invoke(action, loop);

	return event;
}

/**
 * Schedule a speed change.
 *
 * If we are in a pre-recorded mode, apply the change immediately
 * so it has an effect on the next recording.
 *
 * If there is a quantized event that hasn't been started yet, 
 * modify it rather than scheduling another one for the next
 * quantization boundary.  
 *
 */
PUBLIC Event* SpeedFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    MobiusMode* mode = l->getMode();
    SpeedChange change;

    convertAction(action, l, &change);

    if (change.ignore || isIneffective(action, l, &change)) {
        // there is effectively no change, ignore it
        if (!change.ignore)
          Trace(l, 3, "Ignoring ineffective speed change\n");
    }
    else if (mode == ResetMode || 
             mode == ThresholdMode || 
             mode == SynchronizeMode) {

        // Apply immediately
        // Pre 2.2 Speed functions used !l->isAdvancing but that
        // was also true for RunMode and mPause.  Once we get into
        // RunMode we have to schedule.  And Pause is such a mess
        // but since speed changes effect latencies we have to 
        // adjust other events, so go through scheduling.
        applySpeedChange(l, &change, true);

        // This changes effective latency so also adjust the 
        // pre-recording start frame.  Originally did this only
        // for ResetMode but I think it applies to anything that
        // makes the loop not advance.
        InputStream* istream = l->getInputStream();
        l->setFrame(-(istream->latency));
    }
    else {
        EventManager* em = l->getTrack()->getEventManager();
        bool prevModified = false;

        // !! Inconsistency
        // We have historically tried to modify previously scheduled
        // events for the former "rate" functions (normal, up, down, 
        // next, prev, shift) but the former "halfspeed" functions
        // (SUSSpeed, Speed, SpeedCancel, Halfspeed) would be scheduled
        // on successive quantization boundaries.  We're going to continue
        // this since I think people expect it, but if octave toggle
        // obeys quantization then all the speed step functions should too?
        
        // this is what we should do
        // if (!quantized) {
        // this is compatible with 2.1
        if (mType != SPEED_HALF && 
            mType != SPEED_TOGGLE &&
            mType != SPEED_SUS_TOGGLE) {
            
            Event* prev = em->findEvent(this);
            if (prev != NULL) {
                Event* jump = prev->findEvent(JumpPlayEvent);
                if (jump == NULL || !jump->processed) {
                    // they must both be of the same toggle type
                    if (prev->fields.speed.unit == change.unit) {
                        prev->number = change.value;
                        prevModified = true;
                    }
                }
            }
        }

        if (!prevModified) {
            event = Function::scheduleEvent(action, l);
            if (event != NULL) {
                annotateEvent(event, &change);

                if (!event->reschedule)
                  em->schedulePlayJumpAt(l, event, event->frame);
            }
        }
    }

	return event;
}

/**
 * Annotate the event after scheduling.
 */ 
PRIVATE void SpeedFunction::annotateEvent(Event* event, SpeedChange* change)
{

    // transfer the change to the Event
    event->number = change->value;
    event->fields.speed.unit = change->unit;

    // The UI will by default show just the 
    // base Event name "Speed" with the event number.
    // For some of the functions we can be more informative.
    if (mType == SPEED_CANCEL)
      event->setInfo("Cancel");

    else if (mType == SPEED_HALF)
      event->setInfo("Half");

    else if (change->unit == SPEED_UNIT_TOGGLE)
      event->setInfo("Toggle");
}
            
/**
 * Check to see if it makes any sense to schedule an event
 * for this speed change.
 * 
 * If the current input stream speed is the same as what this
 * function would do, then try to ignore the function.  This
 * is usually what you want, especially when quantizing and you
 * can see the event.  But since we now allow unquantized 
 * speed changes to come in through the parameter the event
 * may not be meaningful now, but it could be by the time
 * it is reached.  
 * 
 * NOTE: This is a behavior change from 2.1.  Beforea if the event
 * was quantized we would schedule another one for the next
 * quantization boundary.  This is expected of half speed toggle, 
 * but we we're doing it for 
 */
PRIVATE bool SpeedFunction::isIneffective(Action* a, Loop* l, SpeedChange* change)
{
    bool ineffective = false;

    // Toggles always change
    // SPEED_STEP is always effective if restart is enabled
    // SPEED_NORMAL does more than just the step so always do it

    if (change->unit != SPEED_UNIT_TOGGLE && 
        mType != SPEED_CANCEL &&
        (!mCanRestart || !l->getPreset()->isSpeedShiftRestart())) {

        Stream* istream = l->getInputStream();

        if ((mType == SPEED_BEND && 
             istream->getSpeedBend() == change->value) ||

            (mType == SPEED_STRETCH &&
             istream->getTimeStretch() == change->value) ||

            (mType != SPEED_BEND && 
             mType != SPEED_STRETCH &&
             mType != SPEED_OCTAVE &&
             istream->getSpeedStep() == change->value)) {

            // the delemma...experiment with this and decide what to do
            ineffective = true;
        }
    }

    return ineffective;
}

/**
 * Add or replace a speed change function stacked under
 * a loop switch.  
 * 
 * Toggle events need to cancel each other.  
 * Absolutel change events need to modify previous events.
 *
 */
PUBLIC Event* SpeedFunction::scheduleSwitchStack(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	if (action->down && switchStack) {
		Event* switche = em->getUncomittedSwitch();
		if (switche != NULL) {

            bool schedule = true;
            SpeedChange change;
            convertAction(action, l, &change);

            Event* next = NULL;
            for (Event* e = switche->getChildren() ; e != NULL ; e = next) {
                next = e->getSibling();

                if (e->type == eventType &&
                    e->fields.speed.unit == change.unit) {

                    if (change.unit == SPEED_UNIT_TOGGLE) {
                        if (e->number == change.value) {
                            // if the numbers are the same they cancel
                            em->cancelSwitchStack(e);
                            schedule = false;
                        }
                        else {
                            // change the number
                            e->number = change.value;
                            schedule = false;
                        }
                    }
                    else {
                        // change the number, should only have one
                        e->number = change.value;
                        schedule = false;
                    }
                }
            }

            if (schedule) {
				event = em->newEvent(this, 0);
                annotateEvent(event, &change);
                action->setEvent(event);
				em->scheduleSwitchStack(event);
			}
		}
	}

	return event;
}

/**
 * Schedule events after a loop switch for speed state.
 * If we're using TRANSFER_FOLLOW we don't have to do anything since
 * stream state is kept on the track, we just change loops and it stays.
 */
PUBLIC Event* SpeedFunction::scheduleTransfer(Loop* l)
{
    Event* event = NULL;
    Preset* p = l->getPreset();
    Preset::TransferMode tm = p->getSpeedTransfer();

    if (tm == Preset::XFER_OFF || tm == Preset::XFER_RESTORE) {

        Event* event = NULL;
        EventManager* em = l->getTrack()->getEventManager();

        // If we have any stacked speed events assume that overrides
        // transfer.  May want more here, for example overriding step
        // but not bend.

        Event* prev = em->findEvent(eventType);
        if (prev == NULL) {
            if (tm == Preset::XFER_OFF) {
                event = em->newEvent(SpeedCancel, l->getFrame());
            }
            else {
                StreamState* state = l->getRestoreState();
                event = em->newEvent(SpeedRestore, l->getFrame());
                event->fields.speedRestore.toggle = state->speedToggle;
                event->fields.speedRestore.octave = state->speedOctave;
                event->fields.speedRestore.step = state->speedStep;
                event->fields.speedRestore.bend = state->speedBend;
                event->fields.speedRestore.stretch = state->timeStretch;
            }

            if (event != NULL) {
                event->automatic = true;
                em->addEvent(event);
            }
        }
    }

    return event;
}

/**
 * Speed event handler.
 *
 * OutputStream should already be where we are about to be.
 */
PUBLIC void SpeedFunction::doEvent(Loop* l, Event* e)
{
    if (e->function == SpeedRestore) {
        // we only change the input stream, output stream should
        // have already been done by the JumpPlayEvent

        InputStream* istream = l->getInputStream();

        istream->setSpeed(e->fields.speedRestore.octave, 
                          e->fields.speedRestore.step,
                          e->fields.speedRestore.bend);
        
        istream->setTimeStretch(e->fields.speedRestore.stretch);

        l->getTrack()->setSpeedToggle(e->fields.speedRestore.toggle);

        Synchronizer* sync = l->getSynchronizer();
        sync->loopSpeedShift(l);

        // here only after loop switch, will the SwitchEvent do validation?
        //l->validate(e);
    }
    else if (e->type == SpeedEvent) {

        // convert the Event to a SpeedChange
        SpeedChange change;
        convertEvent(e, &change);

        const char* type = (change.unit == SPEED_UNIT_TOGGLE) ? "Toggling" : "Setting";
        const char* level = "step";
        if (change.unit == SPEED_UNIT_OCTAVE)
          level = "octave";
        else if (change.unit == SPEED_UNIT_BEND)
          level = "bend";

        Trace(l, 2, "Speed: %s speed %s %ld\n", type, level, 
              (long)change.value);

        // !! The old Speed function would cancel Rehearse mode

        applySpeedChange(l, &change, false);

        Synchronizer* sync = l->getSynchronizer();
        sync->loopSpeedShift(l);

        if (mCanRestart && l->getPreset()->isSpeedShiftRestart()) {

            // any other start frame options ?
            l->setFrame(0);
            l->recalculatePlayFrame();

            // Synchronizer may want to send MIDI START
            sync->loopRestart(l);
        }

        // normally we will stay in mute 
        l->checkMuteCancel(e);

        l->validate(e);
    }
}

/**
 * Convert the contents of an Event into a SpeedChange.
 */
PRIVATE void SpeedFunction::convertEvent(Event* e, SpeedChange* change)
{
    change->value = e->number;
    change->unit = (SpeedUnit)e->fields.speed.unit;
}

/**
 * Prepare for a JumpPlayEvent.
 *
 * NOTE: If a SpeedEvent is stacked under a SwitchEvent we will
 * replace all speed related properties in the JumpContext.  This
 * will lose any restored speed properties if speedTransferMode was
 * XFER_RESTORE.  That seems right to me, but might want to merge them?
 * You can tell if we're transfering when next->speedTransfer is true.
 */
PUBLIC void SpeedFunction::prepareJump(Loop* l, Event* e, JumpContext* next)
{
    Event* speedEvent = NULL;
    bool switchStack = false;

	if (e->type == JumpPlayEvent) {
        // simple speed event
        speedEvent = e->getParent();
	}
    else {
        // must be under a switch
        speedEvent = e;
        switchStack = true;
    }

    if (speedEvent->type != SpeedEvent) {
        Trace(l, 1, "SpeedFunction::prepareJump incorrect event type!\n");
    }
    else {
        SpeedChange change;
        convertEvent(speedEvent, &change);

        // There can be several stacked speed events, which accumulate
        // Unlike applySpeedChange we work from state on the JumpContext
        // rather than the InputStream.  JumpContext will have been initialized
        // with the current stream state.
        change.newToggle = next->speedToggle;
        change.newOctave = next->speedOctave;
        change.newStep = next->speedStep;
        change.newBend = next->speedBend;
        change.newStretch = next->timeStretch;

        // calculate what we need to do
        calculateNewSpeed(&change);

        // put it back
        next->speedToggle = change.newToggle;
        next->speedOctave = change.newOctave;
        next->speedStep = change.newStep;
        next->speedBend = change.newBend;
        next->timeStretch = change.newStretch;
    }
}

/**
 * Apply the speed change to the streams.
 * If "both" is true we're before recording and can apply the change
 * to both streams.  If "both" is false then we're processing 
 * a SpeedEvent and only need to set the input stream.  The output
 * stream will have been processed by the JumpPlayEvent.
 */
PRIVATE void SpeedFunction::applySpeedChange(Loop* l, SpeedChange* change, 
                                             bool both)
{
    Track* t = l->getTrack();
    Stream* istream = l->getInputStream();
    Stream* ostream = l->getOutputStream();

    // copy over current stream state, use InputStream consistently
    change->newToggle = t->getSpeedToggle();
    change->newOctave = istream->getSpeedOctave();
    change->newStep = istream->getSpeedStep();
    change->newBend = istream->getSpeedBend();
    change->newStretch = istream->getTimeStretch();

    // calculate what we need to do
    calculateNewSpeed(change);

    applySpeedChange(change, istream);
    if (both)
      applySpeedChange(change, ostream);

    // Once we change the input stream, the track follows the new toggle
    t->setSpeedToggle(change->newToggle);

    if (mType == SPEED_CANCEL) {
        // should this also reset the sequence?  It feels like it should
        l->getTrack()->setSpeedSequenceIndex(0);
    }

}

PRIVATE void SpeedFunction::applySpeedChange(SpeedChange* change, 
                                             Stream* stream)
{
    stream->setSpeed(change->newOctave,  change->newStep, change->newBend);
    stream->setTimeStretch(change->newStretch);
}

/**
 * Calculate the effective speed changes to a stream.
 * SpeedChange is an IO object, the top half conveys what was
 * in the event, and we set the fields in the bottom half.
 *
 * Bend and stretch are easy since these don't combine.
 * Cancel is easy since we just reset everything.
 * 
 * Step and toggle are harder because the perception is that
 * a halfspeed toggle can be combined with SpeedStep.  So if
 * we're in a -12 halfspeed toggle and a SpeedStep -1 is done,
 * the effective step is -13.  Then when the -12 toggle is undone
 * the step is -1.
 *
 * If an overlapping toggle comes in, we first cancel the last toggle
 * then apply the next.
 */
PRIVATE void SpeedFunction::calculateNewSpeed(SpeedChange* change)
{
    if (change->unit == SPEED_UNIT_TOGGLE) {
        int lastToggle = change->newToggle;

        // cancel the previous toggle if there was one
        if (lastToggle != 0) {
            change->newStep = change->newStep - lastToggle;
            change->newToggle = 0;
        }

        // and apply the new toggle if it didn't cancel itself
        if (lastToggle != change->value) {
            change->newStep = change->newStep + change->value;
            change->newToggle = change->value;
        }
    }
    else if (mType == SPEED_CANCEL) {
        change->newToggle = 0;
        change->newOctave = 0;
        change->newStep = 0;
        change->newBend = 0;
    }
    else if (change->unit == SPEED_UNIT_BEND) {
        change->newBend = change->value;
    }
    else if (change->unit == SPEED_UNIT_STRETCH) {
        change->newStretch = change->value;
    }
    else if (change->unit == SPEED_UNIT_OCTAVE) {
        change->newOctave = change->value;
    }
    else {
        // this combines with the toggle
        int lastToggle = change->newToggle;
        change->newStep = lastToggle + change->value;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
