/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract class all control surface interface classes
 * should extend.  Not much here now but it we'll need this as soon
 * as we support another surface like the APC40.
 * 
 */

#ifndef CONTROL_SURFACE_H
#define CONTROL_SURFACE_H

class ControlSurface
{
  public:

    ControlSurface();
    virtual ~ControlSurface();

    void setNext(ControlSurface *c);
    ControlSurface* getNext();
    
    /**
     * Handle an incomming MIDI event.   Return true if the event
     * was handled, false if the event should be passed on to the
     * next surface handler, or to the BindingResolver.
     */
    virtual bool handleEvent(class MidiEvent* event) = 0;

    /**
     * Export state that has changed to the control surface.
     * Called periodically by the mobius refresh thread.
     */
    virtual void refresh() = 0;

    /**
     * Entry point for special script functions that can send commands
     * to the control surface handler.
     * This is what gets called if you use the "Surface" function.
     */
    virtual void scriptInvoke(class Action* a) = 0;


  private:

    ControlSurface* mNext;

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
