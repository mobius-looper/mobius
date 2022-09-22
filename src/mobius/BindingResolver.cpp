/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object used internally by Mobius to quickly lookup trigger bindings.
 * One of these will be constructed whenever the BindingConfig changes.
 * It will create a resolved Action object for each Binding, and place
 * these in arrays so we can quickly locate Actions associated with
 * MIDI and keyboard events.
 * 
 * This is not used for host bindings or OSC bindings.  Host bindings
 * are handled in a similar way in MobiusPlugin and OSC bindings are
 * handled in OscRuntime.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "List.h"
#include "KeyCode.h"
#include "MidiByte.h"
#include "MidiEvent.h"

#include "Mobius.h"
#include "MobiusConfig.h"
#include "Action.h"
#include "Binding.h"
#include "Function.h"
#include "Script.h"
#include "Event.h"

#include "BindingResolver.h"

/****************************************************************************
 *                                                                          *
 *   							   RESOLVER                                 *
 *                                                                          *
 ****************************************************************************/

// KEY_MAX_CODE is defined in keycode.h  as 0xFFF (4096)
#define MAX_MIDI_KEY 128
#define MAX_MIDI_CHANNEL 16

/**
 * Compile an optimized search structure of Action objects
 * combining the base BindingConfig and all of the overlays.
 * At runtime we will decide which overlay bindigns to pay attention
 * to if any.  Merging overlays makes it possible to quickly switch between
 * them withing having to rebuild BindingResolver objets and worrying
 * about multi-threaded access and garbage collection.  Once built, 
 * we can use the same BindingResolver until the bindings are edited.
 *
 */
PUBLIC BindingResolver::BindingResolver(Mobius* mob)
{
    mKeys = allocBindingArray(KEY_MAX_CODE);
	mNotes = allocBindingArray(MAX_MIDI_KEY);
    mControls = allocBindingArray(MAX_MIDI_KEY);
	mPrograms = allocBindingArray(MAX_MIDI_KEY);
	mPitches = allocBindingArray(MAX_MIDI_CHANNEL);

	MobiusConfig* config = mob->getConfiguration();

    mSpreadRange = config->getSpreadRange();

    Trace(2, "Resolving bindings\n");

    // Make sure these are numbered so overlays work, the UI isn't
    // maintaining these.  Presets and Setups have methods on MobiusConfig
    // but doing it here is a bit more robust since we're the only one that
    // needs the numbers.

    int overlay = 0;
    BindingConfig* bindings = config->getBindingConfigs();
    for (BindingConfig* bc = bindings ; bc != NULL ; bc = bc->getNext()) {
        bc->setNumber(overlay);
        assimilate(mob, bc);
        overlay++;
    }

}

PUBLIC BindingResolver::~BindingResolver()
{
	freeBindingArray(mKeys, KEY_MAX_CODE);
	freeBindingArray(mNotes, MAX_MIDI_KEY);
	freeBindingArray(mControls, MAX_MIDI_KEY);
	freeBindingArray(mPrograms, MAX_MIDI_KEY);
	freeBindingArray(mPitches, MAX_MIDI_CHANNEL);
}

PRIVATE Action** BindingResolver::allocBindingArray(int size)
{
	Action** array = new Action*[size];
	for (int i = 0 ; i < size ; i++)
	  array[i] = NULL;
	return array;
}

PRIVATE void BindingResolver::freeBindingArray(Action** array, int size)
{
	if (array != NULL) {
        // NOTE: These are not on Mobius' registered action list
        // we own them
		for (int i = 0 ; i < size ; i++) {
            Action* actions = array[i];
            if (actions != NULL) {
                // deregister them to avoid a warning
                for (Action* a = actions ; a != NULL ; a = a->getNext())
                  a->setRegistered(false);
                delete actions;
            }
        }
		delete array;
	}
}

/**
 * Assimilate a BindingConfig into the resolver.
 * We keep references to things in the BindingConfig object so it
 * has to remain allocated while we use this resolver!
 */
PRIVATE void BindingResolver::assimilate(Mobius* mobius,
                                         BindingConfig* bindings)
{
	if (bindings != NULL) {

		if (bindings->getNumber() == 0)
		  Trace(2, "Assimilating global bindings\n");
		else {
			const char* name = bindings->getName();
			Trace(2, "Assimilating binding overlay %s\n", 
				  (name != NULL) ? name : "???");
		}

        // Convert Bindigns to Actions
		for (Binding* b = bindings->getBindings() ; b != NULL ;
             b = b->getNext()) {

            // ignore ones we can't handle
            Trigger* t = b->getTrigger();
            if (t == TriggerKey ||  
                t == TriggerMidi ||
                t == TriggerNote ||
                t == TriggerProgram ||
                t == TriggerControl ||
                t == TriggerPitch) {

                // Resolve the target and create an action
                Action* a = mobius->resolveAction(b);
                if (a != NULL) {

                    // remember the overlay for runtime filtering
                    a->setOverlay(bindings->getNumber());

                    if (!a->isSpread()) {
                        // normal simple binding
                        assimilate(a);
                    }
                    else {
                        // Spread ranged functions over a range of trigger
                        // values Ranged functions only make sense for Note 
                        // triggers

                        Trigger* trigger = a->trigger;
                        int status = a->getMidiStatus();

                        if (trigger != TriggerMidi) {
                            // keys have to have an argument
                            assimilate(a);
                        }
                        else if (status == MS_CONTROL) {
                            // don't need to setup a binding range, just pass
                            // the CC value as the Action.value
                            assimilate(a);
                        }
                        else if (status == MS_BEND) {
                            // don't need to setup a binding range, just pass
                            // the PB value as the Action.value
                            assimilate(a);
                        }
                        else if (!a->arg.isNull()) {
                            // binding arguments disable spreading
                            assimilate(a);
                        }
                        else {
                            // Must be either MS_NOTEON or MS_PROGRAM
                            // determine the spread range.
                            // It doesn't make a lot of sense to bind range
                            // functions to program changes, but may be useful
                            // for dumb foot controllers that can only send 
                            // program changes?

                            int maxRange = getSpreadRange(a);
                            int center = a->getMidiKey();
                            int start = center - maxRange;
                            int end = center + maxRange;
                            if (start < 0) start = 0;
                            if (end > 127) end = 127;

                            Action** array = mNotes;
                            if (status == MS_PROGRAM)
                              array = mPrograms;

                            for (int i = start ; i <= end ; i++) {
                                Action* clone = new Action(a);
                                clone->triggerOffset = i - center;
                                addBinding(array, MAX_MIDI_KEY, i, clone);
                            }

                            // didn't use this so reclaim it
                            delete a;
                        }
                    }
                }
            }
        }
    }
}

/**
 * Return true if this action is spreadable.
 * We go beyond the spreadiness of the Function and also limit
 * this to just NOTEON and PROGRAM triggers. CONTROL and BEND
 * are bound normally and scale to fit the spread range.
 */
PRIVATE bool BindingResolver::isSpreadable(Action* a)
{
    int status = a->getMidiStatus();
    return (a->isSpread() && a->arg.isNull() &&
            (status == MS_NOTEON || status == MS_PROGRAM));
}

/**
 * Get the range of a spreadable target.
 * This is normally set by a global parameter but scripts
 * may override that.  This must only be called when a->isSpread()
 * is true which means the target object must be a Function.
 */
PRIVATE int BindingResolver::getSpreadRange(Action* a)
{
    int range = mSpreadRange;

    Function* f = (Function*)a->getTargetObject();
    if (f->isScript()) {
        Script* script = (Script*)f->object;
        if (script != NULL) {
            int scriptRange = script->getSpreadRange();
            if (scriptRange > 0)
              range = scriptRange;
        }
    }
    
    return range;
}

/**
 * Assimilate one Action after resolution.
 * This is called in two places, by assimilate for the built-in Mobius
 * functions and later by reresolve to process the mUnresolved list
 * after the UI components have been registered.
 *
 * We're not handling spreads here, for now UI bindings can't
 * be ranged.
 */
PRIVATE void BindingResolver::assimilate(Action* a)
{
    Trigger* trigger = a->trigger;

    if (trigger == NULL) {
        Trace(1, "Unresolved trigger type!!\n");
        delete a;
    }
    else if (trigger == TriggerKey) {
        addBinding(mKeys, KEY_MAX_CODE, a->id, a);
    }
    else if (trigger == TriggerMidi) {

        int status = a->getMidiStatus();
        int index = a->getMidiKey();

        if (status == MS_NOTEON) 
          addBinding(mNotes, MAX_MIDI_KEY, index, a);

        else if (status == MS_PROGRAM)
          addBinding(mPrograms, MAX_MIDI_KEY, index, a);

        else if (status == MS_CONTROL)
          addBinding(mControls, MAX_MIDI_KEY, index, a);

        else if (status == MS_BEND)
          addBinding(mPitches, MAX_MIDI_CHANNEL, a->getMidiChannel(), a);

        else {
            Trace(1, "Invalid MIDI binding status %ld!!\n", (long)status);
            delete a;
        }
    }
    else {
        Trace(1, "Invalid trigger type %s\n", trigger->getName());
        delete a;
    }
}

/**
 * Add an action to one of the trigger arrays.
 * We take ownership of the object, if it can't be added for
 * some reason it is deleted.  If it bumps an existing binding out
 * the other binding is deleted.  It's like a binding roach motel.
 *
 * We allow multiple bindings for the same trigger as long as the targets
 * are different.  If used properly you can get simple "macros" that way
 * but it is also possible to get into trouble so I'm not sure this
 * should be supported.  When checking to see if the targets are
 * the same we ignore scoping, the reason being that with one exception
 * scopes are ambiguous you don't know if the function will be performed
 * twice in the same track which is never what you want.  The one exception
 * is if we have bindings whose scopes are set to different
 * tracks, e.g. "note 42 track 1 record" and "note 42 track 2 record".
 * Again this could be considered a simple macro but it's risky to allow
 * this and you can always scripts if you need that.
 *
 * Spread bindings are allowed to overlap.
 *
 * Spread and non-spread bindings cannot mix.  If a spread binding
 * is being added to a list that already has non-spread bindings it is ignored.
 * If a non-spread binding is being added to a list with spread bindings,
 * all the previous spread bindings are removed.
 * We could relax this rule but usually you want the spread to be "clipped"
 * on the edges by other bindings, there normally won't be holes in the middle.
 *
 */
PRIVATE void BindingResolver::addBinding(Action** array, 
                                            int max, int index,
                                            Action* neu)
{
	if (index >= 0 && index < max) {
        Action* current = array[index];

        if (isSpreadable(neu)) {
            // check for existing non-spread bindings
            for (Action* a = current ; a != NULL ; a = a->getNext()) {
                if (!isSpreadable(a) && 
                    a->getOverlay() == neu->getOverlay() &&
                    a->getMidiChannel() == neu->getMidiChannel()) {
                    // no so fast charlie
                    delete neu;
                    neu = NULL;
                    break;
                }
            }
        }
        else {
            // clean out the spread squaters
            Action* prev = NULL;
            Action* next = NULL;
            for (Action* a = current ; a != NULL ; a = next) {
                next = a->getNext();

                if (isSpreadable(a) && 
                    a->getOverlay() == neu->getOverlay() &&
                    a->getMidiChannel() == neu->getMidiChannel()) {

                    if (prev == NULL)
                      array[index] = next;
                    else
                      prev->setNext(next);
                    
                    a->setNext(NULL);
                    a->setRegistered(false);
                    delete a;
                }
                else 
                  prev = a;
            }
        }

        if (neu != NULL) {
            // find the last one to maintain order, also check for dups
            Action* dup = NULL;
            Action* last = NULL;
            for (Action* a = array[index] ; a != NULL ; a = a->getNext()) {
                // for MIDI bindings the channel factors into trigger equality
                if (neu->getOverlay() == a->getOverlay() &&
                    neu->getMidiChannel() == a->getMidiChannel() &&
                    neu->isTargetEqual(a)) {
                    dup = a;
                    break;
                }
                last = a;
            }

            if (dup != NULL) {
                char buf[1024];
                neu->getDisplayName(buf, sizeof(buf));
                Trace(1, "Ignoring duplicate binding for %s\n", buf);
                delete neu;
            }
            else {
                // flag this so we don't delete it by accident or use
                // it without cloning
                neu->setRegistered(true);

                if (last == NULL)
                  array[index] = neu;
                else
                  last->setNext(neu);
            }
        }
	}
    else {
        Trace(1, "Ignoring binding with invalid index %ld\n", (long)index);
        delete neu;
    }
}

/****************************************************************************
 *                                                                          *
 *                           MIDI EVENT PROCESSING                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Process a MIDI event that may result in the scheduling of one or 
 * more Actions.
 */
PUBLIC void BindingResolver::doMidiEvent(Mobius* mobius, MidiEvent* e)
{
	Action* actions = NULL;
	int channel = e->getChannel();
	int status = e->getStatus();
	int key = e->getKey();
	int value = e->getValue();	// same as velocity

	if (key < 0 || key > 127)
	  Trace(1, "Illegal MIDI event value %ld\n", (long)key);
	else {
		switch (status) {
			case MS_CONTROL: 
				actions = mControls[key];
				break;
			case MS_PROGRAM: 
				actions = mPrograms[key]; 
				break;
			case MS_NOTEON:
			case MS_NOTEOFF: 
				actions = mNotes[key]; 
				break;
            case MS_BEND:
				actions = mPitches[channel]; 
		}
	}

    // determine the overlay to use
    // !! need to be saving the overlay number in the BindingResoler
    // rather than going back to the MobiusConfig?
    int overlay = 0;
    MobiusConfig* config = mobius->getConfiguration();
    BindingConfig* overlayConfig = config->getOverlayBindingConfig();
    if (overlayConfig != NULL) {
        int overlayNumber = overlayConfig->getNumber();
        for (Action* a = actions ; a != NULL ; a = a->getNext()) {
            if (a->getOverlay() == overlayNumber &&
                a->getMidiChannel() == channel) {
                // at least one match, use this overlay
                overlay = overlayNumber;
                break;
            }
        }
    }

	for (Action* a = actions ; a != NULL ; a = a->getNext()) {

        // channel is part of the id so have to check that too
		if (a->getOverlay() == overlay && 
            a->getMidiChannel() == channel) {

            // Ignore PitchBend bingings to non-controls
            Target* target = a->getTarget();
            if (status == MS_BEND && target != TargetParameter) {
                Trace(1, "Can only bind Pitch Bend to a Parameter\n");
                continue;
            }

            // originally any non-zero CC value would be considered up, 
            // but that makes it useless for CC bindings to sliders.
            // This is relatively esoteric but it's better than nothing.
            // Hmm, well actually we can't assume that pedals that send
            // CCs will always send 127, it could be 1
            bool down = (status == MS_PROGRAM ||
                         (status == MS_NOTEON && value > 0) ||
                         (status == MS_CONTROL && value > 0));

            // if this is a fixed or relative value binding for a parameter, 
            // ignore up transitions
            if (target == TargetParameter && !down && 
                (a->actionOperator != NULL || !a->arg.isNull()))
              continue;

            // clone, annotate and post
            // when we start using the resolver we use the action pool
            Action* clone = mobius->cloneAction(a);
            clone->down = down;

            // velocity for notes, value for controllers
            // TODO: binding args for CCs bound to things with
            // centers
            clone->triggerValue = value;

            if (status == MS_BEND) {
                // current "value" isn't enough, have to combine two bytes
                clone->triggerValue = e->getPitchBend();

                // TODO: binding args for more control
                // over the center width.

                // NOTE: This is typically used for pitch/speed
                // bend which has preset parameters for more range control.

                // Presets are track specific though so we can't
                // know them here.  Assume the maximum range and the
                // function will rescale.

                int min = 0;
                int range = 0;
                Parameter* p = (Parameter*)a->getTargetObject();
                if (p != NULL) {
                    min = p->getLow();
                    int max = p->getBindingHigh(mobius);
                    range = (max - min + 1);
                }

                if (range > 0) {
                    // target range / bend range
                    float adjust = (float)range / 16384.0f;
                    int offset = (int)(e->getPitchBend() * adjust);
                    clone->arg.setInt(min + offset);
                }
            }
            else if (status == MS_CONTROL && clone->arg.isNull()) {
                // don't overwrite the binding arg
                if (target == TargetParameter) {
                    Parameter* p = (Parameter*)a->getTargetObject();
                    if (p != NULL) {
                        int min = p->getLow();
                        // note that we use BindingHigh for a 
                        // useful binding range
                        int max = p->getBindingHigh(mobius);
                        int scaled = Scale128ValueIn(value, min, max);
                        clone->arg.setInt(scaled);
                    }
                }
            }

            // If this is a spread function bound to a CONTROL,
            // pass through the value relative to the center of the range. 
            // For notes and programs this will have been calculated during
            // assimilation.  For CCs we calculate it dynamically.
            // !! Only here for PitchStep and SpeedStep, which are
            // also bindable as Parameters that define their own range.
            // So we don't need this, but we have supported function 
            // bindings like this in the past to continue.

            if (status == MS_CONTROL && 
                clone->isSpread() && clone->arg.isNull()) {
                    // For CCs we take the original range and add another 
                    // step for the "center".
                    int range = getSpreadRange(a);
                    // should have caught this by now
                    if (range > 127) range = 127;
                    int half = range / 2;
                    range++;

                    float divisor = (float)(128.0 / (float)range);
                    clone->triggerOffset = (int)(value / divisor) - half;
                    clone->arg.setInt(clone->triggerOffset);

                    // These should always be considered down, because
                    // zero can be a valid value in the range
                    clone->down = true;
            }
            else if (isSpreadable(clone)) {
                // New convention is to pass this as an argument so
                // we're consistent with scripts and binding args
                // note though that we couldn't do this at assimilation 
                // time because we use the nullness of the argument to 
                // determine if this is spreadable. So have to defer it
                // till execution.  Now that we do this, do we really
                // need trigger offset as a distinct entity?  Maybe
                // for spread scripts.
                clone->arg.setInt(clone->triggerOffset);
            }

            if (clone != NULL)
              mobius->doAction(clone);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                         KEYBOARD EVENT PROCESSING                        *
 *                                                                          *
 ****************************************************************************/

/**
 * Process a computer keyboard trigger.
 * The key code must have both the key number and the modifier bits.
 *
 * If the binding is to a ranged function issue a warning.
 * Key triggers can't be used for ranged functions, at least not in the
 * usual way of spreading a contiguous range of ids to the same target.
 * If we wanted to do this for key triggers the physical range would
 * most likely be rows (1-0, qwerty..., num1-num0, etc) that don't
 * have contiguous ids.
 */
PUBLIC void BindingResolver::doKeyEvent(Mobius* mobius, int key, 
                                           bool down, bool repeat)
{
   	if (key < 0 || key > KEY_MAX_CODE) {
        Trace(1, "Illegal key trigger code %ld\n", (long)key);
    }
    else {
        Action* actions = mKeys[key];

        if (!down) {
            int x = 0;
        }

        // determine the overlay to use
        // this actually isn't necessary since we won't have key overlays
        // yet, but maybe someday...
        int overlay = 0;

        for (Action* a = actions ; a != NULL ; a = a->getNext()) {
            
            if (a->getOverlay() == overlay) {

                Action* clone = mobius->cloneAction(a);

                clone->id = key;
                clone->down = down;
                clone->repeat = repeat;

                // value is unspecified here, but Mobius may
                // calculate one if there are binding args

                mobius->doAction(clone);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
