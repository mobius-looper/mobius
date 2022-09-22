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

#ifndef BINDING_RESOLVER_H
#define BINDING_RESOLVER_H

/****************************************************************************
 *                                                                          *
 *   						   BINDING RESOLVER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Class that builds optimized search arrays for trigger bindings.
 */
class BindingResolver {
 
  public:

	BindingResolver(class Mobius* mobius);
	~BindingResolver();

	void doMidiEvent(class Mobius* mob, class MidiEvent* e);
	void doKeyEvent(class Mobius* mob, int key, bool down, bool repeat);

  private:
	
	class Action** allocBindingArray(int size);
	void freeBindingArray(class Action** array, int size);
    void deleteUnresolved();

    class Action* convertBinding(class Binding* src, class ResolvedTarget* t);

    void assimilate(class Mobius* mobius, class BindingConfig* bindings);
    void assimilate(class Action* a);
	void addBinding(class Action** array, int max, int index, Action* b);
    bool targetsEqual(Action* one, Action* other);
    int getSpreadRange(Action* a);
    bool isSpreadable(Action* a);

    int mSpreadRange;

	class Action** mKeys;
	class Action** mNotes;
	class Action** mPrograms;
	class Action** mControls;
	class Action** mPitches;

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
