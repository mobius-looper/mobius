/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extended component using a pair of ListBoxes with buttons 
 * in between to transfer items between the boxes.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

MultiSelect::MultiSelect()
{
    init(false);
}

MultiSelect::MultiSelect(bool moveAll)
{
    init(moveAll);
}

MultiSelect::~MultiSelect()
{
}

// !! need to allow moveAll to be specified AFTER construction

void MultiSelect::init(bool moveAll)
{
	mClassName = "MultiSelect";
    mValues = NULL;
    mAllowedValues = NULL;

	setName(mClassName);

    HorizontalLayout* l = new HorizontalLayout(2);
    l->setCenterY(true);
    setLayout(l);
    
    mAvailableBox = new ListBox();
    mAvailableBox->setMultiSelect(true);
    add(mAvailableBox);

    Panel* buttons = new Panel("buttons");
    VerticalLayout* vl = new VerticalLayout();
    vl->setCenterX(true);
    buttons->setLayout(vl);
    add(buttons);

    mMoveRight = new Button(">");
    mMoveRight->addActionListener(this);
    buttons->add(mMoveRight);

    mMoveAllRight = NULL;
    if (moveAll) {
        mMoveAllRight = new Button(">>");
        mMoveAllRight->addActionListener(this);
        buttons->add(mMoveAllRight);
    }

    mMoveAllLeft = NULL;
    if (moveAll) {
        mMoveAllLeft = new Button("<<");
        mMoveAllLeft->addActionListener(this);
        buttons->add(mMoveAllLeft);
    }

    mMoveLeft = new Button("<");
    mMoveLeft->addActionListener(this);
    buttons->add(mMoveLeft);

    mValuesBox = new ListBox();
    mValuesBox->setMultiSelect(true);
    add(mValuesBox);

    setColumns(10);
}

PUBLIC StringList* MultiSelect::getValues()
{
    return mValuesBox->getValues();
}

PUBLIC void MultiSelect::setColumns(int i)
{
    mAvailableBox->setColumns(i);
    mValuesBox->setColumns(i);
}

PUBLIC void MultiSelect::setRows(int i)
{
    mAvailableBox->setRows(i);
    mValuesBox->setRows(i);
}

PUBLIC void MultiSelect::setAllowedValues(StringList* values)
{
    if (values != mAllowedValues) {
        delete mAllowedValues;
        mAllowedValues = values;
    }

    // !! in theory we now need to filter the selected values
    // based on the new list, but in practice this doesn't change on the fly

    updateAvailableValues();
}

PUBLIC void MultiSelect::setValues(StringList* values)
{
    // we don't need to keep a copy of this, let the ListBox do it
    // !! in theory need to be filtering the list based on allowed values,
    // for now trust the application
    mValuesBox->setValues(values);

    updateAvailableValues();
}

/**
 * Derive the list of values to display on the left box.
 */
PRIVATE void MultiSelect::updateAvailableValues()
{
    StringList* available = NULL;

    if (mAllowedValues != NULL) {
        // could use removeAll now that we have it
        StringList* values = mValuesBox->getValues();
        available = new StringList();
        for (int i = 0 ; i < mAllowedValues->size() ; i++) {
            const char* s = mAllowedValues->getString(i);
            if (values == NULL || !values->contains(s))
              available->add(s);
        }
    }

    mAvailableBox->setValues(available);
}

PUBLIC void MultiSelect::actionPerformed(void* src)
{
    if (src == mMoveLeft) {
        StringList* selected = mValuesBox->getSelectedValues();
        if (selected != NULL) {
            StringList* values = mValuesBox->getValues();
            if (values != NULL) {
                values->removeAll(selected);
                mValuesBox->setValues(values);
                updateAvailableValues();
            }
        }
    }
    else if (src == mMoveAllLeft) {
        StringList* values = mValuesBox->getValues();
        if (values != NULL) {
            mValuesBox->setValues(NULL);
            updateAvailableValues();
        }
    }
    else if (src == mMoveRight) {
        StringList* selected = mAvailableBox->getSelectedValues();
        if (selected != NULL) {
            StringList* values = mValuesBox->getValues();
            if (values == NULL)
              values = new StringList();
            values->addAll(selected);
            mValuesBox->setValues(values);
            updateAvailableValues();
        }
    }
    else if (src == mMoveAllRight) {
        StringList* available = mAvailableBox->getValues();
        if (available != NULL) {
            StringList* values = mValuesBox->getValues();
            if (values == NULL)
              values = new StringList();
            values->addAll(available);
            mValuesBox->setValues(values);
            updateAvailableValues();
        }
    }
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
