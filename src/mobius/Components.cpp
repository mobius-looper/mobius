/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 *
 * ---------------------------------------------------------------------
 *
 *
 * Various Mobius display components.
 *
 * The EDPDisplay component emulates the EDP LED display:
 *
 * Loop Number
 *   - number on the left, indicates which loop is playing
 *
 * AutoUndo Blinker
 *   - decimal place after Loop Number
 *   - flashes when auto-undo has been performed (reusing an unchanged
 *     record loop during shifting
 *
 * Loop Time
 *    - number in the center, two digits to the left, one to the
 *      right of a decimal
 *    - used to display the number of seconds? available at startup
 *      since we have infinite memory, ignore
 *    - displays number of seconds recorded in Record, Multiply, or Insert
 *    - displays the length of the loop in all other modes
 *    - !! does not count the current playback position?
 *	  - blank in Reset
 *    - displays feedback value when turning feedback knob
 *    - displays function names that don't have an LED
 *    - shows expected loop time when MIDI clock is present for sync
 *    - shows "ooo" when quantizing
 *    - shows next loop name (L1, L2, etc) when in SwitchQuantize
 *
 * Multiple
 *   - number on the right
 *   - displays the available number of loops set with the MoreLoops parameter
 *     at starup (or after general reset?)
 *   - displays current cycle during other modes
 *
 * Sync Correction Blinker
 *   - after the first digit of Multiple, ignore
 *
 * Global MIDI Start Point Blinker
 *   - after second digit of Multiple
 *
 * Loop Start Point Blinker
 *   - overloaded with the MIDI LED
 *
 * Cycle Start Point Blinker
 *   - overloaded with the Switches LED
 *
 * SubCycle Start Point Blinker
 *   - overloaded with the Timing LED
 *
 * Since we have infinite memory, display 00.0 on Reset for the loop time.
 *
 *
 * Simplified Digits:
 *
 *   xx  yy.y  zz
 *
 * x = loop number
 * y = loop time
 * z = cycle
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "KeyCode.h"
#include "Qwin.h"
#include "QwinExt.h"
#include "Palette.h"
#include "FontConfig.h"

// !! Think about how much of this stuff we actually need
// should be getting everything from MobiusInterface.h
// and a handful of other files

#include "Action.h"
#include "Event.h"
#include "Expr.h"
#include "Export.h"
#include "Function.h"
#include "Loop.h"
#include "Messages.h"
#include "MobiusConfig.h"
#include "Mode.h"
#include "Track.h"

#include "UIConfig.h"
#include "UITypes.h"
#include "UI.h"

/****************************************************************************
 *                                                                          *
 *   								COLORS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The definitions of the component colors.
 */

ColorDefinition *ColorBackground =
	new ColorDefinition(COLOR_SPACE_BACKGROUND, MSG_COLOR_BACKGROUND);

ColorDefinition *ColorButton =
	new ColorDefinition(COLOR_BUTTON, MSG_COLOR_BUTTON);

ColorDefinition *ColorButtonText =
	new ColorDefinition(COLOR_BUTTON_TEXT, MSG_COLOR_BUTTON_TEXT);

ColorDefinition *ColorBar =
	new ColorDefinition(COLOR_BAR, MSG_COLOR_BAR);

ColorDefinition *ColorActiveBar =
	new ColorDefinition(COLOR_ACTIVE_BAR, MSG_COLOR_ACTIVE_BAR);

ColorDefinition *ColorCheckpointBar =
	new ColorDefinition(COLOR_CHECKPOINT_BAR, MSG_COLOR_CHECKPOINT_BAR);

ColorDefinition *ColorMeter =
	new ColorDefinition(COLOR_METER, MSG_COLOR_METER);

ColorDefinition *ColorlowSlowMeter =
	new ColorDefinition(COLOR_SLOW_METER, MSG_COLOR_SLOW_METER);

ColorDefinition *ColorRecordMeter =
	new ColorDefinition(COLOR_RECORDING_METER, MSG_COLOR_RECORD_METER);

ColorDefinition *ColorMuteMeter =
	new ColorDefinition(COLOR_MUTE_METER, MSG_COLOR_MUTE_METER);

ColorDefinition *ColorEvent =
	new ColorDefinition(COLOR_EVENT, MSG_COLOR_EVENT);

ColorDefinition *ColorAlertBackground =
	new ColorDefinition(COLOR_ALERT_BACKGROUND, MSG_COLOR_ALERT_BACKGROUND);

ColorDefinition *ColorAlertText =
	new ColorDefinition(COLOR_ALERT_TEXT, MSG_COLOR_ALERT_TEXT);

ColorDefinition *ColorBlink =
	new ColorDefinition(COLOR_BLINK, MSG_COLOR_BLINK);

ColorDefinition *ColorParameterName =
	new ColorDefinition(COLOR_PARAM_NAME, MSG_COLOR_PARAM_NAME);

ColorDefinition *ColorParameterValue =
	new ColorDefinition(COLOR_PARAM_VALUE, MSG_COLOR_PARAM_VALUE);

ColorDefinition *ColorGroup1 =
	new ColorDefinition(COLOR_GROUP1, MSG_COLOR_GROUP1);

ColorDefinition *ColorGroup2 =
	new ColorDefinition(COLOR_GROUP2, MSG_COLOR_GROUP2);

ColorDefinition *ColorGroup3 =
	new ColorDefinition(COLOR_GROUP3, MSG_COLOR_GROUP3);

ColorDefinition *ColorGroup4 =
	new ColorDefinition(COLOR_GROUP4, MSG_COLOR_GROUP4);

ColorDefinition *ColorTickCycle =
	new ColorDefinition(COLOR_TICK_CYCLE, MSG_COLOR_TICK_CYCLE);

ColorDefinition *ColorTickSubcycle =
	new ColorDefinition(COLOR_TICK_SUBCYCLE, MSG_COLOR_TICK_SUBCYCLE);

ColorDefinition *ColorTickCue =
	new ColorDefinition(COLOR_TICK_CUE, MSG_COLOR_TICK_CUE);

ColorDefinition *ColorWindow =
	new ColorDefinition(COLOR_LOOP_WINDOW, MSG_COLOR_WINDOW);

PUBLIC ColorDefinition *ColorDefinitions[] = {
	ColorBackground,
	ColorButton,
	ColorButtonText,
	ColorBar,
	ColorActiveBar,
	ColorCheckpointBar,
	ColorMeter,
	ColorlowSlowMeter,
	ColorRecordMeter,
	ColorMuteMeter,
	ColorEvent,
	ColorAlertBackground,
	ColorAlertText,
	ColorBlink,
	ColorParameterName,
	ColorParameterValue,
	ColorGroup1,
	ColorGroup2,
	ColorGroup3,
	ColorGroup4,
	ColorTickCycle,
	ColorTickSubcycle,
	ColorTickCue,
	ColorWindow,
	NULL};

/****************************************************************************
 *                                                                          *
 *   							   TRACKER                                  *
 *                                                                          *
 ****************************************************************************/
//
// Classes that display a dynamically updated value.
// Trackers are given a TrackerSource object which supplies the
// value to track.  They may also be given a SimpleTimer to which
// they will register as listeners for dynamic updates.
//
// After some experimentation, I'm not using Trackers with
// TrackerSources that keep their own time.  Instead, Mobius
// will have a timer and call the various update methods on the
// trackers directly.  This is to reduce the number of holes
// we have to punch through to get to Track state.  See
// MobiusState for more.
//

PUBLIC Tracker::Tracker()
{
	initTracker(NULL, NULL, TRACKER_INT);
}

PUBLIC Tracker::Tracker(int type)
{
	initTracker(NULL, NULL, type);
}

PUBLIC Tracker::Tracker(TrackerSource *src, SimpleTimer *t, int type)
{
	initTracker(src, t, type);
	update();
}

PUBLIC void Tracker::initTracker(TrackerSource *src, SimpleTimer *timer,
								 int type)
{
	mClassName = "Tracker";
	mSource = src;
	mTimer = NULL;
	mType = type;
	mDivisor = 1;
	mMaxChars = 0;
	mValues = NULL;
	strcpy(mValue, "");
	setTimer(timer);

	// this is used by several things, but only ModeDisplay uses text,
	// would have to have an overload for this if we needed different ones
	mFont = GlobalFontConfig->intern("mode", 20);

	// since this isn't a SpaceComponent we have to initialize it
	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND));
	setForeground(GlobalPalette->getColor(COLOR_BUTTON));
}

PUBLIC Tracker::~Tracker()
{
	delete mValues;
}

PUBLIC void Tracker::setSource(TrackerSource *s)
{
	mSource = s;
}

PUBLIC void Tracker::setTimer(SimpleTimer *t)
{
	if (mTimer != NULL)
		mTimer->removeActionListener(this);
	mTimer = t;
	if (mTimer != NULL)
		mTimer->addActionListener(this);
}

PUBLIC void Tracker::setType(int type)
{
	mType = type;
}

PUBLIC void Tracker::setFont(Font *font)
{
	mFont = font;
}

PUBLIC void Tracker::setDivisor(int i)
{
	mDivisor = i;
}

PUBLIC void Tracker::setMaxChars(int i)
{
	mMaxChars = i;
}

PUBLIC void Tracker::setValues(StringList *values)
{
	delete mValues;
	mValues = values;
}

PUBLIC void Tracker::setValue(const char *s)
{
	update(s);
}

PUBLIC void Tracker::setValue(int i)
{
	char buf[32];
	if (mDivisor > 1)
		i = i / mDivisor;
	sprintf(buf, "%d", i);
	update(buf);
}

PUBLIC void Tracker::actionPerformed(void *src)
{
	update();
}

PUBLIC void Tracker::update()
{
	if (mSource != NULL)
	{
		char current[MAX_TRACKED_LENGTH];

		if (mType == TRACKER_STRING)
		{
			const char *s = mSource->getTrackedString(this);
			strcpy(current, s);
		}
		else
		{
			long i = mSource->getTrackedInt(this);
			if (mDivisor > 1)
				i = i / mDivisor;
			sprintf(current, "%ld", i);
		}

		update(current);
	}
}

PUBLIC void Tracker::update(const char *current)
{
	if (current == NULL)
		current = "";

	if (strcmp(current, mValue))
	{
		strcpy(mValue, current);
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void Tracker::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		Bounds b;
		getPaintBounds(&b);

		// first erase the last value
		// !! need to have configurable colors
		g->setColor(getBackground());
		g->fillRect(b.x, b.y, b.width, b.height);

		// then draw the new value
		g->setColor(getForeground());
		g->setBackgroundColor(getBackground());
		g->setFont(mFont);

		TextMetrics *tm = g->getTextMetrics();
		g->drawString(mValue, b.x, b.y + tm->getAscent());
	}
}

PUBLIC Dimension *Tracker::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		if (mValues == NULL)
		{
			if (mMaxChars > 0)
			{
				Dimension em;
				w->getTextSize("M", mFont, &em);
				mPreferred->width = (em.width * mMaxChars);
				mPreferred->height = em.height; // !! need accurate descender
			}
			else
			{
				// have to guess based on one value
				w->getTextSize(mValue, mFont, mPreferred);
			}
		}
		else
		{
			Dimension d;
			for (int i = 0; i < mValues->size(); i++)
			{
				const char *name = mValues->getString(i);

				w->getTextSize(name, mFont, &d);
				// printf("%s %d %d\n", name, d.width, d.height);

				if (d.width > mPreferred->width)
					mPreferred->width = d.width;

				if (d.height > mPreferred->height)
					mPreferred->height = d.height;
			}
		}
	}
	return mPreferred;
}

/****************************************************************************
 *                                                                          *
 *   								BEATER                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Beater::Beater()
{
	init();
}

PUBLIC Beater::Beater(const char *label)
{
	init();
	setLabel(label);
}

PUBLIC Beater::Beater(const char *label, SimpleTimer *t)
{
	init();
	setLabel(label);
	setTimer(t);
}

PUBLIC Beater::Beater(SimpleTimer *t)
{
	init();
	setTimer(t);
}

PUBLIC void Beater::init()
{
	initTracker(NULL, NULL, TRACKER_INT);
	mClassName = "Beater";
	mDiameter = 40; // #012
	// mDiameter = 20;  //#012
	mLabel = NULL;
	// mLabel = "Beater!"; //#012
	mDecay = BEAT_DECAY;
	mDecayCounter = 0;
	mBeat = 0;
	mBeatCounter = 0;
	mBeatColor = GlobalPalette->getColor(COLOR_BLINK);
}

PUBLIC Beater::~Beater()
{
}

PUBLIC void Beater::setLabel(const char *name)
{
	delete mLabel;
	mLabel = CopyString(name);
}

PUBLIC void Beater::setDecay(int i)
{
	mDecay = i;
}

/**
 * If you set a beat, it will beat by itself.
 * Normally you don't want this.
 */
PUBLIC void Beater::setBeat(int i)
{
	mBeat = i;
}

// #012 Expose diameter
void Beater::setDiameter(int i)
{
	mDiameter = i;
}

PUBLIC void Beater::actionPerformed(void *src)
{
	if (mTimer != NULL)
		beat(mTimer->getDelay());
}

PUBLIC void Beater::beat()
{
	beat(10);
}

PUBLIC void Beater::beat(int ticks)
{
	// only if we're an auto-beater
	if (mBeat > 0)
	{
		if (mBeatCounter > 0)
			mBeatCounter -= ticks;

		if (mBeatCounter <= 0)
		{
			mBeatCounter = mBeat;
			mDecayCounter = mDecay;
			if (isEnabled())
				invalidate();
		}
	}

	if (mDecayCounter > 0)
	{
		mDecayCounter -= ticks;
		if (mDecayCounter <= 0)
		{
			// turn it off
			mDecayCounter = 0;
			if (isEnabled())
				invalidate();
		}
	}
}

PUBLIC void Beater::beatOn()
{
	bool refresh = (mDecayCounter == 0);
	mDecayCounter = mDecay;
	if (refresh && isEnabled())
	{
		invalidate();
	}
}

PUBLIC void Beater::beatOff()
{
	bool refresh = (mDecayCounter > 0);
	mDecayCounter = 0;
	if (refresh && isEnabled())
	{
		invalidate();
	}
}

// #define PADDING_BEATER 4
#define PADDING_BEATER 10 // #012

PUBLIC Dimension *Beater::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();

		if (mLabel != NULL)
		{
			mPreferred = new Dimension();
			w->getTextSize(mLabel, mFont, mPreferred);

			// a little padding
			mPreferred->width += PADDING_BEATER;
		}

		mPreferred->width += mDiameter; // #012

		if (mPreferred->height < mDiameter)
			mPreferred->height = mDiameter;
	}
	return mPreferred;
}

/**
 * We originally used a drw() method with an includeLabel flag that could
 * be used to  control refresh of the label, the intent being that if only the
 * beat circle changes we don't have to refresh the label.   Now that
 * this has to be handled indirectly with invalidate() we lose that
 * state so just refresh the whole thing.  If this were a problem we'd
 * have to split up the label into another component.
 */

PUBLIC void Beater::paint(Graphics *g)
{
	bool includeLabel = true;

	if (isEnabled())
	{
		tracePaint();
		Window *w = getWindow();
		if (w != NULL)
		{
			Graphics *g = w->getGraphics();
			if (g != NULL)
			{
				Bounds b;
				getPaintBounds(&b); //<--- check this...

				// For some reason Windows erases the background but
				// Mac doesn't so just drawing the oval doesn't make the
				// filled center go away.
				g->setColor(getBackground());

				// g->setColor(mBeatColor); //#012 Cas
				// g->fillRect(b.x, b.y, b.width, b.height);  //#012 Cas | removed | not correct boud, investigate?
				// In pratica il bound sembra non essere correto.

				// need foreground and background for the label, should
				// we extend Label?

				g->setColor(getForeground());
				g->setBackgroundColor(getBackground());

				if (mLabel != NULL && includeLabel)
				{
					g->setFont(mFont);
					TextMetrics *tm = g->getTextMetrics();
					g->drawString(mLabel, b.x, b.y + tm->getAscent());
				}

				// circle is after the text
				// int left = (b.x + b.width - 1) - mDiameter;   //<--- left is outside of the Bound?zzzzzz

				// Ok Chiaro pos.X + tutta la lunghezza - diametro (perché poteva esserci la label)
				int left = (b.x + b.width - 1) - mDiameter; // #012 Why -mDiameter??
				g->drawOval(left, b.y, mDiameter, mDiameter);

				if (mDecayCounter > 0)
				{
					// Trace(2, "Beat on\n");
					g->setColor(mBeatColor);
					g->fillOval(left + 2, b.y + 2, mDiameter - 4, mDiameter - 4);
				}
				else
				{
					// Walkaround error erases background...  #012 Cas... investigate on code above!
					// Trace(2, "Beat on\n");
					g->setColor(getBackground());
					g->fillOval(left + 2, b.y + 2, mDiameter - 4, mDiameter - 4);
				}

				// else
				// Trace(2, "Beat off\n");
			}
		}
	}
}

PUBLIC void Beater::dumpLocal(int indent)
{
	dumpType(indent, "Beater");
}

/****************************************************************************
 *                                                                          *
 *   							 THERMOMETER                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC Thermometer::Thermometer()
{
	init();
}

PUBLIC Thermometer::Thermometer(TrackerSource *src, SimpleTimer *timer)
{
	init();
	setSource(src);
	setTimer(timer);
}

PUBLIC void Thermometer::init()
{
	initTracker(NULL, NULL, TRACKER_INT);
	mClassName = "Thermometer";
	mRange = 0;
	mValue = 0;
	mMeterColor = GlobalPalette->getColor(COLOR_METER);
}

PUBLIC Thermometer::~Thermometer()
{
}

PUBLIC void Thermometer::setMeterColor(Color *c)
{
	if (c != mMeterColor)
	{
		mMeterColor = c;
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void Thermometer::setRange(int i)
{
	if (mRange != i)
	{
		mRange = i;
		if (mValue > mRange)
			setValue(0);
		// else
		//  invalidate();  ????
	}
}

PUBLIC int Thermometer::getRange()
{
	return mRange;
}

PUBLIC void Thermometer::setValue(int i)
{
	if (mValue != i && i >= 0 && i <= mRange)
	{
		mValue = i;
		if (isEnabled())
			invalidate();
	}
}

PUBLIC int Thermometer::getValue()
{
	return mValue;
}

PUBLIC void Thermometer::actionPerformed(void *src)
{
	if (mSource != NULL)
	{
		// sigh, we'll let this call set our range as a side effect
		// !! we're not using timers any more, rip this shit out
		int value = mSource->getTrackedInt(this);
		if (mRange > 0 && value != mValue)
			setValue(value);
	}
}

PUBLIC Dimension *Thermometer::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		mPreferred->width = 200;
		mPreferred->height = 20;
	}
	return mPreferred;
}

PUBLIC void Thermometer::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		Bounds b;
		getPaintBounds(&b);

		g->setColor(getForeground());

		g->drawRect(b.x, b.y, b.width, b.height);

		b.x += 2;
		b.y += 2;
		b.width -= 4;
		b.height -= 4;

		int level = 0;
		if (mRange > 0)
			level = (int)(((float)(b.width) / (float)mRange) * mValue);

		if (level > 0)
		{
			Color *c = (mMeterColor != NULL) ? mMeterColor : Color::White;
			g->setColor(c);
			g->fillRect(b.x, b.y, level, b.height);
		}

		g->setColor(getBackground());
		g->fillRect(b.x + level, b.y, b.width - level, b.height);
	}
}

PUBLIC void Thermometer::dumpLocal(int indent)
{
	dumpType(indent, "Thermometer");
}

/****************************************************************************
 *                                                                          *
 *                              SPACE COMPONENT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC SpaceComponent::SpaceComponent()
{
	mElementType = NULL;

	// the default colors for all space components
	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setForeground(GlobalPalette->getColor(COLOR_BUTTON, Color::Red));
}

PUBLIC SpaceComponent::~SpaceComponent()
{
}

/**
 * Utility for the subclasses to set both the type and the Component
 * name.  We need the type for display name and possibly other things,
 * we need to set mName to save/restore locations.
 */
void SpaceComponent::setType(DisplayElement *type)
{
	if (type != NULL)
	{
		mElementType = type;
		setName(type->getName());
	}
}

PUBLIC const char *SpaceComponent::getDragName()
{
	return (mElementType != NULL) ? mElementType->getDisplayName() : "???";
}

/**
 * Now that we have to erase through invalidate() we have to set
 * a kludgey flag to get the paint method to erase rather than
 * just ignore it because it is disabled.
 *
 * Think about when this is used, components get enabled/disabled
 * on major boundaries like preferences dialogs, we should just
 * repaint the entire Space?
 * This seems to only happen in UI::updateDisplayConfig so assume
 * that we'll be erasing all of Space.
 *
 * !! KLUDGE: On the Mac we don't do invalidations synchronously,
 * a request is posted to a queue that is then handled in the
 * main window event loop.  For space components that can change
 * their contents like TrackStrip and ParameterDisplay, it is important
 * that we not post invalidation events for child components that are
 * going to be deleted.  This happens in updateDisplayConfiguration
 * because we first set the location and enabled status of the primary
 * components, then we rebuid them.  We only want to invalidate the root
 * component that won't be deleted.  This is done by overloading setEnabled()
 * and calling Component::setEnabled rather than Container::setEnabled
 * which will recurse on children.
 */
PUBLIC void SpaceComponent::setEnabled(bool b)
{
	// just the root, not the children
	// Container::setEnabled(b);
	Component::setEnabled(b);

	if (b)
		invalidate();

	// else
	// erase();
}

PUBLIC void SpaceComponent::erase()
{
	Window *w = getWindow();
	if (w != NULL)
	{
		Graphics *g = w->getGraphics();
		if (g != NULL)
		{
			erase(g);
		}
	}
}

PUBLIC void SpaceComponent::erase(Graphics *g)
{
	if (g != NULL)
	{
		Bounds b;
		getPaintBounds(&b);
		erase(g, &b);
	}
}

PUBLIC void SpaceComponent::erase(Graphics *g, Bounds *b)
{
	if (g != NULL)
	{
		g->setColor(getBackground());
		g->fillRect(b->x, b->y, b->width, b->height);
	}
}

PUBLIC void SpaceComponent::drawMoveBorder(Graphics *g)
{
	if (Space::isDragging() && g != NULL)
	{
		Bounds b;
		getPaintBounds(&b);

		// ignore the component foreground (I'm talking to
		// you ParameterDisplay) and always use the default space colors
		g->setBackgroundColor(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
		g->setColor(GlobalPalette->getColor(COLOR_BUTTON, Color::Red));
		g->drawRect(b.x, b.y, b.width, b.height);

		const char *name = getDragName();
		if (name != NULL)
		{

			// printf("%s: %d %d %d %d\n", name, b.x, b.y, b.width, b.height);

			Font *font = GlobalFontConfig->intern("drag", 10);
			g->setFont(font);
			TextMetrics *tm = g->getTextMetrics();
			int top = b.x + 3;
			g->drawString(name, b.x + 3, b.y + 3 + tm->getAscent());
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                   SPACE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * This is a global which we normally like to avoid so we can reliably
 * support multiple plugins.  This one isn't too bad though since
 * it only effects the top window during mouse drag, even if this did
 * leak to other plugins it just temporarily effects their display.
 *
 * This is relatively difficult to encspaulate.  It could be a field
 * on Space but then all the compents would need to walk up the hierarchy
 * to see it rather than just reffing the global.
 */
PUBLIC bool Space::Dragging = false;

/**
 * This must be a lightweight panel for Windows which doesn't like to
 * draw SpaceComponents in a static panel.  Probably a bug in qwin,
 * not handling the parent HDC in the Graphics properly.
 * On Mac this will quietly become a heavyweight panel because
 * that's a requirement to get mouse tracking.
 */
PUBLIC Space::Space()
{
	init();
	mClassName = "Space";
	mDragable = NULL;
	addMouseListener(this);
	addMouseMotionListener(this);
	addKeyListener(this);

	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setLayout(NULL);
}

PUBLIC Space::~Space()
{
}

PUBLIC void Space::setDragging(bool b)
{
	Dragging = b;
	invalidate();
}

PUBLIC bool Space::isDragging()
{
	return Dragging;
}

/**
 * Draggable paints, it seems to be okay to paint
 * as long as we're in the mouse event handler thread.
 */
PUBLIC void Space::mousePressed(MouseEvent *e)
{
	// printf("Space::mousePressed %d %d\n", e->getX(), e->getY());

	if (Dragging)
	{

		// don't propagate this to child components
		e->setClaimed(true);

		if (mDragable == NULL)
		{

			/*
			  mDragable = new DragRegion(e->getX(), e->getY());
			  mDragable = new DragBox(e->getX(), e->getY(),
			  e->getX() - 10,
			  e->getY() - 10,
			  e->getX() + 10,
			  e->getY() + 10);
			*/

			Component *c = findComponent(e->getX(), e->getY());
			if (c != NULL)
			{
				mDragable = new DragComponent(this, e->getX(), e->getY(), c);
			}
		}
		else
		{
			// must have missed an event
			mDragable->finish();
			delete mDragable;
			mDragable = NULL;
			invalidate();
		}
	}
}

/**
 * Formerly we would listen for the shift key,
 * now you have to bind a trigger to the SpaceDrag UI function.
 */
PUBLIC void Space::keyPressed(KeyEvent *e)
{
	/*
		// 16 is left/right shift
		int code = e->getKeyCode();
		if (code == 16 && !isDragging()) {
			setDragging(true);
		}
	*/
}

PUBLIC void Space::keyReleased(KeyEvent *e)
{
	/*
		int code = e->getKeyCode();
		if (code == 16 && isDragging()) {
			setDragging(false);
		}
	*/
}

PUBLIC Component *Space::findComponent(int x, int y)
{
	Component *found = NULL;

	for (Component *c = getComponents(); c != NULL; c = c->getNext())
	{
		int left = c->getX();
		int top = c->getY();
		int right = left + c->getWidth();
		int bottom = top + c->getHeight();

		if (left <= x && right > x &&
			top <= y && bottom > y)
		{

			// ignore disabled components
			if (c->isEnabled())
			{
				found = c;
				break;
			}
		}
	}
	return found;
}

/**
 * Draggable paints, it seems to be okay to paint
 * as long as we're in the mouse event handler thread.
 */
PUBLIC void Space::mouseReleased(MouseEvent *e)
{
	// printf("Mouse released %d %d\n", e->getX(), e->getY());
	if (mDragable != NULL)
	{
		mDragable->finish();
		delete mDragable;
		mDragable = NULL;
	}
}

/**
 * Draggable paints, it seems to be okay to paint
 * as long as we're in the mouse event handler thread.
 */
PUBLIC void Space::mouseDragged(MouseEvent *e)
{
	// trace("Mouse moved %d %d\n", e->getX(), e->getY());
	if (mDragable != NULL)
	{
		mDragable->trackMouse(e->getX(), e->getY());
		invalidate();
	}
}

/****************************************************************************
 *                                                                          *
 *                                  BEATERS                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Beaters::Beaters(SimpleTimer *t)
{
	mClassName = "Beaters";
	setType(BeatersElement);

	setLayout(new HorizontalLayout());

	mSubCycle = new Beater(t);
	add(mSubCycle);

	mCycle = new Beater(t);
	add(mCycle);

	mLoop = new Beater(t);
	add(mLoop);
}

PUBLIC Beaters::~Beaters()
{
}

PUBLIC void Beaters::setBeaterDiameter(int i) // #012 Expose diameter
{
	mSubCycle->setDiameter(i);
	mCycle->setDiameter(i);
	mLoop->setDiameter(i);
}

PUBLIC void Beaters::reset()
{
	mLoop->beatOff();
	mCycle->beatOff();
	mSubCycle->beatOff();
}

PUBLIC void Beaters::update(MobiusState *mstate)
{
	if (isEnabled() && !Space::isDragging())
	{

		LoopState *lstate = mstate->track->loop;
		if (lstate->beatLoop)
			mLoop->beatOn();

		if (lstate->beatCycle)
			mCycle->beatOn();

		if (lstate->beatSubCycle)
			mSubCycle->beatOn();
	}
}

PUBLIC void Beaters::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
			Container::paint(g);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  BAR GRAPH                                 *
 *                                                                          *
 ****************************************************************************/

#define DEFAULT_BAR_HORIZ_WIDTH 10
#define DEFAULT_BAR_HORIZ_HEIGHT 30
#define DEFAULT_BAR_HORIZ_GAP 4

#define DEFAULT_BAR_VERT_WIDTH 30
#define DEFAULT_BAR_VERT_HEIGHT 10
#define DEFAULT_BAR_VERT_GAP 4

PUBLIC BarGraph::BarGraph()
{
	mClassName = "BarGraph";
	mInitialBars = 0;
	mValue = 0;
	mMaxValue = 0;
	mIncrementalUpdate = false;
	mNewValue = 0;
	mNewMaxValue = 0;
	mVertical = false;
	mBarWidth = DEFAULT_BAR_HORIZ_WIDTH;
	mBarHeight = DEFAULT_BAR_HORIZ_HEIGHT;
	mBarGap = DEFAULT_BAR_HORIZ_GAP;

	mBarColor = GlobalPalette->getColor(COLOR_BAR, Color::Blue);
	mActiveBarColor = GlobalPalette->getColor(COLOR_ACTIVE_BAR, Color::White);
	mSpecialColor = GlobalPalette->getColor(COLOR_CHECKPOINT_BAR, Color::White);
}

PUBLIC BarGraph::~BarGraph()
{
}

PUBLIC void BarGraph::setInitialBars(int i)
{
	mInitialBars = i;
}

PUBLIC void BarGraph::setValue(int i)
{
	mValue = i;
}

PUBLIC void BarGraph::setMaxValue(int i)
{
	mMaxValue = i;
}

PUBLIC void BarGraph::setVertical(bool b)
{
	mVertical = true;
}

PUBLIC void BarGraph::setBarWidth(int i)
{
	mBarWidth = i;
}

PUBLIC void BarGraph::setBarHeight(int i)
{
	mBarHeight = i;
}

PUBLIC void BarGraph::setBarGap(int i)
{
	mBarGap = i;
}

/**
 * Virtual method overloaded by subclasses that support "special" markers.
 * !! Don't really like this, better just to ask the subclass
 * what color to use.
 */
PUBLIC bool BarGraph::isSpecial(int index)
{
	return false;
}

PUBLIC void BarGraph::update(int newValue, int newMax, bool force)
{
	// !! hmm, passing paint info could be risky if we have two
	// update events close together and more than one invalidate
	// event is set, a general problem we have elsewhere

	if (newValue < 0)
		newValue = 0;
	if (newMax < 0)
		newMax = 0;

	if (!isEnabled())
	{
		// avoid the overhead of an event if we know this is disabled,
		// since we normally transition the new value in paint() have to
		// do it here
		mValue = newValue;
		mMaxValue = newMax;
	}
	else if (force || newValue != mValue || newMax != mMaxValue)
	{
		mIncrementalUpdate = !force;
		mNewValue = newValue;
		mNewMaxValue = newMax;
		invalidate();
	}
}

PUBLIC Dimension *BarGraph::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();

		// since this is variable, just use a basic default size and
		// assume it will be moved around
		int bars = mInitialBars;
		if (bars == 0)
			bars = 1;

		if (mVertical)
		{
			mPreferred->width = mBarWidth;
			mPreferred->height = getRequiredSize(bars);
		}
		else
		{
			mPreferred->width = getRequiredSize(bars);
			mPreferred->height = mBarHeight;
		}
	}
	return mPreferred;
}

/**
 * Painting is a little complex because we try to be smart about
 * incremental additions and removals of segments so we don't have
 * to redraw the entire graph.
 *
 * The update() method which is called from the monitor timer will
 * set the mIncrementalUpdate flag to true and the mNewValue and
 * mNewMaxValue to the new values to apply.  If mIncrementalUpdate
 * is false, then the two new values are undefined and we do a full repaint.
 *
 */
PUBLIC void BarGraph::paint(Graphics *g)
{
	// whatever we do, transition the pending value
	int lastValue = mValue;
	int lastMax = mMaxValue;

	mValue = mNewValue;
	mMaxValue = mNewMaxValue;

	mIncrementalUpdate = false;
	mNewValue = 0;
	mNewMaxValue = 0;

	if (isEnabled())
	{
		tracePaint();

		if (Space::isDragging())
		{
			drawMoveBorder(g);
		}
		else if (!mIncrementalUpdate)
		{
			Bounds b;
			getPaintBounds(&b);
			// note that we ignore the initial width and erase what
			// we had drawn last time
			if (mVertical)
				b.height = getRequiredSize(lastMax);
			else
				b.width = getRequiredSize(lastMax);
			erase(g, &b);
			for (int i = 0; i < mMaxValue; i++)
				paintOne(g, &b, i);
		}
		else
		{
			Bounds b;
			getPaintBounds(&b);

			if (lastMax > mMaxValue)
			{
				// have to erase some
				for (int i = mMaxValue; i < lastMax; i++)
					paintOne(g, &b, i);
			}

			// add new bars if max raised
			for (int i = lastMax; i < mMaxValue; i++)
				paintOne(g, &b, i);

			if (lastValue != mValue)
			{
				// move active bar if not already painted above
				if (lastValue < lastMax)
					paintOne(g, &b, lastValue);

				if (mValue < lastMax)
					paintOne(g, &b, mValue);
			}
		}

		// recalculate bonunds as we overflow, this isn't absolutely
		// necessary but makes the drag rectangle more accurate
		if (mVertical)
		{
			int size = getRequiredSize(mMaxValue);
			if (size > mBounds.height)
				mBounds.height = size;
		}
		else
		{
			int size = getRequiredSize(mMaxValue);
			if (size > mBounds.width)
				mBounds.width = size;
		}
	}
}

/**
 * Calculate the required length or width for a given number
 * of bars.  Don't add an extra gap on the end.
 */
PRIVATE int BarGraph::getRequiredSize(int max)
{
	int size = 0;

	if (mVertical)
	{
		size = (max * mBarHeight);
		if (max > 0)
			size += ((max - 1) * mBarGap);
	}
	else
	{
		size = (max * mBarWidth);
		if (max > 0)
			size += ((max - 1) * mBarGap);
	}

	return size;
}

PRIVATE void BarGraph::paintOne(Graphics *g, Bounds *b, int offset)
{

	if (offset < 0)
	{
		// should only be seen when turning off the previous selection
		// when there was no selection
	}
	else if (offset >= mMaxValue)
	{
		// must be removing some from the end
		paintOne(g, b, mBackground, NULL, offset, 0);
	}
	else if (isSpecial(offset))
	{
		if (offset != mValue)
			paintOne(g, b, mSpecialColor, NULL, offset, 0);
		else
		{
			// both special and active, use both colors
			paintOne(g, b, mSpecialColor, mActiveBarColor, offset, 0);
		}
	}
	else if (offset == mValue)
	{
		// currently active value
		paintOne(g, b, mActiveBarColor, NULL, offset, 0);
	}
	else
	{
		paintOne(g, b, mBarColor, NULL, offset, 0);
	}
}

PRIVATE void BarGraph::paintOne(Graphics *g, Bounds *b,
								Color *c, Color *border,
								int offset, int length)
{
	// now that we have the concept of a border, perhaps this should
	// always be used for the active bar?

	int left, top, width, height;
	if (mVertical)
	{
		left = b->x;
		top = b->y + (offset * (mBarHeight + mBarGap));
		width = b->width;
		height = mBarHeight;
	}
	else
	{
		left = b->x + (offset * (mBarWidth + mBarGap));
		top = b->y;
		width = mBarWidth;
		height = b->height;
	}

	if (border != NULL)
	{
		g->setColor(border);
		g->fillRect(left, top, width, height);
		left += 2;
		top += 2;
		width -= 4;
		height -= 4;
	}

	g->setColor(c);
	g->fillRect(left, top, width, height);
}

/****************************************************************************
 *                                                                          *
 *                                 LOOP BARS                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC LoopList::LoopList()
{
	mClassName = "LoopList";
	setType(LoopBarsElement);

	setVertical(true);
	setBarWidth(DEFAULT_BAR_VERT_WIDTH);
	setBarHeight(DEFAULT_BAR_VERT_HEIGHT);
	setBarGap(DEFAULT_BAR_VERT_GAP);
}

PUBLIC LoopList::~LoopList()
{
}

PUBLIC void LoopList::update(MobiusState *mstate)
{
	// maintain some state of the previously rendered loops
	// and call paint() if changed

	TrackState *tstate = mstate->track;
	LoopState *lstate = tstate->loop;

	int newValue = lstate->number - 1;
	int newMax = tstate->loops;

	BarGraph::update(newValue, newMax, false);
}

/****************************************************************************
 *                                                                          *
 *                                 LAYER BARS                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC LayerList::LayerList()
{
	mClassName = "LayerList";
	setType(LayerBarsElement);

	mState.init();
	mFont = GlobalFontConfig->intern("layerList", 8);
}

PUBLIC LayerList::~LayerList()
{
}

/**
 * Called by BarGraph to determine if the bar at an index is
 * considered special.  Must have the transient MobiusState set.
 *
 * This is obscure because we've got two lists (normal layers and redo
 * layers) that we're trying to display as one.  The layer list is in
 * reverse order so we have to reflect the index into the mState layer list.
 * The redo list is in redo order.  With this new structure, it would
 * be easier if BarGraph just asked us what color to draw rather than
 * having it maintain the "active" bar?
 *
 */
PUBLIC bool LayerList::isSpecial(int index)
{
	bool special = false;
	if (index < mState.layerCount)
	{
		// reflect
		index = (mState.layerCount - 1) - index;
		special = mState.layers[index].checkpoint;
	}
	else
	{
		// an index into the redo layers, these don't need reflection
		int redoIndex = index - mState.layerCount;
		special = mState.redoLayers[redoIndex].checkpoint;
	}

	return special;
}

PUBLIC void LayerList::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;
	LoopState *lstate = tstate->loop;

	// if the lost count changes we need to shift the display
	bool force = (mState.layerCount != lstate->layerCount ||
				  mState.lostLayers != lstate->lostLayers ||
				  mState.redoCount != lstate->redoCount ||
				  mState.lostRedo != lstate->lostRedo);

	// Sigh to detect checkpoint toggles, have to look at all of them,
	// would be nice to have a flag set in the state but since we're
	// in the UI thread it probably doesn't matter.  Don't have to do this
	// in the redo list.
	// !! Yes we do, if we're switching tracks.
	int i;
	for (i = 0; i < mState.layerCount && !force; i++)
	{
		if (mState.layers[i].checkpoint != lstate->layers[i].checkpoint)
			force = true;
	}

	for (i = 0; i < mState.redoCount && !force; i++)
	{
		if (mState.redoLayers[i].checkpoint !=
			lstate->redoLayers[i].checkpoint)
			force = true;
	}

	// set this before the refresh for the isSpecial callback
	mState = *lstate;

	// TODO: display the lost layer count on the left and the
	// lost redo count on the right.  May want to enforce our own
	// maximums and center the active layer within that range.

	// the index to the active layer to BarGraph
	int current = lstate->layerCount - 1;

	// the total number of bars to display
	int total = lstate->layerCount + lstate->redoCount;

	BarGraph::update(current, total, force);
}

/**
 * Ugh, we want to add our little "lost counters" but we to have
 * overwrite the painting job done by BarGraph.
 * Need better graphics!! this is still leaving little turds.
 */
PUBLIC void LayerList::paint(Graphics *g)
{
	bool force = !mIncrementalUpdate;

	BarGraph::paint(g);

	if (isEnabled() && !Space::isDragging() && force)
	{
		Window *w = getWindow();
		if (w != NULL)
		{
			Graphics *g = w->getGraphics();
			if (g != NULL)
			{
				char buffer[128];
				Bounds b;
				getPaintBounds(&b);

				if (mState.lostLayers > 0)
				{
					sprintf(buffer, "%d", mState.lostLayers);
					TextMetrics *tm = g->getTextMetrics();
					g->setFont(mFont);
					g->setColor(Color::White);
					g->setBackgroundColor(Color::Red);
					// getting little turds on the left, why??
					int left = b.x + 1;
					g->drawString(buffer, left, b.y + tm->getAscent());
				}

				if (mState.lostRedo > 0)
				{
					sprintf(buffer, "%d", mState.lostRedo);
					TextMetrics *tm = g->getTextMetrics();
					g->setFont(mFont);
					g->setColor(Color::White);
					g->setBackgroundColor(Color::Red);
					g->drawString(buffer, b.x + b.width - mBarWidth,
								  b.y + tm->getAscent());
				}
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                  DISPLAY                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC EDPDisplay::EDPDisplay(int sampleRate)
{
	mClassName = "EDPDisplay";
	setType(CounterElement);

	mLoop = 0;
	mFrame = 0;
	mCycle = 0;
	mCycles = 0;
	mNextLoop = 0;
	mFontOffset = 0;
	mSampleRate = sampleRate;

	mFont = GlobalFontConfig->intern("loopCounterSmall", 30);
	mFont2 = GlobalFontConfig->intern("loopCounterBig", 40);

	for (int i = 0; i < EDP_DISPLAY_UNITS; i++)
	{
		mLeft[i] = 0;
		mTop[i] = 0;
		mValues[i] = -1;
	}
}

PUBLIC EDPDisplay::~EDPDisplay()
{
}

#define DISPLAY_EXTRA_SPACING 40 // #009

/**
 * xx yy.y zz/cc
 * !! need to support 3 digit seconds
 */
PUBLIC Dimension *EDPDisplay::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		Dimension d1, d2;
		mPreferred = new Dimension();

		// 0 width is 20 & 27, M width is 29 & 39
		w->getTextSize("0", mFont, &d1);
		// w->getTextSize("0", mFont2, &d2);
		w->getTextSize("0", mFont2, &d2);
		d2.width += DISPLAY_EXTRA_SPACING; // overlap issue test Cas  //#009  TODO - UiConfig

		// printf("Font1 width %d Font2 width %d\n", d1.width, d2.width);

		mPreferred->height = d2.height;

		mLeft[0] = 0;						 // loop 1
		mLeft[1] = d1.width;				 // loop 2
		mLeft[2] = (d1.width * 2) + 16;		 // time 1
		mLeft[3] = mLeft[2] + d2.width;		 // time 2
		mLeft[4] = mLeft[3] + d2.width;		 // .
		mLeft[5] = mLeft[4] + 16;			 // time 3
		mLeft[6] = mLeft[5] + d1.width + 16; // cycle 1
		mLeft[7] = mLeft[6] + d1.width;		 // cycle 2
		mLeft[8] = mLeft[7] + d1.width;		 // /
		mLeft[9] = mLeft[8] + d1.width;		 // cycles 1
		mLeft[10] = mLeft[9] + d1.width;	 // cycles 2

		mPreferred->width = mLeft[10] + d1.width;

		mFontOffset = d2.height - d1.height;
	}

	return mPreferred;
}

PUBLIC void EDPDisplay::update(MobiusState *mstate)
{
	// don't go into the Lx display if we have a return
	// transition, it will be visible in the event list

	LoopState *s = mstate->track->loop;

	if (mNextLoop > 0 || s->nextLoop > 0)
	{
		mLoop = s->number; // 1 based
		mFrame = s->frame;
		mCycle = s->cycle;
		mCycles = s->cycles;
		mNextLoop = s->nextLoop;
		if (isEnabled())
			invalidate();
	}
	else if (mLoop != s->number || mFrame != s->frame || mCycle != s->cycle ||
			 mCycles != s->cycles)
	{

		mLoop = s->number;
		mFrame = s->frame;
		mCycle = s->cycle;
		mCycles = s->cycles;
		mNextLoop = s->nextLoop;

		if (isEnabled())
			invalidate();
	}
}

PUBLIC void EDPDisplay::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{
			TextMetrics *tm = g->getTextMetrics();
			char buf[128];
			Bounds b;
			getPaintBounds(&b);

			// todo, redraw only the digits that change

			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);

			g->setColor(getForeground());
			g->setBackgroundColor(getBackground());
			g->setFont(mFont);

			int smallTop = b.y + mFontOffset + tm->getAscent();
			int digit = mLoop / 10;
			if (digit > 0)
			{
				sprintf(buf, "%d", digit);
				g->drawString(buf, b.x + mLeft[0], smallTop);
			}
			sprintf(buf, "%d", mLoop % 10);
			g->drawString(buf, b.x + mLeft[1], smallTop);

			g->setFont(mFont2);
			int bigtop = b.y + tm->getAscent();

			if (mNextLoop > 0)
			{
				g->drawString("L", b.x + mLeft[3], bigtop);
				sprintf(buf, "%d", mNextLoop);
				g->drawString(buf, b.x + mLeft[5], bigtop);
			}
			else
			{
				int dseconds = mFrame / (mSampleRate / 10);
				digit = dseconds / 100;
				if (digit > 0)
				{
					sprintf(buf, "%d", digit);
					g->drawString(buf, b.x + mLeft[2], bigtop);
				}

				// for some odd reason the '.' is displayed at the
				// top of the character region on Mac, not sure why
				/*
				sprintf(buf, "%d", (dseconds / 10) % 10);
				g->drawString(buf, b.x + mLeft[3], bigtop);
				g->drawString(".", b.x + mLeft[4], bigtop);
				sprintf(buf, "%d", dseconds % 10);
				g->drawString(buf, b.x + mLeft[5], bigtop);
				*/
				int seconds = (dseconds / 10) % 10;
				int frac = (dseconds % 10);

				// sprintf(buf, "%d . %d", (dseconds / 10) % 10, dseconds % 10);
				sprintf(buf, "%d . %d", seconds, frac);

				g->drawString(buf, b.x + mLeft[3], bigtop);
			}

			g->setFont(mFont);
			digit = mCycle / 10;
			if (digit > 0)
			{
				sprintf(buf, "%d", digit);
				g->drawString(buf, b.x + mLeft[6], smallTop);
			}

			// note that this includes mLeft[8] too
			sprintf(buf, "%d/%d", mCycle % 10, mCycles);
			g->drawString(buf, b.x + mLeft[7], smallTop);
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							 MODE/STATUS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Originally just dropped a Tracker directly into Space, but with the
 * introduction of Space::Move and the borders, we had to wrap the Tracker
 * in a real SpaceComponent.
 *
 */
PUBLIC ModeDisplay::ModeDisplay()
{
	mClassName = "ModeDisplay";
	setType(ModeElement);

	setLayout(new BorderLayout());
	mMode = new Tracker(TRACKER_STRING);
	add(mMode, BORDER_LAYOUT_CENTER);

	// give the tracker an accurate list of values for sizing
	StringList *modes = new StringList();
	for (int i = 0; Modes[i] != NULL; i++)
	{
		MobiusMode *mode = Modes[i];
		modes->add(mode->getDisplayName());
	}
	mMode->setValues(modes);
}

PUBLIC ModeDisplay::~ModeDisplay()
{
}

PUBLIC void ModeDisplay::setValue(const char *value)
{
	mMode->setValue(value);
}

PUBLIC void ModeDisplay::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
			Container::paint(g);
	}
}

PUBLIC void ModeDisplay::update(MobiusState *mstate)
{
	LoopState *lstate = mstate->track->loop;

	if (!Space::isDragging())
	{
		if (mstate->customMode[0] != 0)
			mMode->setValue(mstate->customMode);
		else if (lstate->paused)
		{
			// not a real mode, we're actually in Mute mode
			// !! promote this to a real mode, it makes sense
			mMode->setValue(PauseMode->getDisplayName());
		}
		else if (lstate->mode != RehearseMode)
		{
			mMode->setValue(lstate->mode->getDisplayName());
		}
		else
		{
			// hack, use an alternate mode name when recording,
			// even though this won't actually be set in the loop
			if (lstate->recording)
				mMode->setValue(RehearseRecordMode->getDisplayName());
			else
				mMode->setValue(RehearseMode->getDisplayName());
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                               ACTION BUTTON                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC ActionButton::ActionButton(MobiusInterface *mob, Action *a)
{
	init();
	mMobius = mob;
	mAction = a;

	// these will behave as momentary buttons so make sure this is set
	mAction->triggerMode = TriggerModeMomentary;

	char buffer[1024];
	a->getDisplayName(buffer, sizeof(buffer));
	setText(buffer);

	// do we need this out here or should we just do both up and down
	// and let the action figure it out
	if (a->getTarget() == TargetFunction)
	{
		Function *f = (Function *)a->getTargetObject();
		if (f != NULL)
			setMomentary(f->isSustainable());
	}

	addActionListener(this);
}

PUBLIC ActionButton::~ActionButton()
{
	// allocated by UI, but then we own it
	mAction->setRegistered(false);
	delete mAction;
}

PUBLIC void ActionButton::init()
{
	mClassName = "ActionButton";
	mMobius = NULL;
	mAction = NULL;

	setFont(GlobalFontConfig->intern("button", 14));
	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setForeground(GlobalPalette->getColor(COLOR_BUTTON, Color::Red));
	setTextColor(GlobalPalette->getColor(COLOR_BUTTON_TEXT, Color::White));
}

PUBLIC void ActionButton::actionPerformed(void *o)
{
	// !! the space key will trigger the last active button,
	// need to find a way to detect and disable that

	if (mAction != NULL)
	{
		Action *a = mMobius->cloneAction(mAction);
		a->down = isPushed();
		// there is no value, but Mobius may process binding arguments
		mMobius->doAction(a);
	}
}

/****************************************************************************
 *                                                                          *
 *   								 KNOB                                   *
 *                                                                          *
 ****************************************************************************/

#define KNOB_DEFAULT_DIAMETER 50

/*
 * Circular knob.
 * Probably should be in qwin.
 */
Knob::Knob()
{
	init();
}

Knob::Knob(const char *label)
{
	init();
}

Knob::Knob(const char *label, int max)
{
	init();
	setLabel(label);
	setMaxValue(max);
}

void Knob::init()
{
	mClassName = "Knob";
	debugging = false;
	mLabel = NULL;
	mDiameter = KNOB_DEFAULT_DIAMETER;
	mValue = 0;
	mNoDisplayValue = false;
	mMinValue = 0;
	mMaxValue = 0;
	mDragging = false;
	mDragStartValue = 0;
	mDragOriginX = 0;
	mDragOriginY = 0;
	mDragChanges = 0;
	mFont = GlobalFontConfig->intern("knob", 12);
	mClickIncrement = false;

	addMouseListener(this);
	addMouseMotionListener(this);
}

Knob::~Knob()
{
	delete mLabel;
}

PUBLIC int Knob::getValue()
{
	return mValue;
}

PUBLIC void Knob::setClickIncrement(bool b)
{
	mClickIncrement = b;
}

PUBLIC void Knob::setFont(Font *f)
{
	mFont = f;
}

PUBLIC void Knob::setDiameter(int r)
{
	mDiameter = r;
}

PUBLIC void Knob::setLabel(const char *label)
{
	delete mLabel;
	mLabel = CopyString(label);
}

PUBLIC Dimension *Knob::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{

		// not sure why but on some systems the fonts
		// are larger than my dev machine, I guess it could
		// be differences in screen DPI settings?
		// do a sanity check and make sure the diameter
		// is big enough for a 3 digit number
		Dimension d;
		w->getTextSize("000", mFont, &d);
		int min = d.width + 16;
		if (mDiameter < min)
			mDiameter = min;

		mPreferred = new Dimension(mDiameter, mDiameter);
		if (mLabel != NULL)
		{
			Dimension d;
			w->getTextSize(mLabel, mFont, &d);
			mPreferred->height += (d.height + 2);
			if (d.width > mPreferred->width)
				mPreferred->width = d.width;
		}
	}

	return mPreferred;
}

void Knob::setValue(int i)
{
	if (i < mMinValue)
		mValue = mMinValue;

	else if (i > mMaxValue)
		mValue = mMaxValue;

	else
		mValue = i;
}

void Knob::setNoDisplayValue(bool b)
{
	mNoDisplayValue = b;
}

void Knob::update(int i)
{
	if (!mDragging)
	{
		if (i != mValue)
		{
			setValue(i);
			invalidate();
		}
	}
}

void Knob::setMaxValue(int i)
{
	mMaxValue = i;
	if (mValue > mMaxValue)
		mValue = mMaxValue;
}

void Knob::setMinValue(int i)
{
	mMinValue = i;
	if (mValue < mMinValue)
		mValue = mMinValue;
}

void Knob::mousePressed(MouseEvent *e)
{
	// printf("Knob::mousePressed %d %d\n", e->getX(), e->getY());

	mDragging = true;
	mDragOriginX = e->getX();
	mDragOriginY = e->getY();
	mDragStartValue = mValue;
	mDragChanges = 0;

	// let this become the drag target
	e->setClaimed(true);
}

/**
 * Assuming we're only called from the main window event thread so
 * we can paint.
 */
void Knob::mouseDragged(MouseEvent *e)
{
	// printf("Knob::mouseDragged %d %d\n", e->getX(), e->getY());

	int ydelta = mDragOriginY - e->getY();
	int xdelta = mDragOriginX - e->getX();

	// not quite so sensitive, should base
	// this on the number of increments?
	ydelta = ydelta / 2;
	xdelta = xdelta / 2;

	// value increments up and to the right,
	// decrements down and to the left

	int newValue = mDragStartValue - xdelta + ydelta;
	if (newValue < mMinValue)
		newValue = mMinValue;
	if (newValue > mMaxValue)
		newValue = mMaxValue;

	if (newValue != mValue)
	{
		mValue = newValue;
		Component::paint();
		mDragChanges++;
		// now or wait till we're done?
		fireActionPerformed();
	}
}

/**
 * Assuming we're only called from the main window event thread so
 * we can paint.
 */
void Knob::mouseReleased(MouseEvent *e)
{
	if (mClickIncrement && mDragChanges == 0)
	{
		int mousey = e->getY();
		int centery = mDiameter / 2;

		if (mousey < centery)
		{
			if (mValue < mMaxValue)
			{
				mValue++;
				Component::paint();
				fireActionPerformed();
			}
		}
		else if (mousey > centery)
		{
			if (mValue > mMinValue)
			{
				mValue--;
				Component::paint();
				fireActionPerformed();
			}
		}
	}
	mDragging = false;
}

void Knob::paint(Graphics *g)
{
	tracePaint();

	Dimension d;
	Bounds b;
	getPaintBounds(&b);

	if (debugging)
	{
		int x = 0;
	}

	g->setColor(getBackground());
	g->fillRect(b.x, b.y, b.width, b.height);

	// note that the component width can be wider than mDiameter
	// if the label is long
	int radius = mDiameter / 2;
	int center = b.width / 2;
	int centerx = b.x + center;
	int centery = b.y + radius;
	int left = b.x + (center - radius);
	int top = b.y;

	// two basic methods
	// second-degree polynomial
	//   P = (x, sqrt( r^2 - x^2 ))
	// trigonometric
	//   P = (r * cos(theta), r * sin(theta))
	// where theta is the angle in radians
	// trig is more computationally expensive
	// Breshenham's algorithm is preferred for
	// actually drawing a circle, but its more complicated
	// and we only need to determine one point here

	// leave a little notch in the range like a real knob
	int notchDegrees = 45;

	// determine the number of degrees per value increment
	long range = (mMaxValue - mMinValue) + 1;
	float increments = (float)(360 - notchDegrees) / (float)range;
	int degree = (int)((mValue - mMinValue) * increments) + (notchDegrees / 2);

	// degree zero points straight to the left, have ours
	// start at the bottom
	degree += 90;
	if (degree > 360)
		degree -= 360;

	// radians = degrees * (pi / 180)
	double radians = degree / 57.2957;
	int radialx = centerx + (int)(radius * cos(radians));
	int radialy = centery + (int)(radius * sin(radians));

	g->setColor(getForeground());
	g->setBackgroundColor(getBackground());

	// since this is implemented as a filled oval, it will have the effect
	// of erasing the numeric field each time which is what we want,
	// but technically not like Swing
	g->drawOval(left, top, mDiameter, mDiameter);
	g->drawLine(centerx, centery, radialx, radialy);

	TextMetrics *tm = g->getTextMetrics();
	if (!mNoDisplayValue)
	{
		g->setFont(mFont);

		char buf[128];
		sprintf(buf, "%d", mValue);

		g->getTextSize(buf, mFont, &d);

		int x = centerx - (d.width / 2);
		int y = centery + (tm->getAscent() / 2);
		// when surrounded by the circlt, this ends up being too low and right
		// not sure if this is a calculation error or a rouding error
		y -= 2;
		x -= 1;
		g->drawString(buf, x, y);
	}

	if (mLabel != NULL)
	{
		g->setFont(mFont);
		// could remember some of these calculations
		g->getTextSize(mLabel, mFont, &d);
		int x = centerx - (d.width / 2);

		// font includes some internal leading space, could have some extra
		int y = b.y + mDiameter + tm->getAscent();
		g->drawString(mLabel, x, y);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  SPACE KNOB                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Wraps a Knob and makes it a space component.
 * Ugh, having to proxy all the methods is a pain.  Could try
 * multiple inheritance.
 */

SpaceKnob::SpaceKnob()
{
	mClassName = "SpaceKnob";
	mKnob = new Knob();
	mKnob->addActionListener(this);

	// we don't actually paint anything, so pass our colors on to the knob
	mKnob->setBackground(getBackground());
	mKnob->setForeground(getForeground());

	add(mKnob);
}

SpaceKnob::~SpaceKnob()
{
}

void SpaceKnob::setBackground(Color *c)
{
	mKnob->setBackground(c);
}

void SpaceKnob::setForeground(Color *c)
{
	mKnob->setForeground(c);
}

void SpaceKnob::setLabel(const char *label)
{
	mKnob->setLabel(label);
}

void SpaceKnob::setValue(int i)
{
	mKnob->setValue(i);
}

void SpaceKnob::setNoDisplayValue(bool b)
{
	mKnob->setNoDisplayValue(b);
}

void SpaceKnob::update(int i)
{
	mKnob->update(i);
}

void SpaceKnob::setMinValue(int i)
{
	mKnob->setMinValue(i);
}

void SpaceKnob::setMaxValue(int i)
{
	mKnob->setMaxValue(i);
}

void SpaceKnob::setDiameter(int i)
{
	mKnob->setDiameter(i);
}

int SpaceKnob::getValue()
{
	return mKnob->getValue();
}

void SpaceKnob::actionPerformed(void *o)
{
	fireActionPerformed();
}

/****************************************************************************
 *                                                                          *
 *   							BORDERED GRID                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC BorderedGrid::BorderedGrid(int rows, int columns)
{
	mClassName = "BorderedGrid";
	setLayout(new GridLayout(rows, columns));
	addMouseListener(this);

	// !! seem to require a thickness of 2 in order to see
	// anything on the left and right edges

	Color *black = GlobalPalette->getColor(COLOR_SPACE_BACKGROUND);
	mNoBorder = new LineBorder(black, 2);
	Color *white = GlobalPalette->getColor(COLOR_BUTTON_TEXT);
	mYesBorder = new LineBorder(white, 2);
}

PUBLIC BorderedGrid::~BorderedGrid()
{
	delete mNoBorder;
	delete mYesBorder;
}

/**
 * Overload Container method to add our extra wrapper.
 */
PUBLIC void BorderedGrid::add(Component *c)
{
	Panel *p = new Panel("BorderedGrid cell");
	p->setLayout(new BorderLayout());
	p->add(c, BORDER_LAYOUT_CENTER);

	p->setBorder(mNoBorder);
	Container::add(p);
}

PUBLIC void BorderedGrid::setSelectedIndex(int index)
{
	Component *child;
	int i;

	for (i = 0, child = getComponents();
		 child != NULL; i++, child = child->getNext())
	{

		Border *newBorder;
		if (i == index)
			newBorder = mYesBorder;
		else
			newBorder = mNoBorder;

		if (child->getBorder() != newBorder)
		{
			child->setBorder(newBorder);

			// child->paint();
			child->invalidate();
		}
	}
}

PUBLIC int BorderedGrid::getSelectedIndex()
{
	int index = -1;
	Component *child;
	int i;

	for (i = 0, child = getComponents(); child != NULL;
		 i++, child = child->getNext())
	{
		if (child->getBorder() == mYesBorder)
		{
			index = i;
			break;
		}
	}
	return index;
}

PUBLIC void BorderedGrid::mousePressed(MouseEvent *e)
{
	Component *child;
	Point p;
	int i, index, current;

	// printf("BorderedGrid::mousePressed %d %d\n", e->getX(), e->getY());

	// always propagate to child compenents, but make
	// sure the component is selected

	p.x = e->getX();
	p.y = e->getY();

	current = getSelectedIndex();
	index = current;

	for (i = 0, child = getComponents(); child != NULL;
		 i++, child = child->getNext())
	{

		if (child->isCovered(&p))
		{
			index = i;
			break;
		}
	}

	if (index != current)
	{
		setSelectedIndex(index);

		// setSelectedIndex will invalidate, shoudln't need this!
		// though it is okay since we're in a mouse handler
		// Component::paint();

		fireActionPerformed();
	}
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP METER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Width and height of the marker arrows.
 */
#define MARKER_SIZE 5

/**
 * Height of the tick marks.
 */
#define MAX_TICK_HEIGHT 12
#define CYCLE_TICK_HEIGHT 12
#define SUBCYCLE_TICK_HEIGHT 8
#define CUE_TICK_HEIGHT 8

/**
 * Number of event name rows to display.
 */
#define EVENT_ROWS 8

LoopMeter::LoopMeter()
{
	init(false, false);
}

LoopMeter::LoopMeter(bool ticks, bool markers)
{
	init(ticks, markers);
}

void LoopMeter::init(bool ticks, bool markers)
{
	mClassName = "LoopMeter";
	setType(LoopMeterElement);

	mFont = GlobalFontConfig->intern("loopMeter", 12);
	mTicks = ticks;
	mMarkers = markers;
	mSubcycles = 0;
	mState.init();
	mState.cycles = 0; // make sure!
	setLayout(new BorderLayout());
	mMeter = new Thermometer();
	add(mMeter, BORDER_LAYOUT_CENTER);

	int markerHeight = 0;
	if (mTicks)
		markerHeight += MAX_TICK_HEIGHT;

	if (mMarkers)
	{
		// for the event names, in theory there can be any number of these
		// two is normal, three can happen ending Multiply/Insert,
		// four I think can happen in some switches
		// sadly, don't have accurate font height yet,
		// consider letting Strut have a font so it can compute its
		// preferred size alter
		// int eventHeight = mFont->getHeight() * EVENT_ROWS;
		markerHeight += (EVENT_ROWS * 12);
		// for the arrows on either side
		add(new Strut(10, 0), BORDER_LAYOUT_EAST);
		add(new Strut(10, 0), BORDER_LAYOUT_WEST);
	}

	if (markerHeight > 0)
		add(new Strut(0, markerHeight), BORDER_LAYOUT_SOUTH);

	mColor = GlobalPalette->getColor(COLOR_METER, Color::White);
	mSlowColor = GlobalPalette->getColor(COLOR_SLOW_METER, Color::Gray);
	mRecordingColor = GlobalPalette->getColor(COLOR_RECORDING_METER, Color::Red);
	mMuteColor = GlobalPalette->getColor(COLOR_MUTE_METER, Color::Blue);
	mEventColor = GlobalPalette->getColor(COLOR_EVENT, Color::White);

	// TODO: Nice to have different colors for each tick type
	mTickCycleColor = GlobalPalette->getColor(COLOR_TICK_CYCLE, Color::White);
	mTickSubcycleColor = GlobalPalette->getColor(COLOR_TICK_SUBCYCLE, Color::Gray);
	mTickCueColor = GlobalPalette->getColor(COLOR_TICK_CUE, Color::Red);

	// It Works! I need to set the mMeter size to change size
	// mMeter->setPreferredSize(400,50); //#011 Cas WOW!
}

LoopMeter::~LoopMeter()
{
}

/**
 * A rare overload of setEnabled to pass the flag along
 * to the contained Thermometer.  Most SpaceComponents overload
 * setEnabled and only invalidate the parent to avoid excessive
 * invalidation events.  Our relationship with Thermometer is weird
 * though, we want to update state like the position, range, and colors
 * but doing so cause it to be invalidated if it isn't marked disabled.
 * This is quite messy, would be better not to update it and force a refresh
 * when it is next enabled?
 */
void LoopMeter::setEnabled(bool b)
{
	SpaceComponent::setEnabled(b);
	mMeter->setEnabled(b);
}

/**
 * override baseclass setPreferredSize - custom LoopMeter Size
 * Need to cast to LoopMeter to call the right once...!
 */
void LoopMeter::setPreferredSize(int width, int height) // #011
{
	// Trace(3,"LoopMeter::setPreferredSize! [overrride]");
	mMeter->setPreferredSize(width, height);
}

void LoopMeter::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;
	LoopState *lstate = tstate->loop;

	// don't change anything if we're dragging components around,
	// some child components (Thermometer) don't know to not paint themselves
	// on a update
	// UPDATE: Is this still true??!!
	if (Space::isDragging())
		return;

	// set if we decide to redraw the marker region
	bool refreshMarkers = false;

	if (mMeter->getRange() != lstate->frames)
	{
		// always redraw the event markers if the range changes
		refreshMarkers = true;
		mMeter->setRange(lstate->frames);
	}
	mMeter->setValue(lstate->frame);

	// todo: need Color::Pink if recording in half speed
	// Mute seems to be the most important, though a Mute/Record combo
	// needs to be shown in some way

	if (lstate->mute)
		mMeter->setMeterColor(mMuteColor);
	else if (lstate->recording)
		mMeter->setMeterColor(mRecordingColor);

	// formerly had a halfSpeed flag
	else if (tstate->speedOctave < 0 || tstate->speedStep < 0)
		mMeter->setMeterColor(mSlowColor);
	else
		mMeter->setMeterColor(mColor);

	if (mTicks)
	{
		// TODO: Check cue changes
		if (mState.cycles != lstate->cycles ||
			mSubcycles != tstate->preset->getSubcycles())
			refreshMarkers = true;
	}

	if (mMarkers)
	{
		// try to be smart about changes to avoid flicker
		// since we have to keep track of events for paint() calls,
		// just keep a complete copy the last MobiusState around

		if (mState.eventCount != lstate->eventCount)
		{
			// Trace(2, "UI: Event count changed\n");
			refreshMarkers = true;
		}
		else
		{
			for (int i = 0; i < lstate->eventCount; i++)
			{

				if (lstate->events[i].type != mState.events[i].type ||
					lstate->events[i].function != mState.events[i].function ||
					lstate->events[i].frame != mState.events[i].frame ||
					lstate->events[i].argument != mState.events[i].argument)
				{
					refreshMarkers = true;
					break;
				}
			}
		}
	}

	if (refreshMarkers)
	{
		mState = *lstate;
		mSubcycles = tstate->preset->getSubcycles();
		if (isEnabled())
			invalidate();
	}
}

void LoopMeter::paint(Graphics *g)
{
	if (isEnabled())
	{

		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);

		else
		{
			Container::paint(g);
			Bounds b, mb;
			getPaintBounds(&b);
			mMeter->getPaintBounds(&mb);

			g->setColor(getBackground());
			int top = mb.y + mb.height;
			int height = b.height - (top - b.y);

			// start by clearing the region if we have anything
			if (mTicks || mMarkers)
				g->fillRect(b.x, top, b.width, height);

			// Thermometer has an effective 2 pixel border that
			// must be factored out when positioning the markers
			int left = mb.x + 2;
			int width = mb.width - 4;
			int right = left + width - 1;

			// draw the ticks
			if (mTicks)
			{
				// ?? probably want some maximums so we don't saturate
				// if the loop is empty, still display ticks for one cycle
				int cycles = mState.cycles;
				if (cycles == 0)
					cycles = 1;

				// if subcycles isn't initialized yet, assume one
				int subs = mSubcycles;
				if (subs == 0)
					subs = 1;

				int segments = subs * cycles;
				// important to maintain these as floats for proper alignment
				float segwidth = (float)width / (float)segments;
				float offset = (float)left;
				int count = 0;

				// note that we reach the segment count to draw the final tick
				for (int i = 0; i <= segments; i++)
				{
					Color *color;
					int y2 = top;

					// truncate the fraction
					// on the last one round up to the full width
					int x = (int)offset;
					if (i == segments)
						x = right;

					if (count == 0)
					{
						// on a cycle
						color = mTickCycleColor;
						y2 += CYCLE_TICK_HEIGHT;
					}
					else
					{
						// on a subcycle
						color = mTickSubcycleColor;
						y2 += SUBCYCLE_TICK_HEIGHT;
					}

					g->setColor(color);
					g->drawLine(x, top, x, y2);

					count++;
					if (count >= mSubcycles)
						count = 0;
					offset += segwidth;
				}

				top += MAX_TICK_HEIGHT;
			}

			// then the markers
			if (mMarkers)
			{

				// Trace(2, "*** Drawing Markers ***\n");

				// keep track of the text bounds of the event names as we paint
				// them so we can detect overlap and reposition
				Bounds nameBounds[MAX_INFO_EVENTS];

				for (int i = 0; i < mState.eventCount; i++)
				{
					EventSummary *sum = &(mState.events[i]);
					EventType *type = sum->type;
					Function *f = sum->function;
					long frame = sum->frame;
					int argument = sum->argument;

					int offset = 0;
					if (mState.frames > 0)
						offset = (int)(((float)(width) / (float)mState.frames) * frame);
					int x1 = left + offset;
					int y1 = top;
					int x2 = x1 - MARKER_SIZE;
					int y2 = y1 + MARKER_SIZE;
					int x3 = x1 + MARKER_SIZE;
					int y3 = y2;

					// if we're recording the initial loop, make all events
					// look like they're off the right edge, this should only
					// happen for things like SUSReturnEvent
					if (mState.frames == 0)
						x1 = right + 1;

					if (x1 < left)
					{
						// must be negative event in the next loop
						// !! this is farther from the left edge than
						// the right edge calculations below, something's off
						x1 = mb.x - MARKER_SIZE * 2;
						y1 = top + MARKER_SIZE;
						x2 = x1 + MARKER_SIZE;
						y2 = y1 - MARKER_SIZE;
						x3 = x2;
						y3 = y1 + MARKER_SIZE;
					}
					else if (x1 > right)
					{
						// event is greater than the current range
						// rotate so it points right
						x1 = right + MARKER_SIZE * 2;
						y1 = top + MARKER_SIZE;
						x2 = x1 - MARKER_SIZE;
						y2 = y1 - MARKER_SIZE;
						x3 = x2;
						y3 = y1 + MARKER_SIZE;
					}

					g->setColor(mEventColor);
					g->drawLine(x1, y1, x2, y2);
					g->drawLine(x2, y2, x3, y3);
					g->drawLine(x3, y3, x1, y1);

					// sigh, the name to display is complicated, should just
					// have MobiusState return the proper display name?

					const char *eventName;
					if (f != NULL && f->eventType == type)
					{
						// the initiating event, many functions may use the
						// same event so get the name from the function
						eventName = f->getDisplayName();
					}
					else if (type == InvokeEvent)
					{
						// a speical event for deferred invocation, use
						// the function name
						eventName = f->getDisplayName();
					}
					else
					{
						// An alternate ending, commonly here with
						// RecordStopEvent for one of many functions
						// use the event name.
						eventName = type->name;
					}

					paintEventName(g, f, eventName, argument, &b, x1, y1,
								   nameBounds, i);

					// Trace(2, "Marker %s\n", eventName);
				}
			}
		}
	}
}

/**
 * Need to be a lot smarter about placement.
 */
void LoopMeter::paintEventName(Graphics *g, Function *func, const char *name,
							   int argument,
							   Bounds *b, int left, int top,
							   Bounds *nameBounds, int eventIndex)
{
	char namebuf[128];
	Dimension d;
	int maxRight = b->x + b->width - 1;

	// sigh, argument 0 is usually suppressed for but pitch/speed shift
	// we need to see it
	if (argument > 0 || func == SpeedStep || func == PitchStep)
	{
		sprintf(namebuf, "%s %d", name, argument);
		name = namebuf;
	}

	g->getTextSize(name, mFont, &d);

	// first try to center it
	top += MARKER_SIZE + 2;
	left -= d.width / 2;

	// push on the right
	if (left + d.width > maxRight)
		left = maxRight - d.width;

	// push on the left
	if (left < b->x)
		left = b->x;

	// look for another event in this space
	for (int i = 0; i < eventIndex; i++)
	{
		Bounds *nb = &(nameBounds[i]);
		int thisRight = left + d.width - 1;
		int otherRight = nb->x + nb->width - 1;
		if (left <= otherRight && thisRight >= nb->x)
		{
			// at least some amount of horizontal overlap
			top = nb->y + nb->height;
		}
	}

	// contribute our bounds
	if (eventIndex < MAX_INFO_EVENTS)
	{
		nameBounds[eventIndex].x = left;
		nameBounds[eventIndex].y = top;
		nameBounds[eventIndex].width = d.width;
		nameBounds[eventIndex].height = d.height;
	}

	g->setColor(mEventColor);
	g->setBackgroundColor(getBackground());
	g->setFont(mFont);
	TextMetrics *tm = g->getTextMetrics();
	g->drawString(name, left, top + tm->getAscent());
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP GRID                                 *
 *                                                                          *
 ****************************************************************************/

LoopGrid::LoopGrid()
{
	mClassName = "LoopGrid";
}

LoopGrid::~LoopGrid()
{
}

void LoopGrid::update(MobiusState *state)
{
}

void LoopGrid::paint(Graphics *g)
{
}

/****************************************************************************
 *                                                                          *
 *   							 MESSAGE AREA                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum number of characters we allow in the message.
 */
#define MESSAGE_MAX_CHARS 50 // #005
// #define MESSAGE_MAX_CHARS 30

/**
 * Ticks normally are 1/10 second.
 */
#define TICKS_PER_SECOND 10

MessageArea::MessageArea()
{
	mClassName = "MessageArea";
	setType(MessagesElement);

	mFont = GlobalFontConfig->intern("message", 18);
	strcpy(mMessage, "");
	mDuration = DEFAULT_MESSAGE_DURATION;
	mTicks = 0;
	mRefresh = false;
}

MessageArea::~MessageArea()
{
}

void MessageArea::setDuration(int seconds)
{
	mDuration = seconds;

	// reset the tick counter if we're currently displaying something
	mRefresh = true;
}

void MessageArea::add(const char *msg)
{
	// only one right now, but eventually support several lines or
	// at least a buffer!

	if (msg == NULL)
		strcpy(mMessage, "");
	else
		strcpy(mMessage, msg);

	mRefresh = true;
}

PUBLIC Dimension *MessageArea::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();

		w->getTextSize("M", mFont, mPreferred);
		mPreferred->width *= MESSAGE_MAX_CHARS;

		// pad for a baseline
		// !!?? Should be getting this from TextMetrics instead?
		mPreferred->height += mFont->getAscent();

		// space for the move border
		mPreferred->width += 4;
		mPreferred->height += 4;
	}

	return mPreferred;
}

PUBLIC void MessageArea::update()
{
	if (mRefresh)
	{
		mRefresh = false;
		if (mDuration <= 0)
			mTicks = 0;
		else
			mTicks = mDuration * TICKS_PER_SECOND;
		if (isEnabled())
			invalidate();
	}
	else if (mTicks > 0)
	{
		mTicks--;
		if (mTicks == 0)
		{
			strcpy(mMessage, "");
			if (isEnabled())
				invalidate();
		}
	}
}

PUBLIC void MessageArea::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{
			Bounds b;
			TextMetrics *tm = g->getTextMetrics();

			getPaintBounds(&b);
			// jump over the border
			b.x += 2;
			b.y += 2;
			b.width -= 4;
			b.height -= 4;

			// clear background
			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);

			if (strlen(mMessage))
			{

				g->setColor(getForeground());
				g->setBackgroundColor(getBackground());
				g->setFont(mFont);

				int top = b.y + tm->getAscent();
				g->drawString(mMessage, b.x, top);
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                POPUP ALERT                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC PopupAlert::PopupAlert(Window *parent)
{
	initAlert(parent);
}

PUBLIC PopupAlert::PopupAlert(Window *parent, const char *msg)
{
	initAlert(parent);
	setMessage(msg);
}

PUBLIC void PopupAlert::initAlert(Window *parent)
{
	mClassName = "PopupAlert";

	setParent(parent);
	setModal(false);

	// this disables borders
	setClass(ALERT_WINDOW_CLASS);

	mDuration = 10;
	mCounter = 0;

	// don't have GlobalPalette colors for this yet
	setBackground(GlobalPalette->getColor(COLOR_ALERT_BACKGROUND, Color::Gray));
	setForeground(GlobalPalette->getColor(COLOR_ALERT_TEXT, Color::White));

	setLayout(new BorderLayout());
	setInsets(10, 0, 10, 0);
	mLabel = new Label();
	// mLabel->setLightweight(true);
	mLabel->setFont(GlobalFontConfig->intern("alert", 40));
	mLabel->setBackground(getBackground());
	mLabel->setForeground(getForeground());
	add(mLabel, BORDER_LAYOUT_CENTER);
}

PUBLIC PopupAlert::~PopupAlert()
{
}

PUBLIC void PopupAlert::setDuration(int i)
{
	mDuration = i;
}

PUBLIC void PopupAlert::setFont(Font *font)
{
	mLabel->setFont(font);
}

PUBLIC void PopupAlert::setMessage(const char *msg)
{
	mLabel->setText(msg);
}

PUBLIC bool PopupAlert::tick()
{
	mCounter++;
	return (mCounter >= mDuration);
}

PUBLIC Dimension *PopupAlert::getPreferredSize(Window *w)
{
	/*
	if (mPreferred == NULL)
	  mPreferred = new Dimension(300, 100);
	return mPreferred;
	*/
	return Dialog::getPreferredSize(w);
}

/****************************************************************************
 *                                                                          *
 *   								ALERT                                   *
 *                                                                          *
 ****************************************************************************/
/*
 * This is a funny one in that we don't advertize a preferred size and
 * ignore our bounds.  We create it only to hold the enable/disable
 * status for space components.  UI will create a PopupAlert if something
 * needs to be shown.
 *
 * Originally we let this draw the alert but it really needs to be a
 * popup window to avoid z-order conflicts with other components.
 *
 * UPDATE: This turns out to be really annoying because the popup window
 * steals focus for a few seconds, and since the alert is usually due
 * to changing presets, it happens as you use the keyboard to move the
 * selected track cursor, making the keyboard appear to freeze.
 */
PUBLIC SpaceAlert::SpaceAlert()
{
	mClassName = "SpaceAlert";
	mPopup = NULL;
}

PUBLIC SpaceAlert::~SpaceAlert()
{
	closePopup();
}

PUBLIC void SpaceAlert::openPopup(const char *msg)
{
	if (mPopup == NULL)
	{
		mPopup = new PopupAlert(getWindow(), msg);
		mPopup->show();
	}
}

PUBLIC void SpaceAlert::closePopup()
{
	if (mPopup != NULL)
	{
		// null this early in case we get another update call while
		// we're trying to close it
		PopupAlert *popup = mPopup;
		mPopup = NULL;
		popup->close();
		// can we assume the close went synchronously?
		delete popup;
	}
}

PUBLIC Dimension *SpaceAlert::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		// leave it empty
		mPreferred = new Dimension();
	}
	return mPreferred;
}

/**
 * Popup an alert if we're not already displyaing one.
 * If we were smarter we could either change the text of the
 * existing one or popup another, but overlapping alerts are unusual.
 */
PUBLIC void SpaceAlert::update(const char *msg)
{
	if (mPopup == NULL)
		openPopup(msg);
	else
		update();
}

/**
 * Tick the counter and erase the message when time runs out.
 */
PUBLIC void SpaceAlert::update()
{
	if (mPopup != NULL && mPopup->tick())
		closePopup();
}

//
// Preset alert
//

PUBLIC PresetAlert::PresetAlert()
{
	mPreset = -1;
	setType(PresetAlertElement);
}

PUBLIC PresetAlert::~PresetAlert()
{
}

PUBLIC void PresetAlert::update(MobiusState *mstate)
{
	if (isEnabled())
	{
		Preset *p = mstate->track->preset;
		int index = p->getNumber();
		if (mPreset == index)
			SpaceAlert::update();
		else
		{
			mPreset = index;
			SpaceAlert::update(p->getName());
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   								RADAR                                   *
 *                                                                          *
 ****************************************************************************/

Radar::Radar()
{
	init();
}

void Radar::init()
{
	mClassName = "Radar";
	mRange = 0;
	mDegree = 0;
	mLastDegree = 0;
	mLastRange = 0;
	mPhase = false;
	mDiameter = KNOB_DEFAULT_DIAMETER;
}

Radar::~Radar()
{
}

void Radar::setRange(int i)
{
	if (i >= 0)
		mRange = i;
}

// #004 Expose diameter
void Radar::setDiameter(int i)
{
	mDiameter = i;
}

// #004 Expose diameter
int Radar::getDiameter()
{
	return mDiameter;
}

void Radar::update(int value)
{
	// value is from 0 to mRange, convert to degree and paint

	if (mLastRange != mRange)
	{
		// when the range changes, always force a background clear
		mDegree = 0;
		if (mRange > 0)
		{
			float adjust = 360.0f / mRange;
			mDegree = (int)(adjust * value);
		}
		invalidate();
	}
	else if (mRange > 0)
	{
		float adjust = 360.0f / mRange;
		int degree = (int)(adjust * value);
		if (mLastDegree != degree)
		{
			mDegree = degree;
			invalidate();
		}
	}
}

PUBLIC Dimension *Radar::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
		mPreferred = new Dimension(mDiameter, mDiameter);

	return mPreferred;
}

PUBLIC void Radar::paint(Graphics *g)
{
	tracePaint();

	Bounds b;
	getPaintBounds(&b);

	// Cas | probably we should erase background also if change ForegorundColor

	// erase once we've crossed the start point, or the range changes
	// Trace(2, "Last %ld Degree %ld\n", (long)mLastDegree, (long)mDegree);

	// if (mLastRange != mRange || mDegree == 0 || mLastDegree > mDegree) {
	if (mLastRange != mRange || mDegree == 0 || mLastDegree > mDegree || mForegroundColorChanged)
	{ // #007
		// Trace(3, "RepaintBackground"); // the ForeColor is set every time, no need to reset the flag here [cas]S
		g->setColor(getBackground());
		g->fillRect(b.x, b.y, b.width, b.height);
		// printf("Radar::erase %d %d %d %d\n", b.x, b.y, b.width, b.height);
	}
	mLastRange = mRange;
	mLastDegree = mDegree;

	// If the two radials are the same (at small angles)
	// Pie() creates a filled circle which we don't want,
	// treat this as an indiciation to clear the background.
	// Basically just clear the background until degree exceeds 2.
	// I know 4 is ok, not sure about 3.
	// !! Shouldn't Graphics handle this?

	if (mRange > 0 && mDegree > 2)
	{

		g->setColor(getForeground());

		// fillArc paints degree zero at 3 o'clock, but I think
		// it looks nicer to start from 12 o'clock
		int startAngle = 270;

		// positive degrees are counter clockwise, we want to
		// go clockwise
		g->fillArc(b.x, b.y, b.width, b.height, startAngle, -mDegree);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP RADAR                                *
 *                                                                          *
 ****************************************************************************/

LoopRadar::LoopRadar()
{
	init();
	mClassName = "LoopRadar";

	mColor = GlobalPalette->getColor(COLOR_METER, Color::White);
	mSlowColor = GlobalPalette->getColor(COLOR_SLOW_METER, Color::Gray);
	mRecordingColor = GlobalPalette->getColor(COLOR_RECORDING_METER, Color::Red);
	mMuteColor = GlobalPalette->getColor(COLOR_MUTE_METER, Color::Blue);
}

void LoopRadar::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;
	LoopState *lstate = tstate->loop;

	// color priority has to match LoopMeter!

	if (lstate->mute)
		setForeground(mMuteColor);
	else if (lstate->recording)
		setForeground(mRecordingColor);
	else if (tstate->speedOctave < 0 || tstate->speedStep < 0)
		setForeground(mSlowColor);
	else
		setForeground(mColor);

	long frames = lstate->frames;
	if (frames == 0 && lstate->recording)
	{
		setRange(1);
		Radar::update(1);
	}
	else
	{
		setRange(lstate->frames);
		Radar::update(lstate->frame);
	}
}

/****************************************************************************
 *                                                                          *
 *   							TOGGLE BUTTON                               *
 *                                                                          *
 ****************************************************************************/

#define TRACK_BUTTON_DIAMETER 10

PUBLIC FocusButton::FocusButton(MobiusInterface *m, int trackIndex)
{
	mMobius = m;
	mTrack = trackIndex;
	mClassName = "FocusButton";
	mPushed = false;
	mDiameter = TRACK_BUTTON_DIAMETER;
	addMouseListener(this);

	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setForeground(GlobalPalette->getColor(COLOR_BUTTON, Color::Red));

	mPushColor = GlobalPalette->getColor(COLOR_RECORDING_METER, Color::Red);
}

PUBLIC Dimension *FocusButton::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		mPreferred->width = mDiameter;
		mPreferred->height = mDiameter;
	}
	return mPreferred;
}

PUBLIC void FocusButton::setPushed(bool b)
{
	if (mPushed != b)
	{
		mPushed = b;
		invalidate();
	}
}

PUBLIC bool FocusButton::isPushed()
{
	return mPushed;
}

PUBLIC void FocusButton::mousePressed(MouseEvent *e)
{
	// printf("FocusButton::mousePressed %d %d\n", e->getX(), e->getY());
	mPushed = !mPushed;

	// simulate the FocusLock function
	Action *a = new Action();
	a->setFunction(FocusLock);

	// Action takes a 1 based track number, which is what we have
	a->setTargetTrack(mTrack);

	// Trigger id will be the address of the component
	a->id = (long)this;
	a->trigger = TriggerUI;
	a->triggerMode = TriggerModeOnce;

	// NOTE: This will toggle and is not necessarily the same as our
	// mPushed state.  It doesn't really matter since we'll update
	// mPushed to track it on the next refresh cycle.
	mMobius->doAction(a);

	Component::paint();
}

PUBLIC void FocusButton::paint(Graphics *g)
{
	tracePaint();
	Bounds b;
	getPaintBounds(&b);

	g->setColor(getForeground());
	g->setBackgroundColor(getBackground());

	int left = (b.x + b.width - 1) - mDiameter;
	g->drawOval(left, b.y, mDiameter, mDiameter);

	if (mPushed)
		g->setColor(mPushColor);
	else
		g->setColor(getBackground());

	g->fillOval(left + 2, b.y + 2, mDiameter - 4, mDiameter - 4);
}

/****************************************************************************
 *                                                                          *
 *   							 TRACK NUMBER                               *
 *                                                                          *
 ****************************************************************************/

//
// Extends FocusButton, though I'm not sure that buys us much
//

PUBLIC TrackNumber::TrackNumber(MobiusInterface *m, int trackIndex) : FocusButton(m, trackIndex)
{
	mClassName = "TrackNumber";
	strcpy(mName, "");
	mNumberFont = GlobalFontConfig->intern("trackNumber", 30);
	mNameFont = GlobalFontConfig->intern("trackName", 20);
}

PUBLIC int TrackNumber::getNumber()
{
	return mTrack;
}

PUBLIC void TrackNumber::setNumber(int n)
{
	if (mTrack != n)
	{
		mTrack = n;
		invalidate();
	}
}

PUBLIC void TrackNumber::setName(const char *name)
{
	if (name == NULL || strlen(name) == 0)
	{
		if (strlen(mName) != 0)
		{
			// name taketh away
			strcpy(mName, "");
			invalidate();
		}
	}
	else if (strcmp(mName, name))
	{
		// name differeth
		CopyString(name, mName, sizeof(mName));
		invalidate();
	}
}

PUBLIC Dimension *TrackNumber::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		// we can show one big number and a smaller name
		w->getTextSize("8", mNumberFont, mPreferred);

		Dimension named;
		// sign, can't be more than 7 M's without trashing the right
		// border...we'll clip characters in paint to fit within this
		// !! need to figure out why this doesn't widen the containing
		// track strip?
		w->getTextSize("MMMMMMM", mNameFont, &named);

		if (named.width > mPreferred->width)
			mPreferred->width = named.width;

		if (named.height > mPreferred->height)
			mPreferred->height = named.height;
	}
	return mPreferred;
}

PUBLIC void TrackNumber::paint(Graphics *g)
{
	tracePaint();
	Bounds b;

	getPaintBounds(&b);
	g->setColor(getBackground());
	g->fillRect(b.x, b.y, b.width, b.height);

	g->setBackgroundColor(getBackground());
	if (mPushed)
		g->setColor(mPushColor);
	else
		g->setColor(getForeground());

	// since we're dealing with two different fonts, have to
	// center within the available region, could precalculate
	// the actual dimensions and cache them, but this shouldn't
	// update often

	char *text;
	char buffer[32];

	if (strlen(mName) == 0)
	{
		text = buffer;
		sprintf(buffer, "%d", mTrack);
		g->setFont(mNumberFont);
	}
	else
	{
		text = mName;
		g->setFont(mNameFont);
	}

	TextMetrics *tm = g->getTextMetrics();
	Dimension d;
	g->getTextSize(text, &d);

	// this might overflow
	int left = b.x;
	if (b.width >= d.width)
	{
		left = b.x + ((b.width - d.width) / 2);
	}
	else if (strlen(mName) == 0)
	{
		// overflow on the number, can't happen
	}
	else
	{
		// sigh, finding just the right size is tedious
		int chars = 16;
		while (chars > 0)
		{
			CopyString(mName, buffer, chars);
			g->getTextSize(buffer, &d);
			if (b.width < d.width)
			{
				// take one off and try again
				chars--;
			}
			else
			{
				// this will do
				strcpy(mName, buffer);
				left = b.x + ((b.width - d.width) / 2);
				break;
			}
		}
	}

	// this should never overflow
	int offset = (b.height - d.height) / 2;
	int top = (offset > 0) ? b.y + offset : b.y;

	g->drawString(text, left, top + tm->getAscent());
}

/****************************************************************************
 *                                                                          *
 *   							 TRACK GROUP                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC TrackGroupButton::TrackGroupButton(MobiusInterface *m, int trackIndex)
{
	mMobius = m;
	mTrack = trackIndex;
	mClassName = "TrackGroup";
	mGroup = 0;
	mLabel[0] = 0;

	mFont = GlobalFontConfig->intern("trackGroup", FONT_BOLD, 12);

	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setForeground(GlobalPalette->getColor(COLOR_BUTTON, Color::Red));
	addMouseListener(this);
}

PUBLIC TrackGroupButton::~TrackGroupButton()
{
}

PUBLIC Dimension *TrackGroupButton::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		// let's assume we're just "Group X" for now, no user defined names
		w->getTextSize("Group MM", mFont, mPreferred);
		mPreferred->height += 2;
	}
	return mPreferred;
}

PUBLIC void TrackGroupButton::update(int g)
{
	if (mGroup != g)
	{
		mGroup = g;

		// old way, numbers
		// sprintf(mLabel, "Group %d", mGroup);
		// 1.43 way, letters
		sprintf(mLabel, "Group %c", (char)((int)'A' + (g - 1)));

		if (isEnabled())
			invalidate();
	}
}

PUBLIC void TrackGroupButton::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		Bounds b;
		getPaintBounds(&b);

		g->setColor(getBackground());
		g->fillRect(b.x, b.y, b.width, b.height);

		if (mGroup > 0)
		{
			Color *fore = NULL;
			// cache these
			if (mGroup == 1)
				fore = GlobalPalette->getColor(COLOR_GROUP1);
			else if (mGroup == 2)
				fore = GlobalPalette->getColor(COLOR_GROUP2);
			else if (mGroup == 3)
				fore = GlobalPalette->getColor(COLOR_GROUP3);
			else if (mGroup == 4)
				fore = GlobalPalette->getColor(COLOR_GROUP4);

			if (fore == NULL)
				fore = GlobalPalette->getColor(COLOR_BUTTON, Color::Red);

			g->setColor(fore);
			g->setBackgroundColor(getBackground());
			g->setFont(mFont);

			int left = b.x + 3;
			TextMetrics *tm = g->getTextMetrics();
			int top = b.y + 2 + tm->getAscent();
			g->drawString(mLabel, left, top);
		}
	}
}

PUBLIC void TrackGroupButton::mousePressed(MouseEvent *e)
{
	// cycle through the possible groups
	// simulate the TrackGroup function
	Action *a = new Action();
	a->setFunction(TrackGroup);

	// Action takes a 1 based track number, which is what we have
	a->setTargetTrack(mTrack);

	// Trigger id will be the address of the component
	a->id = (long)this;
	a->trigger = TriggerUI;
	a->triggerMode = TriggerModeOnce;

	mMobius->doAction(a);

	// Component::paint();
}

/****************************************************************************
 *                                                                          *
 *                                ACTION KNOB                               *
 *                                                                          *
 ****************************************************************************/

#define STRIP_KNOB_DIAMETER 30

/**
 * A wrapper around SpaceKnob that is associated with a specific Mobius target.
 * Since we have no other types of SpaceKnob could just have this in
 * SpaceKnob, but who knows...
 */
PUBLIC ActionKnob::ActionKnob(MobiusInterface *m, const char *name, int track)
{
	mMobius = m;
	mAction = NULL;
	mExport = NULL;

	setDiameter(STRIP_KNOB_DIAMETER);
	addActionListener(this);

	resolve(name, track);
}

PUBLIC ActionKnob::~ActionKnob()
{
	delete mAction;
	delete mExport;
}

PUBLIC void ActionKnob::resolve(const char *name, int track)
{
	// fake up a binding to convey our target properties
	Binding b;
	b.setTrigger(TriggerUI);
	b.setTarget(TargetParameter);
	b.setName(name);
	b.setTrack(track);

	mAction = mMobius->resolveAction(&b);
	if (mAction == NULL)
		Trace(1, "ActionKnob: Unable to resolve Action: %s\n", name);
	else
	{
		mAction->triggerMode = TriggerModeContinuous;

		mExport = mMobius->resolveExport(mAction);
		if (mExport == NULL)
			Trace(1, "ActionKnob: Unable to resolve Export: %s\n", name);
		else
		{
			setLabel(mExport->getDisplayName());
			setMaxValue(mExport->getMaximum());
			setMinValue(mExport->getMinimum());
			// todo: don't have a default yet like Parameter does
			setValue(127);
		}
	}
}

PUBLIC void ActionKnob::actionPerformed(void *src)
{
	Action *a = mMobius->cloneAction(mAction);
	a->arg.setInt(getValue());
	mMobius->doAction(a);
}

PUBLIC void ActionKnob::update()
{
	if (mExport != NULL)
	{
		// can assume these are ordinal friendly
		int newValue = mExport->getOrdinalValue();
		// this will refresh only if the value changes
		SpaceKnob::update(newValue);
	}
}

/****************************************************************************
 *                                                                          *
 *   							 TRACK STRIP                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC TrackStrip::TrackStrip(MobiusInterface *m, int track)
{
	mClassName = "TrackStrip";
	setType(TrackStripElement);

	mMobius = m;
	mTrack = track;
	initComponents();

	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	mColor = GlobalPalette->getColor(COLOR_METER, Color::White);
	mSlowColor = GlobalPalette->getColor(COLOR_SLOW_METER, Color::Gray);
	mMuteColor = GlobalPalette->getColor(COLOR_MUTE_METER, Color::Blue);
	mRecordingColor = GlobalPalette->getColor(COLOR_RECORDING_METER, Color::Red);

	// Mac has a pixel of slop due to the scaled drawing modes of PDF
	// so always leave at least a pixel of buffer between components
	VerticalLayout *vl = new VerticalLayout(2);
	vl->setCenterX(true);
	setLayout(vl);
}

PUBLIC TrackStrip2::TrackStrip2(MobiusInterface *m, int track) : TrackStrip(m, track)
{
	mClassName = "TrackStrip2";
	setType(TrackStrip2Element);
}

PUBLIC void TrackStrip::initComponents()
{
	removeAll();

	mLock = NULL;
	mNumber = NULL;
	mGroup = NULL;
	mInput = NULL;
	mOutput = NULL;
	mFeedback = NULL;
	mAltFeedback = NULL;
	mPan = NULL;
	mSpeedOctave = NULL;
	mSpeedStep = NULL;
	mSpeedBend = NULL;
	mPitchOctave = NULL;
	mPitchStep = NULL;
	mPitchBend = NULL;
	mTimeStretch = NULL;
	mMeter = NULL;
	mRadar = NULL;
	mLevel = NULL;
	mLoops = NULL;
}

PUBLIC void TrackStrip::updateConfiguration(StringList *controls, UIConfig *mUIConfig) // #004 2 parameter mod
{
	initComponents();

	// Handling enable/disable is kludgey, like ParameterDisplay
	// if we're disabled we just won't add any controls, so we're not
	// really disabled at all, just empty.  Need to rethink
	// what "enabled" and "visible" mean in Qwin and/or SpaceComponent.
	// !! no shit, when I swapped the order of config update and
	// enabling in UI.cpp, this was left empty because we tried to
	// add child components before it was enabled.

	if (isEnabled() && controls != NULL)
	{
		for (int i = 0; i < controls->size(); i++)
		{
			const char *name = controls->getString(i);
			DisplayElement *el = DisplayElement::get(name);

			// consider encapsulating differences in the TrackControl object
			// so we can create these generically

			if (el == FocusLockElement)
			{
				Panel *buttons = new Panel("TrackStrip LockControl");
				// important for Mac to reduce excessive repaints
				buttons->setLayout(new HorizontalLayout());
				add(buttons);
				add(new Strut(0, 5));
				// mIndex is zero if this is the shared strip, just
				// initialize it to 1 and change it on the next update
				int number = (mTrack > 0) ? mTrack : 1;
				mLock = new FocusButton(mMobius, number);
				buttons->add(mLock);
			}
			else if (el == TrackNumberElement)
			{
				// mIndex is zero if this is the shared strip, just
				// initialize it to 1 and change it on the next update
				int number = (mTrack > 0) ? mTrack : 1;
				mNumber = new TrackNumber(mMobius, number);
				add(mNumber);
			}
			else if (el == GroupNameElement)
			{
				int number = (mTrack > 0) ? mTrack : 1;
				mGroup = new TrackGroupButton(mMobius, number);
				add(mGroup);
			}
			else if (el == InputLevelElement)
			{
				mInput = new ActionKnob(mMobius, "input", mTrack);

				UiDimension *d = mUIConfig->getUiDimensions()->getDimension("InputLevel");
				if (d != NULL)
				{
					Trace(3, "InputLevel::CustomDimension");
					mInput->setDiameter(d->getDiameter());
				}

				add(mInput);
			}
			else if (el == OutputLevelElement)
			{
				mOutput = new ActionKnob(mMobius, "output", mTrack);

				UiDimension *d = mUIConfig->getUiDimensions()->getDimension("OutputLevel");
				if (d != NULL)
				{
					Trace(3, "OutputLevel::CustomDimension");
					mOutput->setDiameter(d->getDiameter());
				}

				add(mOutput);
			}
			else if (el == FeedbackElement)
			{
				mFeedback = new ActionKnob(mMobius, "feedback", mTrack);
				add(mFeedback);
			}
			else if (el == AltFeedbackElement)
			{
				mAltFeedback = new ActionKnob(mMobius, "altFeedback", mTrack);
				add(mAltFeedback);
			}
			else if (el == PanElement)
			{
				mPan = new ActionKnob(mMobius, "pan", mTrack);
				mPan->setNoDisplayValue(true);
				add(mPan);
			}
			else if (el == SpeedOctaveElement)
			{
				mSpeedOctave = new ActionKnob(mMobius, "speedOctave", mTrack);
				add(mSpeedOctave);
			}
			else if (el == SpeedStepElement)
			{
				mSpeedStep = new ActionKnob(mMobius, "speedStep", mTrack);
				add(mSpeedStep);
			}
			else if (el == SpeedBendElement)
			{
				mSpeedBend = new ActionKnob(mMobius, "speedBend", mTrack);
				add(mSpeedBend);
			}
			else if (el == PitchOctaveElement)
			{
				mPitchOctave = new ActionKnob(mMobius, "pitchOctave", mTrack);
				add(mPitchOctave);
			}
			else if (el == PitchStepElement)
			{
				mPitchStep = new ActionKnob(mMobius, "pitchStep", mTrack);
				add(mPitchStep);
			}
			else if (el == PitchBendElement)
			{
				mPitchBend = new ActionKnob(mMobius, "pitchBend", mTrack);
				add(mPitchBend);
			}
			else if (el == TimeStretchElement)
			{
				mTimeStretch = new ActionKnob(mMobius, "timeStretch", mTrack);
				add(mTimeStretch);
			}
			else if (el == SmallLoopMeterElement)
			{

				// add(new Strut(0, 5));
				mMeter = new Thermometer();
				mMeter->setPreferredSize(new Dimension(75, 10));

				// non ho capito cosa è SmallLoopMeter...
				// mMeter->setPreferredSize(new Dimension(mUIConfig->getLevelMeterWidth(), mUIConfig->getLevelMeterHeight()));

				add(mMeter);
			}
			else if (el == LoopRadarElement)
			{
				// add(new Strut(0, 5));
				mRadar = new LoopRadar();
				UiDimension *d = mUIConfig->getUiDimensions()->getDimension("LoopRadar");
				if (d != NULL)
				{
					Trace(3, "LoopRadar::CustomDimension");
					mRadar->setDiameter(d->getDiameter()); // #004 v2
				}

				add(mRadar);
				add(new Strut(0, 15)); // CasAdd | Add spacing after LoopRadar!
			}
			else if (el == OutputMeterElement)
			{
				// output level
				add(new Strut(0, 5));
				mLevel = new AudioMeter();
				// note that we have to use setRequiredSize here since
				// AudioMeter is a SpaceComponent and they lose their
				// preferred size during layout
				UiDimension *d = mUIConfig->getUiDimensions()->getDimension("OutputMeter");
				if (d != NULL)
				{
					Trace(3, "OutputMeter::CustomDimension");
					mLevel->setRequiredSize(new Dimension(d->getWidth(), d->getHeight()));
					
					if (d->getSpacing() > 0)						  		// Range custom  #SPC
						((AudioMeter *)mLevel)->setRange(d->getSpacing()); 	// uso spacing per ora
				}

				add(mLevel);
			}
			else if (el == LoopStatusElement)
			{
				add(new Strut(0, 5));
				// mIndex is zero if this is the shared strip, let it be
				mLoops = new LoopStack(mMobius, mTrack);
				add(mLoops);
			}
		}
	}
}

PUBLIC TrackStrip::~TrackStrip()
{
}

PUBLIC void TrackStrip::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;

	if (Space::isDragging())
		return;

	if (mLock != NULL)
		mLock->setPushed(tstate->focusLock);

	if (mNumber != NULL)
	{
		// this should only be changing for the shared track strip
		int tnum = tstate->number + 1;
		mNumber->setNumber(tnum);
		mNumber->setPushed(tstate->focusLock);
		mNumber->setName(tstate->name);
	}

	if (mGroup != NULL)
		mGroup->update(tstate->group);

	if (mInput != NULL)
		mInput->update();

	if (mOutput != NULL)
		mOutput->update();

	if (mFeedback != NULL)
		mFeedback->update();

	if (mAltFeedback != NULL)
		mAltFeedback->update();

	if (mPan != NULL)
		mPan->update();

	if (mSpeedOctave != NULL)
		mSpeedOctave->update();

	if (mSpeedStep != NULL)
		mSpeedStep->update();

	if (mSpeedBend != NULL)
		mSpeedBend->update();

	if (mPitchOctave != NULL)
		mPitchOctave->update();

	if (mPitchStep != NULL)
		mPitchStep->update();

	if (mPitchBend != NULL)
		mPitchBend->update();

	if (mTimeStretch != NULL)
		mTimeStretch->update();

	if (mLevel != NULL)
		mLevel->setValue(tstate->outputMonitorLevel);

	// !! can't we encasulate this in the meter, we've
	// got this logic in three places now
	if (mMeter != NULL)
	{

		LoopState *lstate = tstate->loop;

		// two things that may need to be set here
		mMeter->setRange(lstate->frames);
		mMeter->setValue(lstate->frame);

		// color priority must match LoopMeter

		if (lstate->mute)
			mMeter->setMeterColor(mMuteColor);
		else if (lstate->recording)
			mMeter->setMeterColor(mRecordingColor);
		else if (tstate->speedOctave < 0 || tstate->speedStep < 0)
			mMeter->setMeterColor(mSlowColor);
		else
			mMeter->setMeterColor(mColor);
	}

	if (mRadar != NULL)
		mRadar->update(mstate);

	if (mLoops != NULL)
		mLoops->update(mstate);
}

PUBLIC void TrackStrip::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
			Container::paint(g);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  PARAMETER                                 *
 *                                                                          *
 ****************************************************************************/

#define PARAMETER_FONT_SIZE 14

/**
 * We always maintain two representations of the parameter value,
 * an integer (ordinal) and a string.  The string is used for display, the
 * integer is used to change values by dragging.
 */

PUBLIC ParameterEditor::ParameterEditor(MobiusInterface *m, Action *action, Export *exp)
{
	mClassName = "ParameterEditor";
	mMobius = m;
	mAction = action;
	mExport = exp;
	mInt = -1;
	mValue[0] = 0;
	mSelected = false;

	mDragging = false;
	mDragStartValue = 0;
	mDragOriginX = 0;
	mDragOriginY = 0;
	mDragChanges = 0;

	mMaxValue = exp->getMaximum();

	setFont(GlobalFontConfig->intern("parameter", PARAMETER_FONT_SIZE));

	// swap the border to show selection
	// this is like BorderedGrid, factor something out?
	Color *black = GlobalPalette->getColor(COLOR_SPACE_BACKGROUND);
	mNoBorder = new LineBorder(black, 2);
	Color *white = GlobalPalette->getColor(COLOR_BUTTON_TEXT);
	mYesBorder = new LineBorder(white, 2);

	setBorder(mNoBorder);

	addMouseListener(this);
	addMouseMotionListener(this);

	// we're not a SpaceComponent so we don't inherit these
	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black));
	setForeground(GlobalPalette->getColor(COLOR_PARAM_VALUE, Color::Red));
}

PUBLIC ParameterEditor::~ParameterEditor()
{
	delete mAction;
	delete mExport;
	delete mNoBorder;
	delete mYesBorder;
}

PUBLIC void ParameterEditor::setFont(Font *f)
{
	mFont = f;
}

PUBLIC void ParameterEditor::setSelected(bool b)
{
	if (mSelected != b)
	{
		mSelected = b;
		if (mSelected)
			setBorder(mYesBorder);
		else
			setBorder(mNoBorder);

		// kludge, this can get called when we're rebuilding the editor list
		// but before we've done a relayout of the window, defer the paint
		// until we have a proper size
		if (mBounds.width > 0)
			invalidate();
	}
}

PUBLIC bool ParameterEditor::isSelected()
{
	return mSelected;
}

/**
 * Called as the UI update thread assimilates state changes.
 * Preset::getParameter is SLOW.  Assume we're only being called
 * when something changes.
 *
 */
PUBLIC void ParameterEditor::update()
{
	int ordinal = mExport->getOrdinalValue();
	if (ordinal != mInt)
	{
		mInt = ordinal;

		// map the ordinal to a label
		ExValue value;
		mExport->getOrdinalLabel(mInt, &value);

		// and copy it to our buffer
		CopyString(value.getString(), mValue, sizeof(mValue));

		invalidate();
	}
}

/**
 * Called as the mouse is dragged or the inc/dec functions
 * are called.  Set the internal integer value, and map this
 * back into the symbolic value for display.
 */
PUBLIC void ParameterEditor::setValue(int i)
{
	ExValue value;

	mInt = i;
	mExport->getOrdinalLabel(mInt, &value);
	CopyString(value.getString(), mValue, sizeof(mValue));

	// can we do this here??!!
	// Component::paint();
	invalidate();
}

/**
 * Commit the current value back to a Preset parameter.
 */
PUBLIC void ParameterEditor::commit()
{
	Action *a = mMobius->cloneAction(mAction);
	a->arg.setInt(mInt);
	mMobius->doAction(a);
}

PUBLIC Dimension *ParameterEditor::getPreferredSize(Window *w)
{
	if (mPreferred == NULL && mExport != NULL)
	{
		mPreferred = new Dimension();

		ExportType etype = mExport->getType();

		if (etype == EXP_INT)
		{
			// 8 digits ought to be enough
			w->getTextSize("0", mFont, mPreferred);
			mPreferred->width = (mPreferred->width * 8);
		}
		else if (etype == EXP_BOOLEAN)
		{
			w->getTextSize("MMMM", mFont, mPreferred);
		}
		else if (etype == EXP_ENUM)
		{
			int maxWidth = 0;
			int maxHeight = 0;
			const char **values = mExport->getValueLabels();
			if (values != NULL)
			{
				Dimension d;
				for (int i = 0; values[i] != NULL; i++)
				{
					const char *value = values[i];
					w->getTextSize(value, mFont, &d);
					if (d.width > maxWidth)
						maxWidth = d.width;
					if (d.height > maxHeight)
						maxHeight = d.height;
					// printf("%s %d %d\n", value, d.width, d.height);
				}
			}
			// ugh, some parameters have extra qualifiers, add some
			// extra space (SyncSource)
			// !! should be asking the parameter for the maximum displayable
			// value
			Dimension d;
			w->getTextSize("()", mFont, &d);
			maxWidth += d.width;

			// !! the values are still going to bounce arond because
			// we're not factoring in baseline
			mPreferred->width = maxWidth;
			mPreferred->height = maxHeight;
		}
		else if (etype == EXP_STRING)
		{
			// This is used for Preset and Bindings, config objects
			// that are on a short list and can be accessed by index but which
			// need to be displayed as names.
			// !! Need to be iterating over the value labels and taking
			// the max
			const char *pseudo = "MMMMMMMMMM";
			w->getTextSize(pseudo, mFont, mPreferred);
		}

		// !! ugh, handling of borders as insets is broken
		// and/or inconsistent.  Containers get their insets
		// factored in by the layout manager.  But since we're not
		// a container, that won't happen.  Can do it here but
		// it's ugly to have to remember.  Need to be doing this
		// in a better way.
		addInsets(mPreferred);
	}
	return mPreferred;
}

void ParameterEditor::mousePressed(MouseEvent *e)
{
	// printf("ParameterEditor::mousePressed %d %d\n", e->getX(), e->getY());

	// tell ParameterDisplay about this so it can switch the selection
	ParameterDisplay *pd = (ParameterDisplay *)getParent();
	pd->setSelectedParameter(this);

	mDragging = true;
	mDragOriginX = e->getX();
	mDragOriginY = e->getY();
	mDragStartValue = mInt;
	mDragChanges = 0;

	// let this become the drag target
	e->setClaimed(true);
}

void ParameterEditor::mouseDragged(MouseEvent *e)
{
	int delta = mDragOriginY - e->getY();

	// not quite so sensitive, should base
	// this on the number of increments?
	delta = delta / 10;

	int newValue = mDragStartValue + delta;
	if (newValue < 0)
		newValue = 0;
	if (newValue > mMaxValue)
		newValue = mMaxValue;

	if (newValue != mInt)
	{
		// this will also invalidate & paint
		setValue(newValue);
		mDragChanges++;
		// now or wait till we're done?
		fireActionPerformed();
	}
}

void ParameterEditor::mouseReleased(MouseEvent *e)
{
	// this is what Knob does to let you make incremental
	// changes by clicking on it, I don't like that here, its too
	// easy to make accidental changes
#if 0
	if (mDragChanges == 0) {
		int mousey = e->getY();
		int centery = mBounds.height / 2;

		if (mousey < centery)
          increment();
		else if (mousey > centery)
          decrement();
	}
#endif
	mDragging = false;

	// no, leave these selected all the time
	// setSelected(false);
}

void ParameterEditor::paint(Graphics *g)
{
	tracePaint();
	Bounds b;
	getPaintBounds(&b);

	// This one is special because we don't have insets factored
	// in by LayoutManager, have to apply them dynamically as we draw.
	Insets *insets = getInsets();
	if (insets != NULL)
	{
		b.x += insets->left;
		b.y += insets->top;
		b.width -= (insets->left + insets->right);
		if (b.width < 0)
			b.width = 0;
		b.height -= (insets->top + insets->bottom);
		if (b.height < 0)
			b.height = 0;
	}

	// just drawing the text with a background isn't enough, it only
	// colors the part the txt occupies, have to fill the entire space
	g->setColor(getBackground());
	g->fillRect(b.x, b.y, b.width, b.height);
	g->setFont(mFont);
	g->setBackgroundColor(getBackground());
	g->setColor(getForeground());

	TextMetrics *tm = g->getTextMetrics();
	int baseline = b.y + tm->getAscent();
	g->drawString(mValue, b.x, baseline);
}

void ParameterEditor::increment()
{
	if (mInt < mMaxValue)
	{
		setValue(mInt + 1);
		fireActionPerformed();
	}
}

void ParameterEditor::decrement()
{
	if (mInt > 0)
	{
		setValue(mInt - 1);
		fireActionPerformed();
	}
}

/****************************************************************************
 *                                                                          *
 *   						  PARAMETER DISPLAY                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC ParameterDisplay::ParameterDisplay(MobiusInterface *mob)
{
	mClassName = "ParameterDisplay";
	setType(ParametersElement);

	mMobius = mob;
	mNames = NULL;
	mEditors = NULL;

	FormLayout *fl = new FormLayout();
	fl->setAlign(FORM_LAYOUT_RIGHT);
	setLayout(fl);
	updateEnabled();

	setForeground(GlobalPalette->getColor(COLOR_PARAM_NAME));
}

PUBLIC ParameterDisplay::~ParameterDisplay()
{
	delete mNames;
	delete mEditors;
}

PRIVATE void ParameterDisplay::updateEnabled()
{
	if (!isEnabled())
	{
		removeAll();
		if (mEditors)
			mEditors->clear();
	}
	else if (getComponents() == NULL && mNames != NULL)
	{
		for (int i = 0; i < mNames->size(); i++)
		{
			const char *name = mNames->getString(i);

			Binding b;
			b.setTrigger(TriggerUI);
			b.setTarget(TargetParameter);
			b.setName(name);

			// This one is weird due to display restrictions
			// that we can't get until the Export.

			Action *action = mMobius->resolveAction(&b);
			if (action != NULL)
			{
				action->triggerMode = TriggerModeOnce;
				Export *exp = mMobius->resolveExport(action);
				if (exp != NULL && exp->isDisplayable())
				{
					Label *l = new Label(exp->getDisplayName());
#ifdef OSX
					// Labels are heavyweight on Mac by default because they look better,
					// this fucks something up in our containment hierarchy, probably space
					// looks like they're being embedded into the Window rather than
					// Space's UserPane
					l->setHeavyweight(false);
#endif
					l->setBackground(getBackground());
					l->setForeground(getForeground());
					l->setFont(GlobalFontConfig->intern("parameter", PARAMETER_FONT_SIZE));
					add(l);
					ParameterEditor *pe = new ParameterEditor(mMobius, action, exp);
					pe->addActionListener(this);
					add(pe);
					if (mEditors == NULL)
						mEditors = new List();
					mEditors->add(pe);
					action = NULL;
					exp = NULL;
				}
				delete exp;
				delete action;
			}
		}

		// always auto select the first one?
		setSelectedIndex(0);
	}

	// clear the last preset so we trigger a full refresh on the
	// next update()
}

/**
 * Called by UI whenever the current preset is edited so we can be
 * sure to refresh.
 */
PUBLIC void ParameterDisplay::refresh()
{
	if (isEnabled())
		invalidate();
}

PUBLIC void ParameterDisplay::setEnabled(bool b)
{
	SpaceComponent::setEnabled(b);
	updateEnabled();
}

/**
 * Apply the parameter change to the current track preset.
 */
PUBLIC void ParameterDisplay::actionPerformed(void *src)
{
	ParameterEditor *pe = (ParameterEditor *)src;
	pe->commit();
}

PUBLIC void ParameterDisplay::update(StringList *names)
{
	delete mNames;
	mNames = NULL;
	if (names != NULL)
		mNames = new StringList(names);

	removeAll();
	if (mEditors)
		mEditors->clear();
	updateEnabled();
}

PUBLIC void ParameterDisplay::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;

	if (mEditors != NULL)
	{
		for (int i = 0; i < mEditors->size(); i++)
		{
			ParameterEditor *pe = (ParameterEditor *)mEditors->get(i);
			pe->update();
		}
	}
}

PUBLIC void ParameterDisplay::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
			Container::paint(g);
	}
}

PUBLIC ParameterEditor *ParameterDisplay::getSelectedEditor()
{
	ParameterEditor *editor;
	if (mEditors != NULL)
	{
		for (int i = 0; i < mEditors->size(); i++)
		{
			ParameterEditor *pe = (ParameterEditor *)mEditors->get(i);
			if (pe->isSelected())
			{
				editor = pe;
				break;
			}
		}
	}
	return editor;
}

PUBLIC int ParameterDisplay::getSelectedIndex()
{
	int index = 0;
	if (mEditors != NULL)
		index = mEditors->indexOf(getSelectedEditor());
	return index;
}

PUBLIC void ParameterDisplay::setSelectedIndex(int i)
{
	if (mEditors != NULL && i >= 0 && i < mEditors->size())
		setSelectedParameter((ParameterEditor *)mEditors->get(i));
}

/**
 * Called by the UI in response to key and buton events so
 * have to invalidate.
 */
PRIVATE void ParameterDisplay::setSelectedParameter(ParameterEditor *p)
{
	if (mEditors != NULL)
	{
		for (int i = 0; i < mEditors->size(); i++)
		{
			ParameterEditor *pe = (ParameterEditor *)mEditors->get(i);
			pe->setSelected(pe == p);
		}
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void ParameterDisplay::nextParameter()
{
	setSelectedIndex(getSelectedIndex() + 1);
}

PUBLIC void ParameterDisplay::prevParameter()
{
	setSelectedIndex(getSelectedIndex() - 1);
}

PUBLIC void ParameterDisplay::incParameter()
{
	ParameterEditor *pe = getSelectedEditor();
	if (pe != NULL)
	{
		pe->increment();
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void ParameterDisplay::decParameter()
{
	ParameterEditor *pe = getSelectedEditor();
	if (pe != NULL)
	{
		pe->decrement();
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void ParameterDisplay::layout(Window *w)
{
	// printf("***Before Layout***\n");
	// dump(0);

	Container::layout(w);

	// printf("***After Layout***\n");
	// dump(0);
}

/****************************************************************************
 *                                                                          *
 *                                MINOR MODES                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ModeMarkers::ModeMarkers()
{
	mClassName = "ModeMarkers";
	setType(MinorModesElement);

	mRecording = false;
	mOverdub = false;
	mMute = false;
	mReverse = false;

	mSpeedToggle = 0;
	mSpeedOctave = 0;
	mSpeedStep = 0;
	mSpeedBend = 0;
	mPitchOctave = 0;
	mPitchStep = 0;
	mPitchBend = 0;
	mTimeStretch = 0;

	mTrackSyncMaster = false;
	mOutSyncMaster = false;
	mSolo = false;
	mGlobalMute = false;
	mGlobalPause = false;
	mWindow = false;

	mFont = GlobalFontConfig->intern("minorModes", 12);
}

PUBLIC ModeMarkers::~ModeMarkers()
{
}

PUBLIC Dimension *ModeMarkers::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		Dimension d;
		int em;

		w->getTextSize("M", mFont, &d);
		em = d.width;

		int maxHeight = 0;

		const char *marker = OverdubMode->getDisplayName();
		mPreferred = new Dimension();
		w->getTextSize(marker, mFont, mPreferred);
		if (d.height > maxHeight)
			maxHeight = d.height;

		marker = MuteMode->getDisplayName();
		w->getTextSize(marker, mFont, &d);
		mPreferred->width += d.width;
		if (d.height > maxHeight)
			maxHeight = d.height;

		marker = ReverseMode->getDisplayName();
		w->getTextSize(marker, mFont, &d);
		mPreferred->width += d.width;
		if (d.height > maxHeight)
			maxHeight = d.height;

		marker = CaptureMode->getDisplayName();
		w->getTextSize(marker, mFont, &d);
		mPreferred->width += d.width;
		if (d.height > maxHeight)
			maxHeight = d.height;

		// !! this is way too short if you've got a lot of
		// pitch/speed adjustments, think about truncating rather
		// than trashing space as we do now...

		marker = "SpeedToggle xx Speed Bend xx PitchBend xx ";
		w->getTextSize(marker, mFont, &d);
		mPreferred->width += d.width;
		if (d.height > maxHeight)
			maxHeight = d.height;

		// Also skipping Solo, GlobalMute, and GlobalPause

		// like ParameterDisplay the values will bounce around since
		// we're not doing baselines properly
		mPreferred->height = maxHeight;

		// spacers
		mPreferred->width += (em * 4);

		// then some air for the border
		mPreferred->width += 6;
		mPreferred->height += 4;
	}
	return mPreferred;
}

PUBLIC void ModeMarkers::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;
	LoopState *lstate = tstate->loop;
	bool windowing = (lstate->windowOffset >= 0);

	if (mstate->globalRecording != mRecording ||
		tstate->reverse != mReverse ||
		lstate->overdub != mOverdub ||
		lstate->mute != mMute ||
		tstate->speedToggle != mSpeedToggle ||
		tstate->speedOctave != mSpeedOctave ||
		tstate->speedStep != mSpeedStep ||
		tstate->speedBend != mSpeedBend ||
		tstate->pitchOctave != mPitchOctave ||
		tstate->pitchStep != mPitchStep ||
		tstate->pitchBend != mPitchBend ||
		tstate->timeStretch != mTimeStretch ||

		tstate->outSyncMaster != mOutSyncMaster ||
		tstate->trackSyncMaster != mTrackSyncMaster ||
		tstate->solo != mSolo ||
		tstate->globalMute != mGlobalMute ||
		tstate->globalPause != mGlobalPause ||
		windowing != mWindow)
	{

		mRecording = mstate->globalRecording;
		mReverse = tstate->reverse;
		mOverdub = lstate->overdub;
		mMute = lstate->mute;
		mSpeedToggle = tstate->speedToggle;
		mSpeedOctave = tstate->speedOctave;
		mSpeedStep = tstate->speedStep;
		mSpeedBend = tstate->speedBend;
		mPitchOctave = tstate->pitchOctave;
		mPitchStep = tstate->pitchStep;
		mPitchBend = tstate->pitchBend;
		mTimeStretch = tstate->timeStretch;

		mOutSyncMaster = tstate->outSyncMaster;
		mTrackSyncMaster = tstate->trackSyncMaster;
		mSolo = tstate->solo;
		mGlobalMute = tstate->globalMute;
		mGlobalPause = tstate->globalPause;
		mWindow = windowing;

		if (isEnabled())
			invalidate();
	}
}

PUBLIC void ModeMarkers::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{
			Bounds b;
			getPaintBounds(&b);

			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);
			g->setColor(getForeground());

			// don't really like the border once it is in position
			// g->drawRect(b.x, b.y, b.width, b.height);

			char buf[128];
			buf[0] = 0;

			if (mOverdub)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, OverdubMode->getDisplayName());
			}

			if (mMute)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, MuteMode->getDisplayName());
			}

			if (mReverse)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, ReverseMode->getDisplayName());
			}

			// !! we have Mode objects for these with display
			// names, should use them

			if (mSpeedOctave != 0)
			{
				char speed[128];
				sprintf(speed, "SpeedOct %d", mSpeedOctave);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, speed);
			}

			if (mSpeedStep != 0)
			{
				// factor out the toggle since they
				// are percived at different things
				int step = mSpeedStep - mSpeedToggle;
				if (step != 0)
				{
					char speed[128];
					sprintf(speed, "SpeedStep %d", step);
					if (strlen(buf))
						strcat(buf, " ");
					strcat(buf, speed);
				}
			}

			if (mSpeedToggle != 0)
			{
				char speed[128];
				sprintf(speed, "SpeedToggle %d", mSpeedToggle);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, speed);
			}

			// This can also be a knob so we don't need
			// this but I'm not sure people want to waste
			// space for a knob that's too fine grained
			// to use from the UI anyway.
			if (mSpeedBend != 0)
			{
				char speed[128];
				sprintf(speed, "SpeedBend %d", mSpeedBend);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, speed);
			}

			if (mPitchOctave != 0)
			{
				char speed[128];
				sprintf(speed, "PitchOctave %d", mPitchOctave);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, speed);
			}

			if (mPitchStep != 0)
			{
				char pitch[128];
				sprintf(pitch, "PitchStep %d", mPitchOctave);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, pitch);
			}

			if (mPitchBend != 0)
			{
				char pitch[128];
				sprintf(pitch, "PitchBend %d", mPitchBend);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, pitch);
			}

			if (mTimeStretch != 0)
			{
				char stretch[128];
				sprintf(stretch, "TimeStretch %d", mTimeStretch);
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, stretch);
			}

			if (mTrackSyncMaster && mOutSyncMaster)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, SyncMasterMode->getDisplayName());
			}
			else if (mTrackSyncMaster)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, TrackSyncMasterMode->getDisplayName());
			}
			else if (mOutSyncMaster)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, MIDISyncMasterMode->getDisplayName());
			}

			if (mRecording)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, CaptureMode->getDisplayName());
			}

			if (mSolo)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, SoloMode->getDisplayName());
			}

			// this is a weird one, it will be set during Solo too...
			if (mGlobalMute && !mSolo)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, GlobalMuteMode->getDisplayName());
			}

			if (mGlobalPause)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, GlobalPauseMode->getDisplayName());
			}

			if (mWindow)
			{
				if (strlen(buf))
					strcat(buf, " ");
				strcat(buf, WindowMode->getDisplayName());
			}

			g->setBackgroundColor(getBackground());
			// g->setColor(Color::White);
			g->setFont(mFont);
			int left = b.x + 3;
			TextMetrics *tm = g->getTextMetrics();
			int top = b.y + 2 + tm->getAscent();
			g->drawString(buf, left, top);
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							 SYNC STATUS                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC SyncMarkers::SyncMarkers()
{
	mClassName = "SyncMarkers";
	setType(SyncStatusElement);

	mTempo = 0;
	mBeat = 0;
	mBar = 0;
	mDoBeat = false;
	mDoBar = false;
	mFont = GlobalFontConfig->intern("sync", 12);
}

PUBLIC SyncMarkers::~SyncMarkers()
{
}

PUBLIC Dimension *SyncMarkers::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		w->getTextSize("Tempo 000.0 Bar 0000 Beat 00", mFont, mPreferred);
		// then some air for the border
		mPreferred->width += 6;
		mPreferred->height += 4;
	}
	return mPreferred;
}

PUBLIC void SyncMarkers::update(MobiusState *mstate)
{
	TrackState *tstate = mstate->track;
	bool doBeat = false;
	bool doBar = false;

	SyncSource src = tstate->syncSource;

	if (src == SYNC_MIDI || src == SYNC_HOST)
		doBeat = true;

	// originally did this only for SYNC_UNIT_BAR but if we're beat
	// syncing it still makes sense to see the bar to know where we are,
	// especially if we're wrapping the beat
	// if (tstate->syncUnit == SYNC_UNIT_BAR)
	if (src == SYNC_MIDI || src == SYNC_HOST)
		doBar = true;

	// normalize tempo to two decimal places to reduce jitter
	int newTempo = (int)(tstate->tempo * 100.0f);

	if (newTempo != mTempo ||
		doBeat != mDoBeat ||
		doBar != mDoBar ||
		(doBeat && (tstate->beat != mBeat)) ||
		(doBar && (tstate->bar != mBar)))
	{

		mTempo = (float)newTempo;
		mDoBeat = doBeat;
		mDoBar = doBar;
		mBeat = tstate->beat;
		mBar = tstate->bar;
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void SyncMarkers::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{
			Bounds b;
			getPaintBounds(&b);

			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);

			// TODO: we've got two decimal places of precision, only
			// show one
			int tempo = (int)((float)mTempo / 100.0f);
			// this gets you the fraction * 100
			int frac = (int)(mTempo - (tempo * 100));
			// cut off the hundredths
			frac /= 10;

			if (tempo > 0)
			{
				g->setColor(getForeground());
				g->setBackgroundColor(getBackground());
				g->setFont(mFont);

				// don't really like the border once it is in position
				// g->drawRect(b.x, b.y, b.width, b.height);

				// !! use catalogs here

				// note that if mBeat is zero it should not be displayed
				// if mBeat is zero, then mBar will also be zero

				char buf[128];
				if (!mDoBeat || mBeat == 0)
					sprintf(buf, "Tempo %d.%d", tempo, frac);
				else if (mDoBar)
					sprintf(buf, "Tempo %d.%d Bar %d Beat %d",
							tempo, frac, mBar, mBeat);
				else
					sprintf(buf, "Tempo %d.%d Beat %d", tempo, frac, mBeat);

				int left = b.x + 3;
				TextMetrics *tm = g->getTextMetrics();
				int top = b.y + 2 + tm->getAscent();
				g->drawString(buf, left, top);
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							 AUDIO METER                                *
 *                                                                          *
 ****************************************************************************/

// #define AMETER_PREFERRED_WIDTH 200
// #define AMETER_PREFERRED_HEIGHT 10

// Ok, let's try to imporve the AudioMeter with the Peak!

#define AMETER_PREFERRED_WIDTH 400
#define AMETER_PREFERRED_HEIGHT 50 // #010 Audio Meter Height Default

PUBLIC AudioMeter::AudioMeter()
{
	mClassName = "AudioMeter";
	setType(AudioMeterElement);

	mRange = 127; //<-- C: MaxVal
	mValue = 0;
	mLevel = 0;
	mRequiredSize = NULL;
	mPeakLevel = NULL; // #015
	mPeakWidth = 10;

	// Dunque - abbassare il MaxRange ci permette di vedere di più la zona bassa
	// ma in questo modo taglio troppo sull'alto e poi per come era fatto
	// in pratica i valori over venivano semplicemente ignorati e rimaneva disegnato il vecchio
	// così non vedevi in realtà i picchi... ora ho messo che se > imposta a max... un po' meglio

	// this seems to be too sensitive, need a trim control?
	// mMeter->setRange((1024 * 32) - 1);
	setRange((1024 * 8) - 1); // MOBUS 2.5 - Prima era così, provo restore per mValue molto sopra range

	// setRange((1024 * 32) - 1);  //cas Test *16 (sarebbe da capre davvero quale è maxValue e poi fare una Curva EXP db

	// foreground color is used for the border, this
	// is used for the meter
	mMeterColor = GlobalPalette->getColor(COLOR_METER, Color::White);
}

PUBLIC AudioMeter::~AudioMeter()
{
	delete mRequiredSize;
}

/**
 * Because AudioMeter is a SpaceComponent which is a Container,
 * LayoutManager will call setPreferredSize(NULL) during layout
 * and we will lose the preferred size.  Probably should fix
 * qwin and/or SpaceCompoennt to not trash the preferred size, but
 * until then keep a seperate copy over here.
 */
PUBLIC void AudioMeter::setRequiredSize(Dimension *d)
{
	delete mRequiredSize;
	mRequiredSize = d;
}

PUBLIC void AudioMeter::setRange(int i)
{
	mRange = i;
	if (mValue > mRange) // fix OutOfBound value
		setValue(0);
}

PUBLIC void AudioMeter::update(int i)
{
	setValue(i);
}

PUBLIC void AudioMeter::setValue(int i)
{
	// if(i > 0)		//
	//	Trace(3, "AudioMeter setValue %i|%i , peak %i", i, mRange, mPeakLevel);

	if ((mValue != i && i >= 0 && i <= mRange) || i >= mRange || mPeakLevel > 0) // bug over mRange?  //C
	// if (mValue != i && i >= 0 && i <= mRange) //<--- here discard value over mRange... but is not right!
	{							   // value over range should be treat as UpperBound, not ingored...
		if (mValue != i && i >= 0) // update only if in bound //C
			mValue = i;

		if (mValue >= mRange) // upperlevel
			mValue = mRange;

		// typically get a lot of low level noise which flutters
		// the value but is not actually visible, remember the last
		// level and don't repaint unless it changes
		int width = mBounds.width - 4;
		int level = (int)(((float)width / (float)mRange) * mValue);

		// if (level != mLevel)
		if (level != mLevel || mPeakLevel > 0) // C
		{
			mLevel = level; // C: Here set the mLevel, but recalculate below the same value twice?

			if (isEnabled())
				invalidate();
		}

		// #015 Here I should calculate the new PeakLevel (consider the decay)
		// if the newPeakLevel is different, I shuold invalidate... but probably
		// can approximate it (and disregard this case) (tbi)
	}
}

#define peak

PUBLIC void AudioMeter::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();

		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{

			Bounds b;
			getPaintBounds(&b);

#if OldCode + Decay with BAR

			// printf("AudioMeter::paint %d %d %d %d\n", b.x, b.y, b.width, b.height);

			// Fill for draw border? (cause flickering sometime...)
			g->setColor(getForeground());
			g->drawRect(b.x, b.y, b.width, b.height); // #016 Fix flickering background AudioMeter (draw rect, not Fill)
			// g->fillRect(b.x, b.y, b.width, b.height);

			b.x += 2;
			b.y += 2;
			b.width -= 4;
			b.height -= 4;

			mLevel = (int)(((float)b.width / (float)mRange) * mValue); //<--- C: here another mLevel calc same as above? Probalby can remove this line

			if (mLevel > 0)
			{
				g->setColor(mMeterColor);
				g->fillRect(b.x, b.y, mLevel, b.height);
			}

			// Paint the background at right [smart, not repaint all backcolor]
			g->setColor(getBackground());
			// g->fillRect(b.x + mLevel, b.y, b.width - mLevel, b.height);
			g->fillRect(b.x + mLevel, b.y, b.width - mLevel, b.height); // When is 100% kill the last pixel?

			// #015 Draw the mMaxLocalValue [ClaudioCas]
			// Update Peak Level
			if (mLevel > mPeakLevel)
				mPeakLevel = mLevel;
			else
				mPeakLevel -= 10; // decay  //Dovrebbe essere proporzionale al width

			// oob
			if (mPeakLevel + mPeakWidth > b.width)
				mPeakLevel = b.width - mPeakWidth; // set to max, not overbound

			if (mPeakLevel > 0) // solo se > 0?
			{
				g->setColor(Color::White);								  // Test
				g->fillRect(b.x + mPeakLevel, b.y, mPeakWidth, b.height); // PeakWidth
			}

#endif

			// Version with meter persisten with decay

			// printf("AudioMeter::paint %d %d %d %d\n", b.x, b.y, b.width, b.height);

			// Fill for draw border? (cause flickering sometime...)
			g->setColor(getForeground());
			g->drawRect(b.x, b.y, b.width, b.height); // #016 Fix flickering background AudioMeter (draw rect, not Fill)

			// innerBound (remove border)
			b.x += 2;
			b.y += 2;
			b.width -= 4;
			b.height -= 4;

			mLevel = (int)(((float)b.width / (float)mRange) * mValue); //<--- C: here another mLevel calc same as above? Probalby can remove this line

			if (mLevel >= mPeakLevel)
				mPeakLevel = mLevel;
			else
			{
				mPeakLevel -= 15; // decay  //Dovrebbe essere proporzionale al width e anche dinamico (che accelera)
				if (mPeakLevel < 0)
					mPeakLevel = 0;
			}

			if (mPeakLevel > 0)
			{
				g->setColor(mMeterColor);
				g->fillRect(b.x, b.y, mPeakLevel, b.height);
			}

			Trace(3, "AudioMeter mLevel %i, mRange %i, mValue %i, mPeak %i", mLevel, mRange, mValue, mPeakLevel);

			// Paint the background at right [smart, not repaint all backcolor]
			g->setColor(getBackground());
			g->fillRect(b.x + mPeakLevel, b.y, b.width - mPeakLevel, b.height);

			//if (b.x + mPeakLevel < b.width)
			//{
				//g->fillRect(b.x + mPeakLevel, b.y, b.width - mPeakLevel, b.height); // When is 100% kill the last pixel?
			//}

			// Get Original Bound and draw external border
			// getPaintBounds(&b);
		}
	}
}


//long intToDB(long x, long in_min, long in_max, long out_min, long out_max)
//{
	//return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;

	// da X32 mixer
// // float to dB // “f” represents OSC float data. f: [0.0, 1.0]
//  // “d” represents the dB float data. d:[-oo, +10]
//  float floatToDB(float f)
//  {
//   float d;
//   if (f >= 0.5)          
//    d = f * 40. - 30.;      
//    // max dB value: +10.     
//   else if (f >= 0.25)    
//    d = f * 80. - 50.;     
//   else if (f >= 0.0625)  
//    d = f * 160. - 70.;   
//   else if (f >= 0.0)  
//    d = f * 480. - 90.; 

//    return d;
//  }

  
//}





PUBLIC Dimension *AudioMeter::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		if (mRequiredSize != NULL)
		{
			mPreferred = new Dimension();
			mPreferred->width = mRequiredSize->width;
			mPreferred->height = mRequiredSize->height;
		}
		else
		{
			mPreferred = new Dimension();
			mPreferred->width = AMETER_PREFERRED_WIDTH;
			mPreferred->height = AMETER_PREFERRED_HEIGHT;
		}
	}
	return mPreferred;
}

PUBLIC void AudioMeter::dumpLocal(int indent)
{
	dumpType(indent, "AudioMeter");
}

/****************************************************************************
 *                                                                          *
 *                                LOOP WINDOW                               *
 *                                                                          *
 ****************************************************************************/

#define LWINDOW_PREFERRED_WIDTH 200
#define LWINDOW_PREFERRED_HEIGHT 20

PUBLIC LoopWindow::LoopWindow()
{
	mClassName = "LoopWindow";
	setType(LoopWindowElement);

	mWindowOffset = -1;
	mWindowFrames = 0; 
	mHistoryFrames = 0;

	// foreground color is used for the border, this
	// is used for the meter
	mWindowColor = GlobalPalette->getColor(COLOR_LOOP_WINDOW, Color::Red);
}

PUBLIC LoopWindow::~LoopWindow()
{
}

PUBLIC Dimension *LoopWindow::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();
		mPreferred->width = LWINDOW_PREFERRED_WIDTH;
		mPreferred->height = LWINDOW_PREFERRED_HEIGHT;
	}
	return mPreferred;
}

PUBLIC void LoopWindow::update(MobiusState *s)
{
	LoopState *l = s->track->loop;

	if (mWindowOffset != l->windowOffset ||
		mWindowFrames != l->frames ||
		mHistoryFrames != l->historyFrames)
	{

		mWindowOffset = l->windowOffset;
		mWindowFrames = l->frames;
		mHistoryFrames = l->historyFrames;
		if (isEnabled())
			invalidate();
	}
}

PUBLIC void LoopWindow::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();

		if (Space::isDragging())
			drawMoveBorder(g);
		else
		{
			Bounds b;
			getPaintBounds(&b);

			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);

			if (mWindowOffset >= 0 && mHistoryFrames > 0)
			{

				b.x += 2;
				b.y += 2;
				b.width -= 4;
				b.height -= 4;

				g->setColor(getForeground());
				g->drawRect(b.x, b.y, b.width, b.height);

				float max = (float)mHistoryFrames;
				float relstart = (float)mWindowOffset / max;
				float relwidth = (float)mWindowFrames / max;
				float fwidth = (float)b.width;
				int xoffset = (int)(fwidth * relstart);
				int width = (int)(fwidth * relwidth);

				// always show something if the window is very small
				if (width < 2)
					width = 2;

				// don't let this trash the border should be off by at most one
				if (xoffset + width > b.width)
				{
					width = b.width - xoffset;
					if (width < 2)
					{
						// odd, very small and at the end
						xoffset = b.width - 2;
						width = 2;
					}
				}

				g->setColor(mWindowColor);
				g->fillRect(b.x + xoffset, b.y, width, b.height);
			}
		}
	}
}

PUBLIC void LoopWindow::dumpLocal(int indent)
{
	dumpType(indent, "LoopWindow");
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP STACK                                *
 *                                                                          *
 ****************************************************************************/
/*
 * A TrackStrip component that displays the status of each loop in the
 * track.  Loops are arranged in a vertical stack with a horizontal rectangle
 * representing the loop.
 */

#define LOOP_STACK_CELL_HEIGHT 16
#define LOOP_STACK_CELL_WIDTH 100

/**
 * Offset from the left to the loop number.
 */
#define LOOP_STACK_TAB1 4

/**
 * Offset from the left to the loop bar, leaving room for the
 * digit and some space.
 */
#define LOOP_STACK_TAB2 20

LoopStack::LoopStack(MobiusInterface *m, int track)
{
	mClassName = "LoopStack";
	addMouseListener(this);

	mMobius = m;
	mFont = GlobalFontConfig->intern("loopStack", 12);

	// same color selections we have in several places, need to share!

	mColor = GlobalPalette->getColor(COLOR_METER, Color::White);
	mSlowColor = GlobalPalette->getColor(COLOR_SLOW_METER, Color::Gray);
	mMuteColor = GlobalPalette->getColor(COLOR_MUTE_METER, Color::Blue);

	// !! need configurable color
	mActiveColor = Color::White;
	mPendingColor = Color::Red;

	for (int i = 0; i < LOOP_STACK_MAX_LOOPS; i++)
	{
		LoopStackState *s = &mLoops[i];
		s->cycles = 0;
		s->mute = false;
		s->speed = false;
		s->active = false;
	}

	MobiusConfig *config = m->getConfiguration();
	mMaxLoops = config->getMaxLoops();
	if (mMaxLoops > LOOP_STACK_MAX_LOOPS)
		mMaxLoops = LOOP_STACK_MAX_LOOPS;

	// will be updated per track later
	mLoopCount = 0;

	// initialize the stub action
	mAction = m->newAction();
	mAction->setFunction(LoopN);
	mAction->setTargetTrack(track);
	// Trigger id will be the address of the component
	mAction->id = (long)this;
	mAction->trigger = TriggerUI;
	// we're not passing down up transitions of the mouse button
	mAction->triggerMode = TriggerModeOnce;
}

LoopStack::~LoopStack()
{
	delete mAction;
}

PUBLIC Dimension *LoopStack::getPreferredSize(Window *w)
{
	if (mPreferred == NULL)
	{
		mPreferred = new Dimension();

		mPreferred->width = LOOP_STACK_CELL_WIDTH;
		mPreferred->height = LOOP_STACK_CELL_HEIGHT * mMaxLoops;
	}
	return mPreferred;
}

void LoopStack::mousePressed(MouseEvent *e)
{
	// printf("LoopStack::mousePressed %d %d\n", e->getX(), e->getY());

	int loop = e->getY() / LOOP_STACK_CELL_HEIGHT;

	Action *a = mMobius->cloneAction(mAction);
	// these are expected to be 1 based
	a->arg.setInt(loop + 1);
	mMobius->doAction(a);
}

void LoopStack::update(MobiusState *mstate)
{
	// don't change anything if we're dragging components around,
	// some child components (Thermometer) don't know to not paint themselves
	// on a update
	// UPDATE: is this still true ??!!
	if (Space::isDragging())
		return;

	TrackState *tstate = mstate->track;
	LoopSummary *summaries = tstate->summaries;
	int currentLoops = tstate->summaryCount;
	bool changes = false;

	if (currentLoops > mMaxLoops)
		currentLoops = mMaxLoops;

	if (mLoopCount != currentLoops)
		changes = true;
	else
	{
		for (int i = 0; i < currentLoops; i++)
		{
			LoopStackState *cur = &mLoops[i];
			LoopSummary *neu = &summaries[i];

			// to us a cycle count of zero means empty, MobiusState
			// always returns a cycles=1 and frames=0 to mean empty
			int newCycles = (neu->frames > 0) ? neu->cycles : 0;

			// does mute make sense if it isn't active?
			if (cur->cycles != newCycles ||
				cur->mute != neu->mute ||
				cur->speed != neu->speed ||
				cur->active != neu->active ||
				cur->pending != neu->pending)
			{
				changes = true;
				break;
			}
		}
	}

	if (changes)
	{
		mLoopCount = currentLoops;
		for (int i = 0; i < currentLoops; i++)
		{
			LoopStackState *cur = &mLoops[i];
			LoopSummary *neu = &summaries[i];
			int newCycles = (neu->frames > 0) ? neu->cycles : 0;
			cur->cycles = newCycles;
			cur->mute = neu->mute;
			cur->speed = neu->speed;
			cur->active = neu->active;
			cur->pending = neu->pending;
		}
		if (isEnabled())
			invalidate();
	}
}

void LoopStack::paint(Graphics *g)
{
	if (isEnabled())
	{
		tracePaint();
		if (Space::isDragging())
			drawMoveBorder(g);

		else
		{
			TextMetrics *tm = g->getTextMetrics();
			Bounds b;
			char buffer[128];

			// clear
			// !! seems to be trashing the track strip border
			getPaintBounds(&b);
			g->setColor(getBackground());
			g->fillRect(b.x, b.y, b.width, b.height);

			int cellTop = b.y;
			int barLeft = b.x + LOOP_STACK_TAB2;
			int barWidth = b.width - LOOP_STACK_TAB2 - 4;
			int barVoffset = 2;
			int barHeight = LOOP_STACK_CELL_HEIGHT - 2;

			for (int i = 0; i < mMaxLoops; i++)
			{
				LoopStackState *s = &mLoops[i];

				// always a number
				// well, some people want a color to designate the
				// non-active loops, so I guess we can just suppress the number
				if (i < mLoopCount)
				{
					Color *c = (s->active) ? mActiveColor : mColor;
					sprintf(buffer, "%d", i + 1);
					g->setColor(c);
					g->setBackgroundColor(getBackground());
					g->setFont(mFont);
					g->drawString(buffer, b.x, cellTop + tm->getAscent());
				}

				// maybe a bar
				if (i < mLoopCount)
				{
					if (s->cycles > 0 || s->active || s->pending)
					{
						Color *c = mColor;
						if (s->mute)
							c = mMuteColor;
						else if (s->speed)
							c = mSlowColor;

						int left = barLeft;
						int top = cellTop + 2;
						int width = barWidth;
						int height = barHeight;

						if (s->active || s->pending)
						{
							Color *border =
								(s->active ? mActiveColor : mPendingColor);
							g->setColor(border);
							g->fillRect(left, top, width, height);
							left += 2;
							top += 2;
							width -= 4;
							height -= 4;
						}

						// normally we don't draw a bar if the loop is empty, but
						// if this is the active loop, we still need a border,
						// change the color so it looks hollow
						if (s->cycles == 0)
							c = getBackground();

						g->setColor(c);
						g->fillRect(left, top, width, height);
					}
				}

				cellTop += LOOP_STACK_CELL_HEIGHT;
			}
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
