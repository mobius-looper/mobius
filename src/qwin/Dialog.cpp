/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * If you want to define a dialog in the resource file you can,
 * but we also support building dialogs out of Components just like Swing.
 *
 * For Component-based dialogs, we create a WS_POPUP window and have
 * to implement some of the same things that the built-in Windows
 * dialog proc does.  Since this isn't well described, I'm guessing 
 * at a few things, but it seems to work reasonably well.  Some issues are:
 *
 * When the dialog is resized we get some unusual paint behavior.
 * The border leaves "trails".
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								DIALOG                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Dialog::Dialog()
{
	initDialog();
}

PUBLIC Dialog::Dialog(Window *parent) {
	initDialog();
	setParent(parent);
}

PUBLIC Dialog::Dialog(Window* parent, const char *title) {
	initDialog();
	setParent(parent);
	setTitle(title);
}

PUBLIC Dialog::~Dialog() {
	delete mResource;
}

void Dialog::initDialog() 
{
	mClassName = "Dialog";
	mResource = NULL;
    mDefault = NULL;
    mModal = false;

    // let's make these auto-sized and auto-centered by default since that's
    // almost always what we want
    mAutoSize = true;
    mAutoCenter = true;
}

ComponentUI* Dialog::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getDialogUI(this);
    return mUI;
}

DialogUI* Dialog::getDialogUI()
{
    return (DialogUI*)getUI();
}

void Dialog::setResource(const char *name) 
{
	delete mResource;
	mResource = CopyString(name);
}

const char* Dialog::getResource()
{
    return mResource;
}

void Dialog::setModal(bool b) 
{
    mModal = b;
}

bool Dialog::isModal()
{
	return mModal;
}

void Dialog::prepareToShow()
{
}

/**
 * Windows returns an unsigned long, not sure if we really need to 
 * propagate that convention, since we don't define what the return
 * value means anyway.
 */
void Dialog::show()
{
    // let subclass initialize
    prepareToShow();

    // find the default button if there is one
    mDefault = findDefaultButton(this);

	// Unlike most ComponentUI's this one is transient
    DialogUI* ui = getDialogUI();

    ui->show();

    if (mModal) {
        delete mUI;
        mUI = NULL;
	}
}

/**
 * Walk over the child component hierarchy looking for a default button.
 * Have to defer this until the dialog is fully constructed.
 */
Button* Dialog::findDefaultButton(Container *parent)
{
    Button* button = NULL;

    for (Component* c = parent->getComponents() ; 
         c != NULL && button == NULL ; 
         c = c->getNext()) {
        Container* container = c->isContainer();
        if (container != NULL)
          button = findDefaultButton(container);
        else {
            Button* b = c->isButton();
            if (b != NULL && b->isDefault())
              button = b;
        }
    }

    return button;
}

void Dialog::dumpLocal(int i)
{
    indent(i);
    fprintf(stdout, "Dialog: %d %d %d %d\n", 
            mBounds.x, mBounds.y, mBounds.width, mBounds.height);
}

/**
 * Called by Component::messageHandler when the return key is
 * pressed while one of our child components has focus.
 * The return key can click any button, by default only the 
 * spacebar will click the focused button.  If the focused
 * component is not a button, click the default button if we have one.
 */
PUBLIC void Dialog::processReturn(Component* c)
{
    // ignore if the component with focus is a button, the default
    // processing will cause it to be clicked and it may not
    // be the default button
    Button* b = c->isButton();
    if (b != NULL)
      b->click();
    else if (mDefault != NULL)
      mDefault->click();
}

/**
 * Called by Component::messageHandler when the escape key is
 * pressed while one of our child components has focus.
 */
PUBLIC void Dialog::processEscape(Component* c)
{
    close();
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 WINDOWS UI                               *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

WindowsDialog::WindowsDialog(Dialog* d) : WindowsWindow(d)
{
}

WindowsDialog::~WindowsDialog()
{
}

PRIVATE BOOL CALLBACK DialogProcedure(HWND dialog, UINT msg, 
									  WPARAM wparam, LPARAM lparam)
{
	BOOL result = FALSE;

	// we actually will never see this since we don't have a way
	// to attach the Dialog right now
	WindowsDialog* ui = (WindowsDialog *)
        GetWindowLong(dialog, GWL_USERDATA);

	if (ui != NULL) {
        result = ui->dialogHandler(msg, wparam, lparam);
    }
	else {
		switch (msg) {
			case WM_INITDIALOG: {
				result = TRUE;	
			}
			break;

			case WM_COMMAND: {
				int id = LOWORD(wparam);
				switch (id) {
					case IDOK:
					case IDCANCEL: {
						// these are standard control constants
						EndDialog(dialog, id);
						result = TRUE;
					}
					break;
				}
			}
			break;
			
		}
	}

	return result;
}

/**
 * Default dialog message handler.
 */
PUBLIC BOOL WindowsDialog::dialogHandler(UINT msg, WPARAM wparam, 
                                           LPARAM lparam)
{
	BOOL result = FALSE;

	switch (msg) {

		case WM_INITDIALOG: {
			result = TRUE;	
		}
		break;

		case WM_COMMAND: {
			int id = LOWORD(wparam);
			switch (id) {
				case IDOK:
				case IDCANCEL: {
					// these are standard control constants
					// the second arg is what will be returned by 
					// the DialogBox call
					if (mHandle != NULL)
					  EndDialog(mHandle, id);
					result = TRUE;
				}
				break;

				default: {
					// figure out a way to pass it back to the class
				}
				break;
			}
		}
		break;

	}
	return result;
}

void WindowsDialog::show()
{
    unsigned long result = 0;
    Dialog* dialog = (Dialog*)mWindow;

    if (dialog->getResource() != NULL) {
        Window* parent = dialog->getParentWindow();
        HWND handle = getHandle(parent);
        if (handle != NULL) {
            WindowsContext* con = (WindowsContext*)
                parent->getContext();
			
            // This really returns an INT_PTR which is the same
            // as unsigned long, but is really confusing since
            // you might think it would be int*
            result = DialogBox(con->getInstance(), dialog->getResource(), 
                               handle, DialogProcedure);

            if (result == -1)
              printf("Unable to open dialog: error %d\n", GetLastError());
            else if (result == 0) {
                // Petzold says this means the parent window was invalid
                printf("Unable to open dialog: invalid parent window\n");
            }
            else {
                printf("Dialog returned %ld\n", result);
            }
        }
	}
	else {
		// no resource, assume we're fleshed out like a Frame
		// !! todo: should seperate visibility and creation
		// of handles so we can pack before display?
		open();
		if (dialog->isModal()) {
			// inform the parent about this so it can lobotomize
			// its message handler and keep us focused!!
			result = modalEventLoop();
		}
	}

	// might want to extend the UI signature to allow the dialog
	// to return a value?
    //return result;
}

/**
 * This is a BAD way to be handling modal dialogs.
 * If the parent window is destroyed while we're in this loop,
 * we will process the WM_DESTROY, call PostQuitMessage and start
 * closing things.  But we're still in the Windows::run event loop,
 * which will never receive any more messages and it will hang.
 *
 * Have to set a kludgey "no close" flag in the parent Window to
 * keep it from processing the WM_CLOSE method.
 */
PRIVATE unsigned long WindowsDialog::modalEventLoop()
{
    unsigned long result = 0;
    BOOL status;
    MSG msg;

	Window* parent = (Window*)mWindow->getParent();
	if (parent != NULL)
	  parent->setNoClose(true);

    // may want some other extit flags here...
    // status will be 0 if the WM_QUIT is retrieved

    while ((status = GetMessage(&msg, NULL, 0, 0)) != 0) {

        if (status == -1) {
            // handle error and possibly exit
            printf("Dialog::run GetMessage error\n");
        }
        else if (mAccel == NULL ||
                 !TranslateAccelerator(mHandle, mAccel, &msg)) {

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	if (parent != NULL)
	  parent->setNoClose(false);

	mHandle = NULL;

    // Though GetMessage returned 0, there will still be a valid
    // WM_QUIT in MSG, return its paramater.
    // This is what we ordinarilly do for normal Windows.
    // How should we return a pointer from a dialog?
    // I guess we would have to leave something behind in the Dialog object.
    result = msg.wParam;

    return result;
}

QWIN_END_NAMESPACE
#endif// _WIN32

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

MacDialog::MacDialog(Dialog* d) : MacWindow(d)
{
}

MacDialog::~MacDialog()
{
}

/**
 * Overload this to make sure it can't be called, you
 * have to use show()
 */
PUBLIC void MacDialog::open() 
{
}

void MacDialog::show()
{
	if (mHandle == NULL) {
		// MacWindow does most of the work 
		MacWindow::open();

		// Bidule uses a floating window, so we have to put any
		// dialogs we open in the floating layer.
		// UGH, we're violating some encapsulation here
		// but I just can stand opening another hole in he WindowUI.

		WindowRef parent = (WindowRef)(mWindow->getNativeHandle());
		if (parent != NULL) {
			WindowClass wclass;
			GetWindowClass(parent, &wclass);
			if (wclass == kFloatingWindowClass) {
				WindowRef win = (WindowRef)mHandle;
				SetWindowGroup(win, GetWindowGroupOfClass(kFloatingWindowClass));
			}
			else {
				// Bidule AU uses a kDocumentWindowClass but it comes
				// out below the Mobius window, not sure what the deal
				// is but force to floating
				WindowRef win = (WindowRef)mHandle;
				SetWindowGroup(win, GetWindowGroupOfClass(kFloatingWindowClass));
			}
		}

		if (mHandle != NULL) {
			Dialog* dialog = (Dialog*)mWindow;
			if (dialog->isModal()) {
				// will hang until close
				RunAppModalLoopForWindow((WindowRef)mHandle);
			}
		}
	}
}

/**
 * Window overload.
 * If we were a modal dialog terminate the modal event loop.
 */ 
PRIVATE void MacDialog::closeEvent()
{
	Dialog* dialog = (Dialog*)mWindow;
	
	// gives modal dialogs a chance to set the cancel flag
	dialog->closing();

	if (dialog->isModal())
	  QuitAppModalLoopForWindow((WindowRef)mHandle);
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
