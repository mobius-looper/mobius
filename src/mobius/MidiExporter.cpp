/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Classes related to exporting the current values of parameters, 
 * controls, and other observable things as MIDI messages.
 * This is used in conjunction with a bi-directional control surface
 * so that changes to things made from the UI or scripts can be
 * reflected by the control surface.
 *
 * There are two ways we could approach this.  First be tighly integrated
 * and have the Parameters, or Functions themselves raise
 * some kind of change event whenever they change.  This is relatively
 * difficult to do accurately, though it is the only good way to export
 * function begin/end if that ever became necessary.
 *
 * The second approach is loosely integrated and more like how the
 * export of plugin parameters is done.  We maintain a list of objects
 * representing the things to export with the last value exported.
 * Periodically we compare the current values with the last exported values
 * and, export the ones that changed and update the last exported value.
 *
 * This is easier though if the list is long we could do a fair bit
 * of work comparing values that never change at each export interval.
 * The export interval doesn't have to be that tight.  Plugin parameters
 * are exported every interrupt which is overkill.  The 1/10 second
 * UI polling interval is probably enough.
 * 
 * LAUNCHPAD
 *
 * For now this is also where we'll control updates to the launchpad
 * if one is configured though the logic to do so is encapsulated
 * in the Launchpad class.  Every 1/10 second we'll compare the previously
 * sent LP state with the new loop state and update as necessary.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"
#include "HostMidiInterface.h"

#include "Mobius.h"
#include "Binding.h"
#include "Export.h"
#include "MobiusConfig.h"

#include "MidiExporter.h"

//////////////////////////////////////////////////////////////////////
//
// MidiExporter
//
//////////////////////////////////////////////////////////////////////

/**
 * Build out a MidiExporter with Exports for the MIDI Bindings.
 * Everything bound to a Midi CC gets an export.
 */
MidiExporter::MidiExporter(Mobius* m)
{
    mHistory = NULL;
    mMobius = m;
    mExports = NULL;

	MobiusConfig* config = m->getConfiguration();

    // start with the defaults
    addExports(m, config->getBaseBindingConfig());

    // and add the overlay
    // !! this isn't tracking overlay changes, we should
    // add all of them and then filter when we export
    addExports(m, config->getOverlayBindingConfig());
}

PUBLIC void MidiExporter::setHistory(MidiExporter* me) 
{
    mHistory = me;
}

PUBLIC MidiExporter* MidiExporter::getHistory()
{
    return mHistory;
}

/**
 * Import a list of Bindings.
 */
PRIVATE void MidiExporter::addExports(Mobius* m, BindingConfig* config)
{
    if (config != NULL) {
        Binding* bindings = config->getBindings();
        if (bindings != NULL) {

            // keep them in order
            Export* last = mExports;
            while (last != NULL && last->getNext() != NULL)
              last = last->getNext();

            for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
                Export* exp = convertBinding(m, b);
                if (exp != NULL) {
                    if (last != NULL)
                      last->setNext(exp);
                    else
                      mExports = exp;
                    last = exp;
                }
            }
        }
    }
}

PRIVATE Export* MidiExporter::convertBinding(Mobius* mobius, Binding* b) 
{
    Export* exp = NULL;

    // only concerned with things that can be controlled with knobs
    Target* target = b->getTarget();
    if (target == TargetParameter) {

        Trigger* trigger = b->getTrigger();

        // I suppose Note and Program could be used for latching buttons?
        if (trigger == TriggerControl) {
            
            exp = mobius->resolveExport(b);
            if (exp != NULL) {
                // save these so we know where to go
                // !! this would be better done as a wrapper object since
                // it only applies to this type of binding
                exp->setMidiChannel(b->getChannel());
                exp->setMidiNumber(b->getValue());
            }
        }
    }
    return exp;
}

MidiExporter::~MidiExporter()
{
	MidiExporter *el, *next;

    delete mExports;

	for (el = mHistory ; el != NULL ; el = next) {
		next = el->getHistory();
		el->setHistory(NULL);
		delete el;
	}
}

/**
 * Here's where the magic happens.
 * Called from MobiusThread.
 *
 * MidiInterface sucks!!
 *
 * Here we might want more control over which device gets the tracking
 * events and which gets sync clocks.  That would require that we
 * be able to open more than one device and address them independently,
 * which we can't do with MidiInterface, it sends to all of them in parallel.
 *
 * Also for plugins it would be better to use the normal VST/AU MIDI
 * wiring rather than require that a device be opened just to get tracking
 * events.
 * 
 */
void MidiExporter::sendEvents()
{
    MobiusConfig* config = mMobius->getConfiguration();
    if (config->isMidiExport() || config->isHostMidiExport()) {

        MobiusContext* con = mMobius->getContext();
        HostMidiInterface* hostMidi = con->getHostMidiInterface();

        // this is both an allocator of MidiEvents and an output
        MidiInterface* midi = con->getMidiInterface();

        for (Export* exp = mExports ; exp != NULL ; exp = exp->getNext()) {

            int newValue = exp->getOrdinalValue();

            if (newValue >= 0) {
                int last = exp->getLast();
                if (last != newValue) {

                    // assuming we only deal with TriggerControl
                    // this was filtered when the Exports were created
                    MidiEvent* e = midi->newEvent(MS_CONTROL, 
                                                  exp->getMidiChannel(), 
                                                  exp->getMidiNumber(),
                                                  newValue);
                    if (e != NULL) {

                        //e->dump(true);
                        //fflush(stdout);

                        // this does not take ownership
                        if (config->isMidiExport())
                          midi->send(e);

                        // this one takes ownership!
                        // don't like the inconsistency but it saves a copy
                        if (config->isHostMidiExport() && hostMidi != NULL)
                          hostMidi->send(e);
                        else
                          e->free();
                    }

                    exp->setLast(newValue);
                }
            }
        }
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
