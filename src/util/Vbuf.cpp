/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Yet another dynamic array with chunky resizing and pooling, because
 * the world just needed another.  This one has awareness of some
 * simple string transformation rules, like quote handling for XML
 * attribute values, and SQL statements.  One could argue that 
 * domain specific buffer handling be performed with subclasses, 
 * but there aren't that many, and its simpler to lump everything in here.
 *
 * Since this is almost always used to build up strings, the buffer
 * stays null terminated so that you can get a copy of the
 * resulting string via the getBuffer() method, without having to worry
 * about termination.
 * 
 */

#include <stdio.h>
#include <string.h>

#include "port.h"
#include "vbuf.h"

/****************************************************************************
 * Vbuf::Vbuf
 *
 * Arguments:
 *	initial: initial size (zero for default)
 *
 * Returns: error code
 *
 * Description: 
 * 
 * Basic initializer for a newly created Vbuf.
 ****************************************************************************/

INTERFACE Vbuf::Vbuf(int initial)
{
	if (initial == 0)
	  initial = VBUF_DEFAULT_SIZE;

    mNext       = NULL;
	mBuffer 	= new char[initial];
	mEnd	 	= mBuffer + initial;
	mPtr		= mBuffer;
	mGrow		= VBUF_GROW_SIZE;

	// maintain null termination
	*mBuffer 	= 0;
}

/****************************************************************************
 * ~Vbuf
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 *
 * Destroy a dynamic buffer.
 * Use the free() method if you want to use pooling.
 ****************************************************************************/

INTERFACE Vbuf::~Vbuf(void) 
{
	delete mBuffer;
}

/****************************************************************************
 * Vbuf::create
 *
 * Arguments:
 *	initial: initial size (zero to default)
 *
 * Returns: buffer
 *
 * Description: 
 * 
 * Creates a new buffer.
 ****************************************************************************/

INTERFACE Vbuf *Vbuf::create(int initial)
{
	// no pooling at the moment
	return new Vbuf;
}

/****************************************************************************
 * Vbuf::free
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Frees the buffer back to the pool.
 ****************************************************************************/

INTERFACE void Vbuf::free(void)
{
	// pooling is a stub right now
	delete this;
}

/****************************************************************************
 * Vbuf::flushPool
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Clears the buffer pool.
 ****************************************************************************/

INTERFACE void Vbuf::flushPool(void)
{
}

/****************************************************************************
 * Vbuf::getSize
 *
 * Arguments:
 *
 * Returns: size of the used portion of the buffer
 *
 * Description: 
 * 
 * Calculates the number of bytes currently being used in the buffer.
 ****************************************************************************/

INTERFACE int Vbuf::getSize(void)
{
	return PTRDIFF(mBuffer, mPtr);
}

/****************************************************************************
 * Vbuf::getLast
 *
 * Arguments:
 *	none: 
 *
 * Returns: last character in the buffer
 *
 * Description: 
 * 
 * Returns the last character in the buffer.  Might be used for
 * some semi-intelligent formatting decisions.
 ****************************************************************************/

INTERFACE int Vbuf::getLast(void)
{
	int last = 0;

	if (mPtr > mBuffer) 
	  last = *(mPtr - 1);

	return last;
}

/****************************************************************************
 * Vbuf::clear
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Resets a dynamic buffer to be "empty".
 ****************************************************************************/

INTERFACE void Vbuf::clear(void)
{
	mPtr = mBuffer;
	*mBuffer = 0;
}

/****************************************************************************
 * grow
 *
 * Arguments:
 *	minsize: minimum size to grow
 *
 * Returns:
 *
 * Description: 
 * 
 * Grows the dynamic buffer to accomodate data of the given size.
 * To prevent excessve resizing, we usually grow larger than the 
 * requested size.
 ****************************************************************************/

PRIVATE void Vbuf::grow(int minsize)
{
	int orig, psn, size;
	char *neu;

	if (minsize < mGrow)
	  minsize = mGrow;

	if (mBuffer) {
		orig = PTRDIFF(mBuffer, mEnd);
		psn  = PTRDIFF(mBuffer, mPtr);
	}
	else {
		orig = 0;
		psn  = 0;
	}

	size = orig + minsize;
	neu  = new char[size];

	if (mBuffer) {
		memcpy(neu, mBuffer, orig);
		delete mBuffer;
	}

	mBuffer	= neu;
	mEnd 	= mBuffer + size;
	mPtr 	= mBuffer + psn;
	*mPtr	= 0;
}

/****************************************************************************
 * Vbuf::add
 *
 * Arguments:
 *	 mem: buffer
 *	size: buffer size
 *
 * Returns: error code
 *
 * Description: 
 * 
 * Appends a block of bytes to the end of the buffer.
 ****************************************************************************/

INTERFACE void Vbuf::add(const char *mem, int size)
{
	if (mem != NULL && size) {
		
		int actual = size + 1;			// maintain null termination
		if (mPtr + actual > mEnd)
		  grow(actual);

		memcpy(mPtr, mem, size);
		mPtr += size;
		*mPtr = 0;
	}
}

/****************************************************************************
 * Vbuf::add(str)
 *
 * Arguments:
 *	str: string
 *
 * Returns: 
 *
 * Description: 
 * 
 * Appends a NULL terminated string to the buffer.
 ****************************************************************************/

INTERFACE void Vbuf::add(const char *str)
{
	// add this a character at a time to avoid the strlen overhead
	if (str) {
		const char *ch;
		for (ch = str ; *ch ; ch++) {
			if (mPtr + 1 > mEnd)
			  grow(1);
			*mPtr = *ch;
			mPtr++;
		}

		// maintain null termination
		if (mPtr + 1 > mEnd)
		  grow(1);
		*mPtr = 0;
	}	
}

/****************************************************************************
 * Vbuf::addChar
 *
 * Arguments:
 *	ch: character to add
 *
 * Returns: error
 *
 * Description: 
 * 
 * Adds a single character to the buffer.
 ****************************************************************************/

INTERFACE void Vbuf::addChar(const char ch)
{
	// +2 to maintain null termination
	if (mPtr + 2 > mEnd)
	  grow(2);

	*mPtr = ch;
	mPtr++;
	*mPtr = 0;
}

/****************************************************************************
 * Vbuf::add(int)
 *
 * Arguments:
 *	v: integer to add
 *
 * Returns: error
 *
 * Description: 
 * 
 * Converts an integer to a string and adds it to the buffer.
 * Can't overload add() since we'll conflict with add(char).
 ****************************************************************************/

INTERFACE void Vbuf::add(int v)
{
	char buf[32];
	sprintf(buf, "%d", v);
	add(buf);
}

/****************************************************************************
 * Vbuf::addXmlAttribute
 *
 * Arguments:
 *	  value: string we want to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a quoted XML attribute value to the buffer.
 * XML attribute values have to be surrounded in quotes, either single or
 * double.  If the value of the attribute contains a single or double
 * quote, then you can use the other quote type to surround the value.
 * If the value contains BOTH quote types, then you have to pick one, 
 * and convert the other to a character enitty reference.  
 * 
 * Should have one of these for element content as well that 
 * handles < and & characters in the text.
 * 
 ****************************************************************************/

INTERFACE void Vbuf::addXmlAttribute(const char *value)
{
	const char *ptr;
	int singles, doubles;
	char delim;

	// assume we're using single quote delimiters
	doubles = 0;
	singles = 0;
	delim   = '\'';
	
	// look for quotes
	for (ptr = value ; ptr && *ptr ; ptr++) {
		if (*ptr == '"')
		  doubles++;
		else if (*ptr == '\'')
		  singles++;
	}

	// if single quotes were detected, switch to double delimiters
	if (singles)
	  delim = '"';

	addChar(delim);

	// add the value, converting embedded quotes if necessary
	for (ptr = value ; ptr && *ptr ; ptr++) {
		if (*ptr != delim)
		  addChar(*ptr);
		else if (delim == '\'')
		  add("&#39;");	     		// convert '
		else
		  add("&#34;");	     		// convert "
	}

	addChar(delim);
}

/****************************************************************************
 * Vbuf::addSqlString
 *
 * Arguments:
 *	  value: string we want to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a quoted SQL style string to a buffer.
 * SQL strings must be enclosed in single quotes, if there are any
 * embedded single quotes, they must be doubled.
 * 
 ****************************************************************************/

INTERFACE void Vbuf::addSqlString(const char *value)
{
	addChar('\'');

	if (!strchr(value, '\'')) {
		// no embedded quote, just blast it out
		add(value);
	}
	else {
		const char *ptr;
		for (ptr = value ; ptr && *ptr ; ptr++) {
			if (*ptr == '\'')
			  addChar(*ptr);		// double it
			addChar(*ptr);
		}
	}

	addChar('\'');
}

/****************************************************************************
 * Vbuf::prepend
 *
 * Arguments:
 *	text: text to prepend
 *
 * Returns: error code
 *
 * Description: 
 * 
 * Like add() but puts the string at the front of the buffer.
 ****************************************************************************/

INTERFACE void Vbuf::prepend(const char *text)
{
	int len, actual, endpsn, i;

	if (text != NULL) {
		len = strlen(text);

		// +1 to maintain null termination
		actual = len + 1;

		if (len) {
			if (mPtr + actual > mEnd)
			  grow(actual);

			// shift everything down
			if (mPtr > mBuffer) {
				endpsn = (unsigned long)mPtr - (unsigned long)mBuffer - 1;
				for (i = endpsn ; i >= 0 ; i--)
				 mBuffer[i + len] = mBuffer[i];
			}

			for (i = 0 ; i < len ; i++)
			  mBuffer[i] = text[i];

			mPtr += len;
			*mPtr = 0;
		}
	}
}

/****************************************************************************
 * Vbuf::getString
 * Vbuf::getBuffer
 *
 * Arguments:
 *
 * Returns: current buffer
 *
 * Description: 
 * 
 * Returns a pointer to the current character buffer.
 * getBuffer returns whatever we have, getString is guaranteed to 
 * return the buffer with null termination.  Since we're maintaining
 * null termination as we go, they end up being identical.
 * Ownership is retained by the Vbuf.
 ****************************************************************************/

INTERFACE const char *Vbuf::getBuffer(void)
{
	return mBuffer;
}

INTERFACE const char *Vbuf::getString(void)
{
	return mBuffer;
}

/****************************************************************************
 * Vbuf::copyString
 *
 * Arguments:
 *
 * Returns: string
 *
 * Description: 
 * 
 * Returns a copy of the buffer as a NULL terminated string.
 * If the buffer is empty, it will return an empty string NOT a NULL pointer.
 ****************************************************************************/

INTERFACE char *Vbuf::copyString(void)
{
	char *str;
	int len;

	len = PTRDIFF(mBuffer, mPtr);
	str = new char[len + 1];
	memcpy(str, mBuffer, len);
	str[len] = 0;
	
	return str;
}

/****************************************************************************
 * Vbuf::stealString
 *
 * Arguments:
 *
 * Returns: string
 *
 * Description: 
 * 
 * Returns the contents of the buffer as a NULL terminated string.
 * Ownership is given to the caller.
 * Unlike getString, this returns the internal data buffer, removing
 * ownership by the Vbuf.  This could be used in cases where you
 * want to avoid a string copy.  You can also use getBuffer() for
 * this purpose provided that you don't need the returned string to 
 * live long.
 * 
 * Since the buffer will be reallocated, the next time it is used,
 * this sort of defeats the purpose of pooling, so try to avoid this
 * and use getBuffer instead.
 *
 * NOTE: The rest of the code isn't prepared to deal with a NULL mBuffer
 * so you can't use this after calling stealString.  Could fix that but
 * this is generally only used for one-shot buffers.
 ****************************************************************************/

INTERFACE char *Vbuf::stealString(void)
{
	char *str;

	str = mBuffer;
	mBuffer = NULL;
	mPtr = NULL;
	mEnd = NULL;

	return str;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
