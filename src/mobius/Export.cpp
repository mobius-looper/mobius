/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for exporting target values out of Mobius.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>

#include "Util.h"

#include "Action.h"
#include "Binding.h"
#include "Mobius.h"
#include "Parameter.h"
#include "Track.h"

#include "Export.h"

/****************************************************************************
 *                                                                          *
 *                                   EXPORT                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Export::Export(Mobius* m)
{
    init();
    mMobius = m;
}

PUBLIC Export::Export(Action* a)
{
    init();
    mMobius = a->mobius;
    mTarget = a->getResolvedTarget();
    mTrack = a->getResolvedTrack();
}

PUBLIC void Export::init()
{
    mNext = NULL;
    mMobius = NULL;
    mTarget = NULL;
    mTrack = NULL;
    mLast = -1;
    mMidiChannel = 0;
    mMidiNumber = 0;
}

Export::~Export()
{
	Export *el, *next;

    // target is normally interned
    setTarget(NULL);

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

Mobius* Export::getMobius()
{
    return mMobius;
}

Export* Export::getNext()
{
    return mNext;
}

void Export::setNext(Export* e)
{
    mNext = e;
}

ResolvedTarget* Export::getTarget() 
{
    return mTarget;
}

void Export::setTarget(ResolvedTarget* t)
{
	// target is never owned
	mTarget = t;
}

Track* Export::getTrack()
{
    return mTrack;
}

void Export::setTrack(Track* t)
{
    mTrack = t;
}

//////////////////////////////////////////////////////////////////////
//
// Client Specific Properties
//
//////////////////////////////////////////////////////////////////////

int Export::getLast()
{
    return mLast;
}

void Export::setLast(int i)
{
    mLast = i;
}

int Export::getMidiChannel()
{
    return mMidiChannel;
}

void Export::setMidiChannel(int i)
{
    mMidiChannel = i;
}

int Export::getMidiNumber()
{
    return mMidiNumber;
}

void Export::setMidiNumber(int i)
{
    mMidiNumber = i;
}

//////////////////////////////////////////////////////////////////////
//
// Target Properties
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a constant representing the data type of the export.
 * So we don't have to expose Parameter.h and ParameterType, 
 * we'll duplicate this in ExportType.
 */
ExportType Export::getType()
{
    ExportType extype = EXP_INT;

    if (mTarget != NULL) {
        ExportType extype = EXP_INT;

        Target* ttype = mTarget->getTarget();

        if (ttype == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            if (p != NULL) {
                ParameterType ptype = p->type;
                if (ptype == TYPE_INT) {
                    extype = EXP_INT;
                }
                else if (ptype == TYPE_BOOLEAN) {
                    extype = EXP_BOOLEAN;
                }
                else if (ptype == TYPE_ENUM) {
                    extype = EXP_ENUM;
                }
                else if (ptype == TYPE_STRING) {
                    extype = EXP_STRING;
                }
            }
        }
    }

    return extype;
}

/**
 * Get the mimum value for the target.
 * Only relevant for some types.
 */
int Export::getMinimum()
{
    int min = 0;

    if (mTarget != NULL) {

        Target* type = mTarget->getTarget();

        if (type == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            if (p != NULL) {
                ParameterType type = p->type;
                if (type == TYPE_INT) {
                    min = p->getLow();
                }
            }
        }
    }

    return min;
}

/**
 * Get the maximum value for the target.
 * Only relevant for some types.
 */
int Export::getMaximum()
{
    int max = 0;

    if (mTarget != NULL) {
        Target* type = mTarget->getTarget();

        if (type == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            // note that we use "binding high" here so that INT params
            // are constrained to a useful range for binding
            max = p->getBindingHigh(mMobius);
        }
    }

    return max;
}

/**
 * For enumeration parameters, return the value labels that
 * can be shown in the UI.
 */
const char** Export::getValueLabels()
{
    const char** labels = NULL;

    if (mTarget != NULL) {

        Target* type = mTarget->getTarget();
        if (type == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            if (p != NULL)
              labels = p->valueLabels;
        }
    }

    return labels;
}

/**
 * Get the display name for the target.
 */
const char* Export::getDisplayName()
{
    const char* dname = NULL;

    if (mTarget != NULL)
      dname = mTarget->getDisplayName();
        
    return dname;
}

/**
 * Convert an ordinal value to a label.
 * This only works for parameters.
 */
PUBLIC void Export::getOrdinalLabel(int ordinal, ExValue* value)
{
    value->setString("???");

    if (mTarget != NULL) {
        Target* target = mTarget->getTarget();
        if (target == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            p->getOrdinalLabel(mMobius, ordinal, value);
        }
    }
}

/**
 * Return true if this is a suitable export to display in the UI.
 * This was simplfied recently, we assume that anything bindable 
 * is also displayable.
 */
bool Export::isDisplayable()
{
    bool displayable = false;

    if (mTarget != NULL) {
        Target* type = mTarget->getTarget();
        if (type == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            displayable = (p != NULL && p->bindable);
        }
    }

    return displayable;
}

//////////////////////////////////////////////////////////////////////
//
// Target Value
//
//////////////////////////////////////////////////////////////////////

/**
 * Select the target track for export.
 * Necessary for resolving groups.
 */
PRIVATE Track* Export::getTargetTrack()
{
    Track* found = NULL;

    if (mTarget->getTrack() > 0) {
        // track specific binding
        found = mMobius->getTrack(mTarget->getTrack() - 1);
    }
    else if (mTarget->getGroup() > 0) {
        // group specific binding
        // for exports we just find the first track in the group
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* track = mMobius->getTrack(i);
            if (track->getGroup() == mTarget->getGroup()) {
                found = track;
                break;
            }
        }
    }
    else {
        // current
        found = mMobius->getTrack();
    }

    return found;
}

/**
 * Get the current value of the export as an ordinal.
 * Used for interfaces like OSC that only support 
 * ordinal parameters.
 *
 * Note for State at the moment.
 */
PUBLIC int Export::getOrdinalValue()
{
    int value = -1;

    if (mTarget != NULL) {

        // resolve track so Parameter doesn't have to
        mTrack = getTargetTrack();

        Target* target = mTarget->getTarget();

        if (target == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            value = p->getOrdinalValue(this);
        }
    }

    return value;
}

/**
 * Get the current value of the export in "natural" form.
 * This may be an enumeration symbol or a string.
 */
PUBLIC void Export::getValue(ExValue* value)
{
    value->setNull();

    if (mTarget != NULL) {

        // have to resresolve the track each time
        mTrack = getTargetTrack();

        Target* target = mTarget->getTarget();

        if (target == TargetParameter) {
            Parameter* p = (Parameter*)mTarget->getObject();
            p->getValue(this, value);
        }
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

