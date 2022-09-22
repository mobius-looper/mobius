/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An early attempt at trees.
 * This was never used by Mobius and the Mac version is stubbed out.
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
 *                                    TREE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Tree::Tree()
{
	mClassName = "Tree";
}

PUBLIC Tree::~Tree()
{
}

PUBLIC ComponentUI* Tree::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getTreeUI(this);
	return mUI;
}

PUBLIC TreeUI* Tree::getTreeUI()
{
	return (TreeUI*)getUI();
}

PUBLIC void Tree::dumpLocal(int indent)
{
    dumpType(indent, "Tree");
}

PUBLIC Dimension* Tree::getPreferredSize(Window* w)
{
    if (mPreferred == NULL) {
        TextMetrics* tm = w->getTextMetrics();
        // todo: calculate max child size + tab height
        mPreferred = new Dimension(200, 100);
    }
    return mPreferred;
}

PUBLIC void Tree::open()
{
    ComponentUI* ui = getUI();
    ui->open();

	// recurse on children
	Container::open();
}
    
QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  WINDOWS                                 *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "UIWindows.h"

QWIN_BEGIN_NAMESPACE

PUBLIC WindowsTree::WindowsTree(Tree* t)
{
    mTree = t;
}

PUBLIC WindowsTree::~WindowsTree()
{
}

/**
 * Tree view styles:
 *  TVS_HASLINES draws lines between nodes
 *  TVS_LINESATROOT draw line to root nodee
 *  TVS_SHOWSELALWAYS selected item remains when focus is lost
 *  TVS_HASBUTTONS adds the plus/minus button
 *  TVS_EDITABLES allows editing of the item labels
 *  TVS_CHECKBOXES creates checkboxes next to items.
 */
PUBLIC void WindowsTree::open()
{
	// Only create a static component if we have a background color
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
        if (parent != NULL) {
            DWORD style = getWindowStyle() | WS_CLIPSIBLINGS;

            // Microsoft example also used TVS_NOTOOLTIPS and
            // TVS_TRACKSELECT
            style |= TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
                TVS_HASBUTTONS | WS_BORDER;

            Bounds* b = mTree->getBounds();
            Point p;
            mTree->getNativeLocation(&p);
            mHandle = CreateWindow(WC_TREEVIEW,
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Tree control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mTree->initVisibility();

                // define the tree
                TVINSERTSTRUCT tvs;
                tvs.item.mask = TVIF_TEXT;
                tvs.hParent = TVI_ROOT;
                tvs.item.pszText = "item";
                tvs.item.cchTextMax = strlen(tvs.item.pszText) + 1;
                tvs.hInsertAfter = TVI_LAST;
                HTREEITEM hNewItem = TreeView_InsertItem(mHandle, &tvs);
                if (hNewItem)
                  TreeView_Select(mHandle, hNewItem, TVGN_CARET);

                // a few more
                TreeView_InsertItem(mHandle, &tvs);
                TreeView_InsertItem(mHandle, &tvs);
                TreeView_InsertItem(mHandle, &tvs);

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
QWIN_BEGIN_NAMESPACE

PUBLIC MacTree::MacTree(Tree* t)
{
    mTree = t;
}

PUBLIC MacTree::~MacTree()
{
}

PUBLIC void MacTree::open()
{
	// give it something to say
	if (mTree->getComponents() == NULL) {
		Label* l = new Label("Tree not implemented");
		mTree->add(l);
		mTree->setBorder(Border::BlackLine);
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
