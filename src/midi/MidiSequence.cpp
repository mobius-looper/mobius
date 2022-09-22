/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for a MIDI sequence.
 * These aren't used by Mobius yet, but give it time...
 * 
 */

#include <stdio.h>
#include <string.h>

#include "Util.h"
#include "XmlModel.h"
#include "XomParser.h"
#include "XmlBuffer.h"

#include "MidiEnv.h"
#include "MidiEvent.h"
#include "MidiSequence.h"

/****************************************************************************
 *                                                                          *
 *   							 CONSTRUCTORS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Allocate a new sequence.
 */
PUBLIC MidiSequence::MidiSequence(void)
{
	init();
}

PUBLIC MidiSequence::MidiSequence(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC void MidiSequence::init(void)
{
    mNext 		= NULL;
    mName 		= NULL;
    mEvents		= NULL;
    mChannel 	= -1;
    mTempo		= 120.0f;
    mDivision	= 96;
    mTimeSigNum = 0;
    mTimeSigDenom = 0;
}

/**
 * Destruct a midi sequence.
 * Note that unlike MidiEvent we do NOT free other sequences
 * on the chain.
 */
PUBLIC MidiSequence::~MidiSequence(void)
{
	delete mName;

    // call free() so the MidiEvent can return itself to the pool
	if (mEvents != NULL)
	  mEvents->free();
}

/**
 * Empty the sequence but leave the sequence structure allocated. 
 * Useful for "erasing" the contents of a sequence.
 * Unclear which structure fields we should be retaining here.
 * Try to clear out the event list related stuff but retain
 * the "meta" information like channel, properties, etc.
 */
PUBLIC void MidiSequence::clear(void)
{
	if (mEvents != NULL) {
        mEvents->free();
        mEvents = NULL;
    }

	// should we clear channel and the other MIDI file parameters too?
}

PUBLIC void MidiSequence::setNext(MidiSequence* s) 
{
    mNext = s;  
}

PUBLIC void MidiSequence::setName(const char *newname)
{
    delete mName;
    mName = CopyString(newname);
}

PUBLIC void MidiSequence::setEvents(MidiEvent *e) 
{
    if (mEvents != NULL)
      mEvents->free();
    mEvents = e;
}

PUBLIC void MidiSequence::setChannel(int c) {
    mChannel = c;
}

PUBLIC void MidiSequence::setTempo(float t) {
    mTempo = t;
}

PUBLIC void MidiSequence::setDivision(int t) {
    mDivision = t;
}

PUBLIC MidiEvent* MidiSequence::stealEvents()
{
    MidiEvent *e = mEvents;
    mEvents = NULL;
    return e;
}

/**
 * Allocate a new event.
 * Faster if kept our own pointer to the MidiEnv or move
 * the pool to MidiEvent.
 */
PUBLIC MidiEvent* MidiSequence::newMidiEvent()
{
    MidiEnv* env = MidiEnv::getEnv();
    return env->newMidiEvent();
}

PUBLIC MidiEvent* MidiSequence::newMidiEvent(int status, int chan, 
                                             int key, int vel)
{
    MidiEnv* env = MidiEnv::getEnv();
    return env->newMidiEvent(status, chan, key, vel);
}

/****************************************************************************
 *                                                                          *
 *                               EVENT ANALYSIS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Return true if there are no events in the sequence.
 */
PUBLIC bool MidiSequence::isEmpty(void)
{
	return (mEvents == NULL);
}

/**
 * Return the event with the highest clock.
 * Since the event list is ordered, this is just the last event.
 * Should just maintain a pointer to it to avoid traversing the list!!
 */
PUBLIC MidiEvent *MidiSequence::getLastEvent()
{
    MidiEvent* e = NULL;

    if (mEvents != NULL)
      e = mEvents->getLast(0);

    return e;
}

/**
 * Insert an event into the sequence.
 */
PUBLIC void MidiSequence::insert(MidiEvent *e)
{
    if (mEvents == NULL)
      mEvents = e;
    else
      mEvents = mEvents->insert(e);
}

/**
 * Insert an event into the sequence, replacing any existing 
 * event of this type on this clock.  Should be used only
 * for control, program, and name events.
 */
PUBLIC void MidiSequence::replace(MidiEvent *e)
{
    if (mEvents == NULL)
      mEvents = e;
    else
      mEvents = mEvents->replace(e);
}

/**
 * Remove an event from the sequence.
 */
PUBLIC void MidiSequence::remove(MidiEvent *e)
{
    if (mEvents != NULL)
      mEvents = mEvents->remove(e);
}

/**
 * Returns the next event of a particular type.
 */
PUBLIC MidiEvent *MidiSequence::getNextEvent(MidiEvent *e)
{
    return (e != NULL) ? e->getNextEvent() : NULL;
}

/**
 * Returns the previous event of a particular type.
 */
PUBLIC MidiEvent *MidiSequence::getPrevEvent(MidiEvent *event)
{
    MidiEvent* prev = NULL;

    if (event != NULL && mEvents != NULL) {
        for (MidiEvent* e = mEvents ; e != NULL && e != event ; 
             e = e->getNext()) {
            if (e->getStatus() == event->getStatus()) {
                prev = e;
                break;
            }
        }
    }
    return prev;
}

/****************************************************************************
 *                                                                          *
 *   								 XML                                    *
 *                                                                          *
 ****************************************************************************/
/*
 * <sequence name='foo' channel='1' tempo='120' division='96'>
 *
 */

#define EL_SEQUENCE "sequence"
#define ATT_NAME "name"
#define ATT_CHANNEL "channel"
#define ATT_TEMPO "tempo"
#define ATT_DIVISION "division"
#define ATT_NUMERATOR "timeSigNumerator" 
#define ATT_DENOMINATOR "timeSigDenominator"

PUBLIC void MidiSequence::parseXml(XmlElement* e)
{
	setName(e->getAttribute(ATT_NAME));

	// defaults to -1
	if (e->getAttribute(ATT_CHANNEL) != NULL)
	  mChannel = e->getIntAttribute(ATT_CHANNEL);

	// should support floating tempos!
	mTempo = (float)e->getIntAttribute(ATT_TEMPO);
	mDivision = e->getIntAttribute(ATT_DIVISION);
	mTimeSigNum = e->getIntAttribute(ATT_NUMERATOR);
	mTimeSigDenom = e->getIntAttribute(ATT_DENOMINATOR);

	MidiEvent* last = NULL;

	for (XmlElement *child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		MidiEvent* e = new MidiEvent(child);
		if (last == NULL)
		  mEvents = e;
		else 
		  last->setNext(e);
		last = e;
	}
}


PUBLIC void MidiSequence::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_SEQUENCE);
	b->addAttribute(ATT_NAME, mName);

	if (mChannel >= 0)
	  b->addAttribute(ATT_CHANNEL, mChannel);

	// !! need to support floating tempos
	if (mTempo > 0)
	  b->addAttribute(ATT_TEMPO, (int)mTempo);

	if (mDivision > 0)
	  b->addAttribute(ATT_DIVISION, mDivision);
	if (mTimeSigNum > 0)
	  b->addAttribute(ATT_NUMERATOR, mTimeSigNum);
	if (mTimeSigDenom > 0)
	  b->addAttribute(ATT_DENOMINATOR, mTimeSigDenom);
	b->add(">\n");

	b->incIndent();
	for (MidiEvent* e = mEvents ; e != NULL ; e = e->getNext())
	  e->toXml(b);
	b->decIndent();

	b->addEndTag(EL_SEQUENCE);
}

PUBLIC char* MidiSequence::toXml()
{
	char* xml = NULL;
	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	xml = b->stealString();
	delete b;
	return xml;
}

PUBLIC void MidiSequence::readXml(const char* file)
{
	clear();
	char* xml = ReadFile(file);
	if (xml != NULL) {
		XomParser* p = new XomParser();
		XmlDocument* d = p->parse(xml);
		if (d != NULL)
		  parseXml(d->getChildElement());
		delete d;
		delete p;
		delete xml;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
