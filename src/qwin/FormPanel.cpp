/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extended component that renders as a two column form with labels
 * on the left and input fields on the right.
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

PUBLIC FormPanel::FormPanel()
{
	mClassName = "FormPanel";
	setName("Form");
	setLayout(new FormLayout());
}

PUBLIC FormPanel::~FormPanel()
{
}

PUBLIC void FormPanel::setAlign(int i)
{
	((FormLayout*)getLayoutManager())->setAlign(i);
}

PUBLIC void FormPanel::setHorizontalGap(int i)
{
	((FormLayout*)getLayoutManager())->setHorizontalGap(i);
}

PUBLIC void FormPanel::setVerticalGap(int i)
{
	((FormLayout*)getLayoutManager())->setVerticalGap(i);
}

PUBLIC void FormPanel::add(const char *name, Component* c)
{
	if (c != NULL) {
        Panel::add(new Label(name));
		Panel::add(c);
	}
}

PUBLIC Text* FormPanel::addText(ActionListener* l, const char *label)
{
	Text* t = new Text();
	t->addActionListener(l);
	add(label, t);
	return t;
}

PUBLIC NumberField* FormPanel::addNumber(ActionListener* l, const char *label,
										 int low, int high)
{
	NumberField* f = new NumberField(low, high);
	f->addActionListener(l);
	add(label, f);
	return f;
}

PUBLIC ComboBox* FormPanel::addCombo(ActionListener* l, const char *label,
									 const char **labels)
{
    return addCombo(l, label, labels, 10);
}

PUBLIC ComboBox* FormPanel::addCombo(ActionListener* l, const char *label,
									 const char **labels,
                                     int columns)
{
	ComboBox* c = new ComboBox(labels);
	c->setColumns(columns);
	c->addActionListener(l);
	add(label, c);
	return c;
}

PUBLIC Checkbox* FormPanel::addCheckbox(ActionListener* l, const char *label)
{
	Checkbox* c = new Checkbox();
	c->addActionListener(l);
	add(label, c);
	return c;
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
