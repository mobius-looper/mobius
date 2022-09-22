/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac implementation of the Menu interface.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"
#include "MacUtil.h"

#include "Qwin.h"
#include "UIMac.h"

QWIN_BEGIN_NAMESPACE

#define MAX_ITEM_LABEL 1024

// this disappeared in xcode 5, found this in an ancient document
// commandMark=17, diamondMark=19,appleMark=20
#define checkMark 18



/**
 * Static initializer for the id generator.
 */
int MacMenuItem::MenuIdFactory = 1;

PUBLIC int MacMenuItem::GenMenuId()
{
	return MenuIdFactory++;
}

MacMenuItem::MacMenuItem(MenuItem* item)

{
    mItem = item;
    mOpen = false;
	mItemsInserted = 0;
}

MacMenuItem::~MacMenuItem()
{
}
    
PRIVATE void MacMenuItem::incItemsInserted()
{
	mItemsInserted++;
}

PRIVATE int MacMenuItem::getItemsInserted()
{
	return mItemsInserted;
}

/**
 * Utility to take the label text from a MenuItem and
 * strip off the Windows-specific hotkey annotations.
 */
PRIVATE void MacMenuItem::getItemLabel(const char* text, char* dest)
{
	int max = MAX_ITEM_LABEL - 2;

	if (text == NULL)
	  strcpy(dest, "");
	else {
		int srclen = strlen(text);
		int psn = 0;
		for (int i = 0 ; i < srclen ; i++) {
			char ch = text[i];
			if (ch != '&') {
				if (psn < max) 
				  dest[psn++] = ch;
				else
				  break;
			}
			dest[psn] = 0;
		}
	}
}

/**
 * Return true if this item is logically opened.
 * For menus we must have a native handle. 
 * For the root menu bar we seem to be implicitly opened.
 * For items open means we've added to the parent menu but we won't
 * have a native handle.
 */
PUBLIC bool MacMenuItem::isOpen()
{
	return (mHandle != NULL || mOpen);
}

/**
 * Open a popup menu at a given location.
 * x and y are window coordinates,  have to convert to "global coordinates".
 */
PUBLIC void MacMenuItem::openPopup(Window* window, int x, int y)
{
	OSStatus status;

	// make sure the native objects have been created
	// recursively create the native menu objects
    mItem->open();
	if (mHandle != NULL) {
		MenuRef menu = (MenuRef)mHandle;

		// This is what windows does, can we get to the Window from 
		// a Mac menu item?  Don't think so, so we pass it in.
        //HWND window = getWindowHandle(mItem);
		WindowRef winref = (WindowRef)window->getNativeHandle();

        // have to get the structure region, Window.mBounds has the content region
		Rect bounds;
		status = GetWindowBounds(winref, kWindowStructureRgn, &bounds);

		::Point location;
		location.h = x + bounds.left;
		location.v = y + bounds.top;

		// will be kCMNothingSelected, kCMMenuItemSelected, kCMShowHelpSelected
		UInt32 selectionType;
		MenuID selectionId;
		MenuItemIndex selectionIndex;

		// This will call the menu event handler which does the work 
		// like the menu bar menus.  Selecting the first item in a submenu
		// seems to generate two events, one for the submenu item and
		// another for the item in the parent menu.  Selecting any item
		// after the first doesn't do this.  Should be okay since menu
		// items with submenus have no interesting watchers.

		status = ContextualMenuSelect(menu, 
									  location,
									  false, // reserved
									  kCMHelpItemRemoveHelp,
									  NULL, // help string
									  NULL, // inSelection
									  &selectionType,
									  &selectionId,
									  &selectionIndex);

		if (status == -128) {
			// this appears to be common and means "no selection"
		}
		else 
		  CheckStatus(status, "MacMenuItem::ContextualMenuSelect");

		// We could also try to disable the usual event handler and
		// fire action events here.  selectionId seems to be the 
		// id of the menu in which the selection is made.  To get the
		// selected item id we'll have to lookup the item by selectionIndex.
		/*
		printf("Contextual menu type %d id %d index %d\n",
			   (int)selectionType, (int)selectionId, (int)selectionIndex);
		fflush(stdout);
		*/
	}
}

/**
 * Open a menu item.
 * Since we share the same ComponentUI class for all menu items,
 * have to use the class identification predicates to figure out
 * what to build.
 */
PUBLIC void MacMenuItem::open()
{
    if (!isOpen()) {

        if (mItem->isPopupMenu())
          openPopupMenu();
		
        else if (mItem->isMenuBar())
          openMenuBar();

        else if (mItem->isMenu())
          openMenu();

        else
          openItem();
    }
}

/**
 * Close a menu item.
 * Called as we destruct the Window object hierarchy.  May also be called
 * to make incremental modifications to menus, though this isn't used
 * much in Mobius.
 *
 * We're more complex than most MacComponents because we have to remove 
 * ourselves from the parent menu.
 * 
 * Note that we can't assume that all of our peers will still be open
 * which means that determining our ordinal position within the parent
 * menu isnt just a matter of counting Components, we only count the Components
 * that proceed us AND still have a native handle.  
 */
PUBLIC void MacMenuItem::close() 
{
	if (isOpen()) {
		MacMenuItem* parent = (MacMenuItem*)getParent();
		if (parent != NULL) {
			int index = parent->getItemIndex(this);
			if (index > 0) {
				// oddly enough this does not return anything
				DeleteMenuItem((MenuRef)parent->getHandle(), index);
			}
		}
		invalidateHandle();
	}
}

/**
 * Calculate the 1 based index of an item witin the parent.
 * Return 0 if the given item is not a child of this parent.
 * 
 * SUBTLETY: Only advance the count as we pass child items that have
 * a native handle (meaning they're still open).  This is necessary
 * because we typically close items before we remove them from the
 * Component list.  If you close more than one item before removal
 * the position of the item in the Component list will no longer match
 * the item index in the native menu.  We only increment the native index
 * for Components that preceed us AND have a native handle (meaning they're
 * still open).
 */
int MacMenuItem::getItemIndex(MacMenuItem* item)
{
	int index = 0;
	int counter = 1;

	for (Component* c = mItem->getComponents() ; c != NULL ; c = c->getNext()) {
		ComponentUI* ui = c->getUI();
		MacMenuItem* other = (MacMenuItem*)ui->getNative();
		if (item == other) {
			index = counter;
			break;
		}
		else if (other->isOpen()) {
			// still open
			counter++;
		}
	}

	return index;
}

PRIVATE void MacMenuItem::openMenuBar()
{
    MenuBar* mb = (MenuBar*)mItem;

	// the bar itself seems to be global the application,
	// we could get the handle to it but it doesn't seem
	// to be necessary to call other Menu Manager functions
	mOpen = true;
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * Don't seem to get any Command events though the window does.
 */
PRIVATE EventTypeSpec MenuEventsOfInterest[] = {
	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassMenu, kEventMenuOpening },
};

PRIVATE OSStatus
MenuEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	MacMenuItem* item = (MacMenuItem*)data;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Menu", cls, kind);

	if (cls == kEventClassCommand) {

		HICommandExtended cmd;
		OSErr err = GetEventParameter(event, kEventParamDirectObject, 
									  typeHICommand, NULL, sizeof(cmd), 
									  NULL, &cmd);
		CheckErr(err, "MenuEventHandler::GetEventParameter");

		if (kind == kEventCommandProcess) {
			MenuRef menu = cmd.source.menu.menuRef;
			int index = cmd.source.menu.menuItemIndex;
			if (item != NULL) {
				// convert from 1 to 0 based
				if (index > 0) index--;
				item->fireSelection(index);
				// Returning eventNotHandledErr will let this event
				// propagate to AppEventHandler (and maybe WindowEventHandler?)	
				// These don't do anything, so it seems better to let events
				// flow so the system defalt handlers can do their thing.
				//result = noErr;
			}
		}
	}
	else if (cls == kEventClassMenu) {
		if (kind == kEventMenuOpening)
		  item->opening();
	}

	return result;
}

/**
 * Fire the selection event on our peer component.
 * This is u sed with the menu event handler.
 */
PUBLIC void MacMenuItem::fireSelection(int index)
{
	// This is only called for items that represent menus
	if (mItem->isMenu()) {
		Menu* menu = (Menu*)mItem;
		MenuItem* item = menu->getItem(index);
		if (item != NULL) {
			// this will walk up until it finds an action listener
			item->fireSelection(item);
		}
	}
}

/**
 * Called when we receive an "opening" event.
 * Let the Menu know so it can call the MenuListener which
 * may make adjustments.
 */
PUBLIC void MacMenuItem::opening()
{
	// we expect to only receive this on menus, not individual items
	if (mItem->isMenu()) {
		((Menu*)mItem)->opening();
	}
	else {
		// interesting?
		printf("WARNING: MacMenuItem::opening on non-menu\n");
	}
}

/**
 * Fire the selection event on a MacMenuItem.
 * This is called when using a command handler on the window.
 * It turns out this can't be used because the index passed
 * to the window command handler handler is weird, we'll 
 * use fireSelectionById instead.
 * 
 */
/*
  PUBLIC void MacMenuItem::fireSelection()
  {
  // this will walk up till it finds a handler
  mItem->fireSelection(mItem);
  }
*/

/**
 * Fire the selection event on a MacMenuItem identified by id.
 * This is called when using a command handler on the window.
 * This object is an arbitrary peer to the item that caused the
 * event, typically it is the first item in the parent menu.
 */
PUBLIC void MacMenuItem::fireSelectionById(int id)
{
	if (mItem->isMenu()) {
		// we've reached the parent menu
		Menu* menu = (Menu*)mItem;
/*
  MenuItem* item = menu->getItemById(id);
  if (item != NULL) {
  // this will walk up until it finds an action listener
  item->fireSelection(item);
  }
*/
		menu->fireSelectionId(id);
	}
	else {
		// walk up and try again
		MacMenuItem* parent = (MacMenuItem*)getParent();
		if (parent != NULL)
		  parent->fireSelectionById(id);
	}
}

/**
 * Open a popup menu.  Same handlers as normal menus but 
 * we don't have a title and don't install ourselves on the application
 * menu bar.
 */
PUBLIC void MacMenuItem::openPopupMenu()
{
	OSStatus status;

	MenuRef menu = NULL;
	// MenuID, MenuAttributes, MenuRef*
	CreateNewMenu(GenMenuId(), 0, &menu);

	status = InstallMenuEventHandler(menu,
									 NewEventHandlerUPP(MenuEventHandler),
									 GetEventTypeCount(MenuEventsOfInterest),
									 MenuEventsOfInterest,
									 this, NULL);
	CheckStatus(status, "MacMenuItem::InstallEventHandler - popup");

	// index zero supposedly references the menu itself
	SetMenuItemRefCon(menu, 0, (UInt32)this);

	mHandle = menu;
}

/**
 * Open a menu.
 */
PRIVATE void MacMenuItem::openMenu()
{
	OSStatus status;
	MenuRef parentHandle = NULL;
	MacMenuItem* parent = (MacMenuItem*)getParent();

	// formerly returned null if the parent was the MenuBar, but
	// now it comes in and we have to check the MenuRef
	if (parent != NULL)
	  parentHandle = (MenuRef)parent->getHandle();

	if (parentHandle == NULL) {
		// a top level menubar menu

		MenuRef menu = NULL;
		// MenuID, MenuAttributes, MenuRef*
		CreateNewMenu(GenMenuId(), 0, &menu);

		status = InstallMenuEventHandler(menu,
										 NewEventHandlerUPP(MenuEventHandler),
										 GetEventTypeCount(MenuEventsOfInterest),
										 MenuEventsOfInterest,
										 this, NULL);
		CheckStatus(status, "MacMenuItem::InstallEventHandler - menubar");

		// index zero supposedly references the menu itself
		SetMenuItemRefCon(menu, 0, (UInt32)this);

		char label[MAX_ITEM_LABEL];
		getItemLabel(mItem->getText(), label);

		CFStringRef cfstr = MakeCFStringRef(label);
		SetMenuTitleWithCFString(menu, cfstr);
		// can we release it now?
		//CFRelease(cfstr);

		// MenuRef, MenuId (beforeID), zero appends 
		InsertMenu(menu, 0);
		
		mHandle = menu;
	}
	else {
		// a child menu
		MenuRef menu = NULL;
		// MenuID, MenuAttributes, MenuRef*
		CreateNewMenu(GenMenuId(), 0, &menu);

		status = InstallMenuEventHandler(menu,
										 NewEventHandlerUPP(MenuEventHandler),
										 GetEventTypeCount(MenuEventsOfInterest),
										 MenuEventsOfInterest,
										 this, NULL);


		// !! It may be better to let this post commands then have the
		// command events forward to the window in the App event handler.
		// Having trouble with modal dialogs run from menu event handlers
		// not letting the menu selection clear.
		CheckStatus(status, "MacMenuItem::InstallEventHandler - child");

		// sigh, menus aren't controls
		//SetControlReference(menu, (SInt32)this);

		//CFStringRef cfstr = MakeCFStringRef(mItem->getText());
		//SetMenuTitleWithCFString(menu, cfstr);
		//// can we release it now?
		////CFRelease(cfstr);

		int index = parent->getItemsInserted();

		// note that indexes are 1 based
		// InsertMenuItemText... takes an "after index", it inserts
		// the item after the one specified by the index, pass zero to insert at
		// the beginning.  So ItemsInserted being zero based is okay here.

		char label[MAX_ITEM_LABEL];
		getItemLabel(mItem->getText(), label);

		CFStringRef cfstr = MakeCFStringRef(label);
		InsertMenuItemTextWithCFString(parentHandle, cfstr, index, 0, mItem->getId());
		// can we release it now?
		//CFRelease(cfstr);

		// here we need the one based index
		SetMenuItemHierarchicalMenu(parentHandle, index + 1, menu);

		mHandle = menu;
		parent->incItemsInserted();
	}
}

/**
 * Open a basic item or seperator.
 */
PRIVATE void MacMenuItem::openItem()
{
	MacMenuItem* parent = (MacMenuItem*)getParent();

    if (parent != NULL)  {
		MenuRef parentHandle = (MenuRef)(parent->getHandle());
		if (parentHandle == NULL) {
			Trace(1, "Unable to locate parent handle!!\n");
		}
		else {
			const char* text = mItem->getText();

			// note that is this is a zero based index but menus have 1 
			// based indexes, but for InsertMenuItemText it is ok because	
			// it takes an "after index"
			int index = parent->getItemsInserted();
			if (text != NULL) {
				
				char label[MAX_ITEM_LABEL];
				getItemLabel(text, label);

				CFStringRef cfstr = MakeCFStringRef(label);

				// !! command ids are supposed to be 4 characters with at least
				// one upper case (all lowercase is reserved by Apple)
				// Need to generate them and figure out how to map

				InsertMenuItemTextWithCFString(parentHandle, cfstr, index, 0, mItem->getId());
				// can we release it now?
				//CFRelease(cfstr);

				if (mItem->isChecked())
				  SetItemMark(parentHandle, index + 1, checkMark);

				// Formerly did this but the event index was unreliable if there
				// were any submenus so now we just put one on the menu itself
				// and get it with item index zero
				//SetMenuItemRefCon(parentHandle, index + 1, (uint32)this);
			}
			else if (mItem->isSeparator()) {
				InsertMenuItemTextWithCFString(parentHandle, CFSTR(""), index,
											   kMenuItemAttrSeparator, 0);
			}
			parent->incItemsInserted();
			// these don't have handles so use a flag
			mOpen = true;
		}
	}
}

/**
 * Only supposed to be called for items so we won't have mHandle but
 * we will have mOpen.
 */
void MacMenuItem::setChecked(bool checked)
{
	if (isOpen()) {
		MacMenuItem* parent = (MacMenuItem*)getParent();
		if (parent != NULL) {
			int index = parent->getItemIndex(this);
			if (index > 0) {
				CharParameter mark = noMark;
				if (checked)
				  mark = checkMark;
				SetItemMark((MenuRef)parent->getHandle(), index, mark);
			}
		}
	}
}

void MacMenuItem::setEnabled(bool enabled)
{
	printf("MacMenuItem::setEnabled not implemented!\n");
}

/**
 * Remove handles in child items after the parent is closed.
 */
PUBLIC void MacMenuItem::invalidateHandle()
{
    mHandle = NULL;
    mOpen = false;
}

/**
 * Container overload used when deleting all the menu items in bulk.
 * We don't really need this since close() and getItemIndex() is smart
 * enough to deal with the disconnect between the Component list and
 * the native item indexes.
 */
PUBLIC void MacMenuItem::removeAll()
{
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
