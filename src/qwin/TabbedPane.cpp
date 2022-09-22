/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
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
 *   							 TABBED PANE                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC TabbedPane::TabbedPane()
{
	mClassName = "TabbedPane";
    mSelected = 0;
	setLayout(new StackLayout());
}

PUBLIC TabbedPane::~TabbedPane()
{
}

PUBLIC ComponentUI* TabbedPane::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getTabbedPaneUI(this);
	return mUI;
}

PUBLIC TabbedPaneUI* TabbedPane::getTabbedPaneUI()
{
	return (TabbedPaneUI*)getUI();
}

PUBLIC int TabbedPane::getTabCount()
{
    int count = 0;
    for (Component* c = getComponents() ; c != NULL ; c = c->getNext())
      count++;
    return count;
}

PUBLIC int TabbedPane::getSelectedIndex()
{
    TabbedPaneUI* ui = getTabbedPaneUI();
    if (ui->isOpen())
      mSelected = ui->getSelectedIndex();
    return mSelected;
}

/**
 * Only for Windows, get the current selection without refreshing.
 */
PUBLIC int TabbedPane::getSelectedIndexNoRefresh()
{
    return mSelected;
}

PUBLIC Component* TabbedPane::getSelectedComponent()
{
    return getComponent(getSelectedIndex());
}

PUBLIC void TabbedPane::setSelectedComponent(Component* comp)
{
    int index = -1;
    int count = 0;
    for (Component* c = getComponents() ; c != NULL ; c = c->getNext()) {
        if (c == comp) {
            index = count;
            break;
        }
        count++;
    }
    setSelectedIndex(index);
}

PUBLIC void TabbedPane::setSelectedIndex(int index)
{
    if (index >= 0) {
        mSelected = index;
        TabbedPaneUI* ui = getTabbedPaneUI();
        ui->setSelectedIndex(index);
    }
}

PUBLIC void TabbedPane::dumpLocal(int indent)
{
    dumpType(indent, "TabbedPane");
}

/**
 * Subtlety, first we'll calculate the preferred size of the child components
 * THEN let the UI make adjustments for the tabs.  UI will use 
 * getCurrentPreferredSize to access the total child bounds.  Also note
 * that if we're calculating for the second time we have to lose the insets
 * left behind by the UI during the previous calculation.  Otherwise the
 * layout manager will factor these into the child preferred size and we'll
 * end up bigger than necessary.
 */
PUBLIC Dimension* TabbedPane::getPreferredSize(Window* w)
{
    if (mPreferred == NULL) {
        
        // insets will be recalculated each time, you can't have
        // user specified insets in a tabbed pane
        setInsets(NULL);
        Container::getPreferredSize(w);

        // now let the UI do it's thing
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC Dimension* TabbedPane::getCurrentPreferredSize()
{
    return mPreferred;
}

PUBLIC void TabbedPane::open()
{
    ComponentUI* ui = getUI();
    ui->open();

	// recurse on children
	Container::open();

	// Embed child components and adjust visibility
	ui->postOpen();
}

/**
 * Paint only the components in the selected tab.
 * For native components, the visibility flags must also be set.
 */
void TabbedPane::paint(Graphics* g)
{
	incTraceLevel();
	int selected = getSelectedIndex();
	int index = 0;
	for (Component* c = getComponents() ; c != NULL ; c = c->getNext()) {
		if (index == selected) {
			c->paintBorder(g);
			c->paint(g);
			break;
		}
		index++;
    }
	decTraceLevel();
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsTabbedPane::WindowsTabbedPane(TabbedPane *tp)
{
	mTabbedPane = tp;
}

PUBLIC WindowsTabbedPane::~WindowsTabbedPane()
{
}

PUBLIC int WindowsTabbedPane::getSelectedIndex()
{
	int selected = 0;
    if (mHandle != NULL)
      selected = SendMessage(mHandle, TCM_GETCURSEL, 0, 0);
    return selected;
}

PUBLIC void WindowsTabbedPane::setSelectedIndex(int index)
{
	if (mHandle != NULL) {
		// returns the index of the previously selected tab or -1 
		// not sure what happens if the tab is out of range
		//SendMessage(mHandle, TCM_SETCURSEL, index, 0);

        TabCtrl_SetCurSel(mHandle, index);

        // notify plays this game, not sure why...
        // unfortunately the previous index has been lost so just
        // iterate over all of them
        Component* child = mTabbedPane->getComponents();
        for (int i = 0 ; child != NULL ; child = child->getNext(), i++) {
            if (index == i)
              child->setVisible(true);
            else
              child->setVisible(false);

        }

        // have to repaint
        mTabbedPane->invalidate();
	}
}

PUBLIC void WindowsTabbedPane::open()
{
	if (mHandle == NULL) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            // !! the parent is supposed to use WS_CLIPSIBLINGS too!
            DWORD style = getWindowStyle() | WS_CLIPSIBLINGS;
            
            // todo: use TCS_MULTILINE to allow multiple rows of tabs
            // then use TCM_GETROWCOUNT to find out how many

            Bounds* b = mTabbedPane->getBounds();
            Point p;
            mTabbedPane->getNativeLocation(&p);

            mHandle = CreateWindow(WC_TABCONTROL,
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL) {
                printf("Unable to create TabbedPane control\n");
            }
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mTabbedPane->initVisibility();

                // todo: use TCM_SETPADDING to set the thickness of the margin

                // define the tabs
				Component* c;
                int index = 0;
                for (c = mTabbedPane->getComponents() ; 
                     c != NULL ; c = c->getNext()) {

					// make sure these start out invisible
					if (index > 0)
					  c->setVisible(false);

                    const char *name = c->getName();
                    if (name == NULL) name = "";
                    TCITEM item;
                    item.mask = TCIF_PARAM | TCIF_TEXT;
                    item.pszText = (char*)name;
                    item.lParam = (LPARAM)this;
                    TabCtrl_InsertItem(mHandle, index, &item);

                    // note that we are not the parent
                    // !! handle this with a parent handle walker
                    //c->createHandle(parent);

                    index++;
                }
            }

            // KLUDGE!!
            forceHeavyLabels(mTabbedPane);
        }
    }
}

/**
 * KLUDGE!!
 * When we started default all panels and labels to lightweight
 * they stopped appearing in Windows tab controls.  I gave
 * up trying to figure out how to get them painted, 
 * invalidate/paint didn't work, nor did forcing the child 
 * panels to heavyweights like we do on Mac.  For now
 * walk through the children and force all labels heavy.
 */
PRIVATE void WindowsTabbedPane::forceHeavyLabels(Component* c)
{
    Container* cont = c->isContainer();
    if (cont != NULL) {
        Component* children = cont->getComponents();
        for (Component* c = children ; c != NULL ; c = c->getNext())
            forceHeavyLabels(c);
    }
    else {
        Label* l = c->isLabel();
        if (l != NULL)
          l->setHeavyweight(true);
    }
}

/**
 * This one is funny because we calculate the child preferred size
 * first, then use TabCtrl_AdjustRect to determine how much
 * space is required for the surrounding tabs.
 *
 */
PUBLIC void WindowsTabbedPane::getPreferredSize(Window* w, Dimension* d)
{
	Dimension em;
		
    // the bounds of the child components have already been calculated
	Dimension* preferred = mTabbedPane->getCurrentPreferredSize();
    //printf("Child preferred %d %d\n", preferred->width, preferred->height);

    if (mHandle != NULL) {
        // calculate the true size including tabs and padding
        // there is typically a 4 pixel pad on the left, right, and bottom
        // edges and a 25 pixel area at the top for tabs
        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = preferred->width;
        r.bottom = preferred->height;
        TabCtrl_AdjustRect(mHandle, true, &r);
        // left,top are normally negative relative to 0,0
        int actualWidth = r.right - r.left;
        int actualHeight = r.bottom - r.top;
        //printf("Required control size %d %d %d %d, width %d height %d\n",
        //r.left, r.top, r.right, r.bottom,
        //actualWidth, actualHeight);

        preferred->width = actualWidth;
        preferred->height = actualHeight;

        // calculate the insets of the content region
        r.left = 0;
        r.top = 0;
        r.right = actualWidth;
        r.bottom = actualHeight;
        TabCtrl_AdjustRect(mHandle, false, &r);
        //printf("Content insets %d %d %d %d\n", 
        //r.left, r.top, r.right, r.bottom);

        mTabbedPane->setInsets(r.left, r.top, 
                               actualWidth - r.right, 
                               actualHeight - r.bottom);
    }

    // why is this necessary?
	Bounds* b = mTabbedPane->getBounds();
	b->width = 0;
	b->height = 0;

    // calculate the width of the tabs
    // this shouldn't be necessary now that we're calling TabCtrl_AdjustRect
	w->getTextSize("M", NULL, &em);

	int tabsWidth = 0;
	for (Component* c = mTabbedPane->getComponents() ; c != NULL ; c = c->getNext()) {
		const char* name = c->getName();
		// seems ot be a default and minimum size of about 5 chars
		// !! need more control
		int tabwidth = em.width * 5;
		if (name != NULL) {
			Dimension d;
			w->getTextSize(name, NULL, &d);
			int namewidth = d.width + em.width;
			if (namewidth > tabwidth)
			  tabwidth = namewidth;
		}

		tabsWidth += tabwidth;
	}
		
	if (preferred->width < tabsWidth)
	  preferred->width = tabsWidth;

	// factor in the insets
	//mTabbedPane->addInsets(preferred);
}

PUBLIC void WindowsTabbedPane::command(int code)
{
    printf("WindowsTabbedPane::command %d\n", code);
}

/**
 * On Mac we have a heavyweight panel that handles visibility.
 * On Windows we have to do it ourselves.
 */
PUBLIC void WindowsTabbedPane::notify(int code)
{
    if (code == TCN_SELCHANGE) {

        // old way
		//mTabbedPane->update();

        int prev = mTabbedPane->getSelectedIndexNoRefresh();

        // could save a list search if we remembered this
        Component* c = mTabbedPane->getComponent(prev);
        if (c != NULL)
          c->setVisible(false);

        int index = mTabbedPane->getSelectedIndex();
        c = mTabbedPane->getComponent(index);
        if (c != NULL)
          c->setVisible(true);

        // have to repaint
        mTabbedPane->invalidate();
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

PUBLIC MacTabbedPane::MacTabbedPane(TabbedPane *tp)
{
	mTabbedPane = tp;
}

PUBLIC MacTabbedPane::~MacTabbedPane()
{
}

PUBLIC int MacTabbedPane::getSelectedIndex()
{
	int selected = 0;
    if (mHandle != NULL) {
		selected = GetControl32BitValue((ControlRef)mHandle);
		// adjust from 1 based to 0 based
		if (selected > 0) selected--;
	}
    return selected;
}

PUBLIC void MacTabbedPane::setSelectedIndex(int index)
{
	if (mHandle != NULL) {
		showTabPane(index);
		// adjust from zero based to one based
	    int tab = index + 1;
		// seems like there will be something more than this...
		SetControl32BitValue((ControlRef)mHandle, tab);
	}
}

PRIVATE void MacTabbedPane::showTabPane(int index)
{
	Component* children = mTabbedPane->getComponents();
	int psn = 0;

	for (Component* c = children ; c != NULL ; c = c->getNext()) {
		ComponentUI* ui = c->getUI();
		if (ui != NULL) {
			MacComponent* mc = (MacComponent*)(ui->getNative());
			if (mc != NULL) {
				ControlRef control = (ControlRef)mc->getHandle();
				if (control != NULL) {
					bool visible = (psn == index);
					SetControlVisibility(control, visible, false);
					psn++;
				}
			}
		}
	}
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * Don't seem to get any Command events though the window does.
 */
PRIVATE EventTypeSpec TabEventsOfInterest[] = {
	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassControl, kEventControlHit },
	{ kEventClassControl, kEventControlClick },
};

PRIVATE OSStatus
TabEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Tab", cls, kind);

	if (cls == kEventClassControl) {
		// wait for the full hit
		if (kind == kEventControlHit) {
			MacTabbedPane* tp = (MacTabbedPane*)data;
			if (tp != NULL)
			  tp->fireActionPerformed();
		}
	}

	return result;
}

PUBLIC void MacTabbedPane::fireActionPerformed()
{
	//printf("  Tab %d\n", getSelectedIndex());

    if (mHandle != NULL) {
		int selected = GetControl32BitValue((ControlRef)mHandle);
		// adjust from 1 based to 0 based
		if (selected > 0) selected--;
		showTabPane(selected);
	}

	mTabbedPane->fireActionPerformed();
}

PUBLIC void MacTabbedPane::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		//bounds.right = 200;
		//bounds.bottom = 200;
		
		// kControlTabSizeSmall, kControlTabSizeLarge
		ControlTabSize size = kControlTabSizeLarge;
		// also South, East, West
		ControlTabDirection direction = kControlTabDirectionNorth;

		int numTabs = 0;
		for (Component* c = mTabbedPane->getComponents() ; 
			 c != NULL ; c = c->getNext()) {
			numTabs++;
		}

		ControlTabEntry* tabs = NULL;
		if (numTabs > 0) {
			tabs = new ControlTabEntry[numTabs];
			int index = 0;
			for (Component* c = mTabbedPane->getComponents() ; 
				 c != NULL ; c = c->getNext(), index++) {
				const char* name = c->getName();
				if (name == NULL) name = "???";
				tabs[index].icon = NULL;
				tabs[index].name = MakeCFStringRef(name);
				tabs[index].enabled = true;
			}
		}

		status = CreateTabsControl(window, &bounds, 
								   size, direction,
								   numTabs,
								   tabs,
								   &control);

		// who owns this??!!
		//delete tabs;

		if (CheckStatus(status, "MacTabbedPane::open")) {
			mHandle = control;
			SetControlReference(control, (SInt32)this);

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(TabEventHandler),
												GetEventTypeCount(TabEventsOfInterest),
												TabEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacButton::InstallEventHandler");

			// saw this in some examples but doesn't appear to be necessary
			/*
			ControlRef root;
			CreateRootControl(window, &root);
			EmbedControl(control, root);
			*/

			// some examples claimed you have to do this
			//SetControlVisibility(control, false, true);
			//SetControlBounds(control, &bounds);
			SetControlVisibility(control, true, true);
		}
	}

    // On Mac the child components must be UserPane controls, which
    // are exposed as "heavyweight" Panels.  We force them to be
    // heavyweight so the application doesn't need to undersand this.

    Component* children = mTabbedPane->getComponents();
    for (Component* c = children ; c != NULL ; c = c->getNext()) {
        Panel* p = c->isPanel();
        if (p != NULL)
          p->setHeavyweight(true);
    }

}

/**
 * After opening the children, have to embed the panels in the tab control.
 * Normally the children should only be heavyweight Panels, though we'll
 * look for anything with a ControlRef.
 */
PUBLIC void MacTabbedPane::postOpen()
{
	//printf("MacTabbedPane::postOpen\n");

	if (mHandle != NULL) {
		// this does the embedding
		embedChildren((ControlRef)mHandle);

		// only one may be visible, note that this is a more restrictive
		// walk than embedChildren, the immediate children MUST be panels
		// need to enforce this in the model?

 		ControlRef tabs = (ControlRef)mHandle;
 		Component* children = mTabbedPane->getComponents();
 		int found = 0;
 		for (Component* c = children ; c != NULL ; c = c->getNext()) {
 			ComponentUI* ui = c->getUI();
 			if (ui != NULL) {
 				MacComponent* mc = (MacComponent*)(ui->getNative());
 				if (mc != NULL) {
 					ControlRef control = (ControlRef)mc->getHandle();
 					if (control != NULL) {
 						found++;
 						// always the first?
  						bool visible = (found == 1);
 						//printf("Setting tab %d to %d\n", found, visible);
 						SetControlVisibility(control, visible, false);
 					}
 				}
 			}
		}
	}
}

PUBLIC void MacTabbedPane::getPreferredSize(Window* w, Dimension* d)
{
	// this has the preferred sizes of the children
	// since we always use CardLayout it is the size of the largest child
	Dimension* childdim = mTabbedPane->getCurrentPreferredSize();

	// this uses GetBestControlRect to get the size of the tab bar
	// as usual this is useless it returns whatever you passed in 
	// to CreateTabsControl
	Dimension tabdim;
	MacComponent::getPreferredSize(w, &tabdim);

	// just guess the sizes
	tabdim.height = 30;

	// For widths it's best to be relatively accurate.  Need to find out
	// where to get reliable metrics for the system fonts.  We'll use
	// the same approximation that Table.cpp uses by measuring text using
	// a hard coded font.  
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

	tabdim.width = 0;
	for (Component* c = mTabbedPane->getComponents() ; c != NULL ; c = c->getNext()) {
		const char* name = c->getName();
		int tabWidth = 0;
		if (name == NULL) {
			// shouldn't happen 
			tabWidth = charWidth;
		}
		else {
			Dimension td;
			g->getTextSize(name, &td);
			tabWidth = td.width;
			// there seems to be a char of padding on either
			// side so always factor that in
			tabWidth += (charWidth * 2);
		}
		tabdim.width += tabWidth;
	}

	// need some padding on each end of the tab bar for the rounded
	// corner border surrounding the panels, this seems to be independent
	// of font size
	tabdim.width += 32;

	/*
	printf("*** Tab size %d %d child size %d %d\n", 
		   tabdim.width, tabdim.height,
		   childdim->width, childdim->height);
	*/

	// let the tab panel contents make us wider
	int width = childdim->width;
	if (tabdim.width > width)
	  width = tabdim.width;

	// have to add insets so we don't draw over the tabs
	mTabbedPane->setInsets(4, tabdim.height, 4, 4);

    d->width = width;

	// add a little extra at the bottom so the surround borrder
	// doesn't get trashed
	d->height = childdim->height + tabdim.height + 4;
}

PUBLIC void MacTabbedPane::updateNativeBounds(Bounds* b) 
{
	MacComponent::updateNativeBounds(b);
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

