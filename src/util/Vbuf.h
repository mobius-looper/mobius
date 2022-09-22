/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Yet another dynamic array with chunky resizing and pooling, because
 * the world just needed another.
 *
 */

#ifndef VBUF_H
#define VBUF_H

#include "port.h"

/****************************************************************************
 * VBUF_INITIAL_SIZE
 *
 * Description: 
 * 
 * Initialize size of the array.
 * In practice use, this should be large enough for the largest
 * blob of pcdata in your XML to reduce growths.
 ****************************************************************************/

#define VBUF_DEFAULT_SIZE 8192
#define VBUF_GROW_SIZE 8192

/****************************************************************************
 * Vbuf
 *
 * Description: 
 * 
 * A class implementing your basic dynamic character array.  Intended for
 * strings, but can put binary stuff in there too.
 * 
 ****************************************************************************/

class Vbuf {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors & accessors
	//
	//////////////////////////////////////////////////////////////////////

	// create a new one
	INTERFACE Vbuf(int initial = 0);

	INTERFACE ~Vbuf(void);

	// allocate from pool
	static INTERFACE Vbuf *create(int initial = 0);

	// return to the pool
	INTERFACE void free(void);

	// flush the pool
	static INTERFACE void flushPool(void);

	// initialization for subclasses
	INTERFACE void init(int initial = 0);

	INTERFACE int getSize(void);
	INTERFACE int getLast(void);

	INTERFACE const char *getBuffer(void);
	INTERFACE const char *getString(void);
	INTERFACE char *copyString(void);
	INTERFACE char *stealString(void);

    INTERFACE void clear(void);
	INTERFACE void add(const char *text);
	INTERFACE void add(const char *data, int size);
	INTERFACE void addChar(const char v);
	INTERFACE void add(int v);
	INTERFACE void addXmlAttribute(const char *value);
	INTERFACE void addSqlString(const char *value);
	INTERFACE void prepend(const char *text);

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	Vbuf		*mNext;			// for pool
	char 		*mBuffer;		// internal buffer we're maintaining
	char		*mEnd;			// one byte past the end of the buffer
	char 		*mPtr;			// current position within the buffer
	int 		mGrow;			// minimum amount to grow

	void grow(int size);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
