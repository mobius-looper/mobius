/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for exporting values out of Mobius.
 *
 * Usually this is used for things that can be binding targets
 * like controls or parameters.  In 2.2 we starting adding Exportables
 * which are engine characteristics like loop position that may also
 * be exported.
 *
 * There are only two ways to get things in and out of the Mobius engine
 * from the UI layer.  Actions are used to set target values or execute
 * functions, and Exports are used to read things.  Currently this is
 * only done for OSC.
 *
 * All code above the MobiusInterface must use Exports to read things,
 * direct access to Parameter or Variable is not allowed.  This includes
 * OscConfig, MobiusPlugin, and MidiExporter.
 *
 * An export is created by calling one of the Mobius::resolveExport functions.
 * An export may be resolved by passing one of these things:
 * 
 *      Binding
 *      ResolvedTarget
 *      Action
 *
 * Null is returned if the target is invalid, or this is not an exportable
 * target.  The Export returned is owned by the caller and must be freed by
 * the caller.
 *
 * Mobius::getExport(Export) is called to get the current value of the export.
 *
 * A few properties are provided for use by the UI: midiChannel, midiNumber, 
 * and lastValue.
 *
 * Export maintains a "last" value that has the last value set by the UI.
 * This is necessary in cases where setting something may not have an immediate
 * effect.
 * !! revisit this, should this be here or in a wrapper class.
 *
 * The MIDI properties are used by MidiExporter to build the appropriate
 * MIDI event to export.
 * !! Must we do this?  Can't we have a wrapper and remember it in MidiExporter?
 */

#ifndef EXPORT_H
#define EXPORT_H


/**
 * An enumeration used to convey the data type of the export.
 * This is a duplicate of ParameterType but I don't want
 * Parameter.h in the external interface.  Eventually Parameter
 * should use this?
 */
typedef enum {

	EXP_INT,
	EXP_BOOLEAN,
	EXP_ENUM,
	EXP_STRING

} ExportType;

class Export {
   
    friend class Mobius;
    friend class ActionDispatcher;
    friend class ScriptInterpreter;
    friend class ScriptResolver;
    friend class ScriptArgument;

  public:

	~Export();

    Export* getNext();
    void setNext(Export* e);

    class Mobius* getMobius();
    class ResolvedTarget* getTarget();
    class Track* getTrack();

    //
    // Target properties
    //

    ExportType getType();
    int getMinimum();
    int getMaximum();
    int getOrdinalValue();
    void getOrdinalLabel(int ordinal, class ExValue* value);
    void getValue(class ExValue* value);
    const char** getValueLabels();
    const char* getDisplayName();
    bool isDisplayable();

    //
    // Client specific properties
    //

    int getMidiChannel();
    void setMidiChannel(int i);
    
    int getMidiNumber();
    void setMidiNumber(int i);

    int getLast();
    void setLast(int i);

  protected:

	Export(class Mobius* m);
	Export(class Mobius* m, class State* s);
	Export(class Action* a);
    void setTarget(class ResolvedTarget* t);
    void setTrack(class Track* t);

  private:

    void init();
    class Track* getTargetTrack();

    /**
     * Exports are usually on a list maintained by the client.
     */
    Export* mNext;

    /**
     * Back ref to the engine so we can put accessor logic
     * in this class rather than having a bunch of
     * Mobius::getMinimum(Export*e) methods.
     */
    class Mobius* mMobius;

    /**
     * The target resolved by Mobius.
     */
    class ResolvedTarget* mTarget;

    /**
     * The specific target track when the target
     * specifies a group.
     */
    class Track* mTrack;

    //
    // Client specific fields
    //

    /**
     * The last value the client exported.
     * This is specific to MobiusPlugin and OscConfig
     * which use it to export only targets with 
     * integer values.
     */
    int mLast;

    /**
     * For MIDI exports, the channel and control number.
     */
    int mMidiChannel;
    int mMidiNumber;

    // TODO: for OSC exports, the path

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
