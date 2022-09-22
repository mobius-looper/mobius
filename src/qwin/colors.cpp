/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An old test application that shows a color slider.
 * Loosely based on Petzold's COLORS1 example.
 * This only works on Windows.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Qwin.h"
#include "UIManager.h"

/****************************************************************************
 *                                                                          *
 *                                COLOR SLIDER                              *
 *                                                                          *
 ****************************************************************************/

class ColorSlider : public Container, ActionListener {
    
    public:

    ColorSlider(const char *name);
    ~ColorSlider();

    ComponentUI* getUI() {  
        if (mUI == NULL)
          mUI = UIManager::getNullUI();
        return mUI;
    }

    int getValue();

    void actionPerformed(void* o);
    void dumpLocal(int indent);

    void setColor(Color* c) {
        mScroll->setBackground(c);
        mLabel->setForeground(c);
    }

    private:

    Label* mLabel;
    ScrollBar* mScroll;
    Label* mValue;

};

ColorSlider::ColorSlider(const char *name)
{
	// since we're going to be on a white background, have to 
	// specify background color for the labels, would be nice
	// if there was a transparent option

    setLayout(new BorderLayout());
    mLabel = new Label(name);
	mLabel->setBackground(Color::White);
    add(mLabel, BORDER_LAYOUT_NORTH);
    mScroll = new ScrollBar(0, 255);
    mScroll->setVertical(true);
	mScroll->setPageSize(10);
    mScroll->addActionListener(this);
    add(mScroll, BORDER_LAYOUT_CENTER);
    mValue = new Label("0");
	mValue->setBackground(Color::White);
    add(mValue, BORDER_LAYOUT_SOUTH);
}

ColorSlider::~ColorSlider()
{
}

int ColorSlider::getValue()
{
    return mScroll->getValue();
}

void ColorSlider::actionPerformed(void* o)
{
    // can only be the scroll bar, update the label
    char buf[128];
    sprintf(buf, "%d", mScroll->getValue());
    mValue->setText(buf);

    // notify our own listeners
    fireActionPerformed();
}

void ColorSlider::dumpLocal(int indent)
{
    dumpType(indent, "ColorSlider");
}

/****************************************************************************
 *                                                                          *
 *                                APPLICATION                               *
 *                                                                          *
 ****************************************************************************/

class Application : public ActionListener {
    
  public:

    Application();
    ~Application();

    int run(Context *c);

    void actionPerformed(void* o);

  private:

    Frame* mFrame;
    ColorSlider* mRed;
    ColorSlider* mGreen;
    ColorSlider* mBlue;
    Static* mColorRect;

};

Application::Application()
{
}

Application::~Application()
{
}

void Application::actionPerformed(void* o)
{
    // must be mRed, mGreen, or mBlue

    // !! need to be MUCH more careful about these
    Color* color = new Color(mRed->getValue(),  
                             mBlue->getValue(),
                             mGreen->getValue());

    mColorRect->setBackground(color);
}


int Application::run(Context *con)
{
    int result = 0;

	mFrame = new Frame(con, "Color Scroll");
    mFrame->setLayout(new GridLayout(1, 2));

    // add spacer panels between each slider
    Panel* sliders = new Panel();
    sliders->setBackground(Color::White);

    sliders->setLayout(new GridLayout(1, 7));
    mFrame->add(sliders);
    
    sliders->add(new Panel());
    mRed = new ColorSlider("Red");
    mRed->addActionListener(this);
    mRed->setColor(Color::Red);
    sliders->add(mRed);
    sliders->add(new Panel());
    mGreen = new ColorSlider("Green");
    mGreen->addActionListener(this);
    mGreen->setColor(Color::Green);
    sliders->add(mGreen);
    sliders->add(new Panel());
    mBlue = new ColorSlider("Blue");
    mBlue->addActionListener(this);
    mBlue->setColor(Color::Blue);
    sliders->add(mBlue);
    sliders->add(new Panel());

    mColorRect = new Static();
    mFrame->add(mColorRect);

    result = mFrame->run();

    delete mFrame;
    
    return result;
}

/****************************************************************************
 *                                                                          *
 *                                  WINMAIN                                 *
 *                                                                          *
 ****************************************************************************/

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance,
				   LPSTR commandLine, int cmdShow)
{
    int result = 0;

	Context* con = new WindowsContext(instance, commandLine, cmdShow);
    Application *app = new Application();

    result = app->run(con);

    delete app;
    
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
