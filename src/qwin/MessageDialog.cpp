/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A component providinging a wrapper around the built-in Message Box dialog.
 * There are a lot more styles available on Windows.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							MESSAGE DIALOG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC MessageDialog::MessageDialog(Window* parent) :
    SystemDialog(parent)
{
	init();
}

PUBLIC MessageDialog::MessageDialog(Window* parent, const char *title, 
									const char *text) :
    SystemDialog(parent)
{
	init();
	setTitle(title);
	setText(text);
}

PRIVATE void MessageDialog::init()
{
	mText = NULL;
	mCancelable = false;
	mInfo = false;
}

PUBLIC MessageDialog::~MessageDialog()
{
	delete mText;
}

PUBLIC void MessageDialog::setText(const char* s) 
{
	delete mText;
	mText = CopyString(s);
}

PUBLIC const char* MessageDialog::getText()
{
    return mText;
}

PUBLIC void MessageDialog::setCancelable(bool b)
{
	mCancelable = b;
}

PUBLIC bool MessageDialog::isCancelable()
{
    return mCancelable;
}

PUBLIC void MessageDialog::setInformational(bool b)
{
	mInfo = b;
}

PUBLIC bool MessageDialog::isInformational()
{
    return mInfo;
}

PUBLIC bool MessageDialog::show()
{
	mCanceled = false;

    SystemDialogUI* ui = UIManager::getMessageDialogUI(this);
    ui->show();
    delete ui;

	return !mCanceled;
}

PUBLIC void MessageDialog::showError(Window* parent, const char *title, 
									 const char* text)
{
	MessageDialog* d = new MessageDialog(parent, title, text);
	d->show();
	delete d;
}

PUBLIC void MessageDialog::showMessage(Window* parent, const char *title, 
									   const char* text)
{
	MessageDialog* d = new MessageDialog(parent, title, text);
	d->setInformational(true);
	d->show();
	delete d;
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							MESSAGE DIALOG                              *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsMessageDialog::WindowsMessageDialog(MessageDialog* d)
{
	mDialog = d;
}

PUBLIC WindowsMessageDialog::~WindowsMessageDialog()
{
}

PUBLIC void WindowsMessageDialog::show()
{
	UINT style = MB_APPLMODAL;
    HWND parent = NULL;

    Window* pw = mDialog->getParent();
    if (pw != NULL)
      parent = WindowsComponent::getHandle(pw);

	if (mDialog->isCancelable())
	  style |= MB_OKCANCEL;
	else
	  style |= MB_OK;

	if (mDialog->isInformational())
	  style |= MB_ICONINFORMATION;
	else
	  style |= MB_ICONEXCLAMATION;

	int rc = MessageBox(parent,
						mDialog->getText(), 
						mDialog->getTitle(), 
						style);

	if (mDialog->isCancelable()) 
	  mDialog->setCanceled(rc == IDCANCEL);
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

PUBLIC MacMessageDialog::MacMessageDialog(MessageDialog* d)
{
	mDialog = d;
}

PUBLIC MacMessageDialog::~MacMessageDialog()
{
}

PUBLIC void MacMessageDialog::show()
{
	// kAlertPlainAlert - the simplest, no application icon
    // kAlertNoteAlert - displays the application icon on the left
	// kAlertStopAlert - looked the same as NoteAlert to me
	// kAlertCautionAlert - ! inside yellow triangle with application icon in 
	//                        lower right corner
	AlertType type;
	if (mDialog->isInformational())
	  type = kAlertNoteAlert;
	else
	  type = kAlertCautionAlert;

	// we have a title and text, Mac alerts don't have titles, should
	// we ignore it or use the error/explanation pair?

	CFStringRef error = MakeCFStringRef(mDialog->getTitle());
	CFStringRef explanation = MakeCFStringRef(mDialog->getText());

	// this is optional but can be used to set a few interesting parameters
	AlertStdCFStringAlertParamRec param;
	param.version = kStdCFStringAlertVersionOne;
	param.movable = true;
	// true to indiciate alert should have a help button
	param.helpButton = false;
	// text for the OK button, to use default pass -1, to disable button pass null
	param.defaultText = (CFStringRef)kAlertDefaultOKText;
	// text for Cancel button, -1 for default, null to disable button
	if (mDialog->isCancelable())
	  param.cancelText = (CFStringRef)kAlertDefaultCancelText;
	else
	  param.cancelText = NULL;
	// text for the "other" (leftmost) button
	param.otherText = NULL;
	// specifies which button acts as the default and cancel buttons
	// not sure why you would want to change these
	param.defaultButton = kAlertStdAlertOKButton;
	param.cancelButton = kAlertStdAlertCancelButton;
	// alert box position, as defined by a window positioning constant
	param.position = kWindowDefaultPosition;

	// various options
	// kStdAlertDoNotDisposeSheet, kStdAlertDoNotAnimateOnDefault,
	// kStdAlertDoNotAnimateOnCancel, kStdAlertDoNotAnimateOnOther,
	// kStdAlertDoNotCloseOnHelp
	param.flags = 0;

	// CreateStandardSheet is almost identical but needs an event handler
	DialogRef alert;
	OSStatus status = CreateStandardAlert(type, error, explanation, &param, &alert);
	if (CheckStatus(status, "MacMessageDialog::CreateStandardAlert")) {
		
		// event filter function to handle events that do not apply to the alert
		ModalFilterUPP filterProc = NULL;
		DialogItemIndex outItemHit;
		status = RunStandardAlert(alert, filterProc, &outItemHit);
		if (CheckStatus(status, "MacMessageDialog::RunStandardAlert")) {

			// cancel button is 2 and ok button is 1, not sure if this
			// changes if the "other" button is enabled
			//printf("RunStandardAlert returned %d\n", outItemHit);

			// outItemHit contains the item index of the button that
			// was pressed to close the alert
			if (mDialog->isCancelable() && outItemHit == 2)
			  mDialog->setCanceled(true);
		}
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
