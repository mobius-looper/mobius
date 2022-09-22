/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extended component that renders a text field with a static
 * label to the left.
 * Should consider just doing this in Text if it has a label though
 * that would be inconsistent with Swing.
 *
 */


#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

LabeledText::LabeledText()
{
	init(NULL, NULL);
}

LabeledText::LabeledText(const char *label, const char *value)
{
	init(label, value);
}

void LabeledText::init(const char *label, const char *value) 
{
	mClassName = "LabeledText";
	setName("LabeledText");
	setLayout(new HorizontalLayout(4));
	mLabel = new Label(label);
	mText = new Text(value);
	mText->setColumns(20);
	mText->addActionListener(this);
	add(mLabel);
	add(mText);
}

LabeledText::~LabeledText()
{
}

void LabeledText::actionPerformed(void* o)
{
	fireActionPerformed();
}

const char *LabeledText::getText()
{
	return mText->getText();
}

void LabeledText::setText(const char *s) 
{
	mText->setText(s);
}

void LabeledText::setColumns(int i)
{
	mText->setColumns(i);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
