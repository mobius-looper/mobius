/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * All of the stock layout managers.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							 NULL LAYOUT                                *
 *                                                                          *
 ****************************************************************************/
/*
 * SUBTLETY: If the container has insets, these will be added to the
 * location of the components after layout.  If you layout again, we need
 * to remove this adjustment, otherwise the insets will be added again,
 * and the size will grow on every layout.
 * 
 * But note that we have to maintain state to know whether the insets
 * have been applied.
 */

PUBLIC Dimension* NullLayout::nullPreferredLayoutSize(Container *container,
													  Window* w)
{
	Dimension *d = new Dimension();

	container->trace("NullLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		int w = c->getX() + ps->width;
		if (w > d->width)
		  d->width = w;

		int h = c->getY() + ps->height;
		if (h > d->height)
		  d->height = h;
	}
    
    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

/**
 * Here we simply set all component sizes to their preferred size.
 */
PUBLIC void NullLayout::nullLayoutContainer(Container* container, Window* w)
{
    Insets* insets = container->getInsets();

	container->trace("NullLayout::layoutContainer");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		c->setSize(c->getPreferredSize(w));
        if (insets != NULL)
          c->setLocation(c->getX() + insets->left, c->getY() + insets->top);
		c->layout(w);
	}

	container->decTraceLevel();
}

/****************************************************************************
 *                                                                          *
 *   							STACK LAYOUT                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Special for TabbedPane.  Assume that only one component will be 
 * visible at a time, calculate the maximum size of the components.
 * Unlike NullLayout, these will be reoriented at 0,0 each time, so 
 * you can use insets.
 */

PUBLIC Dimension* StackLayout::preferredLayoutSize(Container *container,
												   Window* w)
{
	Dimension *d = new Dimension();

	container->trace("StackLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		int w = ps->width;
		if (w > d->width)
		  d->width = w;

		int h = ps->height;
		if (h > d->height)
		  d->height = h;
	}
    
    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

/**
 * Here we simply set all component sizes to their preferred size.
 */
PUBLIC void StackLayout::layoutContainer(Container* container, Window* w)
{
    Insets* insets = container->getInsets();
	int left = (insets != NULL) ? insets->left : 0;
	int top = (insets != NULL) ? insets->top : 0;
	
	container->trace("StackLayout::layoutContainer");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->setBounds(left, top, ps->width, ps->height);
		c->layout(w);
	}

	container->decTraceLevel();
}

/****************************************************************************
 *                                                                          *
 *                               LINEAR LAYOUT                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC void LinearLayout::init() 
{
    mGap = 0;
	mPreGap = 0;
	mPostGap = 0;
    mCenterX = false;
    mCenterY = false;
}

PUBLIC void LinearLayout::setGap(int i) 
{
	mGap = i;
}

PUBLIC void LinearLayout::setPreGap(int i) 
{
	mPreGap = i;
}

PUBLIC void LinearLayout::setPostGap(int i) 
{
	mPostGap = i;
}

PUBLIC void LinearLayout::setCenterX(bool b) 
{
    mCenterX = b;
}

PUBLIC void LinearLayout::setCenterY(bool b) 
{
    mCenterY = b;
}

/****************************************************************************
 *                                                                          *
 *                              VERTICAL LAYOUT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC VerticalLayout::VerticalLayout() 
{
    init();
}

PUBLIC VerticalLayout::VerticalLayout(int gap) 
{
    init();
    setGap(gap);
}

PUBLIC VerticalLayout::~VerticalLayout() {
}

PUBLIC Dimension* VerticalLayout::preferredLayoutSize(Container* container,
													  Window* w) 
{
	Dimension* d = new Dimension();

	d->height = mPreGap;

	container->trace("VerticalLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
		 c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		if (ps->width > d->width)
		  d->width = ps->width;
		d->height += ps->height;
		if (c->getNext() != NULL)
		  d->height += mGap;
	}

	d->height += mPostGap;

    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

PUBLIC void VerticalLayout::layoutContainer(Container* container,
											Window* w) 
{
    Insets* insets = container->getInsets();
    int left = (insets != NULL) ? insets->left : 0;
    int top = (insets != NULL) ? insets->top : 0;
    int maxwidth = container->getWidth();

	container->trace("VerticalLayout::layoutContainer");
	container->incTraceLevel();

    top += mPreGap;

    if (mCenterX || mCenterY) {
        int totalHeight = 0;
        for (Component* c = container->getComponents() ; c != NULL ;
             c = c->getNext()) {
            Dimension* ps = c->getPreferredSize(w);
            totalHeight += ps->height;
            if (c->getNext() != NULL)
              totalHeight += mGap;
            if (ps->width > maxwidth)
              maxwidth = ps->width;
        }
        if (mCenterY) {
            int extra = container->getHeight() - totalHeight - mPreGap - mPostGap;
            if (extra > 0)
              top += (extra / 2);
        }
    }

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {
		Dimension* ps = c->getPreferredSize(w);
        
        int cleft = left;
        if (mCenterX) 
          cleft = left + ((maxwidth - ps->width) / 2);

		c->setBounds(cleft, top, ps->width, ps->height);

        top += ps->height + mGap;
		c->layout(w);
    }

	container->decTraceLevel();
}

/****************************************************************************
 *                                                                          *
 *                             HORIZONTAL LAYOUT                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC HorizontalLayout::HorizontalLayout() 
{
    init();
}

PUBLIC HorizontalLayout::HorizontalLayout(int gap) 
{
    init();
    setGap(gap);
}

PUBLIC HorizontalLayout::~HorizontalLayout() 
{
}

PUBLIC Dimension* HorizontalLayout::preferredLayoutSize(Container* container,
														Window* w) 
{
	Dimension* d = new Dimension();

	d->width = mPreGap;

	container->trace("HorizontalLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		if (ps->height > d->height)
		  d->height = ps->height;
		d->width += ps->width;
		if (c->getNext() != NULL)
		  d->width += mGap;
	}

	d->width += mPostGap;

    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

PUBLIC void HorizontalLayout::layoutContainer(Container* container,
											  Window* w) 
{
    Insets* insets = container->getInsets();
    int left = (insets != NULL) ? insets->left : 0;
    int top = (insets != NULL) ? insets->top : 0;
    int maxheight = container->getHeight();

    left += mPreGap;

	container->trace("HorizontalLayout::layoutContainer");
	container->incTraceLevel();

    if (mCenterX || mCenterY) {
        int totalWidth = 0;
        for (Component* c = container->getComponents() ; c != NULL ; 
             c = c->getNext()) {
            Dimension* ps = c->getPreferredSize(w);
            totalWidth += ps->width;
            if (c->getNext() != NULL)
              totalWidth += mGap;
            if (ps->height > maxheight)
              maxheight = ps->height;
        }
        if (mCenterX) {
            int extra = container->getWidth() - totalWidth - mPreGap - mPostGap;
            if (extra > 0)
              left += (extra / 2);
        }
    }

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {
		Dimension *ps = c->getPreferredSize(w);

        int ctop = top;
        if (mCenterY)
          ctop = top + ((maxheight - ps->height) / 2);

		c->setBounds(left, ctop, ps->width, ps->height);

        left += ps->width + mGap;
		c->layout(w);
    }

	container->decTraceLevel();
}


/****************************************************************************
 *                                                                          *
 *                                FLOW LAYOUT                               *
 *                                                                          *
 ****************************************************************************/
/*
 * Align components in a left-to-right flow, with as many components
 * as will fit on a line.  This may result in multiple lines.  Each
 * line may be centered.  Since there can be more than one line we support
 * both a horizontal and vertical gap.
 *
 * This is a funny one because we have to pay attention to the containers
 * current size and don't ask for more.  I'm not sure how exactly Swing
 * implements this.  The preferredLayoutSize method looks like it always
 * assumes everything will fit on one line, but later in layoutContainer
 * it splits it up into several lines based on the actual size.  But this
 * delayed change of size can screw up the container's layout if it
 * had made decisions based on the preferredLayoutSize.  BorderLayout
 * for example would have to shift everything around to adjust for
 * the new size.  In Swing, when a FlowLayout panel is inside a BorderLayout
 * north region, the flow layout seems to be on a single line and it just
 * truncates the components on the right.  This sort of makes sense since
 * border layout has already sized its regions.  But if you put the FlowLayout
 * in the center region, it breaks up as expected.  
 *
 * Here, I'm going to kludge it to work a certain way to support
 * Mobius.  I'd like this to be more general, though...
 *
 */

PRIVATE void FlowLayout::initFlowLayout()
{
    mAlign = FLOW_LAYOUT_CENTER;
    mHGap = 5;
    mVGap = 5;
}

PUBLIC FlowLayout::FlowLayout() {
    initFlowLayout();
}

PUBLIC FlowLayout::FlowLayout(int align) {
    initFlowLayout();
    mAlign = align;
}

PUBLIC FlowLayout::FlowLayout(int align, int hgap, int vgap) {
    initFlowLayout();
    mAlign = align;
    mHGap = hgap;
    mVGap = vgap;
}

PUBLIC FlowLayout::~FlowLayout() {
}

/**
 * Hmm, now sure how flow layout does this.  We won't know the container's
 * actual size yet, in this implementation it will always be zero because
 * we're working bottom up.  So we have to assume that there is
 * infinite size available.
 *
 * !! If the container is in fact constrained, we have to take
 * that into account or else we'll render in less space than we
 * actually have.  I really don't follow how the Swing code does this.
 * Assume that if the container has a non-zero height that we need to 
 * obey it.
 */
PUBLIC Dimension* FlowLayout::preferredLayoutSize(Container* container,
												  Window* w) 
{
	Bounds* b = container->getBounds();
	
	bool overflow = false;
	int lineWidth = 0;
	int lineHeight = 0;
	int lineOffset = 0;
	int lineMaxWidth = 0;

	container->trace("FlowLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL && !overflow ; 
         c = c->getNext()) {

		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		if ((b->height > 0) && (lineOffset + ps->height > b->height)) {
			// too tall to fit in the remaining space, stop now
			overflow = true;
		}
		else {
			// accumulate max height
			if (ps->height > lineHeight)
			  lineHeight = ps->height;

			// add another component to the line
			int width = ps->width + ((lineWidth > 0) ? mHGap : 0);
			if ((b->width == 0) || (lineWidth + width <= b->width)) {
				// still room on this line
				lineWidth += width;
				if (lineWidth > lineMaxWidth)
				  lineMaxWidth = lineWidth;
			}
			else if (lineWidth == 0) {
				// not enough room for the first component, stop now
				overflow = true;
			}
			else {
				// add another line
				lineOffset += lineHeight;
				lineHeight = ps->height;
				lineWidth = ps->width;

				if ((b->height == 0) || 
					(lineOffset + mVGap <= b->height))
					lineOffset += mVGap;
				else {
					// not enough room for the vgap, so must stop now
					overflow = true;
				}
			}
		}
	}

	// factor in the last line
	lineOffset += lineHeight;

    Dimension* d = new Dimension(lineMaxWidth, lineOffset);
    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

/**
 * We may have less space than than we asked for, but setBounds
 * from our parent container will normally just assume
 * a single line.  Not sure how Swing handles this, maybe
 * it just goes ahead and moves the peers, but doesn't bother
 * to update the component height?
 */
PUBLIC void FlowLayout::layoutContainer(Container* container,
										Window* w) 
{
	Dimension* b = container->getBounds();
    Insets* insets = container->getInsets();
    int maxWidth = b->width;
    int maxHeight = b->height;
    int left = 0;
    int top = 0;

	container->trace("FlowLayout::layoutContainer");
	container->incTraceLevel();

    if (insets != NULL) {
        left = insets->left;
        top = insets->top;
        maxWidth -= (insets->left + insets->right);
        maxHeight -= (insets->top + insets->bottom);
    }

	Component* first = NULL;
	Component* last = NULL;
	bool overflow = false;
	int lineWidth = 0;
	int lineHeight = 0;

	// Formerly checked maxHeight here, but we have
	// to allow this to extend the height, not exactly
	// sure how swing handles this.  If this is always true, then
	// take the check logic out below.
	maxHeight = 100000;

    for (Component* c = container->getComponents() ; c != NULL && !overflow ; 
         c = c->getNext()) {
		Dimension* ps = c->getPreferredSize(w);

		if (top + ps->height > maxHeight) {
			// too tall to fit in the remaining space, stop now
			overflow = true;
		}
		else {
			if (first == NULL) first = c;

			// accumulate max height
			if (ps->height > lineHeight)
				lineHeight = ps->height;

			// add another component to the line
			int width = ps->width + ((lineWidth > 0) ? mHGap : 0);
			if (lineWidth + width <= maxWidth) {
				// still room on this line
				lineWidth += width;
				last = c;
			}
			else if (lineWidth == 0) {
				// not enough room for the first component, stop now
				overflow = true;
			}
			else {
				// add another line
				adjustBounds(w, left, top, lineWidth, lineHeight, maxWidth,
							 first, last);

				top += lineHeight;
				if (top + mVGap <= maxHeight)
					top += mVGap;
				else {
					// not enough room for the vgap, so must stop now
					overflow = true;
				}

				lineHeight = ps->height;
				lineWidth = ps->width;
				first = c;
				last = c;
			}
		}
	}

	// add remainder of last line
	adjustBounds(w, left, top, lineWidth, lineHeight, maxWidth, 
                 first, last);

	// adjust OUR bounds if we overflowed, I don't understand how
	// Swing does this
	if (top + lineHeight > b->height)
		container->setHeight(top + lineHeight);

	container->decTraceLevel();
}

PRIVATE void FlowLayout::adjustBounds(Window* w,
                                      int left,
									  int top,
									  int lineWidth,
									  int lineHeight,
                                      int maxWidth,
									  Component* first,
									  Component* last)
{
	if (first != NULL && last != NULL) {

		if (mAlign == FLOW_LAYOUT_RIGHT)
			left += maxWidth - lineWidth;
		else if (mAlign == FLOW_LAYOUT_CENTER)
			left += (maxWidth - lineWidth) / 2;

		for (Component* c = first ; c != NULL ; c = c->getNext()) {
			Dimension* d = c->getPreferredSize(w);

			// While we're here, center the component vertically 
			// on this line.  Do we always want to do this?
			int centery = top + ((lineHeight - d->height) / 2);

			c->setBounds(left, centery, d->width, d->height);
			left += d->width + mHGap;
			c->layout(w);
			if (c == last)
				break;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                GRID LAYOUT                               *
 *                                                                          *
 ****************************************************************************/
/*
 *
 * Evenly divide the components among the space available.
 * 
 * If columns is <= 0, the column count is determined by dividing
 * the total child components by the row count.
 *
 * If rows is <= 0, the row count is determined by dividing the total
 * child components by the column count.
 *
 * If both are zero, it behaves like FlowLayout but with a fixed
 * cell size.
 *
 */

PUBLIC GridLayout::GridLayout() {
    mRows = 0;
    mColumns = 0;
	mGap = 0;
	mCenter = false;
}

PUBLIC GridLayout::GridLayout(int rows, int columns) {
    mRows = rows;
    mColumns = columns;
	mGap = 0;
	mCenter = false;
}

/**
 * Is this ever not what you want?
 * Could provide more control with independent horizontal and vertical
 * alignment options.
 */
PUBLIC void GridLayout::setCenter(bool b)
{
	mCenter = b;
}

PUBLIC void GridLayout::setGap(int i) 
{
	mGap = i;
}

/**
 * Set the row/column size to force recalculation of the
 * optimal size.
 */
PUBLIC void GridLayout::setDimension(int rows, int columns)
{
    mRows = rows;
    mColumns = columns;
}

PUBLIC GridLayout::~GridLayout() {
}

PRIVATE int ceil(int x, int y)
{
    return (int)::ceil((float)x / (float)y);
}

PUBLIC Dimension* GridLayout::preferredLayoutSize(Container* container,
												  Window* w)
{
	Dimension* d = new Dimension();

	container->trace("GridLayout::preferredLayoutSize");
	container->incTraceLevel();

	// start by calculating the maximum child dimension
    int count = 0;
    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {
        count++;
		Dimension* ps = c->getPreferredSize(w);
		c->trace("Preferred size %d %d", ps->width, ps->height);

		if (ps->width > d->width)
		  d->width = ps->width;
		if (ps->height > d->height)
		  d->height = ps->height;
	}

    // audo size the grid the first time we calculate preferred size,
    // this can be reset with setDimension
    if (mColumns <= 0) {
        if (mRows <= 0) {
            mRows = 1;
            mColumns = count;
        }
        else {
            mColumns = ceil(count, mRows);
        }
    }
    else if (mRows <= 0) {
        mRows = ceil(count, mColumns);
    }
    else {
        // Expand or contract the rows to form the smallest bounding
        // grid for the actual compnents.  Could have an option to favor
        // a fixed row count and expand and contract columns?
        int needRows = ceil(count, mColumns);
        if (mRows < needRows)
          mRows = needRows;
        else {
            // hmm, shrink or leave as placeholders?
        }
    }

	// now determine the grid dimensions
	d->width = (d->width * mColumns) + (mGap * (mColumns - 1));
	d->height = (d->height * mRows) + (mGap * (mRows - 1));

    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

PUBLIC void GridLayout::layoutContainer(Container* container, Window* w)
{
    // the container must have been sized by now
	Bounds* b = container->getBounds();
    Insets* insets = container->getInsets();
    int left = 0;
    int top = 0;
    int width = b->width;
    int height = b->height;

    if (insets != NULL) {
        left += insets->left;
        top += insets->top;
        width -= (insets->left + insets->right);
        height -= (insets->top + insets->bottom);
    }

    int colwidth = width / mColumns;
    int rowheight = height / mRows;

	container->trace("GridLayout::layoutContainer");
	container->incTraceLevel();

    Component *child = container->getComponents();
    for (int i = 0 ; i < mRows && child != NULL; i++) {
        for (int j = 0 ; j < mColumns && child != NULL ; j++) {

			int cellLeft = left + (j * colwidth);
			int cellTop = top + (i * rowheight);
			int cellWidth = colwidth;
			int cellHeight = rowheight;

			// to center we have to use the preferred size so we won't
			// then let it expand to fill the available space
			if (mCenter) {
				Dimension* ps = child->getPreferredSize(w);
				cellLeft += (colwidth - ps->width) / 2;
				cellTop += (rowheight - ps->height) / 2;
				cellWidth = ps->width;
				cellHeight = ps->height;
			}

            child->setBounds(cellLeft, cellTop, cellWidth, cellHeight);
			child->layout(w);
            child = child->getNext();
        }
    }

	container->decTraceLevel();
}

/****************************************************************************
 *                                                                          *
 *   							 FORM LAYOUT                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Similar to GridLayout except that each column may have a different width.
 * The width of each column is the maximum preferred width of
 * each component in the column.  This provides a easy way to build
 * forms of labeled components with the labels aligned in one column
 * and the components in another.
 *
 */

PUBLIC FormLayout::FormLayout() {

	// todo: allow more than two columns?
    mHGap = 10;
    mVGap = 5;
    mAlign = FORM_LAYOUT_LEFT;
}

PUBLIC FormLayout::~FormLayout() {
}

PUBLIC void FormLayout::setAlign(int i)
{
    mAlign = FORM_LAYOUT_RIGHT;
}

PUBLIC void FormLayout::setHorizontalGap(int i)
{
	mHGap = i;
}

PUBLIC void FormLayout::setVerticalGap(int i)
{
	mVGap = i;
}

PUBLIC Dimension* FormLayout::preferredLayoutSize(Container* container,
												  Window* w) 
{
	Dimension* d = new Dimension();

	int col1Width = 0;
	int col2Width = 0;
	int height = 0;

	container->trace("FormLayout::preferredLayoutSize");
	container->incTraceLevel();

    for (Component* c = container->getComponents() ; c != NULL ; 
         c = c->getNext()) {

		int rowHeight = 0;

		// first a label
		container->trace("Calculating label size");
		container->incTraceLevel();

		Dimension* ps = c->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("Label - Preferred size %d %d", ps->width, ps->height);

		if (ps->width > col1Width)
		  col1Width = ps->width;
		rowHeight = ps->height;

		// then a component
		c = c->getNext();
		if (c != NULL) {
			container->trace("Calculating field size");
			container->incTraceLevel();

			ps = c->getPreferredSize(w);

			container->decTraceLevel();
			container->trace("Field - Preferred size %d %d", ps->width, ps->height);

			if (ps->width > col2Width)
			  col2Width = ps->width;
			if (ps->height > rowHeight)
			  rowHeight = ps->height;
		}

		height += rowHeight;
		if (c->getNext() != NULL)
		  height += mVGap;
	}

	d->width = col1Width + mHGap + col2Width;
	d->height = height;

    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

PUBLIC void FormLayout::layoutContainer(Container* container,
										Window* w) 
{
	Component* c;
    Insets* insets = container->getInsets();
    int left = 0;
    int top = 0;
	int col1Width = 0;

    if (insets != NULL) {
        left = insets->left;    
        top = insets->top;
    }

	container->trace("FormLayout::layoutContainer");
	container->incTraceLevel();

	// determine the first column width
    for (c = container->getComponents() ; c != NULL ; c = c->getNext()) {
		Dimension* ps = c->getPreferredSize(w);
		if (ps->width > col1Width)
		  col1Width = ps->width;
		// skip the component
		c = c->getNext();
	}

    for (c = container->getComponents() ; c != NULL ; c = c->getNext()) {
		Component* label = c;
		c = c->getNext();

		Dimension* lps = label->getPreferredSize(w);
		Dimension* cps = (c != NULL) ? c->getPreferredSize(w) : NULL;
		int rowHeight = lps->height;
		if (cps != NULL && cps->height > rowHeight)
		  rowHeight = cps->height;

        int alignedLeft = left;
        if (mAlign == FORM_LAYOUT_RIGHT)
          alignedLeft += col1Width - lps->width;

		int centery = top + ((rowHeight - lps->height) / 2);
		label->setBounds(alignedLeft, centery, lps->width, lps->height);
		label->layout(w);

		if (c != NULL) {
			//centery = top + ((rowHeight - cps->height) / 2);
			centery = top;
			c->setBounds(left + col1Width + mHGap, 
                         centery, cps->width, cps->height);
			c->layout(w);
		}

		top += rowHeight + mVGap;
	}

	container->decTraceLevel();
}

/****************************************************************************
 *                                                                          *
 *                               BORDER LAYOUT                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC BorderLayout::BorderLayout() {
    mNorth = NULL;  
    mSouth = NULL;
    mEast = NULL;
    mWest = NULL;
    mCenter = NULL;
}

PUBLIC BorderLayout::~BorderLayout() {
}


PUBLIC void BorderLayout::addLayoutComponent(Component *c, 
                                             const char *constraints)
{
    // must use constants
    if (checkConstraint(constraints, BORDER_LAYOUT_NORTH))
      mNorth = c;
    else if (checkConstraint(constraints, BORDER_LAYOUT_SOUTH))
      mSouth = c;
    else if (checkConstraint(constraints, BORDER_LAYOUT_EAST))
      mEast = c;
    else if (checkConstraint(constraints, BORDER_LAYOUT_WEST))
      mWest = c;
    else if (checkConstraint(constraints, BORDER_LAYOUT_CENTER))
      mCenter = c;
}

PRIVATE bool BorderLayout::checkConstraint(const char *c1, const char *c2)
{
    return (c1 != NULL && c2 != NULL && !strcmp(c1, c2));
}

PUBLIC void BorderLayout::removeLayoutComponent(Component *c)
{
    if (mNorth == c) mNorth = NULL;
    if (mSouth == c) mSouth = NULL;
    if (mEast == c) mEast = NULL;
    if (mWest == c) mWest = NULL;
    if (mCenter == c) mCenter = NULL;
}

PUBLIC Dimension* BorderLayout::preferredLayoutSize(Container* container,
													Window* w) 
{
	Dimension* d = new Dimension();

	container->trace("BorderLayout::preferredLayoutSize");
	container->incTraceLevel();

	if (mNorth != NULL) {
		container->trace("North");
		container->incTraceLevel();

		Dimension* ps = mNorth->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("North - Preferred size %d %d", ps->width, ps->height);

		d->width = ps->width;
		d->height = ps->height;
	}

	if (mSouth != NULL) {
		container->trace("South");
		container->incTraceLevel();

		Dimension* ps = mSouth->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("South - Preferred size %d %d", ps->width, ps->height);

		d->height += ps->height;
		if (ps->width > d->width)
		  d->width = ps->width;
	}

	int centerHeight = 0;
	int centerWidth = 0;

	if (mWest != NULL) {
		container->trace("West");
		container->incTraceLevel();

		Dimension* ps = mWest->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("West - Preferred size %d %d", ps->width, ps->height);

		centerWidth = ps->width;
		centerHeight = ps->height;
	}

	if (mCenter != NULL) {
		container->trace("Center");
		container->incTraceLevel();

		Dimension* ps = mCenter->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("Center - Preferred size %d %d", ps->width, ps->height);

		centerWidth += ps->width;
		if (ps->height > centerHeight)
		  centerHeight = ps->height;
	}

	if (mEast != NULL) {
		container->trace("East");
		container->incTraceLevel();

		Dimension* ps = mEast->getPreferredSize(w);

		container->decTraceLevel();
		container->trace("East - Preferred size %d %d", ps->width, ps->height);

		centerWidth += ps->width;
		if (ps->height > centerHeight)
		  centerHeight = ps->height;
	}

	d->height += centerHeight;
	if (centerWidth > d->width)
	  d->width = centerWidth;

    addInsets(container, d);

	container->decTraceLevel();
	container->trace("Preferred size %d %d", d->width, d->height);

	return d;
}

/**
 * North/South are full width, east/west fit in between.
 * Hmm, should we have an implicit center that pushes everything
 * out to the sides or let the collapse into the center?
 * What does Swing do?
 * Assuming implicit center.
 */
PUBLIC void BorderLayout::layoutContainer(Container* container,
										  Window* w) 
{
	Bounds* b = container->getBounds();
    Insets* insets = container->getInsets();
    int left = 0;
    int top = 0;
    int right = b->width;
    int bottom = b->height;

    if (insets != NULL) {
        left = insets->left;
        top = insets->top;
        right -= insets->right;
        bottom -= insets->bottom;
    }

	container->trace("BorderLayout::layoutContainer");
	container->incTraceLevel();

    // kludge, in order to get FlowLayout to wrap propertly within
    // our constrained size, set the component widths before calling
    // getPreferredSize,  this is NOT the way Swing does it, but I don't
    // like the behavior of just truncating on the right, and I can't
    // figure out another way to control FlowLayout.  

    if (mNorth != NULL) {
        int width = right - left;
        mNorth->setWidth(width);
        mNorth->setPreferredSize(NULL);
		Dimension* ps = mNorth->getPreferredSize(w);
		mNorth->setBounds(left, top, width, ps->height);
        top += ps->height;
        // Swing allows a vgap here too...
		mNorth->layout(w);
    }

    if (mSouth != NULL) {
        int width = right - left;
        mSouth->setWidth(width);
        mSouth->setPreferredSize(NULL);
		Dimension* ps = mSouth->getPreferredSize(w);
		mSouth->setBounds(left, bottom - ps->height, width, ps->height);
        bottom -= ps->height;
		mSouth->layout(w);
    }

    if (mEast != NULL) {
        // if you've got FlowLayout on the east, it will expand
        // to consume the space, maybe this is where MaximumSize would
        // be useful?
		Dimension* ps = mEast->getPreferredSize(w);
        mEast->setBounds(right - ps->width, top, 
                         ps->width, bottom - top);
        right -= ps->width;
		mEast->layout(w);
    }

    if (mWest != NULL) {
		Dimension* ps = mWest->getPreferredSize(w);
		mWest->setBounds(left, top, ps->width, bottom - top);
        left += ps->width;
		mWest->layout(w);
    }

    if (mCenter != NULL) {
		mCenter->setBounds(left, top, right - left, bottom - top);
		mCenter->layout(w);
	}

	container->decTraceLevel();
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
