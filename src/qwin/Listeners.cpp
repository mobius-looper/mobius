/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Utility class to represent a list of listeners and
 * send events to them.
 *
 * To avoid duplication of trivial classes, I'm putting some
 * listener/event specific methods in here.  Would technically be
 * better to have several collection classes (or *shudder* use a template)
 * but I want to reduce clutter, we don't have that many of these.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "Trace.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 LISTENERS                                *
 *                                                                          *
 ****************************************************************************/


PUBLIC Listeners::Listeners()
{
	mListeners = NULL;
}

PUBLIC Listeners::~Listeners()
{
	delete mListeners;
}

PUBLIC int Listeners::size()
{
    return (mListeners != NULL) ? mListeners->size() : 0;
}

PUBLIC void Listeners::addListener(void* l)
{
    if (l == NULL)
      trace("Attempt to add NULL listener\n");
    else {
        if (mListeners == NULL)
          mListeners = new List();
        mListeners->add(l);
    }
}

PUBLIC void Listeners::removeListener(void* l)
{
	if (mListeners != NULL)
	  mListeners->remove(l);
}

PUBLIC void Listeners::fireActionPerformed(void* o)
{
	if (mListeners != NULL) {
		for (int i = 0 ; i < mListeners->size() ; i++) {
			ActionListener* l = (ActionListener*)mListeners->get(i);
			l->actionPerformed(o);
		}
	}
}

PUBLIC void Listeners::fireMouseEvent(MouseEvent *e)
{
	if (mListeners != NULL) {
		for (int i = 0 ; i < mListeners->size() && !e->isClaimed() ; i++) {
			MouseListener* l = (MouseListener*)mListeners->get(i);

            switch (e->getType()) {
                case MOUSE_EVENT_CLICKED:
                    l->mouseClicked(e);
                    break;
                case MOUSE_EVENT_ENTERED:
                    l->mouseEntered(e);
                    break;
                case MOUSE_EVENT_EXITED:
                    l->mouseExited(e);
                    break;
                case MOUSE_EVENT_PRESSED:
                    l->mousePressed(e);
                    break;
                case MOUSE_EVENT_RELEASED:
                    l->mouseReleased(e);
                    break;
            }
		}
	}
}

PUBLIC void Listeners::fireMouseMotionEvent(MouseEvent *e)
{
	if (mListeners != NULL) {
		for (int i = 0 ; i < mListeners->size() && !e->isClaimed() ; i++) {
			MouseMotionListener* l = (MouseMotionListener*)mListeners->get(i);
            switch (e->getType()) {
                case MOUSE_EVENT_DRAGGED:
                    l->mouseDragged(e);
                    break;
                case MOUSE_EVENT_MOVED:
                    l->mouseMoved(e);
                    break;
            }
		}
	}
}

PUBLIC void Listeners::fireKeyEvent(KeyEvent *e)
{
	// TODO: not supporting keyTyped

	if (mListeners != NULL) {
		for (int i = 0 ; i < mListeners->size() && !e->isClaimed() ; i++) {
			KeyListener* l = (KeyListener*)mListeners->get(i);
			
            switch (e->getType()) {
                case KEY_EVENT_DOWN:
                    l->keyPressed(e);
                    break;
                case KEY_EVENT_UP:
                    l->keyReleased(e);
                    break;
            }
		}
	}
}

PUBLIC void Listeners::fireWindowEvent(WindowEvent *e)
{
	if (mListeners != NULL) {
		for (int i = 0 ; i < mListeners->size() ; i++) {
			WindowListener* l = (WindowListener*)mListeners->get(i);
			
            switch (e->getId()) {
				case WINDOW_EVENT_ACTIVATED:
					l->windowActivated(e);
					break;
				case WINDOW_EVENT_CLOSED:
					l->windowClosed(e);
					break;
				case WINDOW_EVENT_CLOSING:
					l->windowClosing(e);
					break;
				case WINDOW_EVENT_DEACTIVATED:
					l->windowDeactivated(e);
					break;
				case WINDOW_EVENT_DEICONIFIED:
					l->windowDeiconified(e);
					break;
				case WINDOW_EVENT_ICONIFIED:
					l->windowIconified(e);
					break;
				case WINDOW_EVENT_OPENED:
					l->windowOpened(e);
					break;
			}
		}
	}
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
