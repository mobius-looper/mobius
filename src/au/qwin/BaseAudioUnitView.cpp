
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Trace.h"
#include "Context.h"
#include "qwin.h"

#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"
#include "BaseAudioUnit.h"

QWIN_USE_NAMESPACE


//////////////////////////////////////////////////////////////////////
//
// View Class
//
//////////////////////////////////////////////////////////////////////

/**
 * ComponentBase has these interesting virtuals:
 *    PostConstructor
 *    PreDestructor
 *    Version
 *
 * AUCarbonViewBase has these:
 *    CreateCarbonView - if you don't want to use the auto-sizing stuff
 *    CreateUI - the usual place build things
 *    HandleEvent - but we register our ownhandler
 *    RespondToEventTimer
 *
 */
class BaseAudioUnitView : public AUCarbonViewBase, public WindowAdapter, public MouseInputAdapter, public KeyListener
{

  public:

	BaseAudioUnitView(AudioUnitCarbonView auv);
	~BaseAudioUnitView();
	
	/**
	 * Opens everything.
	 * Destructor handles cleanup.
	 */
	virtual OSStatus CreateUI(Float32 xoffset, Float32 yoffset);

	void mousePressed(MouseEvent* e);
	void mouseReleased(MouseEvent* e);
	void windowOpened(WindowEvent* e);
	void windowClosing(WindowEvent* e);
	void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);
	void keyTyped(KeyEvent* e);

  private:
	
	OSStatus CreateUIExample(Float32 xoffset, Float32 yoffset);
	OSStatus CreateUIQwin(Float32 xoffset, Float32 yoffset);

	bool mTrace;
	HostFrame* mHost;

};

/**
 * This someshow magically defines an entry point but I can't
 * find the macro.
 */
COMPONENT_ENTRY(BaseAudioUnitView)

//////////////////////////////////////////////////////////////////////
//
// UI Construction
//
//////////////////////////////////////////////////////////////////////

BaseAudioUnitView::BaseAudioUnitView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv)
{
	mTrace = true;
	mHost = NULL;
	// let framework eventually call CreateUI
}

BaseAudioUnitView::~BaseAudioUnitView()
{
	if (mHost != NULL) {
		mHost->close();
		delete mHost;
		mHost = NULL;
	}
}

OSStatus BaseAudioUnitView::CreateUI(Float32 xoffset, Float32 yoffset)
{
	//CreateUIExample(xoffset, yoffset);
	CreateUIQwin(xoffset, yoffset);
}

//////////////////////////////////////////////////////////////////////
//
// Old Example from the SDK
//
//////////////////////////////////////////////////////////////////////

#define kLabelWidth 80
#define kLabelHeight 16
#define kEditTextWidth 40
#define kMinMaxWidth 32

OSStatus BaseAudioUnitView::CreateUIExample(Float32 xoffset, Float32 yoffset)
{
    // need offsets as int's:
    int xoff = (int)xoffset;
    int yoff = (int)yoffset;
    
	if (mTrace)
	  trace("BaseAudioUnitView::CreateUI xoffset %d yoffset %d\n",
			xoff, yoff);

    // for each parameter, create controls
	// inside mCarbonWindow, embedded in mCarbonPane
	
	ControlRef newControl;
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;
	fontStyle.just = teFlushRight;
	
	Rect r;
	::Point labelSize, textSize;
	labelSize.v = textSize.v = kLabelHeight;
	labelSize.h = kMinMaxWidth;
	textSize.h = kEditTextWidth;
	
	CAAUParameter auvp(mEditAudioUnit, kParamRandomValue, kAudioUnitScope_Global, 0);
		
	// text label
	r.top = 4 + yoff;
	r.bottom = r.top + kLabelHeight;
	r.left = 4 +xoff;
	r.right = r.left + kLabelWidth;
	verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, auvp.GetName(), &fontStyle, &newControl));
	verify_noerr(EmbedControl(newControl));

	r.left = r.right + 4;
	r.right = r.left + 240;
	AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
	
	// set size of overall pane
	SizeControl(mCarbonPane, mBottomRight.h + 8, mBottomRight.v + 8);
	return noErr;
}

//////////////////////////////////////////////////////////////////////
//
// New example using QWIN
//
//////////////////////////////////////////////////////////////////////

OSStatus BaseAudioUnitView::CreateUIQwin(Float32 xoffset, Float32 yoffset)
{
	// have to get this from the BaseAudioUnit!!
	// need to do the same config bootstrapping as VstMain
	MacContext* mc = new MacContext(0, NULL);

	// Unlike VST the window is already open with a random size, we're supposed
	// to size the mCarbonPane the way we want and the framework presumably
	// resizes the window?  In Mobius, we would get these sizes from ui.xml

	Bounds b;
	b.x = 0;
	b.y = 0;
	b.width = 400;
	b.height = 200;

	mHost = new HostFrame(mc, mCarbonWindow, mCarbonPane, &b);
	mHost->addWindowListener(this);
	mHost->addMouseListener(this);
	mHost->addKeyListener(this);
	mHost->setBackground(Color::Black);

	// add stuff
	Label* l = new Label("Hello Audio Unit!");
	l->setForeground(Color::Red);
	l->setBackground(Color::Black);
	mHost->add(l);

	mHost->open();

	return noErr;
}

/**
 * Called when we are registered as a mouse listener
 * for the HostFrame in dual-window mode.
 */
void BaseAudioUnitView::mousePressed(MouseEvent* e)
{
	trace("BaseAudioUnitView::mousePressed %d %d\n", e->getX(), e->getY());
}

void BaseAudioUnitView::mouseReleased(MouseEvent* e)
{
	trace("BaseAudioUnitView::mouseReleased %d %d\n", e->getX(), e->getY());
}

/**
 * WindowListener for VST host window.
 * We care about the opened event because this is when the UI
 * will start the timer and begin periodic refreshes.
 */
void BaseAudioUnitView::windowOpened(WindowEvent* e)
{
	trace("BaseAudioUnitView::windowOpened\n");
}

/**
 * This is where UIFrame would save the ending locations but
 * we don't need to since you can't resize a VST host window.
 */
void BaseAudioUnitView::windowClosing(WindowEvent* e)
{
	trace("BaseAudioUnitView::windowClosing\n");
}

void BaseAudioUnitView::keyPressed(KeyEvent* e)
{
	trace("BaseAudioUnitView::keyPressed %d\n", e->getKeyCode());
}

void BaseAudioUnitView::keyReleased(KeyEvent* e)
{
	trace("BaseAudioUnitView::keyReleased %d\n", e->getKeyCode());
}

void BaseAudioUnitView::keyTyped(KeyEvent* e)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


