/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A Dialog subclass that sets up the usual dialog controls and
 * creates a central panel for adding dialog specific controls.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							SIMPLE DIALOG                               *
 *                                                                          *
 ****************************************************************************/

// Since these are used so often, keep global copies

char* SimpleDialog::OkButtonText = NULL;
char* SimpleDialog::CancelButtonText = NULL;
char* SimpleDialog::HelpButtonText = NULL;

PUBLIC void SimpleDialog::localizeButtons(const char* ok, const char* cancel,
										  const char* help)
{
	if (ok != NULL) {
		delete OkButtonText;
		OkButtonText = CopyString(ok);
	}

	if (cancel != NULL) {
		delete CancelButtonText;
		CancelButtonText = CopyString(cancel);
	}

	if (help != NULL) {
		delete HelpButtonText;
		HelpButtonText = CopyString(help);
	}

}

PUBLIC void SimpleDialog::freeLocalizations()
{
	delete OkButtonText;
	OkButtonText = NULL;
	delete CancelButtonText;
	CancelButtonText = NULL;
	delete HelpButtonText;
	HelpButtonText = NULL;
}

PUBLIC SimpleDialog::SimpleDialog()
{
    initSimpleDialog(true);
}

PUBLIC SimpleDialog::~SimpleDialog()
{
}

PUBLIC SimpleDialog::SimpleDialog(Window *parent) 
{
    initSimpleDialog(true);
	setParent(parent);
}

PUBLIC SimpleDialog::SimpleDialog(Window* parent, const char *title) 
{
    initSimpleDialog(true);
	setParent(parent);
	setTitle(title);
}

PRIVATE void SimpleDialog::initSimpleDialog(bool cancel)
{
    setLayout(new BorderLayout());
    mPanel = new Panel("dialog root");
	VerticalLayout* vl = new VerticalLayout();
//	vl->setPreGap(5);
//	vl->setPostGap(5);
    mPanel->setLayout(vl);
    // put some air around the interior components
//	mPanel->setInsets(new Insets(0, 5, 0, 5));
    add(mPanel, BORDER_LAYOUT_CENTER);

    mButtons = new Panel("dialog buttons");
    mButtons->setInsets(new Insets(0, 5, 0, 5));
    mButtons->setLayout(new FlowLayout());
    add(mButtons, BORDER_LAYOUT_SOUTH);

	mOk = NULL;
	mCancel = NULL;

    mHelp = NULL;
    mCommitted = false;

    // Dialog should support a "default" button and direct the 
    // return key there!

}

/**
 * Originaly did this in the constructor but the overloaded subclass
 * methods weren't being called.  Not sure why, maybe the vtable
 * isn't fully initialized in the constructor?
 */
PUBLIC void SimpleDialog::setupButtons()
{
	if (mOk == NULL) {
		mOk = new Button(getOkName());
		mOk->addActionListener(this);
		mOk->setDefault(true);
		mButtons->add(mOk);
		const char* cancel = getCancelName();
		if (cancel != NULL) {
			mCancel = new Button(cancel);
			mCancel->addActionListener(this);
			mButtons->add(mCancel);
		}
	}
}

PUBLIC const char* SimpleDialog::getOkName()
{
	const char* name = OkButtonText;
	if (name == NULL)
	  name = "  Ok  ";
	return name;
}

PUBLIC const char* SimpleDialog::getCancelName()
{
	const char* name = CancelButtonText;
	if (name == NULL)
	  name = "Cancel";
	return name;
}

PUBLIC Panel* SimpleDialog::getPanel()
{
	return mPanel;
}

PUBLIC void SimpleDialog::addButton(Button* b)
{
    if (b != NULL)
      mButtons->add(b);
}

PUBLIC void SimpleDialog::addHelpButton()
{
    if (mHelp == NULL) {
		const char* label = HelpButtonText;
		if (label == NULL)
		  label = "Help";
        mHelp = new Button(label);
        mHelp->addActionListener(this);
        addButton(mHelp);
    }
}

PUBLIC bool SimpleDialog::isCommitted()
{
    return mCommitted;
}

PUBLIC bool SimpleDialog::isCanceled()
{
    return !mCommitted;
}

void SimpleDialog::prepareToShow()
{
    // make sure these are initialized
    mCommitted = false;
	setupButtons();
}

/**
 * Windows calls this  when the WM_CLOSE message is received.
 * This happens when you click the X button or exit using the
 * system menu.  Treat like a cancel.
 * You will also get here when the window is closed after pressing
 * the Ok button, so don't assume it is a cancel, check the mCommitted flag.
 */
void SimpleDialog::closing()
{
}

/**
 * Called when the OK button is pressed (provided that you don't
 * overload buttonOK.   May be overloaded in subclasses to 
 * validation and process the results of the dialog.  Return false
 * if you need to keep the dialog active.
 *
 * TODO: Should have a built-in way to display validation errors.
 */
PUBLIC bool SimpleDialog::commit()
{
    return true;
}

/**
 * Handle a button.
 */
PUBLIC void SimpleDialog::actionPerformed(void* o)
{
    if (o == mOk) {
        if (commit()) {
            mCommitted = true;
            close();
        }
    }
    else if (o == mCancel) {
        close();
    }
    else if (o == mHelp)
        help();
}

/**
 * Pretend to press the Ok button.
 */
PUBLIC void SimpleDialog::simulateOk()
{
	if (commit()) {
		mCommitted = true;
		close();
	}
}

/**
 * Default handler for the help button.
 * In the BPE, this would try to locate a help file derived
 * from the class name.  Here we would have to allow the help
 * file name to be set.
 */
PUBLIC void SimpleDialog::help() 
{
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
