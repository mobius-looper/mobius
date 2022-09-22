/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for binding triggers to functions, controls, parameters,
 * and configuration objects within Mobius.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "List.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"

#include "KeyCode.h"

// for MidiNoteName
#include "MidiUtil.h"
#include "MidiEvent.h"
#include "MidiMap.h"

#include "Action.h"
#include "Binding.h"
#include "Event.h"
#include "Function.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Parameter.h"
#include "Script.h"
#include "Track.h"

/****************************************************************************
 *                                                                          *
 *   							   BINDABLE                                 *
 *                                                                          *
 ****************************************************************************/

#define ATT_NAME "name"
#define ATT_NUMBER "number"

PUBLIC Bindable::Bindable()
{
	mNumber	= 0;
	mName	= NULL;
}

PUBLIC Bindable::~Bindable()
{
	delete mName;
}

PUBLIC void Bindable::setNumber(int i)
{
	mNumber = i;
}

PUBLIC int Bindable::getNumber()
{
	return mNumber;
}

PUBLIC void Bindable::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

PUBLIC const char* Bindable::getName()
{
	return mName;
}

PUBLIC void Bindable::clone(Bindable* src)
{
	setName(src->mName);
	mNumber = src->mNumber;
}

PUBLIC void Bindable::toXmlCommon(XmlBuffer* b)
{
	// the number is transient on the way to generating a name, 
	// but just in case we don't have a name, serialize it
	if (mName != NULL)
	  b->addAttribute(ATT_NAME, mName);
	else
	  b->addAttribute(ATT_NUMBER, mNumber);
}

PUBLIC void Bindable::parseXmlCommon(XmlElement* e)
{
	setName(e->getAttribute(ATT_NAME));
	setNumber(e->getIntAttribute(ATT_NUMBER));
}

/****************************************************************************
 *                                                                          *
 *   							   TRIGGERS                                 *
 *                                                                          *
 ****************************************************************************/

Trigger* TriggerKey = new Trigger("key", "Key", true);
Trigger* TriggerMidi = new Trigger("midi", "MIDI", false);
Trigger* TriggerNote = new Trigger("note", "Note", true);
Trigger* TriggerProgram = new Trigger("program", "Program", true);
Trigger* TriggerControl = new Trigger("control", "Control", true);
Trigger* TriggerPitch = new Trigger("pitch", "Pitch Bend", true);
Trigger* TriggerHost = new Trigger("host", "Host", true);
Trigger* TriggerOsc = new Trigger("osc", "OSC", false);
Trigger* TriggerUI = new Trigger("ui", "UI", true);
Trigger* TriggerScript = new Trigger("script", "Script", false);
Trigger* TriggerAlert = new Trigger("alert", "Alert", false);
Trigger* TriggerEvent = new Trigger("event", "Event", false);
Trigger* TriggerThread = new Trigger("thread", "Mobius Thread", false);
Trigger* TriggerUnknown = new Trigger("unknown", "unknown", false);

/**
 * Array of all triggers for resolving references in XML.
 * Only bindable triggers should be here
 */
Trigger* Triggers[] = {
	TriggerKey,
	TriggerNote,
	TriggerProgram,
	TriggerControl,
	TriggerPitch,
	TriggerHost,
	TriggerOsc,
    TriggerUI,
	NULL
};

PUBLIC Trigger::Trigger(const char* name, const char* display, bool bindable) :
    SystemConstant(name, display)
{
    mBindable = bindable;
}

PUBLIC bool Trigger::isBindable()
{
    return mBindable;
}

/**
 * Lookup a bindable trigger by name.
 */
Trigger* Trigger::get(const char* name) 
{
	Trigger* found = NULL;
	if (name != NULL) {
		for (int i = 0 ; Triggers[i] != NULL ; i++) {
			Trigger* t = Triggers[i];
			if (!strcmp(t->getName(), name)) {
				found = t;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************
 *                                                                          *
 *                               TRIGGER MODES                              *
 *                                                                          *
 ****************************************************************************/

TriggerMode* TriggerModeContinuous = new TriggerMode("continuous", "Continuous");
TriggerMode* TriggerModeOnce = new TriggerMode("once", "Once");
TriggerMode* TriggerModeMomentary = new TriggerMode("momentary", "Momentary");
TriggerMode* TriggerModeToggle = new TriggerMode("toggle", "Toggle");
TriggerMode* TriggerModeXY = new TriggerMode("xy", "X,Y");

/**
 * Array of all triggers for resolving references in XML.
 */
TriggerMode* TriggerModes[] = {
    TriggerModeContinuous,
    TriggerModeOnce,
    TriggerModeMomentary,
    TriggerModeToggle,
    TriggerModeXY,
	NULL
};

PUBLIC TriggerMode::TriggerMode(const char* name, const char* display) :
    SystemConstant(name, display)
{
}

TriggerMode* TriggerMode::get(const char* name) 
{
	TriggerMode* found = NULL;
	if (name != NULL) {
		for (int i = 0 ; TriggerModes[i] != NULL ; i++) {
			TriggerMode* t = TriggerModes[i];
			if (!strcmp(t->getName(), name)) {
				found = t;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************
 *                                                                          *
 *   							   TARGETS                                  *
 *                                                                          *
 ****************************************************************************/

Target* TargetFunction = new Target("function", "Function");
Target* TargetParameter = new Target("parameter", "Parameter");
Target* TargetSetup = new Target("setup", "Setup");
Target* TargetPreset = new Target("preset", "Preset");
Target* TargetBindings = new Target("bindings", "Bindings");
Target* TargetUIControl = new Target("uiControl", "UI Control");
Target* TargetUIConfig = new Target("uiConfig", "UI Config");
Target* TargetScript = new Target("script", "Script");

Target* Targets[] = {
	TargetFunction,
	TargetParameter,
	TargetSetup,
	TargetPreset,
	TargetBindings,
	TargetUIControl,
	TargetUIConfig,
	TargetScript,
	NULL
};
 
PUBLIC Target::Target(const char* name, const char* display) :
    SystemConstant(name, display)
{
}

Target* Target::get(const char* name) 
{
	Target* found = NULL;

    // auto upgrade old bindings
    if (StringEqual(name, "control"))
      name = "parameter";

	if (name != NULL) {
		for (int i = 0 ; Targets[i] != NULL ; i++) {
			Target* t = Targets[i];
			if (!strcmp(t->getName(), name)) {
				found = t;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************
 *                                                                          *
 *                                 UI CONTROL                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC UIControl::UIControl()
{
    init();
}

PUBLIC UIControl::UIControl(const char* name, int key) :
    SystemConstant(name, key)
{
    init();
}

PRIVATE void UIControl::init() {
}

/****************************************************************************
 *                                                                          *
 *                                UI PARAMETER                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC UIParameter::UIParameter(const char* name, int key) :
    SystemConstant(name, key)
{
}

/****************************************************************************
 *                                                                          *
 *   							   BINDING                                  *
 *                                                                          *
 ****************************************************************************/

PRIVATE void Binding::init()
{
	mNext = NULL;

	// trigger
	mTrigger = NULL;
    mTriggerMode = NULL;
    mTriggerPath = NULL;
	mValue = 0;
	mChannel = 0;

	// target
    mTargetPath = NULL;
	mTarget = NULL;
	mName = NULL;

	// scope
    mScope = NULL;
	mTrack = 0;
	mGroup = 0;

    // arguments
	mArgs = NULL;
}

Binding::Binding()
{
	init();
}

Binding::Binding(XmlElement* e)
{
	init();
	parseXml(e);
}

Binding::~Binding()
{
	Binding *el, *next;

    delete mTriggerPath;
    delete mTargetPath;
	delete mName;
    delete mScope;
	delete mArgs;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}

}

void Binding::setNext(Binding* c)
{
	mNext = c;
}

Binding* Binding::getNext()
{
	return mNext;
}

//
// Trigger
//

void Binding::setTrigger(Trigger* t) 
{
	mTrigger = t;
}

Trigger* Binding::getTrigger()
{
	return mTrigger;
}

void Binding::setValue(int v) 
{
	mValue = v;
}

int Binding::getValue()
{
	return mValue;
}

void Binding::setChannel(int c) 
{
	mChannel = c;
}

int Binding::getChannel()
{
	return mChannel;
}

bool Binding::isMidi()
{
	return (mTrigger == TriggerNote ||
			mTrigger == TriggerProgram ||
			mTrigger == TriggerControl ||
			mTrigger == TriggerPitch);
}

void Binding::setTriggerPath(const char* s)
{
    delete mTriggerPath;
    mTriggerPath = CopyString(s);
}

const char* Binding::getTriggerPath()
{
    return mTriggerPath;
}

void Binding::setTriggerMode(TriggerMode* t)
{
    mTriggerMode = t;
}

TriggerMode* Binding::getTriggerMode()
{
    return mTriggerMode;
}

//
// Target
//

void Binding::setTargetPath(const char* s)
{
    delete mTargetPath;
    mTargetPath = CopyString(s);
}

const char* Binding::getTargetPath()
{
    return mTargetPath;
}

void Binding::setTarget(Target* t) 
{
	mTarget = t;
}

Target* Binding::getTarget()
{
	return mTarget;
}

void Binding::setName(const char *name) 
{
	delete mName;
	mName = CopyString(name);
}

const char* Binding::getName()
{
	return mName;
}

// 
// Scope
//

void Binding::setScope(const char* s)
{
    delete mScope;
    mScope = CopyString(s);
    parseScope();
}

const char* Binding::getScope() 
{
    return mScope;
}

/**
 * Parse a scope into track an group numbers.
 * Tracks are expected to be identified with integers starting
 * from 1.  Groups are identified with upper case letters A-Z.
 */
PRIVATE void Binding::parseScope()
{
    mTrack = 0;
    mGroup = 0;

    if (mScope != NULL) {
        int len = strlen(mScope);
        if (len > 1) {
            // must be a number 
            mTrack = atoi(mScope);
        }
        else if (len == 1) {
            char ch = mScope[0];
            if (ch >= 'A') {
                mGroup = (ch - 'A') + 1;
            }
            else {
                // normally an integer, anything else
                // collapses to zero
                mTrack = atoi(mScope);
            }
        }
    }
}

void Binding::setTrack(int t) 
{
    if (t > 0) {
        char buffer[32];
        sprintf(buffer, "%d", t);
        setScope(buffer);
    }
}

int Binding::getTrack()
{
	return mTrack;
}

void Binding::setGroup(int t) 
{
    if (t > 0) {
        char buffer[32];
        sprintf(buffer, "%c", (char)('A' + (t - 1)));
        setScope(buffer);
    }
}

int Binding::getGroup()
{
	return mGroup;
}

//
// Arguments
//

void Binding::setArgs(const char* args) 
{
	delete mArgs;
	mArgs = CopyString(args);
}

const char* Binding::getArgs() 
{
	return mArgs;
}

//
// Utilities
//

PUBLIC void Binding::getSummary(char* buffer)
{
	strcpy(buffer, "");

	// we display channel consistenly everywhere as 1-16
	int channel = mChannel + 1;

	if (mTrigger == TriggerNote) {
		char note[128];
		MidiNoteName(mValue, note);
		sprintf(buffer, "%d:%s", channel, note);
	}
	else if (mTrigger == TriggerProgram) {
		sprintf(buffer, "%d:Program %d", channel, mValue);
	}
	else if (mTrigger == TriggerControl) {
		sprintf(buffer, "%d:Control %d", channel, mValue);
	}
	else if (mTrigger == TriggerKey) {
		// UI should actually overload this with a smarter key 
		// rendering utility
		sprintf(buffer, "Key %d", mValue);
	}
    else if (mTrigger == TriggerOsc) {
        sprintf(buffer, "OSC %s", mTriggerPath);
    }

}

PUBLIC void Binding::getMidiString(char* buffer, bool includeChannel)
{
	strcpy(buffer, "");

	// we display channel consistenly everywhere as 1-16
	int channel = mChannel + 1;

	if (mTrigger == TriggerControl) {
		int value = getValue();
		if (value >= 0 && value < 128)
		  sprintf(buffer, "%d:Control %d", channel, value);
	}
	else if (mTrigger == TriggerNote) {
		char note[128];
		int value = getValue();
		if (value >= 0 && value < 128) {
			MidiNoteName(value, note);
			sprintf(buffer, "%d:%s", channel, note);
		}
	}
	else if (mTrigger == TriggerProgram) {
		int value = getValue();
		if (value >= 0 && value < 128)
		  sprintf(buffer, "%d:Program %d", channel, value);
	}
}


/**
 * Render a TriggerKey value as a readable string.
 */
PUBLIC void Binding::getKeyString(char* buffer, int max)
{
    strcpy(buffer, "");

    if (mValue == 0) {
        // this can't be bound  
    }
    else {
        GetKeyString(mValue, buffer);
        if (strlen(buffer) == 0)
          sprintf(buffer, "%d", mValue);
    }
}

/****************************************************************************
 *                                                                          *
 *   							 BINDING XML                                *
 *                                                                          *
 ****************************************************************************/

#define EL_BINDING "Binding"
#define ATT_DISPLAY_NAME "displayName"
#define ATT_TRIGGER "trigger"
#define ATT_VALUE "value"
#define ATT_CHANNEL "channel"
#define ATT_TRIGGER_VALUE "triggerValue"
#define ATT_TRIGGER_PATH "triggerPath"
#define ATT_TRIGGER_TYPE "triggerType"
#define ATT_TARGET_PATH "targetPath"
#define ATT_TARGET "target"
#define ATT_ARGS "args"
#define ATT_SCOPE "scope"
#define ATT_TRACK "track"
#define ATT_GROUP "group"

void Binding::parseXml(XmlElement* e) 
{
	// trigger
	mTrigger = Trigger::get(e->getAttribute(ATT_TRIGGER));
    mTriggerMode = TriggerMode::get(e->getAttribute(ATT_TRIGGER_TYPE));
	mValue = e->getIntAttribute(ATT_VALUE);
	mChannel = e->getIntAttribute(ATT_CHANNEL);

    // upgrade old name to new
    const char* path = e->getAttribute(ATT_TRIGGER_PATH);
    if (path == NULL)
      path = e->getAttribute(ATT_TRIGGER_VALUE);
    setTriggerPath(path);

	// target
    setTargetPath(e->getAttribute(ATT_TARGET_PATH));
	mTarget = Target::get(e->getAttribute(ATT_TARGET));
	setName(e->getAttribute(ATT_NAME));

	// scope
    setScope(e->getAttribute(ATT_SCOPE));

    // temporary backward compatibility
    setTrack(e->getIntAttribute(ATT_TRACK));
	setGroup(e->getIntAttribute(ATT_GROUP));

    // arguments
	setArgs(e->getAttribute(ATT_ARGS));
}

/**
 * Check to see if this object represents a valid binding.
 * Used during serialization to filter partially constructed bindings
 * that were created by the dialog.
 */
bool Binding::isValid()
{
	bool valid = false;

	if (mTrigger != NULL && mTarget != NULL && mName != NULL) {
		if (mTrigger == TriggerKey) {
			// key must have a non-zero value
			valid = (mValue > 0);
		}
		else if (mTrigger == TriggerNote ||
				 mTrigger == TriggerProgram ||
				 mTrigger == TriggerControl) {

			// hmm, zero is a valid value so no way to detect if
			// they didn't enter anything unless the UI uses negative
			// must have a midi status
			valid = (mValue >= 0);
		}
        else if (mTrigger == TriggerPitch) {
            // doesn't need a value
            valid = true;
        }
		else if (mTrigger == TriggerHost) {
			valid = true;
		}
        else if (mTrigger == TriggerOsc) {
            // anythign?
            valid = true;
        }
        else if (mTrigger == TriggerUI) {
            valid = true;
        }
		else {
			// not sure about mouse, wheel yet
		}
	}

	return valid;
}

void Binding::toXml(XmlBuffer* b)
{
	if (isValid()) {
		b->addOpenStartTag(EL_BINDING);

		// it reads better to lead with the target
        if (mTargetPath != NULL) {
            b->addAttribute(ATT_TARGET_PATH, mTargetPath);
        }
        else {
            b->addAttribute(ATT_SCOPE, mScope);
            b->addAttribute(ATT_TARGET, mTarget->getName());
            b->addAttribute(ATT_NAME, mName);
        }

        if (mTrigger != NULL)
          b->addAttribute(ATT_TRIGGER, mTrigger->getName());

        if (mTriggerMode != NULL)
          b->addAttribute(ATT_TRIGGER_TYPE, mTriggerMode->getName());

        // will have one of these but not both
        b->addAttribute(ATT_TRIGGER_PATH, mTriggerPath);
		b->addAttribute(ATT_VALUE, mValue);

		if (mTrigger == TriggerNote ||
			mTrigger == TriggerProgram ||
			mTrigger == TriggerControl)
		  b->addAttribute(ATT_CHANNEL, mChannel);

		b->addAttribute(ATT_ARGS, mArgs);

		b->add("/>\n");
	}
}

/****************************************************************************
 *                                                                          *
 *   							BINDING CONFIG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC BindingConfig::BindingConfig()
{
	init();
}

PUBLIC void BindingConfig::init()
{
	mNext = NULL;
	mName = NULL;
	mBindings = NULL;
}

PUBLIC BindingConfig::BindingConfig(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC BindingConfig::~BindingConfig()
{
	BindingConfig *el, *next;

	delete mBindings;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC Target* BindingConfig::getTarget()
{
	return TargetBindings;
}

PUBLIC void BindingConfig::setNext(BindingConfig* c)
{
	mNext = c;
}

PUBLIC BindingConfig* BindingConfig::getNext()
{
	return mNext;
}

PUBLIC Bindable* BindingConfig::getNextBindable()
{
	return mNext;
}

PUBLIC Binding* BindingConfig::getBindings()
{
	return mBindings;
}

PUBLIC void BindingConfig::setBindings(Binding* b)
{
	mBindings = b;
}

PUBLIC void BindingConfig::addBinding(Binding* b) 
{
    if (b != NULL) {
        // keep them ordered
        Binding *prev;
        for (prev = mBindings ; prev != NULL && prev->getNext() != NULL ; 
             prev = prev->getNext());
        if (prev == NULL)
          mBindings = b;
        else
          prev->setNext(b);
    }
}

PUBLIC void BindingConfig::removeBinding(Binding* b)
{
    if (b != NULL) {
        Binding *prev = NULL;
        Binding* el = mBindings;
    
        for ( ; el != NULL && el != b ; el = el->getNext())
          prev = el;

        if (el == b) {
            if (prev == NULL)
              mBindings = b->getNext();
            else
              prev->setNext(b->getNext());
        }
        else {
            // not on the list, should we still NULL out the next pointer?
            Trace(1, "BindingConfig::removeBinding binding not found!\n");
        }

        b->setNext(NULL);
    }
}

/**
 * Search for a binding for a given trigger and value.
 * This is intended for upgrading old KeyBinding objects, once that
 * has passed we can delete this.
 */
PUBLIC Binding* BindingConfig::getBinding(Trigger* trigger, int value)
{
	Binding* found = NULL;

	for (Binding* b = mBindings ; b != NULL ; b = b->getNext()) {
		if (b->getTrigger() == trigger && b->getValue() == value) {
            found = b;
            break;
		}
	}

	return found;
}

void BindingConfig::parseXml(XmlElement* e)
{
	parseXmlCommon(e);

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_BINDING)) {
			Binding* mb = new Binding(child);
			// can't filter bogus functions yet, scripts aren't loaded
			addBinding(mb);
		}
	}
}

void BindingConfig::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_BINDING_CONFIG);

	// name, number
	toXmlCommon(b);

	b->add(">\n");
	b->incIndent();

	for (Binding* c = mBindings ; c != NULL ; c = c->getNext())
	  c->toXml(b);

	b->decIndent();
	b->addEndTag(EL_BINDING_CONFIG);
}

PUBLIC BindingConfig* BindingConfig::clone()
{
	BindingConfig* clone = new BindingConfig();

	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	char* xml = b->stealString();
	delete b;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(xml);
	if (d != NULL) {
		XmlElement* e = d->getChildElement();
		clone = new BindingConfig(e);
		delete d;
	}
    else {
        // must have been a parser error, not supposed
        // to happen
        Trace(1, "Parse error while cloning BindingConfig!!\n");
    }
	delete p;
	delete xml;

	return clone;
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
