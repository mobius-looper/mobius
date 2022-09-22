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
 * We're only supporting Parameter and Control exports for the moment.
 * Some Variables might be interesting though since they can't be
 * set this would be for status displays only.  This will flesh out
 * more when I get the Launchpad.
 */

#ifndef MIDI_EXPORTER_H
#define MIDI_EXPORTER_H

//////////////////////////////////////////////////////////////////////
//
// MidiExporter
//
//////////////////////////////////////////////////////////////////////

/**
 * Class maintaining a list of MidiExport objects and manages the
 * export process.
 */
class MidiExporter {
  public:

    MidiExporter(class Mobius* m);
    ~MidiExporter();

    void setHistory(MidiExporter* me);
    MidiExporter* getHistory();

    void sendEvents();

  private:

    void addExports(class Mobius* m, class BindingConfig* bindings);
    class Export* convertBinding(class Mobius* mobius, class Binding* b);

    MidiExporter* mHistory;
    class Mobius* mMobius;
    class Export* mExports;
};

#endif
