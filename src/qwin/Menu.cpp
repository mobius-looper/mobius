/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This file contains both the MenuItem abstraction and the Windows 
 * implementation.  The Mac implementation is in MacMenu.cpp.
 *
 * Windows menu items are a little odd in that we don't have
 * a native handle, items are manipulated using the parent menu
 * handle and an index.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 MENU ITEM                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Swing JMenuItems are also AbstractButtons.
 * They can have icons as well as text.
 */

/**
 * Kludge to auto-number menu items.
 */
static int ItemNumbers = 10000;

PUBLIC MenuItem::MenuItem()
{
    initMenuItem();
}

PUBLIC MenuItem::MenuItem(const char* text)
{
    initMenuItem();
    setText(text);
}

PUBLIC MenuItem::MenuItem(const char* text, int id)
{
    initMenuItem();
    setText(text);
    setId(id);
}

PUBLIC MenuItem::~MenuItem()
{
    delete mText;
}

PUBLIC void MenuItem::initMenuItem()
{
	mClassName = "MenuItem";
    mText = NULL;
    mId = 0;
    mChecked = false;
    mRadio = false;
}

PUBLIC const char* MenuItem::getTraceName()
{
	return mText;
}

PUBLIC ComponentUI* MenuItem::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getMenuUI(this);
    return mUI;
}

PUBLIC MenuUI* MenuItem::getMenuUI()
{
    return (MenuUI*)getUI();
}

PUBLIC void MenuItem::setText(const char* text)
{
    delete mText;
    mText = CopyString(text);
}

PUBLIC const char* MenuItem::getText()
{
    return mText;
}

PUBLIC void MenuItem::setId(int i)
{
    mId = i;
}

PUBLIC int MenuItem::getId()
{
    return mId;
}

PUBLIC bool MenuItem::isSeparator()
{
    return false;
}

PUBLIC bool MenuItem::isMenu()
{
    return false;
}

PUBLIC bool MenuItem::isMenuBar()
{
    return false;
}

PUBLIC bool MenuItem::isPopupMenu()
{
    return false;
}

PUBLIC void MenuItem::setRadio(bool b)
{
    mRadio = b;
}

PUBLIC bool MenuItem::isRadio()
{
    return mRadio;
}

PUBLIC void MenuItem::setChecked(bool b)
{
    if (mChecked != b) {
        mChecked = b;
        MenuUI* ui = getMenuUI();
        ui->setChecked(b);
    }
}

PUBLIC bool MenuItem::isChecked()
{
    return mChecked;
}

/**
 * Overload this so we can propagate the native state.
 */
PUBLIC void MenuItem::setEnabled(bool b) 
{
    if (isEnabled() != b) {
        Component::setEnabled(b);
        MenuUI* ui = getMenuUI();
        ui->setEnabled(b);
    }
}

PUBLIC void MenuItem::open()
{
    ComponentUI* ui = getUI();
    ui->open();

    for (Component* c = getComponents() ; c != NULL ; c = c->getNext())
        c->open();
}

PUBLIC MenuItem* MenuItem::getSelectedItem()
{
    return mSelectedItem;
}

PUBLIC int MenuItem::getSelectedItemId()
{
    int id = 0;
    if (mSelectedItem != NULL)
      id = mSelectedItem->getId();
    return id;
}

/****************************************************************************
 *                                                                          *
 *                                 EVENTS                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Fire a menu action event for an item contained within
 * a menu hierarchy identified by a unique id.
 *
 * This is what we use on Windows since events come in 
 * with unique item ids but without item peers.  We get the
 * MenuBar from the window and search down from there.
 *
 * Can also use this on Mac if we're using a command event
 * handler rather than a menu event handler.
 */
PUBLIC bool MenuItem::fireSelectionId(int id)
{
    bool handled = false;

	if (mId == id) {
		fireSelection(this);
		handled = true;
	}
	else {
		for (Component* c = getComponents() ; c != NULL && !handled ;
			 c = c->getNext()) {
			MenuItem* item = (MenuItem*)c;
			handled = item->fireSelectionId(id);
		}
	}
	return handled;
}

/**
 * Fire an action handler for a given item.  
 * If we don't have any listeners at one level we walk up.
 * To assist parent menu handlers we'll set the transient
 * mSelectedItem field as we go up.
 *
 * Mac uses this since it has MenuItem peers on each native item.
 */
PUBLIC void MenuItem::fireSelection(MenuItem* item)
{
    if (item != NULL) {
        if (getActionListeners() != NULL) {
            // can process it here
            mSelectedItem = item;
            fireActionPerformed();
        }
        else {
            // back up a level
            Container* parent = getParent();
            MenuItem* pitem = parent->isMenuItem();
            if (pitem != NULL)
              pitem->fireSelection(item);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                                 SEPARATOR                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC MenuSeparator::MenuSeparator()
{
	initMenuItem();
	mClassName = "MenuSeparator";
}

PUBLIC bool MenuSeparator::isSeparator()
{
    return true;
}

/****************************************************************************
 *                                                                          *
 *                                    MENU                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * This just uses the Component list to maintain the list of MenuItems.
 */

PUBLIC Menu::Menu()
{
    initMenu();
}

PUBLIC Menu::Menu(const char* text)
{
    initMenu();
    setText(text);
}

PUBLIC Menu::Menu(const char* text, int id)
{
    initMenu();
    setText(text);
    setId(id);
}

PUBLIC Menu::~Menu()
{
}

PUBLIC void Menu::initMenu()
{
    initMenuItem();
	mClassName = "Menu";
    mSelectedItem = NULL;
    mListener = NULL;
}

PUBLIC bool Menu::isMenu()
{
    return true;
}

/**
 * Overload this so we can add items that don't have handles.
 */
PUBLIC void Menu::add(Component* c)
{
    // this adds it to the hiearchy
    Container::add(c);

    // this opens it if we are, should we just always do this?
    if (isOpen())
      c->open();
}

/**
 * Should just be add() but having conflicts with Container::add.
 */
PUBLIC void Menu::addItem(const char* itemText)
{
    if (itemText != NULL) {
        MenuItem* item = new MenuItem(itemText);
        Container::add(item);
    }
}

PUBLIC void Menu::addSeparator()
{
    Container::add(new MenuSeparator());
}

/**
 * Should have a list of these, but really only need one.
 */
PUBLIC void Menu::addMenuListener(MenuListener* l)
{
    mListener = l;
}

/**
 * Internal accessor for the single listener.
 * Needed by opening.
 */
PRIVATE MenuListener* Menu::getMenuListener()
{
	return mListener;
}

PUBLIC int Menu::getItemCount()
{
    return getComponentCount();
}

PUBLIC MenuItem* Menu::getItem(int index)
{
    return (MenuItem*)getComponent(index);
}

/**
 * Container overload to remove all items from a menu.
 * The default behavior is to first call the close() method on each
 * child, then delete the child list.  Menus items can be unusual though,
 * they may not have a unique "id' but instead by identified by their
 * ordinal position within the parent menu.  This position can change
 * as the native handles are closed, but since we don't delete the Component
 * model until all have been closed it is difficult to know what the
 * actual ordinal position of an item is.  One approach is to determine
 * the location by counting the number of item Components that preceed it
 * AND have non-null native handles.  It is easier though just to push 
 * this down into the ComponentUI and let the peer delete them in bulk.
 * It turns out we're taking the first approach on Mac so we don't really
 * need this overload, but keep it arond for awhile, Windows is still
 * designed to require it.
 */
PUBLIC void Menu::removeAll()
{
	// first let the UI close the native handles
    MenuUI* ui = getMenuUI();
    ui->removeAll();

	// then delete the components
    Container::removeAll();
}

/**
 * Convenience method to perform "radio" checking for the items
 * in a menu.  If you need to check items individually, you'll
 * have to dig them out and call MenuItem::setChecked.
 */
PUBLIC void Menu::checkItem(int index)
{
    int i = 0;
    for (Component* c = getComponents() ; c != NULL ; c = c->getNext(), i++) {
        MenuItem* item = (MenuItem*)c;
        item->setChecked(index == i);
    }
}

/****************************************************************************
 *                                                                          *
 *                                  EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Called on Window when it receives a WM_INITMENU message.
 * Called on Windows when we receive kEventMenuOpening.
 * 
 * The menu is about to be displayed, make any necessary adjustments
 * to the items.
 * 
 * Swing has a MenuListener.  Not sure if its exactly the same thing
 * but let's use it.
 *
 * KLUDGE: On Windows we only receive this for the root menu bar so that's
 * where the application has to park the listener.  On Mac we can receive
 * this for the sub-menus of the bar, so when looking for the listener we
 * have to walk up.
 *
 */
PUBLIC void Menu::opening()
{
	MenuListener* listener = getEffectiveListener();
	if (listener != NULL)
	  listener->menuSelected(this);
}

/**
 * Locate the nearest MenuListener for an item.
 */
PRIVATE MenuListener* Menu::getEffectiveListener()
{
	MenuListener* listener = mListener;

	if (listener == NULL) {
		// look up
		Component* parent = getParent();
		while (parent != NULL && listener == NULL) {
			// the root is a Frame so have to check
			MenuItem* item = parent->isMenuItem();
			if (item != NULL && item->isMenu())
			  listener = ((Menu*)item)->getMenuListener();
			parent = parent->getParent();
		}
	}

	return listener;
}

/****************************************************************************
 *                                                                          *
 *                                  MENU BAR                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC MenuBar::MenuBar()
{
	initMenuBar();
}

PUBLIC MenuBar::MenuBar(const char *resname)
{
	initMenuBar();
	setResource(resname);
}

PUBLIC void MenuBar::initMenuBar()
{
	initMenu();
	mClassName = "MenuBar";
	mResource = NULL;
}

PUBLIC MenuBar::~MenuBar()
{
	delete mResource;
}

PUBLIC bool MenuBar::isMenuBar()
{
    return true;
}

/**
 * We don't allow these to be changed on the fly, set this
 * before the menu is opened.
 */
PUBLIC void MenuBar::setResource(const char *name)
{
	delete mResource;
	mResource = CopyString(name);
}

PUBLIC const char* MenuBar::getResource()
{
    return mResource;
}

PUBLIC Menu* MenuBar::getMenu(int index)
{
    return (Menu*)getComponent(index);
}

PUBLIC int MenuBar::getMenuCount()
{
    return getComponentCount();
}

/****************************************************************************
 *                                                                          *
 *   							  POPUP MENU                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC PopupMenu::PopupMenu()
{
	PopupMenu(NULL);
}

PUBLIC PopupMenu::PopupMenu(const char *resource)
{
	initMenu();
	setResource(resource);
	mClassName = "PopupMenu";
}

PUBLIC bool PopupMenu::isPopupMenu()
{
    return true;
}

PUBLIC void PopupMenu::open(Window* window, int x, int y)
{
    MenuUI* ui = getMenuUI();
    ui->openPopup(window, x, y);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                                 WINDOWS UI                               *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

// not getting these unless WINVER >=0x0500
// I tried asserting that but got some warnings, just drag them over
// till this is resolved

#define MIIM_STRING      0x00000040
#define MIIM_FTYPE       0x00000100

// can't do this anyway, SetMenuInfo isn't defined

#if 0
// ugh, these are enabled only if WINVER > 5.0, but doing so emits annoying
// warning messages in the makefile, for now just copy their definitions
// locally, though this does mean we won't work on Windows 95, and
// I guess not NT 4.0 ?

#define MIM_STYLE                   0x00000010
#define MIM_MENUDATA                0x00000008
#define MNS_NOTIFYBYPOS     0x08000000

typedef struct tagMENUINFO
{
    DWORD   cbSize;
    DWORD   fMask;
    DWORD   dwStyle;
    UINT    cyMax;
    HBRUSH  hbrBack;
    DWORD   dwContextHelpID;
    DWORD   dwMenuData;
}   MENUINFO, FAR *LPMENUINFO;
typedef MENUINFO CONST FAR *LPCMENUINFO;

WINUSERAPI
BOOL
WINAPI
SetMenuInfo(
    HMENU,
    LPCMENUINFO);
#endif // commented out 5.0 shit


WindowsMenuItem::WindowsMenuItem(MenuItem* item)

{
    mItem = item;

	// !! this should do it like Mac and have a void* mHandle
	// in all peer components
    mMenuHandle = NULL;

    mCreated = false;
}

WindowsMenuItem::~WindowsMenuItem()
{
}
    
PUBLIC HMENU WindowsMenuItem::getMenuHandle()
{
    return mMenuHandle;
}

/**
 * Open a menu item.
 * Since we share the same ComponentUI class for all menu items,
 * have to use the class identification predicates to figure out
 * what to build.
 */
PUBLIC void WindowsMenuItem::open()
{
    if (mMenuHandle == NULL && !mCreated) {

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
 * Only for popup menus, open at a screen coordinate.
 */
PUBLIC void WindowsMenuItem::openPopup(Window* window, int x, int y)
{
    // note that you can't call open here since that only creates
    // the root popup menu, have to redirect back to PopupMenu to open
    mItem->open();
    if (mMenuHandle != NULL) {
		// We can get the window handle from the item on Windows, 
		// but not on Mac so Window has to be passed in
        HWND window = getWindowHandle(mItem);
		if (window != NULL) {
			// convert client relative coordinates to screen coordinates
			POINT p;
			p.x = x;
			p.y = y;
			ClientToScreen(window, &p);

			// this will block until a selection is made
			TrackPopupMenu(mMenuHandle, 0, p.x, p.y, 0, window, NULL);
		}
	}
}

PRIVATE void WindowsMenuItem::openMenuBar()
{
    MenuBar* mb = (MenuBar*)mItem;
    const char* resource = mb->getResource();
    if (resource == NULL)
        mMenuHandle = CreateMenu();
    else {
        // load as a resource
        mMenuHandle = openResourceMenu(resource);
    }
}

PRIVATE void WindowsMenuItem::openPopupMenu()
{
    PopupMenu* pm = (PopupMenu*)mItem;
    const char* resource = pm->getResource();
    if (resource == NULL)
        mMenuHandle = CreatePopupMenu();
    else {
        // load as a resource
        mMenuHandle = openResourceMenu(resource);

        // this is the outermost MENU resource, we want the
        // first submenu
        mMenuHandle = GetSubMenu(mMenuHandle, 0);
        if (mMenuHandle == NULL)
            printf("Unable to load popup menu '%s'\n", resource);
    }
}

PRIVATE HMENU WindowsMenuItem::openResourceMenu(const char* resource)
{
    HMENU handle = NULL;
    Window* window = mItem->getWindow();
    if (window != NULL) {
        WindowsContext* con = (WindowsContext*)(window->getContext());
        handle = LoadMenu(con->getInstance(), resource);
        if (handle == NULL)
            printf("Unable to load menu resource '%s'\n", resource);
        else {
            // we don't really need this since we'll always have
            // the Window in the message handler, but keep it around
            // as an example
            // !! hey, where is MENUINFO supposed to be?
#if 0
            MENUINFO info;
            info.cbSize = sizeof(info);
            info.fMask = MIM_MENUDATA;
            info.dwMenuData = this;
            SetMenuInfo(handle, &info);
#endif
        }
    }
    return handle;
}

/**
 * Sigh, we'll do this little dance a lot, but since we bounce between
 * the Component and UI models during construction, it's hard to pass
 * the handle in.
 */
PRIVATE HMENU WindowsMenuItem::getParentHandle()
{
    HMENU handle = NULL;

    MenuItem* parent = (MenuItem*)mItem->getParent();
    if (parent != NULL) { 
        ComponentUI* ui = parent->getUI();
        WindowsMenuItem* native = (WindowsMenuItem*)ui->getNative();
        if (native != NULL)
          handle = native->getMenuHandle();
    }
    return handle;
}

/**
 * Open a menu.
 */
PRIVATE void WindowsMenuItem::openMenu()
{
    HMENU parent = getParentHandle();
    const char* text = mItem->getText();

    if (parent != NULL && text != NULL) {

        mMenuHandle = CreatePopupMenu();

        // Why did we stop doing this?...or did we ever?
        // NOTIFYBYPOS is supposed to result in the generation
        // of WM_MENUCOMMAND which will get us a HMENU handle with
        // dwMenuData that takes us directly here.  The way it's done
        // now it just posts a WM_COMMAND with an id and we have to 
        // search through the menus to find it.
#if 0
        MENUINFO minfo;
        minfo.cbSize = sizeof(minfo);
        minfo.fMask = MIM_STYLE;
        minfo.dwStyle = MNS_NOTIFYBYPOS;
        minfo.dwMenuData = (this);
        SetMenuInfo(mMenuHandle, &minfo);
#endif
        MENUITEMINFO info;
        int count = GetMenuItemCount(parent);
        info.cbSize = sizeof(info);
        info.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_SUBMENU;
        info.dwTypeData = NULL;
        info.cch = 0;
        info.fState = 0;
        info.wID = mItem->getId();
        info.fType = MFT_STRING;
        info.dwTypeData = (char*)text;
        info.cch = strlen(text);
        info.hSubMenu = mMenuHandle;
        info.dwItemData = (ULONG)this;

        InsertMenuItem(parent, count + 1, TRUE, &info);
    }
}

/**
 * Open a basic item or seperator.
 */
PRIVATE void WindowsMenuItem::openItem()
{
    HMENU parent = getParentHandle();
    const char* text = mItem->getText();

    if (parent != NULL && (text != NULL || mItem->isSeparator())) {
        
        MENUITEMINFO info;
        int count = GetMenuItemCount(parent);
        
        info.cbSize = sizeof(info);
        // MIIM_STRING and MIIM_FTYPE require WINVER >= 0x0500
        info.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
        info.dwTypeData = NULL;
        info.cch = 0;

        // also have "default" and "highlight" 
        info.fState = 0;
        if (!mItem->isEnabled())
          info.fState |= MFS_DISABLED;
        if (mItem->isChecked())
          info.fState |= MFS_CHECKED;
        
        // auto number if we get to this point
        if (mItem->getId() <= 0)
          mItem->setId(ItemNumbers++);
        info.wID = mItem->getId();
        
        if (mItem->isSeparator())
          info.fType = MFT_SEPARATOR;
        else {
            info.fType = MFT_STRING;
            if (mItem->isRadio())
              info.fType = MFT_RADIOCHECK;
            info.dwTypeData = (char*)text;
            info.cch = strlen(text);
        }
        
        // this links us with the item, but it doesn't look like there
        // is the concept of a menu item handle
        info.dwItemData = (ULONG)this;
        
        InsertMenuItem(parent, count + 1, TRUE, &info);
        mCreated = true;
    }
}

void WindowsMenuItem::setChecked(bool checked)
{
    setNativeState((checked) ? MFS_CHECKED : MFS_UNCHECKED);
}

void WindowsMenuItem::setEnabled(bool enabled)
{
    setNativeState((enabled) ? MFS_ENABLED : MFS_DISABLED);
}

/**
 * Propagate certain component properties to the
 * native menu item if the item has already been built.
 * We don't have our own handle, everything goes through
 * the parent menu handle.  Rely on the mCreated flag to tell
 * us if we've already been added to the parent menu since it
 * may create a handle before items are inserted.  I suppose
 * we could avoid this by probing the menu too.
 */
PRIVATE void WindowsMenuItem::setNativeState(int mask)
{
    HMENU parent = getParentHandle();
    if (parent != NULL && (mMenuHandle != NULL || mCreated)) {
        MENUITEMINFO info;
        info.cbSize = sizeof(info);
        info.fMask = MIIM_STATE;
        info.fState = mask;

        // the item can be identified by index or id, now that
        // we're auto-numbering them, assume we can use the id, 
        // otherwise would have to lookup the position within 
        // the parent Menu

        SetMenuItemInfo(parent, mItem->getId(), FALSE, &info);
    }        
}

/**
 * Used for dynamic menus that get rebuilt regularly.
 */
PUBLIC void WindowsMenuItem::close()
{
    if (mMenuHandle != NULL) {
        DestroyMenu(mMenuHandle);
        mMenuHandle = NULL;
    }
    else if (mCreated) {
        // an item, have to locate the parent handle
        HMENU parent = getParentHandle();
        if (parent != NULL) {
            // by "command" here actually means by id
            RemoveMenu(parent, mItem->getId(), MF_BYCOMMAND);
        }
    }
    mCreated = false;
}

/**
 * Remove handles in child items after the parent is closed.
 */
PUBLIC void WindowsMenuItem::invalidateHandle()
{
    mMenuHandle = NULL;
    mHandle = NULL;
    mCreated = false;
}

/**
 * Called by the Container::removeAll oveloaded method so we can 
 * remove menu items in bulk.  Unlike Mac these do have unique ids and
 * we could remove them MF_BYCOMMAND, but since we're doing them all at
 * once can just use MF_BYPOSITION until they're gone.
 */
PUBLIC void WindowsMenuItem::removeAll()
{
    if (mMenuHandle != NULL) {
        for (Component* c = mItem->getComponents() ; c != NULL ; 
             c = c->getNext()) {

            // just in case the item ids are screwed up, don't
            // bother removing them by id, just remove the first one each time
            RemoveMenu(mMenuHandle, 0, MF_BYPOSITION);

            c->close();
        }
    }
}

/**
 * Traverse a menu hierarchy looking for the Menu corresponding
 * to a native menu handle.
 */
PUBLIC Menu* WindowsMenuItem::findMenu(HMENU handle)
{
    Menu* found = NULL;

    if (mMenuHandle == handle) {
		if (mItem->isMenu())
		  found = (Menu*)mItem;
	}
    else {
        for (Component* c = mItem->getComponents() ; 
             c != NULL ; c = c->getNext()) {
			// in practice this will always be true
			MenuItem* item = c->isMenuItem();
			if (item != NULL && item->isMenu()) {
				Menu* sub = (Menu*)item;
				ComponentUI* ui = sub->getUI();
				WindowsMenuItem* item = (WindowsMenuItem*)ui->getNative();
				found = item->findMenu(handle);
			}
		}
    }
    return found;
}

QWIN_END_NAMESPACE
#endif // _WIN32

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
