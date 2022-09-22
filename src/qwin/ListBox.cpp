/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * These are like JList with a simplified model structure.
 * 
 * Other control messages:
 *
 *  LB_GETCOUNT - return the number of items in the list
 *  LB_SELECTSTRING - single select based on partial pattern match
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
 *                                  LISTBOX                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC ListBox::ListBox() 
{
	init();
}

PRIVATE void ListBox::init()
{
	mClassName = "ListBox";
	mValues = NULL;
	mMultiSelect = false;
    mRows = 5;
    mColumns = 0;
    mAnnotationColumns = 0;
	mSelected = NULL;
    
    // alternate list of annotations for the right column
    mAnnotations = NULL;
}

PUBLIC ListBox::ListBox(StringList* values) 
{
	init();
	setValues(values);
}

PUBLIC ListBox::~ListBox()
{
    delete mValues;
    delete mAnnotations;
	delete mSelected;
}

PUBLIC ComponentUI* ListBox::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getListBoxUI(this);
	return mUI;
}

PUBLIC ListBoxUI* ListBox::getListBoxUI()
{
    return (ListBoxUI*)getUI();
}

PUBLIC void ListBox::setRows(int i) 
{
    mRows = i;
}

PUBLIC int ListBox::getRows()
{
	return mRows;
}

PUBLIC void ListBox::setColumns(int i) 
{
    mColumns = i;
}

PUBLIC int ListBox::getColumns()
{
	return mColumns;
}

PUBLIC void ListBox::setAnnotationColumns(int i) 
{
    mAnnotationColumns = i;
}

PUBLIC int ListBox::getAnnotationColumns()
{
	return mAnnotationColumns;
}

PUBLIC void ListBox::setMultiSelect(bool b)
{
    mMultiSelect = b;
}

PUBLIC bool ListBox::isMultiSelect()
{
    return mMultiSelect;
}

/**
 * We assume ownership of the list!
 */
PUBLIC void ListBox::setValues(StringList* values)
{
	if (mValues != values) {
		// !! if the UI thread happens to be refreshing right now
		// could get memory violation, need csect or phased delete
		// in practice this won't be a problem yet because we don't
		// have list boxes in the main mobius window, only in dialogs
		delete mValues;
		mValues = values;
	}

    ListBoxUI* ui = getListBoxUI();
    ui->setValues(values);
}

PUBLIC void ListBox::setAnnotations(StringList* values)
{
	if (mAnnotations != values) {
		// !! if the UI thread happens to be refreshing right now
		// could get memory violation, need csect or phased delete
		delete mAnnotations;
		mAnnotations = values;
	}

    ListBoxUI* ui = getListBoxUI();
    ui->setAnnotations(values);
}

PUBLIC StringList* ListBox::getAnnotations()
{
    return mAnnotations;
}

/**
 * Refresh the native components after a change in the value list.
 */
PRIVATE void ListBox::rebuild()
{
	setValues(mValues);
	setAnnotations(mAnnotations);
}

PUBLIC StringList* ListBox::getValues()
{
    return mValues;
}

PUBLIC void ListBox::addValue(const char *value)
{
	if (value != NULL) {
		// put it on this list first, UI may derive it from the full list
		if (mValues == NULL)
		  mValues = new StringList();
		mValues->add(value);

        ListBoxUI* ui = getListBoxUI();
        ui->addValue(value);
	}
}

PUBLIC void ListBox::clearSelection()
{
	delete mSelected;
	mSelected = NULL;
    ListBoxUI* ui = getListBoxUI();
    ui->setSelectedIndex(-1);
}

PUBLIC void ListBox::setSelectedIndex(int i)
{
	if (i >= -1) {
		if (mSelected == NULL)
		  mSelected = new List();

		if (!mSelected->contains((void*)i))
		  mSelected->add((void*)i);

        ListBoxUI* ui = getListBoxUI();
        ui->setSelectedIndex(i);
	}
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 */
PUBLIC int ListBox::getSelectedIndex()
{
    int index = -1;

    ListBoxUI* ui = getListBoxUI();
    if (ui->isOpen())
      index = ui->getSelectedIndex();
    else if (mSelected != NULL && mSelected->size() > 0)
      index = (int)mSelected->get(0);

    return index;
}

PUBLIC List* ListBox::getInitialSelected()
{
	return mSelected;
}

/*
PUBLIC int ListBox::getPrivateSelectedIndex()
{
	int index = -1;

    if (mSelected != NULL && mSelected->size() > 0)
	  index = (int)(mSelected->get(0));

	return index;
}
*/

PUBLIC void ListBox::setSelectedValue(const char *s)
{
	if (mValues != NULL) {
		int index = mValues->indexOf((void*)s);
		if (index >= 0)
		  setSelectedIndex(mValues->indexOf((void*)s));
	}
}

PUBLIC void ListBox::setSelectedValues(StringList* list)
{
	clearSelection();
	if (list != NULL && mValues != NULL) {
		for (int i = 0; i < list->size() ; i++) {
			int index = mValues->indexOf(list->get(i));
			if (index >= 0)
			  setSelectedIndex(index);
		}
	}
}

PUBLIC const char* ListBox::getSelectedValue()
{
	const char *value = NULL;
	if (mValues != NULL) {
		int index = getSelectedIndex();
		if (index >= 0)
		  value = (const char *)mValues->get(index);
	}
	return value;
}

/**
 * Return a List of all selected values.  
 * The list is owned by the caller and must be deleted.
 */
PUBLIC StringList* ListBox::getSelectedValues()
{
	StringList* values = NULL;
	if (mValues != NULL) {
        ListBoxUI* ui = getListBoxUI();
		for (int i = 0 ; i < mValues->size() ; i++) {
		  if (ui->isSelected(i)) {
			  if (values == NULL)
				values = new StringList();
			  values->add((const char*)mValues->get(i));
		  }
		}
	}
	return values;
}

/**
 * Return a list of all selected indexes.
 */
PUBLIC List* ListBox::getSelectedIndexes()
{
	List* indexes = NULL;
	if (mValues != NULL) {
        ListBoxUI* ui = getListBoxUI();
		for (int i = 0 ; i < mValues->size() ; i++) {
		  if (ui->isSelected(i)) {
			  if (indexes == NULL)
				indexes = new StringList();
			  indexes->add((void*)i);
		  }
		}
	}
	return indexes;
}

/**
 * Return the selected values as a CSV or NULL if there are no selections.
 * The string is owned by the caller.
 */
PUBLIC char* ListBox::getSelectedCsv()
{
	char* csv = NULL;
	StringList* names = getSelectedValues();
	if (names != NULL)
	  csv = names->toCsv();
	return csv;
}

/**
 * Delete a value.  Not in Swing but convenient.
 */
PUBLIC void ListBox::deleteValue(int index)
{
	if (mValues != NULL && index >= 0 && index < mValues->size()) {
		mValues->remove(index);
		if (mAnnotations != NULL)
		  mAnnotations->remove(index);
		rebuild();
	}
}

/**
 * Move a value up.  Not in Swing but convenient.
 */
PUBLIC void ListBox::moveUp(int index)
{
	if (mValues != NULL && index > 0 && index < mValues->size()) {
		const char* value = mValues->getString(index);
		if (value != NULL) {
			mValues->add(index - 1, value);
			mValues->remove(index + 1);
		}
		rebuild();
		setSelectedIndex(index - 1);
	}
}

/**
 * Move a value down.  Not in Swing but convenient.
 */
PUBLIC void ListBox::moveDown(int index)
{
	if (mValues != NULL && index >= 0 && index < mValues->size() - 1) {
		const char* value = mValues->getString(index);
		if (value != NULL) {
			mValues->add(index + 2, value);
			mValues->remove(index);
		}
		rebuild();
		setSelectedIndex(index + 1);
	}
}

PUBLIC Dimension* ListBox::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void ListBox::dumpLocal(int indent)
{
    dumpType(indent, "ListBox");
}

PUBLIC void ListBox::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

/**
 * Only necessary for "owner draw" list boxes, do we actually need these?
 */
PUBLIC void ListBox::paint(Graphics* g)
{
    ComponentUI* ui = getUI();
    ui->paint(g);
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  LISTBOX                                 *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsListBox::WindowsListBox(ListBox* lb) 
{
	mListBox = lb;
}

PUBLIC WindowsListBox::~WindowsListBox() 
{
}

/**
 * The control also supports LB_INSERTSTRING and LB_DELETESTRING
 * which can insert and remove elements at specific indexes.
 */
PUBLIC void WindowsListBox::setValues(StringList* values)
{
	if (mHandle != NULL) {
		SendMessage(mHandle, WM_SETREDRAW, FALSE, 0);
		SendMessage(mHandle, LB_RESETCONTENT, 0, 0);
        StringList* values = mListBox->getValues();
		if (values != NULL) {
			for (int i = 0 ; i < values->size() ; i++) {
				const char* value = (const char*)values->get(i);
				SendMessage(mHandle, LB_ADDSTRING, 0, (LPARAM)value);
			}
		}
		SendMessage(mHandle, WM_SETREDRAW, TRUE, 0);

		// ListBox would formerly invalidate() but not it assumes
		// UI will do so if necessary

	}
}

PUBLIC void WindowsListBox::setAnnotations(StringList* values)
{
	if (mHandle != NULL) {
		SendMessage(mHandle, WM_SETREDRAW, FALSE, 0);
        StringList* annotations = mListBox->getAnnotations();
		if (annotations != NULL) {
			for (int i = 0 ; i < annotations->size() ; i++) {
                const char* an = annotations->getString(i);
                if (an != NULL)
                  SendMessage(mHandle, LB_SETITEMDATA, i, (LONG)an);
				else
                  SendMessage(mHandle, LB_SETITEMDATA, i, (LONG)NULL);
			}
		}
		SendMessage(mHandle, WM_SETREDRAW, TRUE, 0);

		// ListBox would formerly invalidate() but not it assumes
		// UI will do so if necessary
	}
}

PUBLIC void WindowsListBox::addValue(const char *value)
{
    if (mHandle != NULL)
      SendMessage(mHandle, LB_ADDSTRING, 0, (LPARAM)value);
}

PUBLIC void WindowsListBox::setSelectedIndex(int i)
{
    if (mHandle != NULL) {
		if (!mListBox->isMultiSelect())
		  SendMessage(mHandle, LB_SETCURSEL, i, 0);
		else {
			// !! what if we're trying to clear out a multiselect, 
			// have to iterate over each of the current selections and disable them?
			if (i >= 0)
			  SendMessage(mHandle, LB_SETSEL, 1, i);
		}

        // Kludge: the control is supposed to auto scroll
        // if the selected item is not visible.  When selecting
        // the first item, I always see it scrolled off the top,
        // the second item is the one at the top of the control.
        // Not sure what causes this, workaround by sending
        // scroll bar commands.  This assumes that the scroll bar
        // units are the same as the number of items, is that
        // automatic or do we have to call SetScrollRange?
		// !! would LB_SETTOPINDEX work here?
		if (i >= 0)
		  SetScrollPos(mHandle, SB_VERT, i, TRUE);
    }
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 */
PUBLIC int WindowsListBox::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
		if (mListBox->isMultiSelect()) {
			// Petzold implies that GETCURSEL won't work here, should try
            StringList* values = mListBox->getValues();
			if (values != NULL) {
				for (int i = 0 ; i < values->size() ; i++)
				  if (SendMessage(mHandle, LB_GETSEL, i, 0)) {
					  selected = i;
					  break;
				  }
			}
		}
		else {
			selected = SendMessage(mHandle, LB_GETCURSEL, 0, 0);
			// not sure if this is -1, make sure
			if (selected == LB_ERR)
			  selected = -1;
		}
	}
	return selected;
}

/**
 * Return true if a given item is selected.
 */
PUBLIC bool WindowsListBox::isSelected(int i)
{
    bool selected = false;
    if (mHandle != NULL) {
        if (SendMessage(mHandle, LB_GETSEL, i, 0))
          selected = true;
    }
	return selected;
}

PUBLIC void WindowsListBox::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
		if (parent != NULL) {
            // LBS_NOTIFY necessary to get WM_COMMAND messages
            // LBS_SORT causes the values to be sorted
            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP | 
                WS_VSCROLL | WS_BORDER | LBS_NOTIFY;
		
            if (mListBox->isMultiSelect())
              style |= LBS_MULTIPLESEL;

            // this will cause it to send WM_MEASUREITEM to the parent window,
            // assume we can ingore??
            // LBS_OWNERDRAWVARIABLE means items don't have the same height
            if (mListBox->getAnnotations() != NULL)
              style |= (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);

            Bounds* b = mListBox->getBounds();
            Point p;
            mListBox->getNativeLocation(&p);

            mHandle = CreateWindow("listbox",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create ListBox control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mListBox->initVisibility();

                // capture state set during construction
                setValues(mListBox->getValues());
                setAnnotations(mListBox->getAnnotations());

                List* selected = mListBox->getInitialSelected();
                if (selected != NULL) {
                    for (int i = 0 ; i < selected->size() ; i++) {
                        setSelectedIndex((int)(selected->get(i)));
                    }
                }
            }
        }
    }
}

/**
 * Assume these are presized, but should whip through
 * the values and determine the max text size!!
 * Must be wide enough for the longest string plus
 * the width of the scroll bar (SM_CSVSCROLL).
 */
PUBLIC void WindowsListBox::getPreferredSize(Window* w, Dimension* d)
{
	TextMetrics* tm = w->getTextMetrics();

	// TODO: calculate optimal text size by looking at all the values
    int cols = mListBox->getColumns();
    if (cols == 0) cols = 20;
	d->width = cols * tm->getMaxWidth();
	d->width += UIManager::getVertScrollBarWidth();
		
	// 1 1/2 times char height if using border
	int fontHeight= tm->getHeight() + tm->getExternalLeading();
	int rows = mListBox->getRows();
    if (rows <= 0) rows = 1;
	d->height = rows * fontHeight;
	d->height += fontHeight / 2;
}

PUBLIC void WindowsListBox::command(int code) 
{
	if (code == LBN_SELCHANGE)
	  mListBox->fireActionPerformed();
}

/**
 * Called for OWNERDRAW list boxes.
 */
PUBLIC void WindowsListBox::paint(Graphics* g)
{

    WindowsGraphics* wg = (WindowsGraphics*)g;
    LPDRAWITEMSTRUCT di = wg->getDrawItem();

    if (di != NULL && di->itemID >= 0) {

        StringList* values = mListBox->getValues();
        bool selected = (di->itemState & ODS_SELECTED);
        const char* item = values->getString(di->itemID);
        const char* annotation = (const char*)di->itemData;

        // action will be ODA_SELECT, ODA_DRAWENTIRE, or ODA_FOCUS
        // ignor ODA_FOCUS
        if (di->itemAction != ODA_FOCUS) {

            // example has this, should be necessary?
            const char* a2 = (const char*)
                SendMessage(di->hwndItem, LB_GETITEMDATA, di->itemID, (LPARAM)0);
            if (a2 != annotation)
              printf("ListBox::paint unexpected annotation\n");
            
            if (di->itemState & ODS_SELECTED)
              g->setColor(Color::Red);
            else
              g->setColor(Color::Black);

            // todo: font

            int left = di->rcItem.left + 8;
            int top = di->rcItem.top;
            int height = di->rcItem.bottom - di->rcItem.top + 1;

            TextMetrics* tm = g->getTextMetrics();
            top += (height / 2) + (tm->getAscent() / 2);
            // this is just a little too low, same thing happened
            // with knob, need to resolve this!!
            top -= 2;

            g->drawString(item, left, top);

			if (annotation != NULL && strlen(annotation) > 0) {
				Dimension d;
				g->getTextSize(annotation, NULL, &d);
				left = di->rcItem.right - d.width - 8;
				g->drawString(annotation, left, top);
			}

        }
        
    }

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
 * We need two unique identifiers for columns, 0-1023 are reserved.
 * Usually these are four character names.
 */
enum {
	kMainColumn = 'main',
	kAnnotationColumn = 'anno'
};

PUBLIC MacListBox::MacListBox(ListBox* lb) 
{
	mListBox = lb;
	mMainWidth = 0;
	mAnnotationWidth = 0;
}

PUBLIC MacListBox::~MacListBox() 
{
}

/**
 * We don't actually pass the data here, just the number of rows.
 * Let the control auto-number the rows from 1
 */
PUBLIC void MacListBox::setValues(StringList* values)
{
	OSStatus status;

	if (mHandle != NULL) {
		ControlRef control = (ControlRef)mHandle;

		int rows = (values != NULL) ? values->size() : 0;

		// first clear the browser
		status = RemoveDataBrowserItems(control, 
										kDataBrowserNoItem, // container
										0, // item count, zero to remove all
										NULL, // item ids, NULL to remove all
										kDataBrowserItemNoProperty); 
		CheckStatus(status, "RemoveDataBrowserItems");

		status = AddDataBrowserItems(control, 
									 kDataBrowserNoItem, // container
									 rows, // numItems
									 NULL, // item ids (auto number from 1)
									 kDataBrowserItemNoProperty); // preSortProperty

		CheckStatus(status, "AddDataBrowserItems");

		mListBox->invalidate();
	}
}

/**
 * We assume that the number of values and annotations is the same
 * so we don't have to rebuild the browser item list.
 * invalidate() doesn't seem to work on DataBrowser.
 *
 * NB: This is used int the Mobius MIDI Control window which can
 * change the annotation list in response to MIDI events on the
 * MIDI handler thread.  It probably needs to be using custom
 * change messages like we do for Text and ComboBox for for whatever
 * reason it seems to be stable.  Maybe because we're not forcing
 * an invalidate() message after changing the columns so there is less
 * chance of thread collisions?
 */
PUBLIC void MacListBox::setAnnotations(StringList* values)
{
	OSStatus status;

	if (mHandle != NULL) {

		int rows = (values != NULL) ? values->size() : 0;

		// invalidate doesn't seem to work on these, you have to call this
		// !! Assuming we're in the UI thread 
		status = UpdateDataBrowserItems((ControlRef)mHandle,
										kDataBrowserNoItem, 
										rows,
										NULL, // items, NULL for all
										kDataBrowserItemNoProperty, // preSortProperty
										kAnnotationColumn);

		CheckStatus(status, "AddDataBrowserItems");
	}
}

/**
 * There are probably ways to do this incrementally but it's easier
 * just to rebuild the whole thing.
 */
PUBLIC void MacListBox::addValue(const char *value)
{
    if (mHandle != NULL) {
		setValues(mListBox->getValues());
	}
}

/**
 * ListBox item indexes start frm 0.
 * DataBrowserItemID starts from 1.
 */
PUBLIC void MacListBox::setSelectedIndex(int i)
{
	OSStatus status;

    if (mHandle != NULL) {
		DataBrowserItemID items[1];
		
		// have to adjust the offset
		int itemId = i + 1;

		if (!mListBox->isMultiSelect()) {
			if (i >= 0) {
				items[0] = itemId;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 1, items, 
													 kDataBrowserItemsAssign);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
			else {
				// clear selection by assigning an empty set
				items[0] = 0;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 0, items, 
													 kDataBrowserItemsAssign);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
		}
		else {
			// !! what if we're trying to clear out a multiselect, 
			// have to iterate over each of the current selections and disable them?
			if (i >= 0) {
				items[0] = itemId;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 1, items, 
													 kDataBrowserItemsAdd);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
		}

		// TODO: Do we need to scroll to the selection?

	}
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 * 
 * The GetDataBrowserItems function can return the ids of all items that
 * have a certaion state (like selected) but it returns a confusing
 * array in a "Handle" and I'm not sure how to deal with those.  Just iterate.
 */
PUBLIC int MacListBox::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
		StringList* values = mListBox->getValues();
		if (values != NULL) {
			for (int i = 0 ; i < values->size() ; i++) {
				if (isSelected(i)) {
					selected = i;
					break;
				}
			}
		}
	}
	return selected;
}

/**
 * Return true if a given item is selected.
 */
PUBLIC bool MacListBox::isSelected(int i)
{
    bool selected = false;
    if (mHandle != NULL) {
		// item ids are 1 based
		int itemID = i + 1;
		selected = (bool)IsDataBrowserItemSelected((ControlRef)mHandle, itemID);
    }
	return selected;
}

OSStatus ListBoxItemDataCallback(ControlRef browser,
								 DataBrowserItemID itemID,
								 DataBrowserPropertyID property,
								 DataBrowserItemDataRef itemData,
								 Boolean changeValue)
{
    OSStatus status = noErr;
	MacListBox* mlb = (MacListBox*)GetControlReference(browser);
	ListBox* lb = (ListBox*)mlb->getComponent();
 
	if (lb == NULL) {
		Trace(1, "ListBoxItemdtaCallback: no link back to ListBox\n");
		status = errDataBrowserPropertyNotSupported;
	}
	else if (changeValue) {
		// this is not a "set request" ignore
		status = errDataBrowserPropertyNotSupported;
	}
	else {
		switch (property) {
			case kMainColumn: {
				CFStringRef cfstring = NULL;
				// itemId is a 1 based list index
				int index = itemID - 1;
				if (index >= 0) {
					StringList* values = lb->getValues();
					if (values != NULL && index < values->size()) {
						const char* str = values->getString(index);
						cfstring = MakeCFStringRef(str);
					}
				}

				status = SetDataBrowserItemDataText (itemData, cfstring);
			}
			break;

			case kAnnotationColumn: {
				CFStringRef cfstring = NULL;
				int index = itemID - 1;
				if (index >= 0) {
					StringList* values = lb->getAnnotations();
					if (values != NULL && index < values->size()) {
						const char* str = values->getString(index);
						cfstring = MakeCFStringRef(str);
					}
				}

				status = SetDataBrowserItemDataText (itemData, cfstring);
			}
            break;

			case kDataBrowserContainerIsSortableProperty:
				status = SetDataBrowserItemDataBooleanValue(itemData, false);
				break;

			default: {
				status = errDataBrowserPropertyNotSupported;
			}
			break;
		}
	}

	return status;
}

/**
 * Called as things change in the data browser.
 * We get ItemSelected as soon as a selection happens, but before
 * ItemDeselected so if we fire the action handler now it will see both
 * the new and the previous selection, need to wait for SelectionSetChanged.
 */
void ListBoxItemNotificationCallback(ControlRef browser,
									 DataBrowserItemID item,
									 DataBrowserItemNotification message) 
{
	MacListBox* mlb = (MacListBox*)GetControlReference(browser);

	// kDataBrowserItemDoubleClicked
	// and many more dealing with containers

	switch (message) {
		case kDataBrowserItemSelected: {
			// happens immediately, before ItemDeselected
		}
		break;
		case kDataBrowserItemDeselected: {
			// happens after ItemSelected but may happen multiple times
		}
		break;
		case kDataBrowserSelectionSetChanged: {
			// we're finished updating the selection set
			// item is meaningless?
			ListBox* lb = (ListBox*)mlb->getComponent();
			lb->fireActionPerformed();
		}
		break;
		default: {
			// getting a lot of kDataBrowserItemRemoved (2) messages 
			// in bulk, not sure why
			//printf("ListBoxItemNotification %d item %d\n", message, item);
			//fflush(stdout);
		}
		break;
	}
}

/**
 * Kludge fixed character width for calculating sizes, 
 * need to find something more accurate.
 * Lower case characters were measured around 8 points on average.averate 
 */
#define LIST_BOX_CHAR_WIDTH 10

/**
 * Calculate the required widths of the columns.
 * This requires that the values and annotations be set BEFORE opening,
 * we do not support changing column widths after the fact.  This
 * may require more parameters to set minimum widths if a browser
 * needs to start out empty. !!
 *
 * Should support something like Table where we can base it on the 
 * initial text or a fixed number of chars.
 *
 * Like Table and TabbedPane we simulate real system font metrics with
 * a Font that looks close enough.  Need to find out where to get
 * more accurate metrics, but this is close enough.
 */
PRIVATE void MacListBox::calcColumnWidths(Window* w)
{
	// a 14 point font looks closes in qwintest but a little narrow
	Graphics* g = w->getGraphics();
	g->setFont(Font::getFont("Helvetica", 0, 16));
	Dimension md;
	g->getTextSize("M", &md);
	int charWidth = md.width;
	// M width for a 16 point font seems to come back 16, 	
	// which is WAY too big, something might be screwed
	// up in text metric, just halve it for now
	charWidth /= 2;

	if (mMainWidth == 0) {
		int width = 0;

		int chars = mListBox->getColumns();
		if (chars > 0)
		  width = chars * charWidth;

		int vwidth = getMaxWidth(g, mListBox->getValues());
		if (vwidth > width)
		  width = vwidth;

		// two chars of padding on either side
		width += (charWidth * 2);

		mMainWidth = width;
	}

	if (mAnnotationWidth == 0) {
		int width = 0;
		int chars = mListBox->getAnnotationColumns();
		if (chars > 0)
		  width = chars * charWidth;

		int vwidth = getMaxWidth(g, mListBox->getAnnotations());
		if (vwidth > width)
		  width = vwidth;

		// two chars of padding on either side
		width += (charWidth * 2);

		mAnnotationWidth = chars * charWidth;
	}
}

/**
 * Calculate the maximum width of a list of strings.
 */
PRIVATE int MacListBox::getMaxWidth(Graphics* g, StringList* list)
{
	int max = 0;
	if (list != NULL) {
		for (int i = 0 ; i < list->size() ; i++) {
			const char* str = list->getString(i);
			if (str != NULL) {
				Dimension d;
				g->getTextSize(str, &d);
				if (d.width > max)
				  max = d.width;
			}
		}
	}
	return max;
}

PUBLIC void MacListBox::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };

		// sigh, give this some girth until we can figure out how to size these
		bounds.bottom = 100;
		bounds.right = 800;

		status = CreateDataBrowserControl(window, &bounds, 
										  kDataBrowserListView,
										  &control);

		if (CheckStatus(status, "MacListBox::open")) {
			mHandle = control;

			SetControlReference(control, (SInt32)this);

			/*
			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(ListBoxEventHandler),
												GetEventTypeCount(ListBoxEventsOfInterest),
												ListBoxEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacListBox::InstallEventHandler");
			*/

			calcColumnWidths(mListBox->getWindow());
			addColumn(control, kMainColumn, mMainWidth);
			addColumn(control, kAnnotationColumn, mAnnotationWidth);

			// horiz, vert
			SetDataBrowserHasScrollBars(control, false, true);

			// turns off the header
			SetDataBrowserListViewHeaderBtnHeight(control, 0);

			// kDataBrowserSelectOnlyOne - makes it like a radio
			// kDataBrowserResetSelection - clears selected items before processing next
			// kDataBrowserNoDisjointSelection - prevent discontinuous selection
			// kDataBrowserAlwaysExtendSelection - select multiple without holding down modifier key
			// kDataBrowserNeverEmptySelectionSet - user can never deselect last item
			int flags = kDataBrowserCmdTogglesSelection;
			if (!mListBox->isMultiSelect())
			  flags |= kDataBrowserSelectOnlyOne;

			SetDataBrowserSelectionFlags(control, flags);
										  
			// set callbacks
			DataBrowserCallbacks callbacks;
			callbacks.version = kDataBrowserLatestCallbacks;
			InitDataBrowserCallbacks(&callbacks);

			// supposedly we're supposed to free the UPPs later
			// with DisposeDataBrowserItemNotificationUPP and the like

			callbacks.u.v1.itemDataCallback = 
				NewDataBrowserItemDataUPP((DataBrowserItemDataProcPtr)
										  ListBoxItemDataCallback);
			callbacks.u.v1.itemNotificationCallback = 
				NewDataBrowserItemNotificationUPP(ListBoxItemNotificationCallback);

			status = SetDataBrowserCallbacks(control, &callbacks);
			CheckStatus(status, "SetDataBrowserCallbacks");

			// allows column dragging
			//SetAutomaticControlDragTrackingEnabledForWindow(window, true);
			
			// capture state set during construction
			//mListBox->rebuild();
			setValues(mListBox->getValues());
			setAnnotations(mListBox->getAnnotations());

			List* selected = mListBox->getInitialSelected();
			if (selected != NULL) {
				for (int i = 0 ; i < selected->size() ; i++) {
					setSelectedIndex((int)(selected->get(i)));
				}
			}
	
			SetControlVisibility(control, true, true);
		}
    }
}

/**
 * Helper to add one column to the list view.
 */
PRIVATE void MacListBox::addColumn(ControlRef control, int id, int width)
{
	
	DataBrowserListViewColumnDesc col;

	col.propertyDesc.propertyID = id;
	col.propertyDesc.propertyType = kDataBrowserTextType;

	// kDataBrowserPropertIsMutable, kDataBrowserDefaultPropertyIsEditable,
	// kDataBrowserDoNotTruncateText, kDataBrowserListViewMovableColumn,
	// kDataBrowserListViewSortableColumn
	col.propertyDesc.propertyFlags = kDataBrowserListViewSelectionColumn;
			
	col.headerBtnDesc.version = kDataBrowserListViewLatestHeaderDesc;
	// set these different if you want the columns resizeable
	col.headerBtnDesc.minimumWidth = width;
	col.headerBtnDesc.maximumWidth = width;
	col.headerBtnDesc.titleOffset = 0;
	col.headerBtnDesc.titleString = NULL;
	col.headerBtnDesc.initialOrder = kDataBrowserOrderUndefined;
	// use all defaults for the title (which we don't have anyway)
	col.headerBtnDesc.btnFontStyle.flags = 0;
	// allows icons and other things for buttons
	col.headerBtnDesc.btnContentInfo.contentType = kControlNoContent;

	OSStatus status = AddDataBrowserListViewColumn(control, &col, 
												   kDataBrowserListViewAppendColumn);
	CheckStatus(status, "AddDataBrowserListViewColumn");
}

/**
 * On Windows we get the system TextMetrics which defines the sizes
 * of characters used in the standard components.  I don't know if such
 * a thing exists on Mac, so we're going to fake it for now and measure
 * text using a font of about the same size, like we do in TabbedPane
 * and Table.
 */
PUBLIC void MacListBox::getPreferredSize(Window* w, Dimension* d)
{
	// the default is as high as necessary to display all items
	// we could at least use this to calculate the line height!!
	//MacComponent::getPreferredSize(w, d);
	//printf("*** ListBox default height %d\n", d->height);

	// measured distance between baselines of the digit 1 was 19 pixels
	int fontHeight = 19;
	int rows = mListBox->getRows();
    if (rows <= 0) rows = 5;
	d->height = rows * fontHeight;

	// need a little extra padding at the bottom
	d->height += 4;

	// system font looks close to 14, but that's a little too narrow so
	// use 16
	Graphics* g = w->getGraphics();
	g->setFont(Font::getFont("Helvetica", 0, 16));

	// !! TextMetrics aren't working on Mac, find out
	// what the deal is...
	//TextMetrics* tm = g->getTextMetrics();
	//int charWidth = tm->getMaxWidth();
	Dimension md;
	g->getTextSize("M", &md);
	int charWidth = md.width;
	// M width for a 16 point font seems to come back 16, 	
	// which is WAY too big, something might be screwed
	// up in text metric, just halve it for now
	charWidth /= 2;

	// always use the measurements calculated during open
	int width = mMainWidth + mAnnotationWidth;

	d->width = width + UIManager::getVertScrollBarWidth();
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
