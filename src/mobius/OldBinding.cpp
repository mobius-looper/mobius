/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Old MidiConfig classes for MIDI function binding.
 * The model has to be kept around for awhile so it can be auto-upgraded
 * during the initial load of the configuration.   Once this happens
 * we shouldn't see these objects any more.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"
#include "Qwin.h"

#include "MidiByte.h"
#include "MidiUtil.h"
#include "MidiEvent.h"
#include "MidiMap.h"

#include "Mobius.h"
#include "MobiusConfig.h"
#include "Track.h"
#include "Function.h"
#include "Parameter.h"
#include "Script.h"
#include "Event.h"
#include "Sample.h"
#include "Setup.h"

#include "Binding.h"
#include "OldBinding.h"

//////////////////////////////////////////////////////////////////////
//
// XML Constants
//
//////////////////////////////////////////////////////////////////////

#define EL_MIDI_BINDING "MidiBinding"
#define ATT_NAME "name"
#define ATT_TYPE "type"
#define ATT_VALUE "value"
#define ATT_STATUS "status"
#define ATT_CHANNEL "channel"
#define ATT_TRACK "track"
#define ATT_GROUP "group"
#define ATT_TRACK_GROUPS "trackGroups"
#define ATT_PEDAL_MODE "pedalMode"

#define STATUS_NOTE "note"
#define STATUS_CONTROL "control"
#define STATUS_PROGRAM "program"

/****************************************************************************
 *                                                                          *
 *                           MIDI CONFIG MANAGEMENT                         *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiConfig* MobiusConfig::getMidiConfigs()
{
	return mMidiConfigs;
}

PUBLIC const char* MobiusConfig::getSelectedMidiConfig()
{
    return mSelectedMidiConfig;
}

PUBLIC void MobiusConfig::clearMidiConfigs()
{
    delete mMidiConfigs;
    delete mSelectedMidiConfig;
    mMidiConfigs = NULL;
    mSelectedMidiConfig = NULL;
}
	
PUBLIC void MobiusConfig::addMidiConfig(MidiConfig* c) 
{
	// keep them ordered
	MidiConfig *prev;
	for (prev = mMidiConfigs ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());
	if (prev == NULL)
	  mMidiConfigs = c;
	else
	  prev->setNext(c);
}

PUBLIC void MobiusConfig::setSelectedMidiConfig(const char* s)
{
    delete mSelectedMidiConfig;
    mSelectedMidiConfig = CopyString(s);
}

/****************************************************************************
 *                                                                          *
 *   							 MIDI BINDING                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiBinding::MidiBinding()
{
	init();
}

PUBLIC MidiBinding::MidiBinding(XmlElement* e)
{
	init();
	parseXml(e);
}

PRIVATE void MidiBinding::init()
{
	mNext = NULL;
	mName = NULL;
	mType = BindableFunction;
	mTrack = 0;
	mGroup = 0;
	mChannel = 0;
	mStatus = NONE;
	mValue = 0;
}

PUBLIC MidiBinding::~MidiBinding()
{
	MidiBinding *el, *next;

	delete mName;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}

}

PUBLIC void MidiBinding::setNext(MidiBinding* c)
{
	mNext = c;
}

PUBLIC MidiBinding* MidiBinding::getNext()
{
	return mNext;
}


PUBLIC void MidiBinding::setType(BindingType type) 
{
	mType = type;
}

PUBLIC BindingType MidiBinding::getType()
{
	return mType;
}

PUBLIC void MidiBinding::setName(const char *name) 
{
	delete mName;
	mName = CopyString(name);
}

PUBLIC const char* MidiBinding::getName()
{
	return mName;
}

PUBLIC void MidiBinding::setTrack(int t) 
{
	mTrack = t;
}

PUBLIC int MidiBinding::getTrack()
{
	return mTrack;
}

PUBLIC void MidiBinding::setGroup(int t) 
{
	mGroup = t;
}

PUBLIC int MidiBinding::getGroup()
{
	return mGroup;
}

void MidiBinding::setStatus(MidiStatus s)
{
	mStatus = s;
}

MidiStatus MidiBinding::getStatus()
{
	return mStatus;
}

void MidiBinding::setChannel(int c) 
{
	mChannel = c;
}

int MidiBinding::getChannel()
{
	return mChannel;
}

void MidiBinding::setValue(int v) 
{
	mValue = v;
}

int MidiBinding::getValue()
{
	return mValue;
}

void MidiBinding::parseXml(XmlElement* e) 
{
	setName(e->getAttribute(ATT_NAME));
	setType(getBindingType(e->getAttribute(ATT_TYPE)));
	setTrack(e->getIntAttribute(ATT_TRACK));
	setGroup(e->getIntAttribute(ATT_GROUP));
	setChannel(e->getIntAttribute(ATT_CHANNEL));
	const char* status = e->getAttribute(ATT_STATUS);
	if (status != NULL) {
		if (!strcmp(status, STATUS_CONTROL))
		  mStatus = CONTROL;
		else if (!strcmp(status, STATUS_NOTE))
		  mStatus = NOTE;
		else if (!strcmp(status, STATUS_PROGRAM))
		  mStatus = PROGRAM;
	}

	const char *v = e->getAttribute(ATT_VALUE);
	if (v != NULL)
	  mValue = ToInt(v);
}

const char* MidiBinding::getBindingTypeName(BindingType type) 
{
	const char* name = "function";
	switch (type) {
		case BindableControl: name = "control"; break;
		case BindableSetup:   name = "setup"; break;
		case BindablePreset:  name = "preset"; break;
		case BindableMidiConfig:    name = "midi"; break;
		case BindableFunction: name = "function"; break;
	}
	return name;
}

BindingType MidiBinding::getBindingType(const char* name)
{
	BindingType type = BindableFunction;
	if (name != NULL) {
		if (!strcmp(name, "function"))
		  type = BindableFunction;
		else if (!strcmp(name, "control"))
		  type = BindableControl;
		else if (!strcmp(name, "setup"))
		  type = BindableSetup;
		else if (!strcmp(name, "preset"))
		  type = BindablePreset;
		else if (!strcmp(name, "midi"))
		  type = BindableMidiConfig;
	}
	return type;
}

void MidiBinding::toXml(XmlBuffer* b)
{
	// filter out unbound functions during serialization since
	// MidiControlDialog will generate one for everything that gets clicked
	if (mStatus != NONE) {

		b->addOpenStartTag(EL_MIDI_BINDING);
		b->addAttribute(ATT_TYPE, getBindingTypeName(mType));
		b->addAttribute(ATT_NAME, mName);

		const char *status = NULL;
		switch (mStatus) {
			case CONTROL:
				status = STATUS_CONTROL;
				break;
			case NOTE:
				status = STATUS_NOTE;
				break;
			case PROGRAM:
				status = STATUS_PROGRAM;
				break;
			case NONE:
				break;
		}
		if (mTrack > 0)
		  b->addAttribute(ATT_TRACK, mTrack);
		if (mGroup > 0)
		  b->addAttribute(ATT_GROUP, mGroup);
		b->addAttribute(ATT_CHANNEL, mChannel);
		b->addAttribute(ATT_STATUS, status);
		b->addAttribute(ATT_VALUE, mValue);
		b->add("/>\n");
	}
}

PUBLIC void MidiBinding::getMidiString(char* buffer, bool includeChannel)
{
	MidiStatus status = getStatus();

	strcpy(buffer, "");

	// we display channel consistenly everywhere as 1-16
	int channel = mChannel + 1;

	if (status == CONTROL) {
		int value = getValue();
		if (value >= 0 && value < 128)
		  sprintf(buffer, "%d:Control %d", channel, value);
	}
	if (status == NOTE) {
		char note[128];
		int value = getValue();
		if (value >= 0 && value < 128) {
			MidiNoteName(value, note);
			sprintf(buffer, "%d:%s", channel, note);
		}
	}
	else if (status == PROGRAM) {
		int value = getValue();
		if (value >= 0 && value < 128)
		  sprintf(buffer, "%d:Program %d", channel, value);
	}
}

/****************************************************************************
 *                                                                          *
 *   							 MIDI CONFIG                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiConfig::MidiConfig()
{
	init();
}

PUBLIC void MidiConfig::init()
{
	mNext = NULL;
	mName = NULL;
	mBindings = NULL;
}

PUBLIC MidiConfig::MidiConfig(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC MidiConfig::~MidiConfig()
{
	MidiConfig *el, *next;

	delete mBindings;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC Target* MidiConfig::getTarget()
{
	return TargetBindings;
}

PUBLIC void MidiConfig::setNext(MidiConfig* c)
{
	mNext = c;
}

PUBLIC MidiConfig* MidiConfig::getNext()
{
	return mNext;
}

PUBLIC Bindable* MidiConfig::getNextBindable()
{
	return mNext;
}

PUBLIC void MidiConfig::setTrackGroups(int g)
{
	if (g >= 0 && g <= MAX_TRACK_GROUPS)
	  mTrackGroups = g;
}

PUBLIC int MidiConfig::getTrackGroups()
{
	return mTrackGroups;
}

PUBLIC MidiBinding* MidiConfig::getBindings()
{
	return mBindings;
}

PUBLIC void MidiConfig::addBinding(MidiBinding* p) 
{

	// keep them ordered
	MidiBinding *prev;
	for (prev = mBindings ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());
	if (prev == NULL)
	  mBindings = p;
	else
	  prev->setNext(p);
}

PUBLIC void MidiConfig::removeBinding(MidiBinding* c)
{
}



void MidiConfig::parseXml(XmlElement* e)
{
	parseXmlCommon(e);
	setTrackGroups(e->getIntAttribute(ATT_TRACK_GROUPS));

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_MIDI_BINDING)) {
			MidiBinding* mb = new MidiBinding(child);
			// auto-filter bogus bindings
			// NO, if scripts haven't been loaded yet these won't resolve
			//if (mb->getFunction() != NULL)
			addBinding(mb);
		}
	}
}

void MidiConfig::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_MIDI_CONFIG);
	// name, number
	toXmlCommon(b);

	b->addAttribute(ATT_TRACK_GROUPS, mTrackGroups);

	b->add(">\n");
	b->incIndent();

	for (MidiBinding* c = mBindings ; c != NULL ; c = c->getNext())
	  c->toXml(b);

	b->decIndent();
	b->addEndTag("MidiConfig");
}

PUBLIC MidiConfig* MidiConfig::clone()
{
	MidiConfig* clone = new MidiConfig();

	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	char* xml = b->stealString();
	delete b;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(xml);
	if (d != NULL) {
		XmlElement* e = d->getChildElement();
		clone = new MidiConfig(e);
		delete d;
	}
	delete p;
	delete xml;

	return clone;
}

/****************************************************************************
 *                                                                          *
 *                                 CONVERSION                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Build a new BindingConfig from an old MidiConfig.
 */
PUBLIC BindingConfig* MidiConfig::upgrade()
{
    BindingConfig* config = new BindingConfig();
    config->setName(mName);

    for (MidiBinding* old = mBindings ; old != NULL ; old = old->getNext()) {
        
        Binding* neu = new Binding();

        // trigger
        Trigger* trigger = NULL;
        MidiStatus status = old->getStatus();
        switch (status) {
            case NOTE: trigger = TriggerNote; break;
            case CONTROL: trigger = TriggerControl; break;
            case PROGRAM: trigger = TriggerProgram; break;
			case NONE: break;
        }
        neu->setTrigger(trigger);
        neu->setChannel(old->getChannel());
        neu->setValue(old->getValue());

        // target
        Target* target = NULL;
        BindingType type = old->getType();
        switch (old->getType()) {
            case BindableFunction: 
                target = TargetFunction;
                break;
            case BindableControl:
                target = TargetParameter;
                break;
            case BindableSetup:
                target = TargetSetup;
                break;
            case BindablePreset:
                target = TargetPreset;
                break;
            case BindableMidiConfig:
                target = TargetBindings;
                break;
        }
        neu->setTarget(target);
        neu->setName(old->getName());

        // scope
        neu->setTrack(old->getTrack());
        neu->setGroup(old->getGroup());

        config->addBinding(neu);
	}

    return config;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

