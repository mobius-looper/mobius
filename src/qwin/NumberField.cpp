/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extended component using a text field with extra validation
 * for entering numbers within a range.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include "Util.h"
#include "Qwin.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

PUBLIC NumberField::NumberField() 
{
	init();
}

PRIVATE void NumberField::init()
{
	mClassName = "NumberField";
	mValue = 0;
    mLow = 0;
	mHigh = 0;
	mNullValue = 0;
	mHideNull = false;
	setLayout(new HorizontalLayout());
	mText = new Text();
	mText->addActionListener(this);
	mText->setColumns(4);
	add(mText);
}

PUBLIC NumberField::~NumberField()
{
}

PUBLIC NumberField::NumberField(int low, int high)
{
	init();
    mLow = low;
	mHigh = high;
}

PUBLIC void NumberField::setNullValue(int i)
{
	mNullValue = i;
}

PUBLIC void NumberField::setHideNull(bool b)
{
	mHideNull = b;
}

PUBLIC void NumberField::setLow(int i)
{
	mLow = i;
}

PUBLIC void NumberField::setHigh(int i)
{
	mHigh = i;
}

PUBLIC void NumberField::setDisplayOffset(int i)
{
}

PUBLIC void NumberField::addException(int i)
{
}

PUBLIC void NumberField::setValue(int i)
{
	if (i < mLow) i = mLow;

    // guard against zero
	if (i > mHigh && mHigh > 0) 
      i = mHigh;

	mValue = i;

	if (mHideNull && mValue == mNullValue)
	  mText->setText(NULL);
	else {
		char buf[64];
		sprintf(buf, "%d", mValue);
		mText->setText(buf);
	}
}

PUBLIC int NumberField::getValue()
{
	// never trust mValue, always go back to the text component
	mValue = mNullValue;
	const char *s = mText->getText();
	if (s != NULL && strlen(s) > 0)
	  mValue = atoi(s);

	if (mValue < mLow) mValue = mLow;
	if (mValue > mHigh && mHigh > 0) mValue = mHigh;

	return mValue;
}

PUBLIC void NumberField::actionPerformed(void* o)
{
	// must be mText
	fireActionPerformed(this);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
