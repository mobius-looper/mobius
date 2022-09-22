/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for defining bindings between external triggers like MIDI,
 * keyboard, and OSC events with Mobius targets like functions and
 * parameters.  This model is only for the definitions of the bindings
 * and is persisted in mobius.xml.  At runtime we build the 
 * Action model which is transient.  
 * 
 * The relevant classes are:
 *
 *   Trigger				constant objects for triggers
 *   Target					constant object for targets
 *   Bindable				abstract class for bindable config objects
 *   Binding				trigger, target, bindable
 *   BindingConfig 			collection of Bindings
 *
 * OLD MODEL
 *
 * Before BindingConfig we used MidiConfig which was funtionally
 * similar but only supported MIDI bindings.  We keep this model
 * around in OldBinding.* so that we can parse old XML config files
 * anc convert it to the new BindingConfig model.  This is done
 * in Mobius::loadConfiguration.
 *
 * MobiusConfig
 *   MidiConfig				now BindingConfig
 *     MidiBinding			now Binding
 * 
 * ===
 * 
 * A binding is composed of three main parts: Trigger, Target, and Scope.
 *
 * The binding triggers are:
 *
 *    Key - a key on the computer keyboard, may include shift qualifiers
 *    Note - a MIDI note
 *    Controller - a MIDI continuous controller
 *    Program - a MIDI program change
 *    Host - a parameter automation event from a plugin host
 *    UI - a button in the user interface
 *    Mouse - a button on the mouse or alternate input device
 *    Wheel - the scroll wheel of a mouse or input device
 *            don't like this in retrospect, use wheel for changing the
 *            selected UI element
 *
 * Mouse and Wheel are defined but not currently used.
 *
 * In addition we also define these trigger types for various internal
 * triggers, these cannot be dynamically bound to targets with a
 * Binding object, binding is either implicit or done with a different
 * kind of configuration object.
 *
 *    Script - an executing script (only for calling functions)
 *    Alert - an interesting engine state is reached (always bound to scripts)
 *
 * The binding targets are:
 *
 *    Function - a Mobius function, may have selector argument
 *    Parameter - A Global, Preset,  track parameter, or control
 *    Setup - a Setup configuration object
 *    Preset - a Preset configuration object
 *    Bindings - a BindingConfig configuration object
 *    UI Control - a UI control (cursor up/down, value up/down)
 *
 * The binding scopes are:
 *
 *   Global 
 *     No specific track assignment, applied to the current track and
 *     any track with focus lock or in the same group as the selected track.
 *
 *   Track
 *     A specific track number is included in the binding.  Only that
 *     track is affected.
 *
 *   Group
 *     A specific group number is included in the binding.  All tracks
 *     in that group are affected.
 *
 */

#ifndef BINDING_H
#define BINDING_H

#include "SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *                                  TRIGGER                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Triggers are the "who" of a binding.
 * They define where the trigger came from which in turn may
 * imply things about the way the action should be processed.
 */
class Trigger : public SystemConstant {
  public:

	static Trigger* get(const char* name);

    Trigger(const char* name, const char* display, bool bindable);

    bool isBindable();

  private:

   // true if this can be dynamically bound with a Binding object.
   bool mBindable;

};

extern Trigger* TriggerKey;
extern Trigger* TriggerMidi;
extern Trigger* TriggerHost;
extern Trigger* TriggerOsc;
extern Trigger* TriggerUI;

// these are used only for binding definitions, not for actions
extern Trigger* TriggerNote;
extern Trigger* TriggerProgram;
extern Trigger* TriggerControl;
extern Trigger* TriggerPitch;

// internal triggers not used in bindings
extern Trigger* TriggerScript;
extern Trigger* TriggerThread;
extern Trigger* TriggerAlert;
extern Trigger* TriggerEvent;
extern Trigger* TriggerUnknown;

/****************************************************************************
 *                                                                          *
 *                                TRIGGER MODE                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Defines the behavior of the trigger over time.
 * 
 * Triggers can behave in several ways, the most common are
 * as momentary buttons and as continuous controls.
 *
 * Some trigger constants imply their mode, TriggerNote
 * for example can be assumed to behave like a momentary button.
 * Others like TriggerOsc and TriggerUI are more generic.  They
 * may have several behaviors.  
 *
 * If an Binding is created with an ambiguous Trigger, a TriggerMode
 * must also be specified.  If not then TriggerTypeButton is assumed.
 *
 * TODO: I think I'd rather do MIDI triggers like this:
 *
 *    TriggerNote = TriggerMidi + TriggerModeMomentary
 *    TriggerProgram = TriggerMidi + TriggerModeButton
 *    TriggerControl = TriggerMidi = TriggerModeContiuous;
 *
 */
class TriggerMode : public SystemConstant {
  public:

	static TriggerMode* get(const char* name);

    TriggerMode(const char* name, const char* display);

  private:

};

extern TriggerMode* TriggerModeContinuous;
extern TriggerMode* TriggerModeOnce;
extern TriggerMode* TriggerModeMomentary;
extern TriggerMode* TriggerModeToggle;
extern TriggerMode* TriggerModeXY;
extern TriggerMode* TriggerModes[];

/****************************************************************************
 *                                                                          *
 *   							   TARGETS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A Target represents the various things within Mobius that can 
 * be bound to a Trigger or used in an Export.
 *
 * UIConfig is not currrently used.
 *
 * You can also set setups, presets, and bindings through Parameters
 * named "setup", "preset", and "bindings".  The only reason these are
 * exposed as individual targets is to make it easier to bind 
 * MIDI events directly to them, the alternative would be to bind go 
 * Parameter:setup and use an argument with the setup name.
 */
class Target : public SystemConstant {
  public:

	static Target* get(const char* name);

	Target(const char* name, const char* display);

  private:

};

extern Target* TargetFunction;
extern Target* TargetParameter;
extern Target* TargetSetup;
extern Target* TargetPreset;
extern Target* TargetBindings;
extern Target* TargetUIControl;
extern Target* TargetUIConfig;

// internal targets, can't be used in bindings
extern Target* TargetScript;

extern Target* Targets[];

/****************************************************************************
 *                                                                          *
 *                                 UI CONTROL                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Defines a control managed by the UI that may be a binding target.
 * These are given to Mobius during initialization, the core code
 * does not have any predefined knowledge of what these are.
 *
 * These are functionally the same as a Function or Parameter objects,
 * so they don't really belong with the Binding definition classes
 * but I don't have a better place for it.  UITypes.h shouldn't be used
 * because that has things in it specific to the UI which core Mobius
 * shouldn't know about.
 * 
 * There are two types of controls: instant and continuous.
 * Instant controls are like Mobius functions, they are one-shot
 * actions that do not have a range of values.
 *
 * Continuous controls are like Mobius controls, they have a range
 * of values.
 *
 * NOTE: Continuous controls have never been used, the current
 * controls are: nextParameter, prevParameter, incParameter, decParameter,
 * spaceDrag (aka Move Display Components).
 *
 * We don't have a way to define min/max ranges even if we did have
 * continuous controls and we don't have a way to define sustain behavior.
 * Basically we'd need things from Function and Parameter combined, 
 * this isn't such a bad thing but it may be better to have the UI
 * give us Function and Parameter objects instead so we have
 * a consistent way of dealing with both internal and UI functions.
 *
 */
class UIControl : public SystemConstant {

  public:

	UIControl();
	UIControl(const char* name, int key);

  private:

    void init();

};

/****************************************************************************
 *                                                                          *
 *                                UI PARAMETER                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Defines a UI parameter that may be manipulated abstractly in the dialogs.
 * This is analogous to Parameter in Mobius, but for the UI.  We don't
 * have many of these but I wanted to start getting the abstraction
 * going since I can envision more of these.
 *
 * !! Rethink this.  Can this just be a Parameter subclass so we don't
 * have to deal with SystemConstant and another binding type?
 * If we can bind to these then this needs to go in Binding.h with UIControl.
 */

class UIParameter : public SystemConstant {

  public:

    UIParameter(const char* name, int key);

    // never existed...
	//static UIParameter** getParameters();
	//static UIParameter* getParameter(const char* name);
	//static void localizeAll(class MessageCatalog* cat);

  private:

};

/****************************************************************************
 *                                                                          *
 *   							   BINDABLE                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Common base class for configuration objects that can be selected
 * with Triggers.
 *
 * Currently this is Setup, Preset, and BindingConfig.
 *
 * Like UIControl, this isn't part of the Binding model so it doesn't
 * really belong here, but I don't have a better place for it.
 */
class Bindable {

  public:

	Bindable();
	~Bindable();
	void clone(Bindable* src);

	void setNumber(int i);
	int getNumber();

	void setName(const char* name);
	const char* getName();

    virtual Bindable* getNextBindable() = 0;
	virtual class Target* getTarget() = 0;

	void toXmlCommon(class XmlBuffer* b);
	void parseXmlCommon(class XmlElement* e);

  protected:

	/**
	 * A non-persistent internal number.
	 * Used to uniquely identity presets that may not have names
	 * or have ambiguous names.
	 */
	int mNumber;

	/**
	 * Let configurations be named.
	 */
	char* mName;

};

/****************************************************************************
 *                                                                          *
 *   							   BINDING                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Defines a binding between a trigger and a target.
 * These are owned by a BindingConfig or an OscBindingSet object.
 *
 * OSC bindings were added later, need to revisit the design and consider
 * using "paths" consistently everywhere rather than breaking it down
 * into discrete target/name/scope/args, at least not in the persistent model.
 *
 * Scope started out as a pair of track and group numbers but now
 * it is string whose value is a scope name with these conventions:
 *
 *    null - a global binding (current track, focused tracks, group tracks)
 *    digit - track number
 *    letter - group identifier (A,B,C...)
 *
 * In theory we can add other scopes like "muted", simliar to the
 * track identifiers we have with the "for" statement in scripts.
 * 
 * There is usually a number associated with the trigger.
 *   Note - note number
 *   Program - program number 
 *   Control - control number
 *   Key - key code
 *   Wheel - NA
 *   Mouse - ?
 *   Host - host parameter number
 *
 * MIDI triggers may have an additional channel number.
 *
 * Some Targets may have an additional numeric argument.
 *   - only functions like Track ?
 *
 * OSC bindings use targetPath instead of discrete target, name, scope, 
 * and arguments values.  This will be parsed into the discrete values.
 * OSC bindings also have a string value (the source method) rather than
 * a simple integer like MIDI notes.  This is only used when Bindings
 * are inside an OscBindingSet.
 * 
 */
class Binding {
	
  public:
	
	Binding();
	Binding(class XmlElement* e);
	virtual ~Binding();

	void setNext(Binding* c);
	Binding* getNext();

	bool isValid();
	bool isMidi();

	//
	// trigger
	//

	void setTrigger(Trigger* t);
	Trigger* getTrigger();

    // for MIDI, key, and host parameter triggers
	void setValue(int v);
	int getValue();

	// only for MIDI triggers
	void setChannel(int c);
	int getChannel();

    // only for OSC triggers
    void setTriggerPath(const char* s);
    const char* getTriggerPath();

    // only for OSC triggers
    void setTriggerMode(TriggerMode* tt);
    TriggerMode* getTriggerMode();

	//
	// target
	//

    void setTargetPath(const char* s);
    const char* getTargetPath();

	void setTarget(Target* t);
	Target* getTarget();

	void setName(const char* s);
	const char* getName();

	void setArgs(const char* c);
	const char* getArgs();

	//
	// scope
	//

    const char* getScope();
    void setScope(const char* s);

    // parsed scopes
	void setTrack(int t);
	int getTrack();
	void setGroup(int g);
	int getGroup();

	//
	// Utils
	//

	void getSummary(char* buffer);
    void getMidiString(char* buffer, bool includeChannel);
    void getKeyString(char* buffer, int max);
	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:

	void init();
    void parseScope();

	Binding* mNext;

	// trigger
	Trigger* mTrigger;
    TriggerMode* mTriggerMode;
    char* mTriggerPath;
	int mValue;
	int mChannel;

	// target
    char* mTargetPath;
	Target* mTarget;
	char* mName;

	// scope, tracks and groups are both numberd from 1
    // both zero means "currently selected track"
    char* mScope;
	int mTrack;
	int mGroup;

    // arguments
	char* mArgs;

};

/****************************************************************************
 *                                                                          *
 *   							BINDING CONFIG                              *
 *                                                                          *
 ****************************************************************************/

/**
 * XML Name for BindingConfig.
 * Public so we can parse it in MobiusConfig.
 */
#define EL_BINDING_CONFIG "BindingConfig"

/**
 * An object managing a named collection of Bindings, with convenience
 * methods for searching them.
 */
class BindingConfig : public Bindable {

  public:

	BindingConfig();
	BindingConfig(class XmlElement* e);
	~BindingConfig();
	BindingConfig* clone();

    Bindable* getNextBindable();
	Target* getTarget();
	
	void setNext(BindingConfig* c);
	BindingConfig* getNext();

	void addBinding(Binding* c);
	void removeBinding(Binding* c);

	Binding* getBindings();
	void setBindings(Binding* b);

    Binding* getBinding(Trigger* trig, int value);

	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:

	void init();

	BindingConfig* mNext;
	Binding* mBindings;
	
};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
