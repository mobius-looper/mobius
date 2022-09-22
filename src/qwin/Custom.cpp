/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An example of a custom painted component.
 * This is how most of the Mobius components are implemented.
 *
 * If you want to receive mouse events the compoent MUST extend
 * Panel and set the MouseTracking property.  This is a kludge for
 * Mac.  With the standard event handler most mouse events get eaten
 * by the Control Manager.  Windows will receive mouse down events but
 * not move or up events.
 *
 * The only workaround I found was to overload the HitTest event
 * in a custom UserPane and return zero so Control Manager won't take
 * the events.  This is all done in Panel.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Thread.h"
#include "Trace.h"

#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Example
//
//////////////////////////////////////////////////////////////////////

class CustomThread : public Thread
{
  public:

    CustomThread(CustomExample* comp);
    ~CustomThread();

    void eventTimeout();

  private:
	
	CustomExample* mCustom;
};

CustomThread::CustomThread(CustomExample* c)
{
	mCustom = c;
}

CustomThread::~CustomThread()
{
}

void CustomThread::eventTimeout()
{
	mCustom->nextLevel();
}

PUBLIC CustomExample::CustomExample()
{
	mClassName = "Custom";
	mLevel = 0;

    addMouseListener(this);
    addMouseMotionListener(this);
    addKeyListener(this);

//	CustomThread* ct = new CustomThread(this);
//	ct->start();
}

PUBLIC CustomExample::~CustomExample()
{
}

#define MAX_LEVEL 1

PUBLIC void CustomExample::nextLevel()
{
	mLevel++;
	if (mLevel > MAX_LEVEL)
	  mLevel = 0;

	invalidate();
}

PUBLIC Dimension* CustomExample::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
		mPreferred->width = 50;
		mPreferred->height = 50;
	}
	return mPreferred;
}

PUBLIC void CustomExample::paint(Graphics* g)
{
	tracePaint();

	Bounds b;
	getPaintBounds(&b);

	Color* color;
	if (mLevel == 0)
	  color = Color::Red;
	else
	  color = Color::Green;

	g->setColor(color);
	g->fillRect(b.x, b.y, b.width, b.height);
}

PUBLIC void CustomExample::keyPressed(KeyEvent* e)
{
	printf("Custom: KeyEvent pressed type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}

PUBLIC void CustomExample::keyReleased(KeyEvent* e)
{
	printf("Custom: KeyEvent released type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}

PUBLIC void CustomExample::keyTyped(KeyEvent* e)
{
	printf("Custom: KeyEvent typed type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}
                    
PUBLIC void CustomExample::mousePressed(MouseEvent* e)
{
	printf("Custom: MouseEvent pressed type %d button %d clicks %d x %d y%d\n",
		   e->getType(), 
		   e->getButton(),
		   e->getClickCount(),
		   e->getX(),
		   e->getY());
    fflush(stdout);
}

PUBLIC void CustomExample::mouseReleased(MouseEvent* e)
{
	printf("Custom: MouseEvent released type %d button %d clicks %d x %d y%d\n",
		   e->getType(), 
		   e->getButton(),
		   e->getClickCount(),
		   e->getX(),
		   e->getY());
    fflush(stdout);
}

PUBLIC void CustomExample::mouseMoved(MouseEvent* e)
{
	printf("Custom: MouseEvent moved type %d button %d clicks %d x %d y%d\n",
		   e->getType(), 
		   e->getButton(),
		   e->getClickCount(),
		   e->getX(),
		   e->getY());
    fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *   							CUSTOM BUTTON                               *
 *                                                                          *
 ****************************************************************************/
/*
 * Base class for a typical owner draw button.
 * This also provides some infrastructure for momentary buttons.
 * You can use this as is, but generally want to subclass it.
 *
 * Originally this was a subclass of Button and on Windows used the
 * "ownerdraw" flag to let us control the rendering but still have
 * Windows handle the click events.
 *
 * Mac didn't have this, and I had issues with ownerdraw nested in static 
 * components (not getting drawitem events).  So this was changed to be
 * a pure lighthweight component that handles it's own drawing and mouse 
 * handling.
 * 
 * This can't be a Button subclass any more because Mac needs to create
 * a UserPane control to receive mouse events.
 */

PUBLIC CustomButton::CustomButton()
{
	initCustomButton();
}

PUBLIC CustomButton::CustomButton(const char* text)
{
	initCustomButton();
	setText(text);
}

PUBLIC void CustomButton::initCustomButton()
{
	mClassName = "CustomButton";
	mText = NULL;
	mFont = NULL;
	mTextColor = NULL;
	mMomentary = false;
    mToggle = false;
	mPushed = false;
	mVerticalPad = 10;
	mHorizontalPad = 4;
	mArcWidth = 10;

    addMouseListener(this);

	setFont(Font::getFont("Helvetica", 0, 14));
}

PUBLIC CustomButton::~CustomButton()
{
	delete mText;
}

PUBLIC void CustomButton::setText(const char *s) 
{
	delete mText;
	mText = CopyString(s);
}

PUBLIC const char* CustomButton::getText()
{
    return mText;
}

PUBLIC void CustomButton::setFont(Font* f)
{
	mFont = f;
}

PUBLIC Font* CustomButton::getFont()
{
    return mFont;
}

PUBLIC void CustomButton::setVerticalPad(int i)
{
	mVerticalPad = i;
}

PUBLIC void CustomButton::setHorizontalPad(int i)
{
	mHorizontalPad = i;
}

PUBLIC void CustomButton::setArcWidth(int i)
{
	mArcWidth = i;
}

PUBLIC void CustomButton::setTextColor(Color* c)
{
    mTextColor = c;
}

PUBLIC Color* CustomButton::getTextColor()
{
    return mTextColor;
}

PUBLIC void CustomButton::setMomentary(bool b)
{
	mMomentary = b;
}

PUBLIC bool CustomButton::isMomentary()
{
	return mMomentary;
}

PUBLIC void CustomButton::setToggle(bool b)
{
	mToggle = b;
}

PUBLIC bool CustomButton::isToggle()
{
	return mToggle;
}

PUBLIC void CustomButton::setPushed(bool b)
{
    mPushed = b;
}

PUBLIC bool CustomButton::isPushed()
{
    return mPushed;
}

/**
 * Programatically simulate the clicking of the button.
 * For real buttons this sends an event, here we just fire the action handlers.
 */
PUBLIC void CustomButton::click()
{
	fireActionPerformed();
}

PUBLIC Dimension* CustomButton::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();

		w->getTextSize(mText, getFont(), mPreferred);
			
		// add some girth for the hotdog edges, this should probably
		// be proportional to the font?
		mPreferred->width += mHorizontalPad;
		mPreferred->height += mVerticalPad;
			
		// the arc width defines the effective minimum in both dimensions
		int min = mArcWidth * 2;
		if (mPreferred->width < min) mPreferred->width = min;
		if (mPreferred->height < min) mPreferred->height = min;
	}
	return mPreferred;
}

PUBLIC void CustomButton::paint(Graphics* g)
{
    tracePaint();

    Bounds b;
    getPaintBounds(&b);

    int width = getWidth();
    int height = getHeight();
						 
    // formerly required an LPDRAWITEMSTRUCT but now
    // we're trying to be a "pure" lightweight button

    if (!isEnabled()) {
        g->setColor(getBackground());
        g->fillRect(b.x, b.y, width, height);
    }
    else {
        // clear the background
        g->setColor(getBackground());
        g->fillRect(b.x, b.y, width, height);

		// you can different width and height for the arc, this looks ok
        g->setColor(getForeground());
        g->fillRoundRect(b.x, b.y, width, height, mArcWidth, mArcWidth);

		if (mText != NULL) {

			// note that the text background is the button foreground 
			g->setBackgroundColor(getForeground());
			if (isPushed())
			  g->setColor(getTextColor());
			else
			  g->setColor(getBackground());
			g->setFont(getFont());

			TextMetrics* tm = g->getTextMetrics();
			Dimension d;
			g->getTextSize(mText, &d);

			int tleft = b.x + ((width - d.width) / 2);
			if (tleft < 0) tleft = 0;
			int ttop = b.y + (height / 2) + (tm->getAscent() / 2);

			// have traditionally done this, doesn't  look good on mac
			//ttop -= 2;

			g->drawString(getText(), tleft, ttop);
		}
	}
}

PUBLIC void CustomButton::mousePressed(MouseEvent* e) 
{
    //printf("CustomButton::mousePressed\n");
    //fflush(stdout);

    // claim this event so we can get the release event
    // if the mouse strays outside our bounds
    e->setClaimed(true);

    if (mToggle)
      setPushed(!isPushed());
    else
      setPushed(true);

	fireActionPerformed();

    invalidate();
}

PUBLIC void CustomButton::mouseReleased(MouseEvent* e) 
{
    //printf("CustomButton::mouseReleased\n");
    //fflush(stdout);

    if (!mToggle) {
        setPushed(false);
        invalidate();
    }

    if (mMomentary)
      fireActionPerformed();
}

/**
 * We don't have any double click behavior yet, though
 * I suppose we could add something to the action event?
 * Double click events come in as:
 *    pressed
 *    released
 *    clicked clickCount=2
 *    released
 */
PUBLIC void CustomButton::mouseClicked(MouseEvent* e) 
{
    //printf("CustomButton::mouseClicked %d\n", e->getClickCount());
    //fflush(stdout);

    if (e->getClickCount() == 2)
      mousePressed(e);
}

QWIN_END_NAMESPACE



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
