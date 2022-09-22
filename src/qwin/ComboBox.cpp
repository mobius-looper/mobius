/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Very similar to ListBox, consider factoring out a superclass.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 COMBO BOX                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ComboBox::ComboBox() 
{
	initComboBox();
}

PRIVATE void ComboBox::initComboBox()
{
	mClassName = "ComboBox";
	mValues = NULL;
    mValue = NULL;
	mEditable = false;
    mRows = 5;
    mColumns = 40;
    mSelected = -1;
}

PUBLIC ComboBox::ComboBox(StringList* values)
{
	initComboBox();
	setValues(values);
}

PUBLIC ComboBox::ComboBox(const char** values)
{
	initComboBox();
	if (values == NULL)
	  setValues(NULL);
	else
	  setValues(new StringList(values));
}

PUBLIC ComboBox::~ComboBox()
{
	delete mValues;
	delete mValue;
}

PUBLIC ComponentUI* ComboBox::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getComboBoxUI(this);
	return mUI;
}

PUBLIC ComboBoxUI* ComboBox::getComboBoxUI()
{
    return (ComboBoxUI*)getUI();
}

PUBLIC void ComboBox::setEditable(bool b) 
{
    mEditable = b;
}

PUBLIC bool ComboBox::isEditable()
{
    return mEditable;
}

/**
 * Swing calls this setMaximumRowCount, simplifying this to "rows".
 */
PUBLIC void ComboBox::setRows(int i)
{
    mRows = i;
}

PUBLIC int ComboBox::getRows() 
{
    return mRows;
}

/**
 * Swing doesn't appear to have anything like this, it auto-sizes
 * based on the widest string int the model.  We should do the
 * same but until then allow this.
 */
PUBLIC void ComboBox::setColumns(int i)
{
    mColumns = i;
}

PUBLIC int ComboBox::getColumns() 
{
    return mColumns;
}

/**
 * We assume ownership of the list!
 *
 * Swing has setModel though it does have addItem for incremental changes.
 * Call these "values" instead of "items" for consistency with ListBox.
 */
PUBLIC void ComboBox::setValues(StringList* values)
{
	if (mValues != values) {
		delete mValues;
		mValues = values;
	}

    // should we reset mValue too?
    ComboBoxUI* ui = getComboBoxUI();
    ui->setValues(values);

	updateNativeBounds();
}

PUBLIC StringList* ComboBox::getValues()
{
    return mValues;
}

/**
 * Swing calls this addItem, but let's use "value" consistently.
 */
PUBLIC void ComboBox::addValue(const char *value)
{
	if (value != NULL) {
		if (mValues == NULL)
		  mValues = new StringList();
		mValues->add(value);
	
        ComboBoxUI* ui = getComboBoxUI();
        ui->addValue(value);

		updateNativeBounds();
	}
}

/**
 * Set to -1 to clear the selection.
 */
PUBLIC void ComboBox::setSelectedIndex(int i)
{
    if (i == -1 || i >= 0) {
        mSelected = i;
        ComboBoxUI* ui = getComboBoxUI();
        ui->setSelectedIndex(i);
	}
}

/**
 * Return the index of the selected item.
 * Not sure what happens if this is editable and the selection field
 * has text not in the value list.  Hopefully -1
 */
PUBLIC int ComboBox::getSelectedIndex()
{
    ComboBoxUI* ui = getComboBoxUI();
    if (ui->isOpen())
      mSelected = ui->getSelectedIndex();
	return mSelected;
}

/**
 * Swing calls this setSelectedItem but we're using "value" consistently.
 */
PUBLIC void ComboBox::setSelectedValue(const char *s)
{
	if (mValues != NULL) {
        if (s == NULL) {
            // same as setting selected index to -1
            ComboBoxUI* ui = getComboBoxUI();
            ui->setSelectedIndex(-1);
        }
        else {
            int index = mValues->indexOf((void*)s);
            if (index >= 0)
              setSelectedIndex(index);
            else if (mEditable) {
                if (mValue != s) {
                    mValue = CopyString(s);

                    ComboBoxUI* ui = getComboBoxUI();
                    ui->setSelectedValue(s);
                }
            }
            else {
                // a value we didn't recognize, could either ignore
                // or treat it like setting to null, ignore feels better
            }
        }
    }
}

PUBLIC void ComboBox::setValue(const char* s)
{
	setSelectedValue(s);
}

PUBLIC void ComboBox::setValue(int i)
{
	setSelectedIndex(i);
}

PUBLIC const char* ComboBox::getValue()
{
	return getSelectedValue();
}

PUBLIC const char* ComboBox::getSelectedValue()
{
    const char *value = NULL;
	if (mValues != NULL) {
		int index = getSelectedIndex();
		if (index >= 0)
		  value = (const char *)mValues->get(index);
	}
    
    // prefer the UI model if it is open
    // !! this is very confusing why don't we always set mValue?

    ComboBoxUI* ui = getComboBoxUI();
    if (value == NULL && mEditable && ui->isOpen()) {
        char* uivalue = ui->getSelectedValue();
        if (uivalue != NULL) {
            delete mValue;
            mValue = uivalue;
            value = mValue;
        }
    }
    return value;
}

PUBLIC Dimension* ComboBox::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void ComboBox::dumpLocal(int indent)
{
    dumpType(indent, "ComboBox");
}

PUBLIC void ComboBox::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                                  WINDOWS                                 *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsComboBox::WindowsComboBox(ComboBox* cb) 
{
    mComboBox = cb;
}

PUBLIC WindowsComboBox::~WindowsComboBox() 
{
}

/**
 * The control also supports CB_INSERTSTRING and CB_DELETESTRING
 * which can insert and remove elements at specific indexes.
 *
 */
PUBLIC void WindowsComboBox::setValues(StringList* values)
{
	if (mHandle != NULL) {
		SendMessage(mHandle, WM_SETREDRAW, FALSE, 0);
		SendMessage(mHandle, CB_RESETCONTENT, 0, 0);
		if (values != NULL) {
			for (int i = 0 ; i < values->size() ; i++)
			  SendMessage(mHandle, CB_ADDSTRING, 0, (LPARAM)values->get(i));
		}
		SendMessage(mHandle, WM_SETREDRAW, TRUE, 0);
	}
}

PUBLIC void WindowsComboBox::addValue(const char *value)
{
	if (value != NULL) {
		if (mHandle != NULL)
		  SendMessage(mHandle, CB_ADDSTRING, 0, (LPARAM)value);
	}
}

/**
 * Set to -1 to clear the selection.
 */
PUBLIC void WindowsComboBox::setSelectedIndex(int i)
{
    if (mHandle != NULL)
      SendMessage(mHandle, CB_SETCURSEL, i, 0);
}

/**
 * Return the index of the selected item.
 * Not sure what happens if this is editable and the selection field
 * has text not in the value list.  Hopefully -1
 */
PUBLIC int WindowsComboBox::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
        selected = SendMessage(mHandle, CB_GETCURSEL, 0, 0);
        if (selected == CB_ERR)
          selected = -1;
    }
	return selected;
}

PUBLIC void WindowsComboBox::setSelectedValue(const char *s)
{
    if (mHandle != NULL)
      SendMessage(mHandle, WM_SETTEXT, 0, (LONG)s);
}

PUBLIC char* WindowsComboBox::getSelectedValue()
{
    char *value = NULL;
    if (mHandle != NULL) {
        // get whatever is displayed
        char buf[256];
        SendMessage(mHandle, WM_GETTEXT, sizeof(buf), (LONG)buf);
        if (strlen(buf) > 0)
          value = CopyString(buf);
    }
	return value;
}

PUBLIC void WindowsComboBox::open()
{
	if (mHandle == NULL) {

		// capture the initial index before we open, once open
		// ComboBox will always defer to us for the selection
		int initialSelection = mComboBox->getSelectedIndex();

		HWND parent = getParentHandle();
        if (parent != NULL) {
            // NOINTEGRALHEIGHT specifies to keep the combo box at the
            // size specified in CreateWindow, normally it will be resized
            // to not display partial items.
            // OEMCONVERT might be necessary for dropdowns with file names?
            // SORT will sort the items.

            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP;
        
            // CBS_NOINTEGRALHEIGHT;

            // SIMPLE rather than DROPDOWN displays the list box at all times, 
            // AUTOSCROLL enables entry of text wider than the box
            if (mComboBox->isEditable())
              style |= CBS_DROPDOWN | CBS_AUTOHSCROLL;
            else
              style |= CBS_DROPDOWNLIST;

            Bounds* b = mComboBox->getBounds();
            Point p;
            mComboBox->getNativeLocation(&p);

            // The height of the dropdown must be set in the CreateWindow call
            // XP supports a message to set this later, but try to 
            // be compatible with older 98.  
            // The actual height will be adjusted down to an 
            // integral number of items, truncating items that can't be fully
            // displayed in the alloted space.  It doesn't seem possible
            // to scroll these, but combo boxes probably shouldn't be that big
            // !! Need to defer this to getPreferredSize somehow like all
            // the others

            int height = getFullHeight();

            mHandle = CreateWindow("COMBOBOX",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create ComboBox control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mComboBox->initVisibility();

                // this is only available in XP
                //ComboBox_SetMinVisible(mHandle, mRows);

                // initialize the native object with pre defined settings 
				setValues(mComboBox->getValues());
				setSelectedIndex(initialSelection);
            }
        }
    }
}

/**
 * Assume these are presized, but should whip through
 * the values and determine the max text size!!
 * Must be wide enough for the longest string plus
 * the width of the scroll bar (SM_CSVSCROLL).
 *
 * Note that this is just the size of the unopened
 * field, the actual window size will be calculated
 * later to include the size of the drop down.
 */
PUBLIC void WindowsComboBox::getPreferredSize(Window* w, Dimension* d)
{
    TextMetrics* tm = w->getTextMetrics();

    // TODO: calculate optimal text size by looking at all the values
    int cols = mComboBox->getColumns();
    if (cols <= 0) cols = 20;
    d->width = cols * tm->getMaxWidth();
    d->width += UIManager::getVertScrollBarWidth();
		
    // 1 1/2 times char height if using border
    // note that the number of values doesn't factor into this
    // we only display the selection field
    int fontHeight= tm->getHeight() + tm->getExternalLeading();
    d->height = fontHeight;
    d->height += fontHeight / 2;
}

PUBLIC void WindowsComboBox::command(int code) 
{
    // other messages include CBN_DROPDOWN when the list is opened
    if (code == CBN_SELCHANGE)
      mComboBox->fireActionPerformed();
}

/**
 * A component overload called whenever the location or size
 * of the component is changed by a layout manager.  Since
 * our preferred size is different than the actual window size, 
 * have to keep adjusting it.
 */
PUBLIC void WindowsComboBox::updateBounds() 
{
    if (mHandle != NULL) {
        Bounds* b = mComboBox->getBounds();
        Point p;
        mComboBox->getNativeLocation(&p);

		int height = getFullHeight();

        MoveWindow(mHandle, p.x, p.y, b->width, height, true);
    }
}

/**
 * Calculate the height of the dropdown.  This must
 * be the height given in the CreateWindow call and we must
 * also set the size every time the value list changes.
 *
 * XP supports a message to set this later, but try to be compatible
 * with older 98.  The actual height will be adjusted down to an 
 * integral number of items, truncating items that can't be fully
 * displayed in the alloted space.  It doesn't seem possible
 * to scroll these, but combo boxes probably shouldn't be that large.
 * !! Need to defer this to getPreferredSize somehow like all
 * the others?
 */
PRIVATE int WindowsComboBox::getFullHeight()
{
	int height = 0;

	Window* w = mComboBox->getWindow();
	if (w != NULL) {
        TextMetrics* tm = w->getTextMetrics();
        StringList* values = mComboBox->getValues();

		int fontHeight= tm->getHeight() + tm->getExternalLeading();
		height = fontHeight + (fontHeight / 2);

        int rows = (values != NULL) ? values->size() : 1;

		// having trouble with missing rows on Vista, kludge this
		rows++;

        // GAG!  Originally had fontHeight+2 but that is too small, try 8
        // figure this shit out!
        height += (rows * (fontHeight + 4));

		// this is too short, we lose a row, might have
		// to include the height of the starting field too?
        Dimension* preferred = mComboBox->getPreferredSize(w);
		if (preferred != NULL)
		  height += preferred->height;
		
		// still too short, figure this shit out
		height += fontHeight;
	}

	return height;
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

/**
 * The HIView ComboBox is closest to the WIN32 COMBOBOX, except that
 * it also allows you to type in text.  I don't want that so we'll
 * have to use Pop-up menus instead.  
 */
PUBLIC MacComboBox::MacComboBox(ComboBox* cb) 
{
    mComboBox = cb;
}

PUBLIC MacComboBox::~MacComboBox() 
{
	// NOTE: This is not the place to release associated stuff, 
	// may need to overload the close() method
}

/**
 * In theory we should allow the values in the menu to change
 * after opening.  We don't seem to need that yet....
 */
PUBLIC void MacComboBox::setValues(StringList* values)
{
	if (mHandle != NULL) {

		ControlRef button = (ControlRef)mHandle;
		
		// remember the current selection if we have one
		int initialSelection = GetControl32BitValue(button);

		// what this amounts to is a menu definition
		// this will get the existing menu if we want to 
		// changed these after construction
		// err = GetControlData(button, 0, kControlPopupButtonMenuRefTag,
		// sizeof( MenuRef ), outMenu, NULL );
	
		// make a menu
		MenuRef menu;
		CreateNewMenu(MacMenuItem::GenMenuId(), 0, &menu);

		int items = 0;
		if (values != NULL) {
			items = values->size();
			for (int i = 0 ; i < items ; i++) {
				const char* str = values->getString(i);
				CFStringRef cfstr = MakeCFStringRef(str);
				// last arg is the item id, it does not matter what they are
				// since they are managed by the PopupButton?
				InsertMenuItemTextWithCFString(menu, cfstr, i, 0, i);
			}
		}

		SetControl32BitMinimum(button, 0);
		SetControl32BitMaximum(button, items);

		if (initialSelection > items) {
			if (items > 0)
			  initialSelection = 1;
			else
			  initialSelection = 0;		// not sure what this means
		}
		SetControl32BitValue(button, initialSelection);

		OSErr err = SetControlData(button, 
								   kControlEntireControl, // inPart
								   kControlPopupButtonMenuRefTag,
								   (Size)sizeof(MenuRef),
								   &menu);

		CheckErr(err, "MacComboBox::setValues");
	}
}

/**
 * I don't think we use this one.  In theory we have to do the same as setVales if
 * there is no menu, or get the current menu and append a value.
 */
PUBLIC void MacComboBox::addValue(const char *value)
{
	if (mHandle != NULL && value != NULL) {

		ControlRef button = (ControlRef)mHandle;

		// what this amounts to is a menu definition
		// this will get the existing menu if we want to 
		// changed these after construction
		MenuRef menu = NULL;
		// 0 is apparently kControlEntireControl?
		OSErr err = GetControlData(button, 0, kControlPopupButtonMenuRefTag,
								   sizeof(MenuRef), &menu, NULL);

		CheckErr(err, "MacComboBox::addValue GetControlData");

		if (menu == NULL) {
			CreateNewMenu(MacMenuItem::GenMenuId(), 0, &menu);
			err = SetControlData(button, 
								 kControlEntireControl, // inPart
								 kControlPopupButtonMenuRefTag,
								 (Size)sizeof(MenuRef),
								 &menu);
			CheckErr(err, "MacComboBox::addValue SetControlData");
		}

		// assuming the control max parallels the menu items
		SInt32 items = GetControl32BitMaximum(button);

		CFStringRef cfstr = MakeCFStringRef(value);
		// last arg is the item id, it does not matter what they are
		// since they are managed by the PopupButton?
		InsertMenuItemTextWithCFString(menu, cfstr, items, 0, items);

		SetControl32BitMaximum(button, items + 1);
		// auto select the first if we're starting from an empty list
		int value = GetControl32BitValue(button);
		if (value == 0)
		  SetControl32BitValue(button, 1);
	}
}

/**
 * Set to -1 to clear the selection.
 * Note that the Qwin indexes are 0 based while the Mac menu item
 * indexes are 1 based.
 *
 * NB: We're using a custom message to change the index to make
 * sure that it gets done in the UI thread.  This was necessary
 * for the Mobius MIDI Control window that wants to change combo
 * box selections when "capture" is on.  These changes come in 
 * on the MIDI handler thread and can cause a crash if the UI
 * thread is still processing an invalidate() request from the
 * last MIDI message.  At least that's true for Text, ComboBox
 * may be more resiliant since it's only dealing with numbers
 * but be safe. 
 */
PUBLIC void MacComboBox::setSelectedIndex(int i)
{
    if (mHandle != NULL) {
		bool doAsync = true;
		if (doAsync) {
			void* value = (void*)i;
			sendChangeRequest(0, value);
		}
		else {
			setSelectedIndexNow(i);
			mComboBox->invalidate();
		}
	}
}

PUBLIC void MacComboBox::handleChangeRequest(int type, void* value)
{
	setSelectedIndexNow((int)value);

	// need to invalidate to see changes
	// don't use mComboBox->invalidate() which will send another message,
	// since we know we're in the UI thread call invalidateNative
	// directly
	invalidateNative(mComboBox);
}

PRIVATE void MacComboBox::setSelectedIndexNow(int index)
{
	// we normally maintain indexes zero based with -1 for no select
	// control needs 1 based with zero for no select
	SetControl32BitValue((ControlRef)mHandle, index + 1);
}

/**
 * Return the index of the selected item.
 * Not sure what happens if this is editable and the selection field
 * has text not in the value list.  Hopefully -1
 */
PUBLIC int MacComboBox::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
		selected = GetControl32BitValue((ControlRef)mHandle);
		// adjust from 1 based to 0 based
		selected--;
	}
	return selected;
}

/**
 * I'm not sure what this is suppsed to do, we only use it if
 * the mEditable flag is set in the ComboBox.  Maybe this changes
 * the label of the menu item?
 *
 * Or maybe it is supposed to match the value to the existing menu
 * items and select the matching one.  If so this should be done
 * in ComboBox, just calling down to setSelectedIndex().
 */
PUBLIC void MacComboBox::setSelectedValue(const char *s)
{
    if (mHandle != NULL) {
		//SendMessage(mHandle, WM_SETTEXT, 0, (LONG)s);
		Trace(1, "MacComboBox::setSelectedValue not implemented!\n");
	}
}

/**
 * Just let ComboBox return the value calculated from the selected index.
 */
PUBLIC char* MacComboBox::getSelectedValue()
{
	return NULL;
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * We also get a Command/Process but it happens before the value changes.
 * Have to wait for Hit so the action handler sees the new value.
 */
PRIVATE EventTypeSpec ComboBoxEventsOfInterest[] = {
	{ kEventClassControl, kEventControlHit }
};

PRIVATE OSStatus
ComboBoxEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("ComboBox", cls, kind);

	if (cls == kEventClassControl) {
		if (kind == kEventControlHit) {
			MacComboBox* b = (MacComboBox*)data;
			if (b != NULL)
			  b->fireActionPerformed();
		}
	}

	return result;
}

PUBLIC void MacComboBox::fireActionPerformed()
{
	mComboBox->fireActionPerformed();
}

/**
 * There are two ways to associate a menu with the button.
 * First create a normal menu and use InsertMenu to add it to the menu
 * bar with a "beforeId" of kInsertHierarchicalMenu.  These menus are
 * managed by the menu bar but are not visible.  They are however searched
 * when looking for command key bindings so having a lot of submenus in the
 * menu bar may affect performance.  For Mobius dialogs this seems silly
 * since there will be a lot of popups so we'll do it the other way.
 * 
 * If you pass -12345 as the MenuID, the control will "delay its acquisition
 * of a menu", you later call SetControlData with kControlPopupButtonMenuRefTag
 * to assign a menu.
 */
MenuID MagicConstant = -12345;

PUBLIC void MacComboBox::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		// capture the initial index before we open, once open
		// ComboBox will always defer to us for the selection
		int initialSelection = mComboBox->getSelectedIndex();

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };

		status = CreatePopupButtonControl(window, &bounds, 
										  NULL, // title
										  MagicConstant,
										  false, // variableWidth
										  0, // titleWidth,
										  teFlushLeft, // title justification
										  0, // QuickDraw style bitfield for the title
										  &control);

		if (CheckStatus(status, "MacComboBox::open")) {
			mHandle = control;

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(ComboBoxEventHandler),
												GetEventTypeCount(ComboBoxEventsOfInterest),
												ComboBoxEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacComboBox::InstallEventHandler");

			setValues(mComboBox->getValues());
			setSelectedIndex(initialSelection);
	
			SetControlVisibility(control, true, false);
		}
	}
}


QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
