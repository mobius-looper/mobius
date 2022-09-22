/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Old MidiConfig classes for MIDI function binding.
 * Soon to be replaced with the new Binding classes.
 *
 */

#ifndef OLD_BINDING_H
#define OLD_BINDING_H

#include "Binding.h"

/****************************************************************************
 *                                                                          *
 *   							 MIDI CONFIG                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Root XML element.
 */
#define EL_MIDI_CONFIG "MidiConfig"

/**
 * Type constants for things used in Bindings.
 * Also serves as a way to type Bindable objects, though only some are used.
 * There is a name mapping array in Binding.cpp that depends on the
 * order of this enum.
 * OBSOLETE: soon to be removed
 */
typedef enum {

	BindableFunction,
	BindableControl,
	BindableSetup,
	BindablePreset,
	BindableMidiConfig

} BindingType;

/*
 * We don't implement the ControlSource and Source# parameters
 * as this is more flexible.
 *
 * VolumeControl and FeedbackControl are here rather than global just
 * in case you need to change them.  
 * 
 * Loop Trigger = 0 - 127, default 84, is the base note number for loop
 * triggers on the EDP.  Don't need that, we have direct assignment.
 *
 * Feedback Control and Volume Control are normally continuous controllers,
 * they're in MIDI Config rather than the Preset since its unlikely
 * that they will be different for each preset, though it feels more
 * likely that these could be usefully changed per-preset.  You might
 * want to switch among several controller devices?
 *
 * It could be useful to set feedback to specific values with 
 * non-controller events like notes.
 *
 */

typedef enum {
	NONE,
	CONTROL, 
	NOTE, 
	PROGRAM, 
} MidiStatus;

/**
 * Defines a binding between a MIDI event and a function, controller, 
 * or configuration object.  If the track field is non-zero, this is a
 * track specific binding, if the group field is non-zero it is a group
 * binding, otherwise this is a global binding.
 *
 */
class MidiBinding {
	
  public:
	
	MidiBinding();
	MidiBinding(class XmlElement* e);
	~MidiBinding();

	void setNext(MidiBinding* c);
	MidiBinding* getNext();

	void setType(BindingType t);
	BindingType getType();

	void setName(const char* s);
	const char* getName();

	void setTrack(int t);
	int getTrack();
	void setGroup(int g);
	int getGroup();
	void setChannel(int c);
	int getChannel();
	void setStatus(MidiStatus s);
	MidiStatus getStatus();
	void setValue(int v);
	int getValue();

	void getMidiString(char* buffer, bool includeChannel);
	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:

	void init();
	const char* getBindingTypeName(BindingType type);
	BindingType getBindingType(const char* name);

	MidiBinding* mNext;
	BindingType mType;
	char* mName;
	MidiStatus mStatus;
	int mTrack;
	int mGroup;
	int mChannel;
	int mValue;
};


class MidiConfig : public Bindable {

  public:

	MidiConfig();
	MidiConfig(class XmlElement* e);
	~MidiConfig();
	MidiConfig* clone();

    class BindingConfig* upgrade();

    Bindable* getNextBindable();
	class Target* getTarget();
	void select();

	void setNext(MidiConfig* c);
	MidiConfig* getNext();

	int getTrackGroups();
	void setTrackGroups(int g);

	MidiBinding* getBindings();

	void addBinding(MidiBinding* c);
	void removeBinding(MidiBinding* c);

	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:

	void init();

	MidiConfig* mNext;
	int mTrackGroups;
	MidiBinding* mBindings;

};

/**
 * Represents a binding between a MIDI event and something within Mobius
 * that can be controlled by MIDI.  These include functions, parameters,
 * presets, and midi configurations.
 *
 * They are maintained in the BindingCache and used at runtime as opposed to
 * MidiBinding which represnts the a static binding stored in the mobius.xml file.
 *
 * At the moment the only thing this does that MidiBinding doesn't is provide 
 * a "next" field for chaining bindings in the cache, and the relative position for
 * ranged functions.
 *
 * Formerly did more, but this collapsed when Bindable was introduced.
 * 
 */
class EventBinding {

  public:

	EventBinding();
	~EventBinding();
	bool equal(EventBinding* src);

	EventBinding* next;

	/**
	 * MidiBinding for function and controller bindings.
	 */
	MidiBinding* source;

	/**
	 * For ranged functions, the relative position of this key to the
	 * center key.
	 */
	int rangePosition;

};

/**
 * Class that builds optimized search arrays for a MidiConfig, Project,
 * and other MIDI bindings.
 */
class BindingCache {

  public:

	BindingCache(class Mobius* mobius);
	~BindingCache();

	void doEvent(class Mobius* mob, MidiEvent* e);

  private:
	
	bool isRanged(Function* f);
	bool hasBinding(EventBinding** array, int index, MidiBinding* src);
	void addBinding(EventBinding** array, int index, EventBinding* b);
	void addBinding(EventBinding** array, MidiBinding* b);

	EventBinding** allocBindingArray();
	void freeBindingArray(EventBinding** array);

	EventBinding** mNotes;
	EventBinding** mControllers;
	EventBinding** mPrograms;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
