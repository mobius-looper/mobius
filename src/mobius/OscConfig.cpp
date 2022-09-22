/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * All things OSC
 *
 * How the message and the argument is processed depends on options
 * that can are specified in the address.  Binding.triggerMode may
 * override this.
 *
 * /mobius/<function>
 *   TriggerModeMomentary, 0.0 to 1.0
 * 
 * /mobius/noup/<function>
 *   TriggerModeOnce, argument ignored
 * 
 * /mobius/function/arg
 *   TriggerModeOnce, osc argument passed as function argument
 * 
 * /mobius/function/<arg>
 *   TriggerModeMomentary, 0.0 to 1.0
 * 
 * /mobius/noup/function/<arg>
 *   TriggerModeOnce, argument ignroed
 * 
 * /mobius/noup/function/arg
 *   TriggerModeOnce, not a meaningful combo, arg implies noup
 * 
 * /mobius/parameter
 *   TriggerModeContinuous, 0.0 to 1.0
 * 
 * /mobius/noup/parameter
 *   TriggerModeContinuous, not meaningful, continuous implies noup
 * 
 * /mobius/range(x,y)/parameter
 *   TriggerModeContinuous, user defined range
 * 
 * /mobius/parameter/<value>
 *   TriggerModeOnce, argument ignored
 *
 * /mobius/parameter/arg
 *   TriggerModeOnce, argument is passed as ordinal
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "Map.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"

// these are okay
#include "Action.h"
#include "MobiusInterface.h"
#include "Export.h"
#include "Expr.h"

// these should be refactored
#include "Function.h"
#include "Binding.h"
#include "MobiusConfig.h"
#include "Track.h"

#include "OscConfig.h"

//////////////////////////////////////////////////////////////////////
//
// XML CONSTANTS
//
//////////////////////////////////////////////////////////////////////

#define ATT_NAME "name" 
#define ATT_INPUT_PORT "inputPort"
#define ATT_OUTPUT_PORT "outputPort"
#define ATT_OUTPUT_HOST "outputHost"
#define ATT_TRACE "trace" 
#define EL_BINDING_SET "OscBindingSet"
#define EL_OSC_CONFIG "OscConfig"
#define EL_COMMENTS "Comments"

#define EL_WATCHER "OscWatcher"
#define ATT_PATH "path"
#define ATT_TRACK "track"

// duplicated from Binding.cpp, not a good way to share
#define EL_BINDING "Binding"

//////////////////////////////////////////////////////////////////////
//
// Scaling
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert an OSC argument from the device to an internal target value.
 * The body of this was moved to Util because we need it in a few places.
 */
PRIVATE int OscScaleValueIn(float value, int min, int max)
{
    int ivalue = 0;

    if (min == 0 && max == 0) {
        // something wrong with the binding, ignore
    }
    else {
        // originally let boolean be zero, non-zero
        // but keeping it consistent with enumerations
        // that only have two values (presets)
        ivalue = ScaleValueIn(value, min, max);
    }

    return ivalue;
}


/**
 * Scale an internal value to one that can be sent back to the device.
 * TouchOSC uses floats from 0.0 to 1.0 like VST.
 * Use that for now but need more generalized scaling!!
 */
PRIVATE float OscScaleValueOut(int value, int min, int max)
{
    float fvalue = 0.0f;

    if (min == 0 && max == 0) {
        // something wrong with the bindings, ignore
    }
    else if (min == 0 && max == 1) {
        // shortcut for boolean so sliders snap
        // to the edges rather than the middle
        fvalue = (value > 0) ? 1.0f : 0.0f;
    }
    else {
        fvalue = ScaleValueOut(value, min, max);
    }

    return fvalue;
}

//////////////////////////////////////////////////////////////////////
//
// OscConfig
//
//////////////////////////////////////////////////////////////////////

PUBLIC OscConfig::OscConfig()
{
	init();
}

PUBLIC OscConfig::OscConfig(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC OscConfig::OscConfig(const char* xml)
{
    init();
    mError[0] = 0;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(xml);
    XmlElement* e = NULL;

	if (d != NULL)
      e = d->getChildElement();

    if (e != NULL)
      parseXml(e);
    else {
        // must have been a parse error
        Trace(1, "Error parsing OSC config file: %s\n", p->getError());
        CopyString(p->getError(), mError, sizeof(mError));
    }
    delete d;
	delete p;
}

PRIVATE void OscConfig::init()
{
	mInputPort = 0;
	mOutputHost = NULL;
	mOutputPort = 0;
	mBindings = NULL;
    mWatchers = NULL;
}

PUBLIC OscConfig::~OscConfig()
{
	delete mOutputHost;
	delete mBindings;
    delete mWatchers;
}

PUBLIC const char* OscConfig::getError()
{
    return (mError[0] != 0) ? mError : NULL;
}

PUBLIC int OscConfig::getInputPort()
{
	return mInputPort;
}

PUBLIC void OscConfig::setInputPort(int i)
{
	mInputPort = i;
}

PUBLIC const char* OscConfig::getOutputHost()
{
	return mOutputHost;
}

PUBLIC void OscConfig::setOutputHost(const char* s)
{
	delete mOutputHost;
	mOutputHost = CopyString(s);
}

PUBLIC int OscConfig::getOutputPort()
{
	return mOutputPort;
}

PUBLIC void OscConfig::setOutputPort(int i)
{
	mOutputPort = i;
}

PUBLIC OscBindingSet* OscConfig::getBindings()
{
	return mBindings;
}

PUBLIC OscWatcher* OscConfig::getWatchers()
{
    return mWatchers;
}

PRIVATE void OscConfig::parseXml(XmlElement* e)
{
	OscBindingSet* last = NULL;
    OscWatcher* lastWatcher = NULL;

	mInputPort = e->getIntAttribute(ATT_INPUT_PORT);
	mOutputPort = e->getIntAttribute(ATT_OUTPUT_PORT);
	setOutputHost(e->getAttribute(ATT_OUTPUT_HOST));

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_BINDING_SET)) {
			OscBindingSet* b = new OscBindingSet(child);
			if (mBindings == NULL)
			  mBindings = b;
			else 
			  last->setNext(b);
			last = b;
		}
        else if (child->isName(EL_WATCHER)) {
            OscWatcher* w = new OscWatcher(child);
            if (mWatchers == NULL)
              mWatchers = w;
            else 
              lastWatcher->setNext(w);
            lastWatcher = w;
        }
    }
}

PRIVATE void OscConfig::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_OSC_CONFIG);
	b->addAttribute(ATT_INPUT_PORT, mInputPort);
	b->addAttribute(ATT_OUTPUT_PORT, mOutputPort);
	b->addAttribute(ATT_OUTPUT_HOST, mOutputHost);
	b->add(">\n");
	b->incIndent();

    for (OscWatcher* w = mWatchers ; w != NULL ; w = w->getNext())
      w->toXml(b);

	for (OscBindingSet* set = mBindings ; set != NULL ; set = set->getNext())
	  set->toXml(b);


	b->decIndent();
	b->addEndTag(EL_OSC_CONFIG);
}

//////////////////////////////////////////////////////////////////////
//
// OscBindingSet
//
//////////////////////////////////////////////////////////////////////

PUBLIC OscBindingSet::OscBindingSet()
{
	init();
}

PUBLIC OscBindingSet::OscBindingSet(XmlElement* e)
{
	init();
	parseXml(e);
}

PRIVATE void OscBindingSet::init()
{
	mNext = NULL;
    mName = NULL;
    mComments = NULL;
    mActive = false;
	mInputPort = 0;
	mOutputHost = NULL;
	mOutputPort = 0;
	mBindings = NULL;
}

PUBLIC OscBindingSet::~OscBindingSet()
{
	OscBindingSet *el, *next;

    delete mName;
    delete mComments;
	delete mOutputHost;
	delete mBindings;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC OscBindingSet* OscBindingSet::getNext()
{
    return mNext;
}

PUBLIC void OscBindingSet::setNext(OscBindingSet* s)
{
    mNext = s;
}

PUBLIC const char* OscBindingSet::getName()
{
    return mName;
}

PUBLIC void OscBindingSet::setName(const char* s)
{
    delete mName;
    mName = CopyString(s);
}

PUBLIC const char* OscBindingSet::getComments()
{
    return mComments;
}

PUBLIC void OscBindingSet::setComments(const char* s)
{
    delete mComments;
    mComments = CopyString(s);
}

PUBLIC bool OscBindingSet::isActive()
{
    // ignore the active flag for now until we have a UI
    return true;
}

PUBLIC void OscBindingSet::setActive(bool b)
{
    mActive = b;
}

PUBLIC int OscBindingSet::getInputPort()
{
	return mInputPort;
}

PUBLIC void OscBindingSet::setInputPort(int i)
{
	mInputPort = i;
}

PUBLIC const char* OscBindingSet::getOutputHost()
{
	return mOutputHost;
}

PUBLIC void OscBindingSet::setOutputHost(const char* s)
{
	delete mOutputHost;
	mOutputHost = CopyString(s);
}

PUBLIC int OscBindingSet::getOutputPort()
{
	return mOutputPort;
}

PUBLIC void OscBindingSet::setOutputPort(int i)
{
	mOutputPort = i;
}

PUBLIC Binding* OscBindingSet::getBindings()
{
	return mBindings;
}

PRIVATE void OscBindingSet::parseXml(XmlElement* e)
{
	Binding* last = NULL;

	mInputPort = e->getIntAttribute(ATT_INPUT_PORT);
	mOutputPort = e->getIntAttribute(ATT_OUTPUT_PORT);
	setOutputHost(e->getAttribute(ATT_OUTPUT_HOST));

    setName(e->getAttribute(ATT_NAME));

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_BINDING)) {
			Binding* b = new Binding(child);
			if (mBindings == NULL)
			  mBindings = b;
			else 
			  last->setNext(b);
			last = b;
		}
        else if (child->isName(EL_COMMENTS)) {
            setComments(child->getContent());
        }
	}
}

PRIVATE void OscBindingSet::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_OSC_CONFIG);
	b->addAttribute(ATT_INPUT_PORT, mInputPort);
	b->addAttribute(ATT_OUTPUT_PORT, mOutputPort);
	b->addAttribute(ATT_OUTPUT_HOST, mOutputHost);
	b->add(">\n");
	b->incIndent();

    if (mComments != NULL) {
        b->addStartTag(EL_COMMENTS);
        b->add(mComments);
        b->addEndTag(EL_COMMENTS);
    }

	for (Binding* binding = mBindings ; binding != NULL ; 
		 binding = binding->getNext())
	  binding->toXml(b);

	b->decIndent();
	b->addEndTag(EL_OSC_CONFIG);
}

//////////////////////////////////////////////////////////////////////
//
// OscBinding
//
//////////////////////////////////////////////////////////////////////

PUBLIC OscBinding::OscBinding(MobiusInterface* m, Binding* b, Action* a)
{
    init();
    mMobius = m;
    mAction = a;

    // action needs a unique id for up/down tracking with script targets
    // anythig will do as long as it will be the same for both down and
    // up messages
    mAction->id = (long)this;

    // ExportAddress is used when the trigger OSC path is different
    // than the target
    mExportAddress = CopyString(b->getTriggerPath());
    if (mExportAddress == NULL)
      mExportAddress = CopyString(b->getTargetPath());

    // Until we have a flag in the Binding to drive this,
    // assume that anything that expects a continuous trigger
    // is exportable.  
    // NOTE: !continuous scripts will have TriggerModeContinuous
    // but we can't export those.
    if (mAction->triggerMode == TriggerModeContinuous &&
        mAction->getTarget() != TargetFunction) {

        // this will return NULL if not exportable
        mExport = mMobius->resolveExport(mAction);

        if (mExport != NULL) {
            mMin = mExport->getMinimum();
            mMax = mExport->getMaximum();
        }
    }
}

PUBLIC void OscBinding::init()
{
    mNext = NULL;
    mMobius = NULL;
    mAction = NULL;
    mExport = NULL;
    mExportDevice = NULL;
    mExportAddress = NULL;
    mExportable = false;
    mMin = 0;
    mMax = 0;
    mId = 0;
}

PUBLIC OscBinding::~OscBinding()
{
    delete mAction;
    delete mExport;
    delete mExportAddress;
    
	OscBinding *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC bool OscBinding::isResolved()
{
    // this did the work
    return mAction->getResolvedTarget()->isResolved();
}

PUBLIC bool OscBinding::isExportable()
{
    return (mExport != NULL);
}

PUBLIC OscBinding* OscBinding::getNext()
{
    return mNext;
}

PUBLIC void OscBinding::setNext(OscBinding* e)
{
    mNext = e;
}

PUBLIC Action* OscBinding::getAction()
{
    return mAction;
}

PUBLIC void OscBinding::setExportDevice(OscDevice* d)
{
    mExportDevice = d;
}

PUBLIC OscDevice* OscBinding::getExportDevice()
{
    return mExportDevice;
}

PUBLIC const char* OscBinding::getExportAddress()
{
    return mExportAddress;
}

//
// Incomming Changes
//

/**
 * Change the value given an incomming argument.
 * See file header comments for what Mobius will expect
 * for certain TriggerModes and target path options.
 */
PUBLIC void OscBinding::setValue(float value)
{
    Target* target = mAction->getTarget();
    TriggerMode* mode = mAction->triggerMode;
    int ivalue = 0;
    bool down = false;
    bool doit = true;
    bool setarg = true;

    if (mode == TriggerModeContinuous) {
        // TODO: scale if there was a range(low,high) on the path
        ivalue = OscScaleValueIn(value, mMin, mMax);
    }
    else if (mode == TriggerModeOnce) {
        if (mAction->passOscArg) {
            // expected to be an ordinal
            // ignore of negative for buttons that have to 
            // send an up message
            if (value >= 0.0)
              ivalue = (int)value;
            else
              doit = false;
        }
        else {
            // A path with an explicit value, ignore unless
            // osc arg is positive, so momentary buttons
            // don't set it twice
            doit = (value > 0.0);
            setarg = false;
        }
    }
    else if (mode == TriggerModeMomentary || mode == TriggerModeToggle) {
        // sustainable, needs down/up
        down = (value != 0.0f);
        setarg = false;
    }
    else {
        // shouldn't be here
        Trace(1, "OscBinding: invalid TriggerMode\n");
        doit = false;
    }

    if (doit) {
        // clone the action and decorate it
        // !! consider passing the float in and letting Mobius
        // do the range checking, could be resued for host parameters
        Action* a = mMobius->cloneAction(mAction);
        a->down = down;
        if (setarg)
          a->arg.setInt(ivalue);
        mMobius->doAction(a);
    }
}

//
// Outgoing changes
//

/**
 * Refresh the exported value and return true if it changed
 * since the last export
 */
PUBLIC bool OscBinding::refreshValue()
{
	bool changed = false;
    if (mExport != NULL) {
        int value = mExport->getOrdinalValue();
        if (value != mExport->getLast()) {
            changed = true;
            mExport->setLast(value);
        }
    }
	return changed;
}

/**
 * Convert the internal value to export to a scaled float.
 */
PUBLIC float OscBinding::getExportValue()
{
    return OscScaleValueOut(mExport->getLast(), mMin, mMax);
}

//////////////////////////////////////////////////////////////////////
//
// OscWatcher
//
//////////////////////////////////////////////////////////////////////

PUBLIC OscWatcher::OscWatcher()
{
    init();
}

PUBLIC OscWatcher::OscWatcher(XmlElement* e)
{
    init();
    parseXml(e);
}

void OscWatcher::init()
{
    mNext = NULL;
    mPath = NULL;
    mName = NULL;
    mTrack = 0;
}

PUBLIC OscWatcher::~OscWatcher()
{
    delete mName;
    delete mPath;
    
	OscWatcher *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC OscWatcher* OscWatcher::getNext()
{
    return mNext;
}

PUBLIC void OscWatcher::setNext(OscWatcher* w)
{
    mNext = w;
}

PUBLIC const char* OscWatcher::getPath()
{
    return mPath;
}

PUBLIC void OscWatcher::setPath(const char* path)
{
    delete mPath;
    mPath = CopyString(path);
}

PUBLIC const char* OscWatcher::getName()
{
    return mName;
}

PUBLIC void OscWatcher::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

PUBLIC int OscWatcher::getTrack()
{
    return mTrack;
}

PUBLIC void OscWatcher::setTrack(int t)
{
    mTrack = t;
}

void OscWatcher::parseXml(XmlElement* e)
{
    mPath = CopyString(e->getAttribute(ATT_PATH));
    mName = CopyString(e->getAttribute(ATT_NAME));
    mTrack = e->getIntAttribute(ATT_TRACK);
}

PUBLIC void OscWatcher::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_WATCHER);
	b->addAttribute(ATT_PATH, mPath);
	b->addAttribute(ATT_NAME, mName);
    b->addAttribute(ATT_TRACK, mTrack);
    b->add("/>\n");
}

//////////////////////////////////////////////////////////////////////
//
// OscRuntimeWatcher
//
//////////////////////////////////////////////////////////////////////

#define DECAY_TICKS 2

OscRuntimeWatcher::OscRuntimeWatcher(OscConfig* config, OscWatcher* src)
{
    // copy these since this object will have an independent lifespan
    mPath = CopyString(src->getPath());
    mName = CopyString(src->getName());
    mTrack = src->getTrack();

    mBehavior = WATCH_MOMENTARY;
    mMin = 0;
    mMax = 0;
    mOsc = NULL;
    mDevice = NULL;
    mLast = 0;
    mSends = 0;
    mTicks = 0;
    mPending = false;
    mPendingValue = 0;
    mDecaying = false;
    mTrace = false;
}

OscRuntimeWatcher::~OscRuntimeWatcher()
{
    delete mPath;
    delete mName;
}

void OscRuntimeWatcher::finish(MobiusInterface* m, WatchPoint* wp, 
                               OscInterface* osc, OscDevice* dev)
{
    // we could maintain a handle to this since it's a system object
    mBehavior = wp->getBehavior();
    mMin = wp->getMin(m);
    mMax = wp->getMax(m);
    mOsc = osc;
    mDevice = dev;

    MobiusConfig* mconfig = m->getConfiguration();
    mTrace = mconfig->isOscTrace();
}

void OscRuntimeWatcher::setTrace(bool b)
{
    mTrace = b;
}

const char* OscRuntimeWatcher::getWatchPointName()
{
    return mName;
}

int OscRuntimeWatcher::getWatchPointTrack()
{
    return mTrack;
}

void OscRuntimeWatcher::watchPointEvent(int value)
{
    if (value != mLast || mSends == 0) {

        bool doItNow = false;
        
        if (doItNow) {
            mLast = value;
            mSends++;
            mTicks = 0;
            float fvalue = OscScaleValueOut(value, mMin, mMax);
            send(fvalue);
        }
        else {
            mPending = true;
            mPendingValue = value;
        }
    }
}


/**
 * Called indirectly by MobiusThread every 1/10 second.
 */
void OscRuntimeWatcher::tick()
{
    if (mPending) {
        mPending = false;
        mLast = mPendingValue;
        mSends++;   
        mTicks = 0;
        mDecaying = (mBehavior == WATCH_MOMENTARY);
        
        float fvalue = OscScaleValueOut(mPendingValue, mMin, mMax);
        //Trace(2, "OSC on\n");
        send(fvalue);
    }
    else if (mDecaying) {
        mTicks++;
        if (mTicks >= DECAY_TICKS) {
            mDecaying = 0;
            mTicks = 0;
            mLast = 0;
            //Trace(2, "OSC off\n");
            send(0.0);
        }
    }
}

void OscRuntimeWatcher::send(float value)
{
    if (mDevice != NULL) {
        OscMessage msg;

        msg.setAddress(mPath);
        msg.setArg(0, value);

        if (mTrace) {
            printf("OSC send: %s %f\n", mPath, value);
            fflush(stdout);

            // gag, background Trace doesn't like floats
            char buf[80];
            sprintf(buf, "%f", value);
            Trace(2, "OSC send: %s %s\n", mPath, buf);
        }
    
        mOsc->send(mDevice, &msg);
    }
}

//////////////////////////////////////////////////////////////////////
//
// OscResolver
//
//////////////////////////////////////////////////////////////////////

OscResolver::OscResolver(MobiusInterface* mobius, OscInterface* osc, 
                         OscConfig* config)
{
    mMobius = mobius;
    mOsc = osc;
    mConfig = config;
    mBindings = NULL;
    mBindingMap = NULL;
	mExports = NULL;

    // formerly in OscConfig, this is now global
    MobiusConfig* mconfig = mobius->getConfiguration();
    mTrace = mconfig->isOscTrace();

    if (mConfig != NULL) {
        // find the active set
        OscBindingSet* active = NULL;
        for (OscBindingSet* set = mConfig->getBindings() ; set != NULL ; 
             set = set->getNext()) {
            if (set->isActive()) {
                active = set;
                break;
            }
        }

        // if none explicitly marked active, pick the first
        if (active == NULL)
          active = mConfig->getBindings();

        if (active != NULL) {
            for (Binding* b = active->getBindings() ; b != NULL ; 
                 b = b->getNext()) {
                addBinding(active, b);
            }
        }
    }
}

OscResolver::~OscResolver()
{
    // we don't own mOsc
    delete mExports;
    delete mBindings;
    delete mConfig;

    // !! this contains address copies that will leak
    // need a way to tell it to release keys
    delete mBindingMap;

	OscResolver *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC OscResolver* OscResolver::getNext()
{
    return mNext;
}

PUBLIC void OscResolver::setNext(OscResolver* res)
{
    mNext = res;
}

void OscResolver::setTrace(bool b)
{
    mTrace = b;
}

/**
 * Used during initialization to install one binding.
 * The Binding is converted to an Action with a ResolvedTarget
 * like we do for other trigger types.  But unlike key/midi bindings
 * we resolve using a target path rather than explicit target, name, 
 * and value attributes.
 *
 * This will also build a list of Exports if any of the bound
 * targets are exportable.
 */
PRIVATE void OscResolver::addBinding(OscBindingSet* set, Binding* b)
{
    const char* trigger = b->getTriggerPath();
    const char* target = b->getTargetPath();

    if (trigger == NULL) {
        Trace(1, "OscRuntime::addBinding missing triggerValue\n");
    }
    else if (target == NULL) {
        // you must have a target path, not an old-style <Binding>
        // with target, name, and value attributes split out
        // I like to have bindings without targets in the config to serve
        // as documentation for the things that can be bound so don't
        // trace an error.
        //Trace(1, "OscRuntime::addBinding missing targetPath\n");
    }
    else {
        // this is optional in the XML, set it so Mobius won't complain
        b->setTrigger(TriggerOsc);

        Action* a = mMobius->resolveAction(b);
        if (a == NULL) {
            char buf[128];
            b->getSummary(buf);
            Trace(1, "OscRuntime: Unresolved target for trigger: %s\n", buf);
        }
        else {
            // need a stable key for the Map
            a->setName(trigger);

            // add it to our list
            OscBinding* ob = new OscBinding(mMobius, b, a);
            addBinding(ob);

            // add to the export list if exportable and we can determine
            // where it should go
            addExport(set, ob);
        }
    }
}

/**
 * Add our wrapped OscBinding to the list and the map.
 */
PRIVATE void OscResolver::addBinding(OscBinding* b)
{
    // address must have been saved here
    Action* action = b->getAction();
    const char* address = action->getName();
    if (address == NULL)
      Trace(1, "Attempt to add binding without address\n");
    else {
        b->setNext(mBindings);
        mBindings = b;

        if (mBindingMap == NULL)
          mBindingMap = new Map();
        mBindingMap->put(address, b);
    }
}

/**
 * Lookup an OscBinding by path received.
 */
PRIVATE OscBinding* OscResolver::getBinding(const char* trigger)
{
    OscBinding* b = NULL;
    if (mBindingMap != NULL) 
      b = (OscBinding*)mBindingMap->get(trigger);
    return b;
}

/**
 * After creating an OscBinding either from the OscConfig or dynamically,
 * see if it can be added to the exports list.
 */
PRIVATE void OscResolver::addExport(OscBindingSet* set, OscBinding* ob)
{
    if (ob->isExportable()) {

        OscDevice* device = NULL;
        const char* host = NULL;
        int port = 0;

        // first check the bindingset
        if (set != NULL) {
            host = set->getOutputHost();
            port = set->getOutputPort();
        }

        // then the OscConfig
        if (mConfig != NULL) {
            if (host == NULL)
              host = mConfig->getOutputHost();
            if (port <= 0)
              port = mConfig->getOutputPort();
        }

        // finally fall back to global config
        MobiusConfig* mc = mMobius->getConfiguration();
        if (host == NULL)
          host = mc->getOscOutputHost();

        if (port <= 0)
          port = mc->getOscOutputPort();

        if (host != NULL) {
            // should be smarter here about valid port ranges...
            if (port <= 0)
              Trace(1, "OscRuntime: invalid port range %ld\n", 
                    (long)port);
            else
              device = mOsc->registerDevice(host, port);
        }

        if (device != NULL) {

            ob->setExportDevice(device);

            if (mExports == NULL)
              mExports = new List();
            mExports->add(ob);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// OSC Message Receive & Export
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by OscInterface whenever a message is received.
 * Message is owned by the calling OscRuntime.
 */
PUBLIC void OscResolver::oscMessage(OscMessage* msg)
{
    const char* address = msg->getAddress();

    if (mTrace) {
        // nice for me since I launch from the console
		printf("OSC received: %s ", address);
		int nargs = msg->getNumArgs();
		for (int i = 0 ; i < nargs ; i++)
		  printf("%f ", msg->getArg(i));
		printf("\n");
		fflush(stdout);

        // but this is what everyone else will use
        // gag, background Trace doesn't like floats
        // only pick the first one
        char buf[80];
        if (nargs <= 0)
          strcpy(buf, "");
        else {
            sprintf(buf, "%f", msg->getArg(0));
            Trace(2, "OSC received: %s %s\n", address, buf);
        }
    }

    // TouchOSC sends /ping periodically, and who knows what others
    // will do.  Ignore anything that doesn't have our prefix.
    if (StartsWith(address, "/mobius")) {

        OscBinding* ob = getBinding(address);

        if (ob == NULL) {
            // Not currently mapped, try to resolve within our address space
            // and guess at the triggerMode
            Binding* b = new Binding();
            b->setTrigger(TriggerOsc);
            b->setTargetPath(address);

            Action* a = mMobius->resolveAction(b);
            if (a == NULL) {
                // To keep this from happening every time could go ahead and
                // make a binding that goes nowhere?  Since we're
                // limiting this to /mobius addresses it really 
                // should be fixed.  resolveAction will have traced enough.
                //Trace(2, "Unresolved OSC address: %s\n", address);
            }
            else {
                // copy this for the map key
                a->setName(address);

                // add it to our list
                ob = new OscBinding(mMobius, b, a);
                addBinding(ob);

                // Add to the export list if exportable and we can determine
                // where it should go.  Since we have no OscBindingSet we have
                // to use the global export host and port
                addExport(NULL, ob);
            }

            delete b;
        }

        if (ob != NULL) {
            // need to be smarter about multiple args?
            float arg = msg->getArg(0);
            ob->setValue(arg);
        }
    }
}

/**
 * Send messages for each exportable binding.
 * Called once during initialization to send initial state,
 * and periodically by MobiusThread.
 */
PUBLIC void OscResolver::exportStatus(bool force)
{
    if (mExports != NULL) {
        for (int i = 0 ; i < mExports->size() ; i++) {
            OscBinding* exp = (OscBinding*)mExports->get(i);

            if (exp->refreshValue() || force) {

                OscDevice* dev = exp->getExportDevice();
                if (dev != NULL) {
                    OscMessage msg;
                    const char* address = exp->getExportAddress();
                    float neu = exp->getExportValue();

                    msg.setAddress(address);
                    // TODO: more flexible on arg placement...
                    msg.setArg(0, neu);

                    if (mTrace) {
                        printf("OSC send: %s %f\n", address, neu);
                        fflush(stdout);
                        
                        char buf[80];
                        sprintf(buf, "%f", neu);
                        Trace(2, "OSC send: %s %s\n", address, buf);
                    }

                    mOsc->send(dev, &msg);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// OscRuntime
//
//////////////////////////////////////////////////////////////////////

PUBLIC OscRuntime::OscRuntime(MobiusInterface* m)
{
	mMobius = m;
    mOsc = OscInterface::getInterface();
    mResolver = NULL;
    mWatchers = NULL;
    mInputPort = 0;
    mOutputPort = 0;
    mOutputHost = NULL;

    MobiusConfig* mc = m->getConfiguration();
    if (mc->isOscEnable()) {
        // build the mResolver and send out the initial exports
        reloadConfigurationFile(m);

        // open input port and start tracking changes to the output port
        // we've already sent exports so don't do them again
        updateGlobalConfiguration(m, false);
    }
}

PUBLIC OscRuntime::~OscRuntime()
{
    if (mOsc != NULL)
      mOsc->stop();

    delete mOsc;
    // might want to wait to make sure we're not receiving anything!!
    delete mResolver;
    delete mWatchers;
    delete mOutputHost;
}

/**
 * Adjust the OSC interface after changes to the global MobiusConfig.
 */
PUBLIC void OscRuntime::updateGlobalConfiguration(MobiusInterface* m)
{
    updateGlobalConfiguration(m, true);
}

/**
 * Adjust the OSC interface after construction or after changes to
 * the global MobiusConfig.  If the refreshExports flag is on, we'll
 * refresh export state if any of the outputs have changed.
 *
 * Currently the input port is always specified in the MobiusConfig.
 * Originaly this was set in the OscConfig and that property still
 * exists, though we're not using it.  Each BindingSet also
 * has a port but that was never used.  Think about whether
 * we really need to support multiple ports, if not this can go away.
 *
 * Output host and ports can be set at two levels within the OscConfig.
 * We're not currently using that but that seems more useful than
 * multiple input ports.  The mOutputPort and mOutputHost fields
 * are only used to track changes to the global config.  OscResolver
 * does not actually use those, it pulls them directly from
 * MobiusInterface if it needs them.
 *
 * NOTE: If you change output ports or output hosts this is NOT
 * closing connections to host/port combos we no longer use.
 * Need to give OscInterface a method to close all current
 * OscDevices.
 */
PRIVATE void OscRuntime::updateGlobalConfiguration(MobiusInterface* m,
                                                   bool refreshExports)
{
    MobiusConfig* mc = m->getConfiguration();

    if (!mc->isOscEnable()) {
        Trace(2, "Stopping OSC listener\n");
        mOsc->stop();
    }
    else {
        // The configuration can also have a port, though this convention
        // has never been used

        int port = mc->getOscInputPort();
        if (port <= 0) {
            // this is a sign to stop
            if (mInputPort > 0) {
                Trace(2, "Stopping OSC listener\n");
                mOsc->stop();
            }
        }
        else if (port != mInputPort) {
            Trace(2, "Starting OSC listener on port %ld\n", (long)port);
            mOsc->setListener(this);
            mOsc->setReceivePort(port);
            mOsc->start();
        }

        bool reload = false;

        const char* host = mc->getOscOutputHost();
        if (host == NULL) {
            // TODO: should we reset any Exports?
            // just leave them for now
        }
        else if (!StringEqual(host, mOutputHost)) {
            delete mOutputHost;
            mOutputHost = CopyString(host);
            reload = true;
        }

        port = mc->getOscOutputPort();
        if (port <= 0) {
            // TODO: should we reset any Exports?
            // just leave them for now
        }
        else if (port != mOutputPort) {
            // Technically we only need to do this if any of the
            // current bindings used this port.  At the moment they 
            // always do, but once we allow OscConfig editing, it will
            // be more common to put them in the config.
            mOutputPort = port;
            reload = true;
        }

        if (refreshExports && reload) {
            // TODO: this is a big hammer, we could just iterate
            // over the existing model and change the ports 
            reloadConfigurationFile(m);
        }
        else {
            // we don't have to reload, but propagate trace desires
            MobiusConfig* mc = m->getConfiguration();
            mResolver->setTrace(mc->isOscTrace());
            if (mWatchers != NULL) {
                for (int i = 0 ; i < mWatchers->size() ; i++) {
                    OscRuntimeWatcher* rw = (OscRuntimeWatcher*)mWatchers->get(i);
                    rw->setTrace(mc->isOscTrace());
                }
            }
        }
    }
}

/**
 * Load the OSC configuration file.
 * MobiusConfig also contains an OscConfig but we're ignore that
 * as of 2.2.
 */
PUBLIC void OscRuntime::reloadConfigurationFile(MobiusInterface* m)
{
    // Mobius isn't range checking so make this big
    char buffer[1024 * 8];

    if (!m->findConfigurationFile("osc.xml", buffer, sizeof(buffer))) {
        // it is normal for this to be missing so don't complain
    }
    else {
        printf("Reading Mobius OSC configuration file: %s\n", buffer);
        fflush(stdout);

        char* xml = ReadFile(buffer);
        if (xml == NULL || strlen(xml) == 0) {
            // leave an error message behind to show when the UI
            // eventually comes up?
            Trace(1, "Empty osc.xml file\n");
        }
        else {
            OscConfig* config = new OscConfig(xml);
            if (config->getError() != NULL) {
                // save error for later display when we have a window?
                Trace(1, "Exception loading osc.xml %s\n", config->getError());
                delete config;
            }
            else {
                OscResolver* res = new OscResolver(mMobius, mOsc, config);
                // TODO: Need a way to safely GC these
                res->setNext(mResolver);
                mResolver = res;

                // since the outputs may have changed, export
                mResolver->exportStatus(true);

                // register the watchers    
                registerWatchers(config);
            }
        }
        delete xml;
    }
    
    // if we had a problem loading the file, make a dummy resolver so the
    // reset of the code doesn't have to check for null
    if (mResolver == NULL) {
        OscConfig* config = new OscConfig();
        OscResolver* res = new OscResolver(mMobius, mOsc, config);
        // TODO: Need a way to safely GC these
        res->setNext(mResolver);
        mResolver = res;
    }

}

/**
 * Register WatchPointListeners for each OscWatcher definition.
 */
void OscRuntime::registerWatchers(OscConfig* config)
{
    MobiusConfig* mc = mMobius->getConfiguration();
    const char* host = mc->getOscOutputHost();
    int port = mc->getOscOutputPort();
    
    // someday be smarter about port ranges
    if (host != NULL && port > 0) {

        // someday we could allow OscWather to specify
        // it's own host/port
        OscDevice* device = mOsc->registerDevice(host, port);

        // tell Mobius to delete the old ones when it has time
        if (mWatchers == NULL) {
            mWatchers = new List();
        }
        else {
            for (int i = 0 ; i < mWatchers->size() ; i++) {
                OscRuntimeWatcher* w = (OscRuntimeWatcher*)mWatchers->get(i);
                // after calling this, you must NOT touch it again
                // Mobius own's it's sorry ass
                w->remove();
            }
            mWatchers->reset();
        }

        // add the new ones
        for (OscWatcher* w = config->getWatchers() ; w != NULL ; 
             w = w->getNext()) {

            OscRuntimeWatcher* rw = new OscRuntimeWatcher(config, w);

            // this returns null if the name was invalid
            WatchPoint* wp = mMobius->addWatcher(rw);
            if (wp == NULL)
              delete rw;
            else {
                rw->finish(mMobius, wp, mOsc, device);
                mWatchers->add(rw);
            }
        }
    }
}

/**
 * Called by OscInterface whenever a message is received.
 */
PUBLIC void OscRuntime::oscMessage(OscMessage* msg)
{
    const char* address = msg->getAddress();

    // resolver does all the work
    if (mResolver != NULL)
      mResolver->oscMessage(msg);

    msg->free();
}

/**
 * Called by MobiusThread periodically to export status.
 */
PUBLIC void OscRuntime::exportStatus()
{
    if (mResolver != NULL)
      mResolver->exportStatus(false);

    // let each of the watchers know, in case they want to do something
    // that happens outside of an interrupt
    if (mWatchers != NULL) {
        int max = mWatchers->size();
        for (int i = 0 ; i < max ; i++) {
            OscRuntimeWatcher* w = (OscRuntimeWatcher*)mWatchers->get(i);
            w->tick();
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
