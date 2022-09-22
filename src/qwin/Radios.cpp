/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A container of several Radio components.  
 * 
 * Originally this extended Panel, but with Mac we have to have a specific UI,
 * which is arguably better anyway.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

PUBLIC Radios::Radios()
{
	initRadios();
}

PUBLIC Radios::Radios(List* labels)
{
	initRadios();
	setLabels(labels);
}

PUBLIC Radios::Radios(const char** labels)
{
	initRadios();
	setLabels(labels);
}

PRIVATE void Radios::initRadios()
{
	mClassName = "Radios";
	mGroup = NULL;
	setLayout(new HorizontalLayout());
}

PUBLIC Radios::~Radios()
{
}

PUBLIC ComponentUI* Radios::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getRadiosUI(this);
	return mUI;
}

PUBLIC RadiosUI* Radios::getRadiosUI()
{
	return (RadiosUI*)mUI;
}

PUBLIC void Radios::setLabels(List* labels)
{
	removeAll();

	//mGroup = new GroupBox();
	//add(mGroup);
	
	if (labels != NULL) {
		for (int i = 0 ; i < labels->size() ; i++) {
			const char *s = (const char *)labels->get(i);
			RadioButton* b = new RadioButton(s);
			if (i > 0)
			  b->setGroup(true);
			b->addActionListener(this);
			add(b);
		}

		// we assume ownership of the list, the list elements
		// have been copied into RadioButton names
		delete labels;
	}
}

PUBLIC void Radios::setLabels(const char** labels)
{
	removeAll();
	if (labels != NULL) {
		for (int i = 0 ; labels[i] != NULL ; i++) {
			RadioButton* b = new RadioButton(labels[i]);
			if (i > 0)
			  b->setGroup(true);
			b->addActionListener(this);
			add(b);
		}
	}
}

PUBLIC void Radios::addLabel(const char* label)
{
	if (label != NULL) {
		RadioButton* b = new RadioButton(label);
		if (getComponents() != NULL)
		  b->setGroup(true);
		b->addActionListener(this);
		add(b);
	}
}

PUBLIC void Radios::setVertical(bool b)
{
	if (b)
	  setLayout(new VerticalLayout());
	else
	  setLayout(new HorizontalLayout());
}

PUBLIC int Radios::getSelectedIndex()
{
	int index = -1;
	int i = 0;
	for (Component* c = getComponents() ; c != NULL ; c = c->getNext(), i++) {
		RadioButton* b = (RadioButton*)c;
		if (b->isSelected()) {
			index = i;
			break;
		}
	}
	return index;
}

PUBLIC void Radios::setSelectedIndex(int index)
{
	int i = 0;
	for (Component* c = getComponents() ; c != NULL ; c = c->getNext(), i++) {
		RadioButton* b = (RadioButton*)c;
		b->setSelected(i == index);
	}
}

PUBLIC RadioButton* Radios::getSelectedButton()
{
	RadioButton* selected = NULL;
	for (Component* c = getComponents() ; c != NULL ; c = c->getNext()) {
		RadioButton* b = (RadioButton*)c;
		if (b->isSelected()) {
			selected = b;
			break;
		}
	}
	return selected;
}

PUBLIC void Radios::setSelectedButton(RadioButton* selected)
{
	int i = 0;
	for (Component* c = getComponents() ; c != NULL ; c = c->getNext(), i++) {
		RadioButton* b = (RadioButton*)c;
		b->setSelected(b == selected);
	}
}

PUBLIC const char* Radios::getSelectedValue()
{
	RadioButton* selected = getSelectedButton();
	return (selected != NULL) ? selected->getText() : NULL;
}

/**
 * For compatibility with other value producing components.
 */
PUBLIC const char* Radios::getValue()
{
	return getSelectedValue();
}

PUBLIC void Radios::open()
{
    ComponentUI* ui = getUI();
    ui->open();
	
	// recurse on children
	Container::open();

	// make the initial selection?	
	setSelectedIndex(0);
}

/**
 * Convert actions on the buttons to actions on the container.
 */
PUBLIC void Radios::actionPerformed(void* src)
{
    RadiosUI* ui = getRadiosUI();
    ui->changeSelection((RadioButton*)src);

	fireActionPerformed(this);
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * This doesn't do anything on Windows but need one for consistency
 * with Mac.  This might be where we cold push the RadioButton::mGroup concept?
 */

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"

QWIN_BEGIN_NAMESPACE

PUBLIC WindowsRadios::WindowsRadios() 
{
	mRadios = NULL;
}

PUBLIC WindowsRadios::WindowsRadios(Radios* r) 
{
	mRadios = r;
}

PUBLIC WindowsRadios::~WindowsRadios()
{
}

/**
 * In the original implementation we subclassed Panel and therefore
 * would create a static window if we had a background color.
 * Now that we've moved to our own RadiosUI we don't do that and I don't
 * think it was ever really necessary.
 */
PUBLIC void WindowsRadios::open()
{
}

/**
 * On Windows the checkboxes automatically mutex.
 */
PUBLIC void WindowsRadios::changeSelection(RadioButton* b) 
{
}

QWIN_END_NAMESPACE
#endif // _WIN32

//////////////////////////////////////////////////////////////////////
//
// OSX
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
#include "MacUtil.h"

QWIN_BEGIN_NAMESPACE

PUBLIC MacRadios::MacRadios() 
{
	mRadios = NULL;
}

PUBLIC MacRadios::MacRadios(Radios* r) 
{
	mRadios = r;
}

PUBLIC MacRadios::~MacRadios()
{
}

/**
 * We could create a RadioButtonGroup control, but it's harder
 * and we can easily implement the mutex ourselves.  Any disadvantages?
 */
PUBLIC void MacRadios::open()
{
}

/**
 * On Windows the checkboxes automatically mutex.
 */
PUBLIC void MacRadios::changeSelection(RadioButton* b) 
{
	// this will do the work though it will set the selected
	// value redundantly
	mRadios->setSelectedButton(b);
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
