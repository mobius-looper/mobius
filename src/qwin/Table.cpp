/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simplified data table.  
 * Not trying to copy Spring here, just get the immediate job done.
 *
 * Scrolling panels are ungodly complicated so we'll implement this
 * on top of ListBox for now.  The OSX implementation of ListBox is
 * already using a data table which is appropriate, for Windows though
 * we're faking it and using an "ownerdraw" listbox.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// AbstractTableModel
//
//////////////////////////////////////////////////////////////////////

int AbstractTableModel::getColumnPreferredWidth(int index)
{
    return 0;
}

Font* AbstractTableModel::getColumnFont(int index)
{
    return NULL;
}

Color* AbstractTableModel::getColumnForeground(int index)
{
    return NULL;
}

Color* AbstractTableModel::getColumnBackground(int index)
{
    return NULL;
}

Font* AbstractTableModel::getCellFont(int row, int column)
{
    return NULL;
}

Color* AbstractTableModel::getCellForeground(int row, int colunn)
{
    return NULL;
}

Color* AbstractTableModel::getCellBackground(int row, int colunn)
{
    return NULL;
}

Color* AbstractTableModel::getCellHighlight(int row, int colunn)
{
    return NULL;
}

//////////////////////////////////////////////////////////////////////
//
// SimpleTableModel
//
//////////////////////////////////////////////////////////////////////

SimpleTableModel::SimpleTableModel()
{
    mColumns = NULL;
    mRows = NULL;
}

SimpleTableModel::~SimpleTableModel()
{
    delete mColumns;
    if (mRows != NULL) {
        // mRows is a generic list that won't delete it's elements
        for (int i = 0 ; i < mRows->size() ; i++) {
            StringList* row = (StringList*)mRows->get(i);
            // StringList will delete the char*
            delete row;
        }
        delete mRows;
    }
}

int SimpleTableModel::getColumnCount()
{
    return (mColumns != NULL) ? mColumns->size() : 0;
}

const char* SimpleTableModel::getColumnName(int index)
{
    return (mColumns != NULL) ? mColumns->getString(index) : NULL;
}

int SimpleTableModel::getRowCount()
{
    return (mRows != NULL) ? mRows->size() : 0;
}

const char* SimpleTableModel::getCellText(int row, int column)
{
    const char* value = NULL;
    if (mRows != NULL) {
        StringList* col = (StringList*)mRows->get(row);
        if (col != NULL)
            value = col->getString(column);
    }
    return value;
}

//////////////////////////////////////////////////////////////////////
//
// GeneralTableColumn
//
//////////////////////////////////////////////////////////////////////

GeneralTableColumn::GeneralTableColumn()
{
    init();
}

GeneralTableColumn::GeneralTableColumn(const char* text)
{
    init();
    setText(text);
}

void GeneralTableColumn::init()
{
    mText = NULL;
    mWidth = 0;
	mFont = NULL;
	mForeground = NULL;
	mBackground = NULL;
}

GeneralTableColumn::~GeneralTableColumn()
{
    delete mText;
}

const char* GeneralTableColumn::getText()
{
    return mText;
}

void GeneralTableColumn::setText(const char* s)
{
    delete mText;
    mText = CopyString(s);
}

int GeneralTableColumn::getWidth()
{
    return mWidth;
}

void GeneralTableColumn::setWidth(int i)
{
    mWidth = i;
}

Font* GeneralTableColumn::getFont()
{
	return mFont;
}

void GeneralTableColumn::setFont(Font* f)
{
	mFont = f;
}

Color* GeneralTableColumn::getForeground()
{
	return mForeground;
}

void GeneralTableColumn::setForeground(Color* c)
{
	mForeground = c;
}

Color* GeneralTableColumn::getBackground()
{
	return mBackground;
}

void GeneralTableColumn::setBackground(Color* c)
{
	mBackground = c;
}

//////////////////////////////////////////////////////////////////////
//
// GeneralTableCell
//
//////////////////////////////////////////////////////////////////////

GeneralTableCell::GeneralTableCell()
{
    init();
}

GeneralTableCell::GeneralTableCell(const char* text)
{
    init();
    setText(text);
}

void GeneralTableCell::init()
{
    mText = NULL;
    mFont = NULL;
    mForeground = NULL;
    mBackground = NULL;
	mHighlight = NULL;
}

GeneralTableCell::~GeneralTableCell()
{
    delete mText;
}

const char* GeneralTableCell::getText()
{
    return mText;
}

void GeneralTableCell::setText(const char* s)
{
    delete mText;
    mText = CopyString(s);
}

Font* GeneralTableCell::getFont()
{
    return mFont;
}

void GeneralTableCell::setFont(Font* f)
{
    mFont = f;
}

Color* GeneralTableCell::getForeground()
{
    return mForeground;
}

void GeneralTableCell::setForeground(Color* c)
{
    mForeground = c;
}

Color* GeneralTableCell::getBackground()
{
    return mBackground;
}

void GeneralTableCell::setBackground(Color* c)
{
    mBackground = c;
}

Color* GeneralTableCell::getHighlight()
{
    return mHighlight;
}

void GeneralTableCell::setHighlight(Color* c)
{
    mHighlight = c;
}

//////////////////////////////////////////////////////////////////////
//
// GeneralTableModel
//
//////////////////////////////////////////////////////////////////////

GeneralTableModel::GeneralTableModel()
{
    mColumns = NULL;
    mRows = NULL;
	mColumnFont = NULL;
	mColumnForeground = NULL;
	mColumnBackground = NULL;
    mCellFont = NULL;
	mCellForeground = NULL;
	mCellBackground = NULL;
	mCellHighlight = NULL;
}

GeneralTableModel::~GeneralTableModel()
{
    deleteColumns();
    deleteRows();
}

PRIVATE void GeneralTableModel::deleteColumns()
{
    if (mColumns != NULL) {
        // mColumns is a generic list that won't delete it's elements
        for (int i = 0 ; i < mColumns->size() ; i++) {
            GeneralTableColumn* col = (GeneralTableColumn*)mColumns->get(i);
            delete col;
        }
        delete mColumns;
        mColumns = NULL;
    }
}

PRIVATE void GeneralTableModel::deleteRows()
{
    if (mRows != NULL) {
        // mRows is a generic list that won't delete it's elements
        for (int i = 0 ; i < mRows->size() ; i++) {
            List* row = (List*)mRows->get(i);
            // also a generic list
            for (int j = 0 ; j < row->size() ; j++) {
                GeneralTableCell* cell = (GeneralTableCell*)row->get(j);
                delete cell;
            }
            delete row;
        }
        delete mRows;
        mRows = NULL;
    }
}

PRIVATE GeneralTableColumn* GeneralTableModel::getColumn(int index)
{
    return (mColumns != NULL) ? (GeneralTableColumn*)mColumns->get(index) : NULL;
}

PRIVATE List* GeneralTableModel::getRow(int index)
{
    return (mRows != NULL) ? (List*)mRows->get(index) : NULL;
}

PRIVATE GeneralTableCell* GeneralTableModel::getCell(int rowIndex, int columnIndex)
{
    List* row = getRow(rowIndex);
    return (row != NULL) ? (GeneralTableCell*)row->get(columnIndex) : NULL;
}

void GeneralTableModel::setColumnFont(Font* f)
{
    mColumnFont = f;
}

void GeneralTableModel::setColumnForeground(Color* c)
{
    mColumnForeground = c;
}

void GeneralTableModel::setColumnBackground(Color* c)
{
    mColumnBackground = c;
}

void GeneralTableModel::setCellFont(Font* f)
{
    mCellFont = f;
}

void GeneralTableModel::setCellForeground(Color* c)
{
    mCellForeground = c;
}

void GeneralTableModel::setCellBackground(Color* c)
{
    mCellBackground = c;
}

void GeneralTableModel::setCellHighlight(Color* c)
{
    mCellHighlight = c;
}

//
// TableModel
//

int GeneralTableModel::getColumnCount()
{
    return (mColumns != NULL) ? mColumns->size() : 0;
}

const char* GeneralTableModel::getColumnName(int index)
{
    GeneralTableColumn* col = getColumn(index);
    return (col != NULL) ? col->getText() : NULL;
}

int GeneralTableModel::getColumnPreferredWidth(int index)
{
    GeneralTableColumn* col = getColumn(index);
    return (col != NULL) ? col->getWidth() : NULL;
}

Font* GeneralTableModel::getColumnFont(int index)
{
    GeneralTableColumn* col = getColumn(index);
    return (col != NULL) ? col->getFont() : NULL;
}

Color* GeneralTableModel::getColumnForeground(int index)
{
    GeneralTableColumn* col = getColumn(index);
    return (col != NULL) ? col->getForeground() : NULL;
}

Color* GeneralTableModel::getColumnBackground(int index)
{
    GeneralTableColumn* col = getColumn(index);
    return (col != NULL) ? col->getBackground() : NULL;
}

int GeneralTableModel::getRowCount()
{
    return (mRows != NULL) ? mRows->size() : 0;
}

const char* GeneralTableModel::getCellText(int row, int column)
{
    GeneralTableCell* cell = getCell(row, column);
    return (cell != NULL) ? cell->getText() : NULL;
}

Font* GeneralTableModel::getCellFont(int row, int column)
{
	Font* font = mCellFont;
    GeneralTableCell* cell = getCell(row, column);
	if (cell != NULL) {
		Font* f = cell->getFont();
		if (f != NULL)
		  font = f;
	}
	return font;
}

Color* GeneralTableModel::getCellForeground(int row, int colunn)
{
	Color* color = mCellForeground;
    GeneralTableCell* cell = getCell(row, colunn);
	if (cell != NULL) {
		Color* c = cell->getForeground();
		if (c != NULL)
		  color = c;
	}
	return color;
}

Color* GeneralTableModel::getCellBackground(int row, int column)
{
	Color* color = mCellBackground;
    GeneralTableCell* cell = getCell(row, column);
	if (cell != NULL) {
		Color* c = cell->getBackground();
		if (c != NULL)
		  color = c;
	}
	return color;
}

Color* GeneralTableModel::getCellHighlight(int row, int colunn)
{
	Color* color = mCellHighlight;
    GeneralTableCell* cell = getCell(row, colunn);
	if (cell != NULL) {
		Color* c = cell->getHighlight();
		if (c != NULL)
		  color = c;
	}
	return color;
}

//
// GeneralTableModel
//

/**
 * Arg must be List<GeneralTableColumn> we retain ownership.
 */
void GeneralTableModel::setColumns(List* cols)
{
    deleteColumns();
    mColumns = cols;
}

/**
 * We take ownership of the argument.
 */
void GeneralTableModel::addColumn(GeneralTableColumn* col)
{
    if (mColumns == NULL)
      mColumns = new List();
    mColumns->add(col);
}

void GeneralTableModel::addColumn(const char* text)
{
    addColumn(text, 0);
}

void GeneralTableModel::addColumn(const char* text, int width)
{
    GeneralTableColumn* col = new GeneralTableColumn(text);
    col->setWidth(width);
    addColumn(col);
}

/**
 * Argument must be List<GeneralTableCell>, we take ownership.
 */
void GeneralTableModel::addRow(List* row)
{
    if (mRows == NULL)
      mRows = new List();
    mRows->add(row);
}

void GeneralTableModel::addCell(GeneralTableCell* cell, int rowIndex, int colIndex)
{
    List* row = getRow(rowIndex);
    if (row == NULL) {
        row = new List();
        if (mRows == NULL)
          mRows = new List();
        mRows->set(rowIndex, row);
    }

    GeneralTableCell* current = (GeneralTableCell*)row->get(colIndex);
    if (current != NULL)
      delete current;
    
    row->set(colIndex, cell);
}

void GeneralTableModel::addCell(const char* text, int rowIndex, int colIndex)
{
    GeneralTableCell* cell = new GeneralTableCell(text);
    addCell(cell, rowIndex, colIndex);
}

/****************************************************************************
 *                                                                          *
 *                                   TABLE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Table::Table() 
{
	init();
}

PUBLIC Table::Table(TableModel* model)
{
	init();
    mModel = model;
}

PRIVATE void Table::init()
{
	mClassName = "Table";
    mModel = NULL;
	mMultiSelect = false;
    mVisibleRows = 5;
	mSelected = NULL;
}

PUBLIC Table::~Table()
{
    delete mModel;
	delete mSelected;
}

PUBLIC ComponentUI* Table::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getTableUI(this);
	return mUI;
}

PUBLIC TableUI* Table::getTableUI()
{
    return (TableUI*)getUI();
}

/**
 * We assume ownership of the model!
 */
PUBLIC void Table::setModel(TableModel* model)
{
	if (mModel != model) {
		// !! if the UI thread happens to be refreshing right now
		// could get memory violation, need csect or phased delete
		// in practice this won't be a problem yet because we don't
		// have tables boxes in the main mobius window, only in dialogs
		delete mModel;
		mModel = model;
    }
    
    // unconditionally rebuild in case the previous model was changed
    // internally
    rebuild();
}

PUBLIC TableModel* Table::getModel()
{
    return mModel;
}

/**
 * Force a rebuild of the native components to reflect changes to the
 * current model.
 */
PUBLIC void Table::rebuild()
{    
    TableUI* ui = getTableUI();
    ui->rebuild();
}

PUBLIC void Table::setVisibleRows(int i) 
{
    mVisibleRows = i;
}

PUBLIC int Table::getVisibleRows()
{
	return mVisibleRows;
}

PUBLIC void Table::setMultiSelect(bool b)
{
    mMultiSelect = b;
}

PUBLIC bool Table::isMultiSelect()
{
    return mMultiSelect;
}

PUBLIC void Table::clearSelection()
{
	delete mSelected;
	mSelected = NULL;
    TableUI* ui = getTableUI();
    ui->setSelectedIndex(-1);
}

PUBLIC void Table::setSelectedIndex(int i)
{
	if (i >= -1) {
		if (mSelected == NULL)
		  mSelected = new List();

		if (!mSelected->contains((void*)i))
		  mSelected->add((void*)i);

        TableUI* ui = getTableUI();
        ui->setSelectedIndex(i);
	}
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 */
PUBLIC int Table::getSelectedIndex()
{
    int index = -1;

    TableUI* ui = getTableUI();
    if (ui->isOpen())
      index = ui->getSelectedIndex();
    else if (mSelected != NULL && mSelected->size() > 0)
      index = (int)mSelected->get(0);

    return index;
}

PUBLIC List* Table::getInitialSelected()
{
	return mSelected;
}

PUBLIC Dimension* Table::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void Table::dumpLocal(int indent)
{
    dumpType(indent, "Table");
}

PUBLIC void Table::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

/**
 * Necessary for the header and "owner draw" list boxes on windows.
 */
PUBLIC void Table::paint(Graphics* g)
{
    ComponentUI* ui = getUI();
    ui->paint(g);
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                               WINDOWS TABLE                              *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

/**
 * We will initially implement this as an "ownerdraw" ListBox.
 *
 */
PUBLIC WindowsTable::WindowsTable(Table* t) 
{
	mTable = t;
	mColumnWidths = NULL;
	mDefaultColumnFont = Font::getFont("Helvetica", 0, 12);
	mDefaultCellFont = NULL;
	mHeaderHeight = 0;
}

PUBLIC WindowsTable::~WindowsTable() 
{
	delete mColumnWidths;
}

/**
 * Reflect changes to the table model.
 * ListBox adds a string value for each row even though we use
 * ownerdraw and don't display them.  I don't know if this is necessary
 * but we'll give it something just in case.
 */
PUBLIC void WindowsTable::rebuild()
{
	if (mHandle != NULL) {
		SendMessage(mHandle, WM_SETREDRAW, FALSE, 0);
		SendMessage(mHandle, LB_RESETCONTENT, 0, 0);
        TableModel* model = mTable->getModel();
        if (model != NULL) {
            int rows = model->getRowCount();
            for (int i = 0 ; i < rows ; i++) {
                // not sure what the lifespan of the argument needs to be,
                // it seems to be copied but be safe and use a static
                SendMessage(mHandle, LB_ADDSTRING, 0, (LPARAM)"row");
            }
        }
		SendMessage(mHandle, WM_SETREDRAW, TRUE, 0);
        // will this invalidate?
	}
}

PUBLIC void WindowsTable::setSelectedIndex(int i)
{
    if (mHandle != NULL) {
		if (!mTable->isMultiSelect())
		  SendMessage(mHandle, LB_SETCURSEL, i, 0);
		else {
			// !! what if we're trying to clear out a multiselect, 
			// have to iterate over each of the current selections and disable them?
			if (i >= 0)
			  SendMessage(mHandle, LB_SETSEL, 1, i);
		}

        // Kludge: the control is supposed to auto scroll
        // if the selected item is not visible.  When selecting
        // the first item, I always see it scrolled off the top,
        // the second item is the one at the top of the control.
        // Not sure what causes this, workaround by sending
        // scroll bar commands.  This assumes that the scroll bar
        // units are the same as the number of items, is that
        // automatic or do we have to call SetScrollRange?
		// !! would LB_SETTOPINDEX work here?
		if (i >= 0)
		  SetScrollPos(mHandle, SB_VERT, i, TRUE);
    }
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 */
PUBLIC int WindowsTable::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
        TableModel* model = mTable->getModel();
		if (model != NULL) {
            if (mTable->isMultiSelect()) {
                // Petzold implies that GETCURSEL won't work here, should try
                int rows = model->getRowCount();
                for (int i = 0 ; i < rows ; i++)
                  if (SendMessage(mHandle, LB_GETSEL, i, 0)) {
                      selected = i;
                      break;
                  }
            }
            else {
                selected = SendMessage(mHandle, LB_GETCURSEL, 0, 0);
                // not sure if this is -1, make sure
                if (selected == LB_ERR)
                  selected = -1;
            }
        }
    }
	return selected;
}

/**
 * Return true if a given item is selected.
 */
PUBLIC bool WindowsTable::isSelected(int i)
{
    bool selected = false;
    if (mHandle != NULL) {
        if (SendMessage(mHandle, LB_GETSEL, i, 0))
          selected = true;
    }
	return selected;
}

PUBLIC void WindowsTable::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
		if (parent != NULL) {
            // LBS_NOTIFY necessary to get WM_COMMAND messages
            // LBS_SORT causes the values to be sorted
            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP | 
                WS_VSCROLL | WS_BORDER | LBS_NOTIFY;
		

            // always enable ownerdraw
            // this will cause it to send WM_MEASUREITEM to the parent window,
            // assume we can ingore??
            // LBS_OWNERDRAWVARIABLE means items don't have the same height
            style |= (LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);

            if (mTable->isMultiSelect())
              style |= LBS_MULTIPLESEL;

            Bounds* b = mTable->getBounds();
            Point p;
            mTable->getNativeLocation(&p);

            mHandle = CreateWindow("listbox",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Table control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mTable->initVisibility();

                // capture state set during construction
                rebuild();

                List* selected = mTable->getInitialSelected();
                if (selected != NULL) {
                    for (int i = 0 ; i < selected->size() ; i++) {
                        setSelectedIndex((int)(selected->get(i)));
                    }
                }
            }
        }
    }
}

/**
 * Calculate the pixel widths of each column.  The total determines
 * the preferred width of the component.
 *
 * Let the columns from the model determine the column widths,
 * then validate this against the max width of each cell in the column.
 * 
 * If we allow fonts for cells this will need to be more complicated.  
 * Currently assuming system fonts.
 *
 * While we're here also calculate mHeaderHeight depending on whether
 * we have column titles.
 */
PUBLIC List* WindowsTable::getColumnWidths(Window* w)
{
    if (mColumnWidths == NULL) {
        mColumnWidths = new List();

		if (w == NULL) {
			// should only be here when called from paint() and 
			// we lost the widths calculated by getPreferredSize!
			Trace(1, "Lost TableHeader column widths!");
		}
		else if (mTable != NULL) {
			TableModel* model = mTable->getModel();
			if (model != NULL) {

				// we'll base the width by the max char width, not an
				// exact measurement of the cell text, this is usually
				// what you want since cells can be variable
				Graphics* g = w->getGraphics();

				// will be be set if we notice column header text
				bool needsHeader = false;

				int cols = model->getColumnCount();
				for (int i = 0 ; i < cols ; i++) {

					int width = 0;

                    // 12 point default font has a 32 pixel max width
                    // which is VERY high, if they don't specify a column
                    // width use accurate text measurements
                    Font* font = model->getColumnFont(i);
                    if (font == NULL) 
                      font = mDefaultColumnFont;

                    g->setFont(font);
                    TextMetrics* tm = g->getTextMetrics();
                    // max is generally way to large on Windows, use average
                    int charWidth = tm->getAverageWidth();

					// hmm, should this be pixel width or character count?
					// characters is easier
					int chars = model->getColumnPreferredWidth(i);
					const char* name = model->getColumnName(i);

                    if (chars > 0) {
                        // this tends to be longer than necessary so don't
                        // bother padding
                        width = chars * charWidth;
                    }
                    else if (name != NULL) {
						Dimension d;
						g->getTextSize(name, font, &d);
						width = d.width;
						// char of padding
						width += charWidth;
					}
					else {
						// give it one char just to have some padding
						width = charWidth;
					}

					// compare to current cell data
					int cellMax = getMaxColumnWidth(w, model, i);
					if (cellMax > width)
					  width = cellMax;

					mColumnWidths->set(i, (void*)width);
				}
			}
		}
	}

    return mColumnWidths;
}

/**
 * Calculate the maximum pixel width of a column in a table model.
 * Since we're dealing with the system font, don't have a Font
 * we can use for metrics, should try to make one!! 
 * Get the text metrics from the Window and base it on the max width.
 * This sucks because cells will have lots of lowercase.
 */
PRIVATE int WindowsTable::getMaxColumnWidth(Window* w, TableModel* model, 
											int col)
{
	int max = 0;
	Dimension d;
	bool useSystemFont = false;

	Graphics* g = w->getGraphics();

	// use this if we don't trust default font
	TextMetrics* tm = w->getTextMetrics();
	int charWidth = tm->getAverageWidth();

	for (int i = 0 ; i < model->getRowCount() ; i++) {
		const char* str = model->getCellText(i, col);
		if (str != NULL) {
			int width = 0;
			if (useSystemFont) {
				int chars = strlen(str);
				width = chars * charWidth;
			}
			else {
				Font* f = model->getCellFont(col, i);
				if (f == NULL) 
                  f = mDefaultCellFont;
				g->setFont(f);
				g->getTextSize(str, &d);
				width = d.width;
                // char of padding
                width += 8;
			}

			if (width > max)
			  max = width;
		}
	}

	return max;
}

/**
 * Assume that the column widths define the total width.
 * We could iterate over ever cell building out the max width, but the
 * app should be able to set column widths appropriately.
 *
 */
PUBLIC void WindowsTable::getPreferredSize(Window* w, Dimension* d)
{
	// the text metrics from the Window is supposed to represent
	// the "system font" used for heavyweight components, I HATE this

	List* widths = getColumnWidths(w);
	int totalWidth = 0;
	for (int i = 0 ; i < widths->size() ; i++) {
		int width = (int)widths->get(i);
		totalWidth += width;
	}

	// force it to something we can see
	if (totalWidth == 0) totalWidth = 20;

    // add the scroll bar
    totalWidth += UIManager::getVertScrollBarWidth();

	d->width = totalWidth;

	TextMetrics* tm = w->getTextMetrics();
		
	// 1 1/2 times char height if using border
    // this assumes system font!! getColumnWidths allows custom font
    // but we're not using it
	int fontHeight = tm->getHeight() + tm->getExternalLeading();
	int rows = mTable->getVisibleRows();
    if (rows <= 0) rows = 1;
    int height = rows * fontHeight;
    height += fontHeight / 2;

	// add in column headers if any of them are set
	mHeaderHeight = 0;
	TableModel* model = mTable->getModel();
	if (model != NULL) {
		Graphics* g = w->getGraphics();
		for (int i = 0 ; i < model->getColumnCount() ; i++) {
			const char* text = model->getColumnName(i);
			if (text != NULL) {
				Font* f = model->getColumnFont(i);
				if (f == NULL) f = mDefaultColumnFont;
				g->setFont(f);
				TextMetrics* tm = g->getTextMetrics();
				int height = tm->getHeight() + tm->getExternalLeading();
                // if light on dark background we need an extra pixel at
                // the bottom for a nice border, otherwise the extenders
                // put "holes" in the bottom
                height++;
				if (height > mHeaderHeight)
				  mHeaderHeight = height;
			}
		}
	}

    d->height = height + mHeaderHeight;
}

/**
 * All sizing methods eventually get here.
 * Overloaded from WindowsComponent so we can factor out the table header.
 */
PUBLIC void WindowsTable::updateBounds() 
{
	Bounds b;
	mTable->getNativeBounds(&b);

	b.y += mHeaderHeight;
	b.height -= mHeaderHeight;

	updateNativeBounds(&b);
}

/**
 * Called for OWNERDRAW list boxes.
 *
 * If LPDRAWITEMSTRUCT is NULL this is a draw of the full component
 * which is where we render the table header.  If non-NULL this is
 * called in response to a WM_DRAWITEM event and we only draw a cell.
 *
 * Action will be ODA_SELECT, ODA_DRAWENTIRE, or ODA_FOCUS.  Ignore ODA_FOCUS.
 */
PUBLIC void WindowsTable::paint(Graphics* g)
{
    WindowsGraphics* wg = (WindowsGraphics*)g;
    LPDRAWITEMSTRUCT di = wg->getDrawItem();
    TableModel* model = mTable->getModel();

	if (model == NULL) {
		// ignore
	}
	else if (di == NULL) {
		// paint header
		Bounds b;
		mTable->getPaintBounds(&b);
    
		g->setColor(Color::Black);
		g->fillRect(b.x, b.y, b.width, mHeaderHeight);

		List* widths = getColumnWidths(NULL);
		TextMetrics* tm = g->getTextMetrics();
		int left = b.x;
		int top = b.y + tm->getAscent();
		int cols = model->getColumnCount();

		// Table is bordered with some left indent,
		// no easy way to determine what that is, and it
		// will be UI specific!!
		left += 8;

		for (int i = 0 ; i < cols ; i++) {
			const char* text = model->getColumnName(i);
			if (text != NULL) {
				Font* font = model->getColumnFont(i);
				Color* foreground = model->getColumnForeground(i);
				if (foreground == NULL) foreground = Color::White;
				Color* background = model->getColumnBackground(i);
				if (background == NULL) background = Color::Black;

				g->setFont(font);
				g->setColor(foreground);
				g->setBackgroundColor(background);
				g->drawString(text, left, top);
			}
            
			left += (int)widths->get(i);
		}
	}
	else if (di->itemID >= 0 && di->itemAction != ODA_FOCUS) {
		int row = di->itemID;

        // TODO: DRAWITEM already assumes a bounds, stay within it
        // to properly support fonts will need to inform it of the
        // maximum row height somehow!

        int left = di->rcItem.left + 8;
        int top = di->rcItem.top;
        int height = di->rcItem.bottom - di->rcItem.top + 1;

        TextMetrics* tm = g->getTextMetrics();
        top += (height / 2) + (tm->getAscent() / 2);
        // this is just a little too low, same thing happened
        // with knob, need to resolve this!!
        top -= 2;

        bool selected = (di->itemState & ODS_SELECTED);

        int cols = model->getColumnCount();
        List* widths = getColumnWidths(NULL);
        for (int i = 0 ; i < cols ; i++) {
            
            const char* text = model->getCellText(row, i);
            if (text != NULL) {
                Color* background = model->getCellBackground(row, i);
				if (background == NULL) background = Color::White;
                Color* foreground;
                if (di->itemState & ODS_SELECTED) {
                    Color* c = model->getCellHighlight(row, i);
                    if (c != NULL)
                      foreground = c;
					else
					  foreground = Color::Red;
				}
                else {
                    Color* c = model->getCellForeground(row, i);
                    if (c != NULL)
                      foreground = c;
					else
					  foreground = Color::Black;
                }

                g->setColor(foreground);
                g->setBackgroundColor(background);
				g->setFont(model->getCellFont(row, i));

                // TODO: Would be nice to have some justification options
                g->drawString(text, left, top);
            }

            // advance by the previously calculated cell width
            left += (int)widths->get(i);
        }
    }
}

PUBLIC void WindowsTable::command(int code) 
{
	if (code == LBN_SELCHANGE)
	  mTable->fireActionPerformed();
}

QWIN_END_NAMESPACE
#endif // _WIN32

/****************************************************************************
 *                                                                          *
 *                                 MAC TABLE                                *
 *                                                                          *
 ****************************************************************************/

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
#include "MacUtil.h"

QWIN_BEGIN_NAMESPACE

/**
 * We need unique identifiers for columns, 0-1023 are reserved.
 * Usually these are four character "names" like 'main' and 'anno'
 * used in MacListBox.  Here the columns are variable so have to use
 * a number base.
 */
#define BASE_COLUMN_ID 2000

PUBLIC MacTable::MacTable(Table* t) 
{
	mTable = t;
	mColumnWidths = NULL;
	mHeaderHeight = 0;
}

PUBLIC MacTable::~MacTable() 
{
	delete mColumnWidths;
}

/**
 * Reflect changes to the table model.
 * We don't actually pass the data here, just the number of rows.
 * Let the control auto-number the rows from 1
 */
PUBLIC void MacTable::rebuild()
{
	OSStatus status;

	if (mHandle != NULL) {
		ControlRef control = (ControlRef)mHandle;
		TableModel* model = mTable->getModel();
		if (model != NULL) {
			int rows = model->getRowCount();

			// first clear the browser
			status = RemoveDataBrowserItems(control, 
											kDataBrowserNoItem, // container
											0, // item count, zero to remove all
											NULL, // item ids, NULL to remove all
											kDataBrowserItemNoProperty); 
			CheckStatus(status, "RemoveDataBrowserItems");

			status = AddDataBrowserItems(control, 
										 kDataBrowserNoItem, // container
										 rows, // numItems
										 NULL, // item ids (auto number from 1)
										 kDataBrowserItemNoProperty); // preSortProperty

			CheckStatus(status, "AddDataBrowserItems");
		}
		mTable->invalidate();
	}
}

/**
 * Table item indexes start frm 0.
 * DataBrowserItemID starts from 1.
 */
PUBLIC void MacTable::setSelectedIndex(int i)
{
	OSStatus status;

    if (mHandle != NULL) {
		DataBrowserItemID items[1];
		
		// have to adjust the offset
		int itemId = i + 1;

		if (!mTable->isMultiSelect()) {
			if (i >= 0) {
				items[0] = itemId;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 1, items, 
													 kDataBrowserItemsAssign);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
			else {
				// clear selection by assigning an empty set
				items[0] = 0;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 0, items, 
													 kDataBrowserItemsAssign);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
		}
		else {
			// !! what if we're trying to clear out a multiselect, 
			// have to iterate over each of the current selections and disable them?
			if (i >= 0) {
				items[0] = itemId;
				status = SetDataBrowserSelectedItems((ControlRef)mHandle,
													 1, items, 
													 kDataBrowserItemsAdd);
				CheckStatus(status, "SetDataBrowserSelectedItems");
			}
		}

		// TODO: Do we need to scroll to the selection?

	}
}

/**
 * Return the index of the selected item.
 * If this is a multi-select, return the index of the first selected item.
 * 
 * The GetDataBrowserItems function can return the ids of all items that
 * have a certaion state (like selected) but it returns a confusing
 * array in a "Handle" and I'm not sure how to deal with those.  Just iterate.
 */
PUBLIC int MacTable::getSelectedIndex()
{
    int selected = -1;
	if (mHandle != NULL) {
		TableModel* model = mTable->getModel();
		if (model != NULL) {
			int rows = model->getRowCount();
			for (int i = 0 ; i < rows ; i++) {
				if (isSelected(i)) {
					selected = i;
					break;
				}
			}
		}
	}
	return selected;
}

/**
 * Return true if a given item is selected.
 */
PUBLIC bool MacTable::isSelected(int i)
{
    bool selected = false;
    if (mHandle != NULL) {
		// item ids are 1 based
		int itemID = i + 1;
		selected = (bool)IsDataBrowserItemSelected((ControlRef)mHandle, itemID);
    }
	return selected;
}

OSStatus TableItemDataCallback(ControlRef browser,
								 DataBrowserItemID itemID,
								 DataBrowserPropertyID property,
								 DataBrowserItemDataRef itemData,
								 Boolean changeValue)
{
    OSStatus status = noErr;
	MacTable* mt = (MacTable*)GetControlReference(browser);
 
	if (mt == NULL) {
		Trace(1, "TableItemdtaCallback: no link back to Table\n");
		status = errDataBrowserPropertyNotSupported;
	}
	else if (changeValue) {
		// this is not a "set request" ignore
		status = errDataBrowserPropertyNotSupported;
	}
	else if (property == kDataBrowserContainerIsSortableProperty) {
		// not sure what this is
		status = SetDataBrowserItemDataBooleanValue(itemData, false);
	}
	else {
		Table* table = (Table*)mt->getComponent();
		TableModel* model = table->getModel();
		if (model == NULL) {
		  status = errDataBrowserPropertyNotSupported;
		}
		else {
			int cols = model->getColumnCount();
			int col = property - BASE_COLUMN_ID;
			if (col < 0 || col >= cols) {
				// out of range
				status = errDataBrowserPropertyNotSupported;
			}
			else {
				CFStringRef cfstring = NULL;
				// itemId is a 1 based list index
				int row = itemID - 1;
				if (row >= 0) {
					const char* str = model->getCellText(row, col);
					if (str != NULL)
					  cfstring = MakeCFStringRef(str);
				}
				if (cfstring != NULL)
				  status = SetDataBrowserItemDataText (itemData, cfstring);
			}
		}
	}

	return status;
}

/**
 * Called as things change in the data browser.
 * We get ItemSelected as soon as a selection happens, but before
 * ItemDeselected so if we fire the action handler now it will see both
 * the new and the previous selection, need to wait for SelectionSetChanged.
 */
void TableItemNotificationCallback(ControlRef browser,
									 DataBrowserItemID item,
									 DataBrowserItemNotification message) 
{
	MacTable* mt = (MacTable*)GetControlReference(browser);

	// kDataBrowserItemDoubleClicked
	// and many more dealing with containers

	switch (message) {
		case kDataBrowserItemSelected: {
			// happens immediately, before ItemDeselected
		}
		break;
		case kDataBrowserItemDeselected: {
			// happens after ItemSelected but may happen multiple times
		}
		break;
		case kDataBrowserSelectionSetChanged: {
			// we're finished updating the selection set
			// item is meaningless?
			Table* lb = (Table*)mt->getComponent();
			lb->fireActionPerformed();
		}
		break;
		default: {
			// getting a lot of kDataBrowserItemRemoved (2) messages 
			// in bulk, not sure why
			//printf("TableItemNotification %d item %d\n", message, item);
			//fflush(stdout);
		}
		break;
	}
}

/**
 * Kludge fixed character width for calculating sizes, 
 * need to find something more accurate.
 * Lower case characters were measured around 8 points on average.averate 
 */
#define LIST_BOX_CHAR_WIDTH 10

/**
 * Calculate the pixel widths of each column.  The total determines
 * the preferred width of the component.
 *
 * We use the column widths calculated by the header if found, else
 * we try to calculate the maximum width of each cell in a column.
 *
 * If we allow fonts this will need to be more complicated.  Currently
 * assuming system fonts.
 *
 * !! This calculates way to large or longer strings that have a 
 * good percentage of lowercase letters.  Need to find a way to accurately
 * measure the size of system fonts.  Until then you have to set explicit
 * column counts.
 * 
 */
PUBLIC List* MacTable::getColumnWidths(Window* w)
{
    if (mColumnWidths == NULL) {
        mColumnWidths = new List();

		if (w == NULL) {
			// should only be here when called from paint() and 
			// we lost the widths calculated by getPreferredSize!
			Trace(1, "Lost TableHeader column widths!");
		}
		else if (mTable != NULL) {
			TableModel* model = mTable->getModel();
			if (model != NULL) {

				// TODO: Find a way to get this reliably, the M width
				// of the default font measured around 8, but we should
				// really try to find out the true font size.  14 looks closest
				// in qwintest, but it comes up short.
				Graphics* g = w->getGraphics();
				g->setFont(Font::getFont("Helvetica", 0, 16));

				// !! TextMetrics aren't working on Mac, find out
				// what the deal is...
				//TextMetrics* tm = g->getTextMetrics();
				//int charWidth = tm->getMaxWidth();
				Dimension d;
				g->getTextSize("M", &d);
				int charWidth = d.width;
				// M width for a 16 point font seems to come back 16, 	
				// which is WAY too big, something might be screwed
				// up in text metric, just halve it for now
				charWidth /= 2;

				int cols = model->getColumnCount();
				for (int i = 0 ; i < cols ; i++) {

					int width = 0;

					// hmm, should this be pixel width or character count?
					// characters is easier
					int chars = model->getColumnPreferredWidth(i);
					const char* name = model->getColumnName(i);

					if (chars > 0) {
						width = chars * charWidth;
					}
					else if (name != NULL) {
						g->getTextSize(name, &d);
						width = d.width;
					}
					else {
						width = 8;
					}

					// compare to current cell data
					int cellMax = getMaxColumnWidth(g, model, i);
					if (cellMax > width)
					  width = cellMax;

					// always two chars of padding around it
					width += (charWidth * 2);

					mColumnWidths->set(i, (void*)width);

					// We add the header buttons as soon as we have any
					// non-empty column text.  17 is the default header size
					// according to the referene docs.
					if (name != NULL)
					  mHeaderHeight = 17;
				}
			}
		}
	}

    return mColumnWidths;
}

/**
 * Calculate the maximum width of a table cell.
 * I don't understand how to get accurate metrics on this yet so
 * we have a hard-coded constant.
 */
PRIVATE int MacTable::getMaxColumnWidth(Graphics* g, TableModel* model, int col)
{
	int max = 0;
	for (int i = 0 ; i < model->getRowCount() ; i++) {
		const char* str = model->getCellText(i, col);
		if (str != NULL) {
			// old way, fixed char width
			//int width = strlen(str) * LIST_BOX_CHAR_WIDTH;

			// new way, try to be accurate
			Dimension d;
			g->getTextSize(str, &d);
			if (d.width > max)
			  max = d.width;
		}
	}
	return max;
}

PUBLIC void MacTable::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();
	TableModel* model = mTable->getModel();

	if (mHandle == NULL && window != NULL && model != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };

		// sigh, give this some girth until we can figure out how to size these
		bounds.bottom = 100;
		bounds.right = 800;

		status = CreateDataBrowserControl(window, &bounds, 
										  kDataBrowserListView,
										  &control);

		if (CheckStatus(status, "MacTable::open")) {
			mHandle = control;

			SetControlReference(control, (SInt32)this);

			/*
			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(TableEventHandler),
												GetEventTypeCount(TableEventsOfInterest),
												TableEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacTable::InstallEventHandler");
			*/

			int cols = model->getColumnCount();
			List* widths = getColumnWidths(mTable->getWindow());

			for (int i = 0 ; i < cols ; i++) {
				const char* name = model->getColumnName(i);
				int width = (int)widths->get(i);
				addColumn(control, BASE_COLUMN_ID + i, name, width);
			}

			// horiz, vert
			SetDataBrowserHasScrollBars(control, false, true);

			// getColumnWidths also calculates header size, this must go to 
			// zero if we want to hide the title bar
			SetDataBrowserListViewHeaderBtnHeight(control, mHeaderHeight);

			// kDataBrowserSelectOnlyOne - makes it like a radio
			// kDataBrowserResetSelection - clears selected items before processing next
			// kDataBrowserNoDisjointSelection - prevent discontinuous selection
			// kDataBrowserAlwaysExtendSelection - select multiple without holding down modifier key
			// kDataBrowserNeverEmptySelectionSet - user can never deselect last item
			int flags = kDataBrowserCmdTogglesSelection;
			if (!mTable->isMultiSelect())
			  flags |= kDataBrowserSelectOnlyOne;

			SetDataBrowserSelectionFlags(control, flags);
										  
			// set callbacks
			DataBrowserCallbacks callbacks;
			callbacks.version = kDataBrowserLatestCallbacks;
			InitDataBrowserCallbacks(&callbacks);

			// supposedly we're supposed to free the UPPs later
			// with DisposeDataBrowserItemNotificationUPP and the like

			callbacks.u.v1.itemDataCallback = 
				NewDataBrowserItemDataUPP((DataBrowserItemDataProcPtr)
										  TableItemDataCallback);
			callbacks.u.v1.itemNotificationCallback = 
				NewDataBrowserItemNotificationUPP(TableItemNotificationCallback);

			status = SetDataBrowserCallbacks(control, &callbacks);
			CheckStatus(status, "SetDataBrowserCallbacks");

			// allows column dragging
			//SetAutomaticControlDragTrackingEnabledForWindow(window, true);
			
			// capture state set during construction
			rebuild();
			List* selected = mTable->getInitialSelected();
			if (selected != NULL) {
				for (int i = 0 ; i < selected->size() ; i++) {
					setSelectedIndex((int)(selected->get(i)));
				}
			}
	
			SetControlVisibility(control, true, true);
		}
    }
}

/**
 * Helper to add one column to the list view.
 */
PRIVATE void MacTable::addColumn(ControlRef control, int id, const char* name, 
								 int width)
{
	DataBrowserListViewColumnDesc col;

	col.propertyDesc.propertyID = id;
	col.propertyDesc.propertyType = kDataBrowserTextType;

	// kDataBrowserPropertIsMutable, kDataBrowserDefaultPropertyIsEditable,
	// kDataBrowserDoNotTruncateText, kDataBrowserListViewMovableColumn,
	// kDataBrowserListViewSortableColumn
	col.propertyDesc.propertyFlags = kDataBrowserListViewSelectionColumn;
			
	col.headerBtnDesc.version = kDataBrowserListViewLatestHeaderDesc;
	// set these different if you want the columns resizeable
	col.headerBtnDesc.minimumWidth = width;
	col.headerBtnDesc.maximumWidth = width;
	// offset in pixels from the left
	col.headerBtnDesc.titleOffset = 0;
	col.headerBtnDesc.titleString = MakeCFStringRef(name);
	col.headerBtnDesc.initialOrder = kDataBrowserOrderUndefined;

	// use all defaults for the title, can set font, justification, 
	// foreground/background, 
	// see HIToolbox.framework/Headers/Control.h/ControlFontStyleRec
	// "just" field combines with titleOffset
	col.headerBtnDesc.btnFontStyle.flags = 0;

	// allows icons and other things for buttons
	// see HIToolbox.framework/Headers/Control.h
	col.headerBtnDesc.btnContentInfo.contentType = kControlNoContent;

	OSStatus status = AddDataBrowserListViewColumn(control, &col, 
												   kDataBrowserListViewAppendColumn);
	CheckStatus(status, "AddDataBrowserListViewColumn");
}

/**
 * On Windows we get the system TextMetrics which defines the sizes
 * of characters used in the standard components.  I don't know if such
 * a thing exists on Mac, so we're going to fake it for now.
 * Redesign the TextMetric handling, we should have a concrete class
 * that the ComponentUI's can fill in.
 */
PUBLIC void MacTable::getPreferredSize(Window* w, Dimension* d)
{
	// the default is as high as necessary to display all items
	// we could at least use this to calculate the line height!!
	//MacComponent::getPreferredSize(w, d);
	//printf("*** Table default height %d\n", d->height);

	// measured distance between baselines of the digit 1 was 19 pixels
	int cellFontHeight = 19;

	int rows = mTable->getVisibleRows();
	if (rows <= 0) rows = 5;
	d->height = rows * cellFontHeight;

	// need a little extra padding at the bottom
	d->height += 4;

	// always use the measurements calculated during open
	int totalWidth = 0;
	List* widths = getColumnWidths(w);
	for (int i = 0 ; i < widths->size() ; i++) {
		int width = (int)widths->get(i);
		totalWidth += width;
	}

	d->width = totalWidth + UIManager::getVertScrollBarWidth();

	// headers are optional, if need them the header height
	// will have been calculated by getColumnWidths and left here
	d->height += mHeaderHeight;

}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
