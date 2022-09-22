/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Extended component that combines a scroll bar with a label that
 * tracks the position and displays a number within a defined range.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

Slider::Slider(bool vertical, bool showValue)
{
	mClassName = "Slider";
	setName("Slider");

	if (vertical)
	  setLayout(new VerticalLayout(4));
	else
	  setLayout(new HorizontalLayout(4));

	mScroll = new ScrollBar();
	mScroll->setVertical(vertical);
	mScroll->addActionListener(this);
	mScroll->setPageSize(1);
	add(mScroll);

	mLabel = NULL;
	if (showValue) {
		mLabel = new Label();
		mLabel->setColumns(4);
		add(mLabel);
	}
}

Slider::~Slider()
{
}

void Slider::setValue(int i)
{
	mScroll->setValue(i);
	updateLabel();
}

int Slider::getValue()
{
	return mScroll->getValue();
}

PUBLIC void Slider::setMinimum(int i) 
{  
	mScroll->setMinimum(i);
}

PUBLIC int Slider::getMinimum()
{
    return mScroll->getMinimum();
}

PUBLIC void Slider::setMaximum(int i) 
{
	mScroll->setMaximum(i);
}

PUBLIC int Slider::getMaximum()
{
    return mScroll->getMaximum();
}

/**
 * Must be caleld before the native handle is created!!
 */
PUBLIC void Slider::setSliderLength(int i)
{
	if (mScroll->isVertical())
	  mScroll->setPreferredSize(0, i);
	else
	  mScroll->setPreferredSize(i, 0);
}

PUBLIC void Slider::setLabelColumns(int i)
{
	if (mLabel != NULL)
	  mLabel->setColumns(i);
}

void Slider::actionPerformed(void* o)
{
	updateLabel();
	fireActionPerformed();
}

void Slider::updateLabel()
{
	if (mLabel != NULL) {
		char buf[128];
		sprintf(buf, "%d", getValue());
		mLabel->setText(buf);
	}
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
