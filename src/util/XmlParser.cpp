/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A light-weight XML parser for valid well formed files.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
 
#include "Trace.h"
#include "Vbuf.h"
// for some ERR_ constants
#include "Util.h"

#include "XmlParser.h"

/****************************************************************************
 *                                                                          *
 *   							 CONSTRUCTORS                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Creates an XML parser object.
 ****************************************************************************/

INTERFACE XmlMiniParser::XmlMiniParser(void)
{
	mPreserveCharent	= 0;
	mInlineEntref		= 0;
	mFilterComments		= 0;

	mHandler			= NULL;
	mInputBuffer 		= NULL;
	mInputBufferLength	= 0;
	mInputFile 			= NULL;
	mInputFp 			= NULL;

	mLine				= 0;
	mColumn				= 0;
	mOffset				= 0;

	mInDoctype			= 0;
	mEof				= 0;
	mStarted			= 0;
    mInputIsOpen	    = 0;

	mPbuf 				= NULL;
	mPtr 				= NULL;
	mEnd 				= NULL;
	mTokenIndex			= 0;
	mLookaheadIndex		= 0;
	mDatabuf 			= NULL;

	mTempString1		= NULL;
	mTempString2		= NULL;

    mErrorCode          = 0;
    mError[0]           = 0;
}

/****************************************************************************
 * ~XmlMiniParser
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destroys a mini parser object.
 * Note that we don't delete the event handler object, we assume that
 * the caller will free that in due time.
 ****************************************************************************/

INTERFACE XmlMiniParser::~XmlMiniParser(void)
{
	if (mInputFp != NULL) 
	  fclose(mInputFp);

	delete mInputFile;
	delete mPbuf;
	delete mDatabuf;
	delete mTempString1;
	delete mTempString2;
}

/****************************************************************************
 * XmlMiniParser::setFile
 *
 * Arguments:
 *	file: file name
 *
 * Returns: error
 *
 * Description: 
 * 
 * Assigns an input file.
 ****************************************************************************/

INTERFACE void XmlMiniParser::setFile(const char *name)
{
	if (name == NULL) {
		delete mInputFile;
		mInputFile = NULL;
	}
	else {
		char *copy = new char[strlen(name) + 1];
		if (!copy)
		  throwException(ERR_MEMORY);

		strcpy(copy, name);
		delete mInputFile;
		mInputFile = copy;
	}

    mInputIsOpen = 0;
}

/****************************************************************************
 * XmlMiniParser::setBuffer
 *
 * Arguments:
 *	buffer: input buffer
 *	length: length of buffer
 *
 * Returns: none
 *
 * Description: 
 * 
 * Sets up to parse from a memory buffer.
 ****************************************************************************/

INTERFACE void XmlMiniParser::setBuffer(const char *buffer,
										int length)
{
	mInputBuffer = buffer;
	mInputBufferLength = length;
    mInputIsOpen = 0;
}

/****************************************************************************
 * XmlMiniParser::reset
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Resets the parser to its initial state.  We keep track of run-time options
 * but we lose the input source.
 ****************************************************************************/

INTERFACE void XmlMiniParser::reset(void)
{
	closeInput();

	delete mInputFile;
	mInputFile = NULL;

	mInputBuffer = NULL;
	mInputBufferLength = 0;

    mInputIsOpen = 0;

	mLine = 0;
	mColumn = 0;
	mOffset = 0;
	mInDoctype = 0;
	mEof = 0;
	
	mTokenIndex = 0;
	mLookaheadIndex = 0;

	mDatabuf->clear();

    mErrorCode = 0;
    mError[0] = 0;
}

/****************************************************************************
 *                                                                          *
 *   							  EXCEPTIONS                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser::throwException
 *
 * Arguments:
 *   code: error code to ponder
 *
 * Returns: none
 *
 * Description: 
 * 
 * Throws an exception after capturing interesting information
 * about the error and the parse location.
 * 
 ****************************************************************************/

PUBLIC void XmlMiniParser::throwException(int code, const char *more)
{
	Vbuf *buf;
	char *msg;
	int addpsn;

	addpsn = 1;
	msg = NULL;
	buf = Vbuf::create();

	buf->add("XML Parser ");

	switch (code) {
		
		case ERR_XMLP_INTERNAL:
			buf->add("internal error");
			break;

		case ERR_MEMORY:
			buf->add("memory allocation failure");
			break;

		case ERR_XMLP_SYNTAX:
			buf->add("syntax error");
			break;

		case ERR_XMLP_FILE_OPEN:
			buf->add("file open error");
			addpsn = 0;
			break;

		case ERR_XMLP_FILE_READ:
			buf->add("file read error");
			addpsn = 0;
			break;

		case ERR_XMLP_NO_INPUT:
			buf->add("empty input stream");
			addpsn = 0;
			break;

		case ERR_XMLP_EOF:
			buf->add("end of file");
			break;

		case ERR_XMLP_HALT:
			buf->add("halted");
			break;

		default:
			buf->add("unknown error ");
			buf->add(code);
			break;
	}

	if (addpsn) {
		buf->add(" at line ");
		buf->add(getLine() + 1);
		buf->add(" column ");
		buf->add(getColumn() + 1);
		buf->add(".");
	}

	if (more) {
		buf->add(" ");
		buf->add(more);
	}

	msg = buf->stealString();
	delete buf;

    Trace(1, "XmlMidiParser: %s\n", msg);
	throw new AppException(code, msg, true);	// set "no copy" flag
}

/****************************************************************************
 *                                                                          *
 *   							EVENT ADAPTER                               *
 *                                                                          *
 ****************************************************************************/
/*
 * A collection of default handler methods that do nothing.
 *
 */

INTERFACE void XmlEventAdapter::openDoctype(XmlMiniParser *p, 
											char *name, 
											char *pubid,
											char *sysid)
{
	p=p;
	delete name;
	delete pubid;
	delete sysid;
}

INTERFACE void XmlEventAdapter::closeDoctype(XmlMiniParser *p)
{
	p=p;
}

INTERFACE void XmlEventAdapter::openStartTag(XmlMiniParser *p, 
											 char *name)
{
	p=p;
	delete name;
}

INTERFACE void XmlEventAdapter::attribute(XmlMiniParser *p, 
										  char *name, 
										  char *value)
{
	p=p;
	delete name;
	delete value;
}

INTERFACE void XmlEventAdapter::closeStartTag(XmlMiniParser *p, int empty)
{
	p=p;
	empty=empty;
}

INTERFACE void XmlEventAdapter::endTag(XmlMiniParser *p, char *name)
{
	p=p;
	delete name;
}

INTERFACE void XmlEventAdapter::comment(XmlMiniParser *p, char *text)
{
	p=p;
	delete text;
}

INTERFACE void XmlEventAdapter::pi(XmlMiniParser *p, char *text)
{
	p=p;
	delete text;
}

INTERFACE void XmlEventAdapter::pcdata(XmlMiniParser *p, char *text)
{
	p=p;
	delete text;
}

INTERFACE void XmlEventAdapter::entref(XmlMiniParser *p, char *name)
{
	p=p;
	delete name;
}

INTERFACE void XmlEventAdapter::cdata(XmlMiniParser *p, char *text)
{
	p=p;
	delete text;
}

INTERFACE void XmlEventAdapter::error(XmlMiniParser *p, int code, 
									  const char *msg)
{
	p->throwException(code, msg);
}

/****************************************************************************
 *                                                                          *
 *   							 INPUT STREAM                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser::openInput
 *
 * Arguments:
 *	none: 
 *
 * Returns: error
 *
 * Description: 
 * 
 * Prepares the input "stream" for reading.  Required before you start
 * calling nextchar or advance.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::openInput(void)
{
    if (mInputIsOpen)
        return;

	// initialize some parse state

	mLookaheadIndex 	= 0;
	mTokenIndex 		= 0;
	mEof				= 0;
	mInDoctype 			= 0;
	mLine				= 0;
	mColumn				= 0;
	mOffset				= 0;

	clearToken();
	mDatabuf->clear();

	// set up input stream

	if (mInputBuffer != NULL) {
		// we're parsing a user defined buffer
		if (!mInputBufferLength)
		  throwException(ERR_XMLP_NO_INPUT);
		
		mPtr = (char *)mInputBuffer;
		mEnd = mPtr + mInputBufferLength;
        mInputIsOpen = 1;
	}
	else if (mInputFile != NULL) {
		// parsing a file
		size_t n;

		mInputFp = fopen(mInputFile, "rb");
		if (mInputFp == NULL)
		  throwException(ERR_XMLP_FILE_OPEN, mInputFile);

		n = fread(mPbuf, 1, MAX_XML_PARSEBUF, mInputFp);
		if (n == 0) {
			if (feof(mInputFp)) {
				mEof = 1;
				throwException(ERR_XMLP_NO_INPUT);
			}
			else
			  throwException(ERR_XMLP_FILE_READ, mInputFile);
		}
		else {
			mPtr = mPbuf;
			mEnd = mPtr + n;
            mInputIsOpen = 1;
		}
	}
	else {
		// no input source defined, error
		mEof = 1;
		throwException(ERR_XMLP_NO_INPUT);
	}
}

/****************************************************************************
 * XmlMiniParser::closeInput
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Releases any resources that may have been opened for the input stream.
 ****************************************************************************/

PRIVATE void XmlMiniParser::closeInput(void)
{
	if (mInputFp != NULL) {
		fclose(mInputFp);
		mInputFp = NULL;
	}

    mInputIsOpen = 0;
}

/****************************************************************************
 * XmlMiniParser::nextchar
 *
 * Arguments:
 *	retval: next character code
 *
 * Returns: error/EOF status
 *
 * Description: 
 * 
 * Returns the character at the current read position.
 * Throws an exception if we're at the end of file.  
 * Normally you don't call this unless this isn't the end of file.
 * 
 ****************************************************************************/

PRIVATE int XmlMiniParser::nextchar(void)
{
	if (mEof)
	  throwException(ERR_XMLP_EOF);

	return *mPtr; 
}

/****************************************************************************
 * XmlMiniParser::advance
 *
 * Arguments:
 *	none: 
 *
 * Returns: next character
 *
 * Description: 
 * 
 * Advances the read position to the next character, and returns it.
 * Throws an exception on read error, or EOF.
 * This is where we also try to keep track of the read position in the file.
 ****************************************************************************/

PRIVATE int XmlMiniParser::advance(void)
{
	if (!mEof) {
		if (mInputBuffer != NULL) {
			// we're parsing a buffer
			if (mPtr < mEnd)
			  mPtr++;

			if (mPtr >= mEnd)
			  mEof = 1;
		}
		else {
			// we're parsing a file
			if (mPtr < mEnd)
			  mPtr++;

			if (mPtr >= mEnd) {
				// get the next block
				size_t n;
				n = fread(mPbuf, 1, sizeof(mPbuf), mInputFp);
				if (n == 0) {
					// eof or error?
					if (feof(mInputFp))
					  mEof = 1;
					else {
						// misc file read error
						throwException(ERR_XMLP_FILE_READ);
					}
				}
				else {
					mPtr = mPbuf;
					mEnd = mPtr + n;
				}
			}
		}
	}

	if (mEof)
	  throw ERR_XMLP_EOF;

	// keep track of file position

	mOffset++;
	mColumn++;
	if (*mPtr == '\n') {
		mLine++;
		mColumn = 0;
	}

	return *mPtr;
}

/****************************************************************************
 * XmlMiniParser::advanceToChar
 *
 * Arguments: 
 *   retval: returned character code
 *
 * Returns: error
 *
 * Description: 
 * 
 * Built on advance(), but skips over whitespace and returns the next
 * printable character.
 * 
 ****************************************************************************/

PRIVATE int XmlMiniParser::advanceToChar(void)
{
	int ch;

	for (ch = nextchar() ; isspace(ch) ; ch = advance());

	return ch;
}

/****************************************************************************
 *                                                                          *
 *   							PARSE BUFFERS                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser::addLookahead
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds the character at the current read position to the lookahead buffer.
 * Used when we have to examine a few characters in a row to find special
 * sequences.  If the sequence isn't found, then the lookahead buffer is
 * usually dumped into the data buffer.
 * 
 * Lookahead is expected to be small, it should never exceed 4 characters.
 * Can throw an exception if we're at EOF.
 * 
 * shiftLookahead is used for dealing with comments which end
 * in -->.  Simple lookahead doesn't work here, because we've got
 * a character that may or may not be part of the data or
 * the markup.  Example "<!--------------->" is a valid comment, but you
 * have to be careful about which '-' are data and which are markup.
 * Same applies for PI's "<????????????>" and marked sections
 * <![INCLUDE[]]]]]]]>
 ****************************************************************************/

PRIVATE void XmlMiniParser::addLookahead(void)
{
	if (mLookaheadIndex >= MAX_XML_LOOKAHEAD)
	  throwException(ERR_XMLP_INTERNAL, "Lookahead overflow");

	mLookahead[mLookaheadIndex] = nextchar();
	mLookaheadIndex++;
}

PRIVATE void XmlMiniParser::shiftLookahead(int max)
{
	if (max >= MAX_XML_LOOKAHEAD)
	  throwException(ERR_XMLP_INTERNAL, "Lookahead overflow");

	if (mLookaheadIndex < max) {
		mLookahead[mLookaheadIndex] = nextchar();
		mLookaheadIndex++;
	}
	else {
		int i;

		// shift one character into the data buffer
		addData(mLookahead[0]);

		// shift remainder down
		for (i = 0 ; i < mLookaheadIndex - 1 ; i++)
		  mLookahead[i] = mLookahead[i+1];

		// and accumulate the next incomming character
		mLookahead[mLookaheadIndex - 1] = nextchar();
	}
}

PRIVATE int XmlMiniParser::compareLookahead(const char *pattern)
{
	int len = strlen(pattern);

	if (len >= MAX_XML_LOOKAHEAD)
	  throwException(ERR_XMLP_INTERNAL, "Lookahead overflow");

	return !strncmp(mLookahead, pattern, len);
}

/****************************************************************************
 * XmlMiniParser::clearLookahead
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Clears the lookahead buffer.
 ****************************************************************************/

PRIVATE void XmlMiniParser::clearLookahead(void)
{
	mLookaheadIndex = 0;
}

/****************************************************************************
 * XmlMiniParser::saveLookahead
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds the contents of the lookahead buffer to the data buffer.
 * Might throw an exception if we can't extend the data buffer.
 * The lookahead buffer is left in a cleared state.
 ****************************************************************************/

PRIVATE void XmlMiniParser::saveLookahead(void)
{
	int i;

	for (i = 0 ; i < mLookaheadIndex ; i++)
	  addData(mLookahead[i]);
	
	mLookaheadIndex = 0;
}

/****************************************************************************
 * XmlMiniParser::addToken
 *
 * Arguments:
 *	ch: character to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds the given character to the token buffer.
 * Used when we're trying to isolation things like element names, 
 * attribute names, entity names, etc.  The buffer is expected to be 
 * large enough, we'll throw an exception if it isn't.
 ****************************************************************************/

PRIVATE void XmlMiniParser::addToken(int ch)
{
	if (mTokenIndex + 2 >= MAX_XML_TOKEN)
	  throwException(ERR_XMLP_INTERNAL, "Token buffer overflow");

	mTokbuf[mTokenIndex] = ch;
	mTokenIndex++;

	// always leave mTokbuf null terminated
	mTokbuf[mTokenIndex] = '\0';
}

/****************************************************************************
 * XmlMiniParser::clearToken
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Clears the token buffer.
 ****************************************************************************/

PRIVATE void XmlMiniParser::clearToken(void)
{
	mTokenIndex = 0;
	// always leave mTokbuf null terminated
	mTokbuf[0] = '\0';
}

/****************************************************************************
 * XmlMiniParser::addData
 *
 * Arguments:
 *	ch: character to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a character to the data buffer.
 * Since this can be arbitrarily long, we use a Vbuf to avoid memory churn.
 ****************************************************************************/

PRIVATE void XmlMiniParser::addData(int ch)
{
	mDatabuf->addChar(ch);
}

PRIVATE void XmlMiniParser::addData(const char *str)
{
	mDatabuf->add(str);
}

/****************************************************************************
 * XmlMiniParser::getData
 *
 * Arguments:
 *
 * Returns: data string
 *
 * Description: 
 * 
 * Returns a copy of the string currently in the data buffer.
 * The data buffer is then cleared.
 ****************************************************************************/

PRIVATE char *XmlMiniParser::getData(void)
{
	char *data = NULL;
	
	// sigh, Vbuf::getString  will give us an empty string if nothing was
	// in the buffer, which is NOT what we want here.  
	// We could end up making generating an empty XmlPcdata event.

	if (mDatabuf->getSize()) {
		data = mDatabuf->copyString();
		mDatabuf->clear();
	}

	return data;
}

/****************************************************************************
 * XmlMiniParser::finishData
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Consolodates the pieces of PCDATA that we've accumulated, and generates
 * a data event.
 ****************************************************************************/

PRIVATE void XmlMiniParser::finishData(void)
{
	char *data;

	// consolodate the data, and clear the buffers
	data = getData();
	if (data != NULL) {
		if (mHandler == NULL)
		  delete data;
		else 
		  mHandler->pcdata(this, data);
	}
}

/****************************************************************************
 * XmlMiniParser::clearData
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Discards any data we may have pending.
 ****************************************************************************/

PRIVATE void XmlMiniParser::clearData(void)
{
	mDatabuf->clear();
}

/****************************************************************************
 *                                                                          *
 *   							FIELD PARSERS                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser::syntaxError
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Raises a syntax error.  We call this rather than have throw's 
 * scattered around in case there is some extra data gathering that
 * we want to do here.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::syntaxError(void)
{
	throwException(ERR_XMLP_SYNTAX);
}

/****************************************************************************
 * XmlMiniParser::halt
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called whenever a handler method returns non-zero, which indicates the
 * desire to halt parsing.  This isn't an error, parsing can resume 
 * later.
 ****************************************************************************/

PRIVATE void XmlMiniParser::halt(void)
{
	throw ERR_XMLP_HALT;
}

/****************************************************************************
 * XmlMiniParser::consumeKeyword
 *
 * Arguments:
 *	expected: expected string
 *
 * Returns: error
 *
 * Description: 
 * 
 * Helper for parseTagOpen.  Here, we've already parsed enough of a 
 * <! pattern to know what this is supposed to be, and now we want to 
 * strip off the remaining characters, which must be what we expect.
 * If the expected string isn't found, then this represents a syntax error.
 * 
 * Hack, if any space characters are found, they will match any whitespace
 * character in the input stream.  This is used for the trailing whitespace
 * expected after <! names.  I'm assuming that this is required, if not
 * just take the space out of the callers.
 * 
 * Expected string is expected to be downcased!
 ****************************************************************************/

PRIVATE void XmlMiniParser::consumeKeyword(const char *expected)
{
	const char *ptr;
	int ch;

	for (ptr = expected ; *ptr ; ptr++) {
		ch = tolower(advance());
		if ((isspace(*ptr) && !isspace(ch)) ||
			(!isspace(*ptr) && *ptr != ch)) {

			syntaxError();
		}
	}
}

/****************************************************************************
 * parseTagOpen
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Begin parsing after recognition of a '<' in the data stream.  
 * We will not be in a CDATA section at this point, to this must be the 
 * start of some valid markup.  If we can't make sense out of this, we
 * raise an error and abort parsing.  I suppose we could by default continue
 * and treat this as data, but I don't think that's part of the spec ? 
 * 
 * The read position is over the '<'.
 * 
 * We recognize the following patterns, then call out to other more
 * specific field parsers:
 * 
 * 			<?			processing instruction
 * 			</			end tag
 * 			<!			DOCTYPE, ENTITY, NOTATION, ELEMENT, ATTLIST
 * 			<![CDATA[	cdata section
 * 			<!%			conditional section
 * 
 * Anything that doesn't fit the above patterns is assumed to be an
 * element start tag.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseTagOpen(void)
{
	int ch;

	// generate an event for any pending pcdata 
	finishData();

	// get the next character
	ch = advance();

	if (ch == '?') {
		// processing innstruction
		parsePi();
	}
	else if (ch == '/') {
		// end tag
		parseEtag();
	}
	else if (ch == '!') {
		// <!DOCTYPE, <!ENTITY, <!NOTATION, <!ELEMENT, <!ATTLIST, 
		// <!--, <![CDATA[, <!%, 

		ch = tolower(advance());

		if (ch == '-') {
			consumeKeyword("-");
			parseComment();
		}
		else if (ch == '[') {
			consumeKeyword("cdata[");
			parseCdata();
		}
		else if (ch == '%') {
			parseConditional();
		}
		else if (ch == 'd') {
			if (mInDoctype)
			  syntaxError();
			else {
				consumeKeyword("octype ");
				parseDoctype();
			}
		}
		else if (ch == 'n') {
			if (!mInDoctype) 
			  syntaxError();
			else {
				consumeKeyword("otation ");
				parseNotation();
			}
		}
		else if (ch == 'a') {
			if (!mInDoctype)
			  syntaxError();
			else {
				consumeKeyword("ttlist ");
				parseAttlist();
			}
		}
		else if (ch == 'e') {
			if (!mInDoctype)
			  syntaxError();
			else {
				// need more
				ch = advance();
				if (ch == 'l') {
					consumeKeyword("ement ");
					parseElement();
				}
				else if (ch == 'n') {
					consumeKeyword("tity ");
					parseEntity();
				}
				else
				  syntaxError();
			}
		}
		else {
			// do we treat what we just saw as data?
			syntaxError();
		}
	}
	else {
		// treat whatever it is as an element start tag
		parseStag();
	}
}

/****************************************************************************
 * XmlMiniParser::parseCloseBracket
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Here from parseLoop after discovering a ']' when we're in "doctype" mode.
 * If mInDoctype is off, then this will have been treated as just another
 * data character.
 * 
 * We look for ']>' which ends the doctype statement.
 * 
 * Read position is advanced to the next unanalyzed.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseCloseBracket(void)
{
	int ch;

	// remember this character, and get the next one
	clearLookahead();
	addLookahead();
	ch = advance();

	if (ch != '>') {
		// normal data
		saveLookahead();
		addData(ch);
	}
	else {
		// found the doctype terminator
		finishData();
		if (mHandler != NULL)
		  mHandler->closeDoctype(this);
	}

	// advance past '>' or last data character
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseEntityName
 *
 * Arguments:
 *
 * Returns: entity name in "mTokbuf"
 *
 * Description: 
 * 
 * Isolates an entity name in the input stream, the current read position
 * is expected to be the first charcter after "&" (or "&#" if this
 * is a numeric character entity reference).
 * 
 * We stack characters in "mTokbuf" until we find the ';' terminator.  
 * Should we be trimming leading and trailing whitespace?
 *
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseEntityName(void)
{
	int ch;

	clearToken();
	for (ch = nextchar() ; ch != ';' ; ch = advance())
	  addToken(ch);

	// check degenerate cases &; or &#;
	if (strlen((char *)mTokbuf) == 0)
	  syntaxError();
}

/****************************************************************************
 * XmlMiniParser::copyString
 *
 * Arguments:
 *	src: source string
 *
 * Returns: string copy
 *
 * Description: 
 * 
 * Copies a string, usually from one of our internal data buffers so it
 * can be passed to and owned by a handler method.
 ****************************************************************************/

PRIVATE char *XmlMiniParser::copyString(const char *src)
{
	char *copy;
	int len;

	copy = NULL;
	if (src != NULL) {
		len = strlen((char *)src);
		copy = new char [len + 1];
		if (copy == NULL)
		  throwException(ERR_MEMORY);
		strcpy((char *)copy, (char *)src);
	}

	return copy;
}

/****************************************************************************
 * XmlMiniParser::parseEntref
 *
 * Arguments:
 *
 * Returns: advances read position
 *
 * Description: 
 * 
 * Here we've on a '&' and need to process an entity reference.
 * We have to handle these cases:
 *
 * 			&#xxx;			character entity
 * 			&foo;			named entity reference
 * 			&lt;			built-in text entity for '<'
 * 			&amp;			built-in text entity for '&'
 * 			&sq;			built-in text entity for '\''
 * 			&dq;			built-in text entity for '"'
 *
 * Normally we convert character entity references, &lt; and &amp;
 * into the corresponding characters in the data string being built.  
 * References to other named entities raise an entity event.
 *
 * This behavior can be controlled using two parameters:
 *
 * 		void setPreserveCharacterEntities(int enable);
 *
 * 			When enable is on, character entity syntax passes through
 * 			unchanged into the data string.
 * 
 * 		void setNoEntrefEvents(int enable);
 * 			
 * 			When enable is on, we don't genrate entity refernce events,
 * 			but rather just include the text of the reference in the
 * 			data stream.
 *
 * May need more options here.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseEntref(void)
{
	int ch;

	// get the next character
	ch = advance();

	if (ch == '#') {
		// character entity
		// skip over the #, and get the entity name into "mTokbuf"
		(void)advance();
		parseEntityName();

		if (mPreserveCharent) {
			// sigh, have to assemble it again
			addData("&#");
			addData(mTokbuf);
			addData(";");
		}
		else {
			// Determine the numeric character code.
			// Probably should do move validation on the characters.
			int ival = atoi((char *)mTokbuf);

			// need to be smart about multi-byte here !
			if (ival > 0xFF) 
			  syntaxError();
			else 
			  addData(ival);
		}
	}
	else {
		// named entity reference, get the name into "mTokbuf"
		parseEntityName();

		if (!strcmp((char *)mTokbuf, "lt")) {
			if (mPreserveCharent)
			  addData("&lt;");
			else
			  addData("<");
		}
		else if (!strcmp((char *)mTokbuf, "amp")) {
			if (mPreserveCharent)
			  addData("&amp;");
			else
			  addData("&");
		}
		else if (!strcmp((char *)mTokbuf, "sq")) {
			if (mPreserveCharent)
			  addData("&amp;");
			else
			  addData("'");
		}
		else if (!strcmp((char *)mTokbuf, "dq")) {
			if (mPreserveCharent)
			  addData("&amp;");
			else
			  addData("\"");
		}
		else {
			// Its some other entity reference raise an event.
			if (!mInlineEntref) {
				// raise event for any pending data
				finishData();
				if (mHandler != NULL)
				  mHandler->entref(this, copyString(mTokbuf));
			}
			else {
				// Don't want events, leave it in the data stream.
				// Would be nice if we didn't have to reassemble this.
				addData("&");
				addData(mTokbuf);
				addData(";");
			}
		}
	}

	// advance past ';'
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseName
 *
 * Arguments:
 *
 * Returns: name in "mTokbuf"
 *
 * Description: 
 * 
 * Isolates an name in the input stream, the current read position
 * is expected to be on the first character of the name, or on some
 * whitespace which will be ignored.  
 * 
 * We stack characters in "mTokbuf" until we find more whitespace, or the
 * special characters '>', '/', and '['.
 * 
 * This tokenizer can be used for the doctype name, as well as element names
 * in start and end tags.  
 *
 * There are probably more character restrictions here we could detect.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseName(void)
{
	int ch;

	clearToken();

	// ignore leading whitespace
	for (ch = nextchar() ; isspace(ch) ; ch = advance());

	// extract the token
	for (ch = nextchar() ; 
		 !isspace(ch) && ch != '>' && ch != '[' && ch != '/' ; 
		 ch = advance())
	  addToken(ch);

	// check degenerate cases <>, </>, <!DOCTYPE>, and <!DOCTYPE[
	if (strlen((char *)mTokbuf) == 0)
	  syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseDoctype
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Here we've just consumed the string "<!DOCTYPE " the read position will
 * be on the character after the one character of trailing whitespace.
 * We're expecting a statement of this form:
 *
 * 		<!DOCTYPE NAME PUBLIC "pubid" SYSTEM "sysid">
 * 
 * optionaally with [ instaed of > at the end to enter into an internal 
 * subset. 
 *
 * Odd cases:
 * 
 * 		<!DOCTYPE foo>
 * 		<!DOCTYPE foo[
 * 
 * Note that we use the "mTempString1" and "mTempString2" members of the
 * mini parser object to store the pubid and sysid we're working with.  
 * This is necessary to prevent leaks, since if we're using longjump, 
 * we have to attach resources to the parser object, we can't ensure that
 * catch blocks are going to get entered on the way out.  There are probably
 * other places that need this treatment, which may require that we be
 * more rigerous about declaring when the two temp strings can be used.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseDoctype(void)
{
	int ch;

	// pubid is mTempString1
	// sysid is mTempString2

	delete mTempString1;
	delete mTempString2;
	mTempString1 = NULL;
	mTempString2 = NULL;

	// extract the doctype name into the token buffer
	parseName();

	// parse doctype fields
	do {
		ch = tolower(advanceToChar());

		if (ch == 'p') {
			if (mTempString1 != NULL)
			  syntaxError();
			else {
				consumeKeyword("ublic ");
				mTempString1 = parseString();
			}
		}
		else if (ch == 's') {
			if (mTempString2 != NULL)
			  syntaxError();
			else {
				consumeKeyword("ystem ");
				mTempString2 = parseString();
			}
		}
		else if (ch == '"') {
			// must be a sysid following a public id
			if (!mTempString1 || mTempString2)
			  syntaxError();
			else
			  mTempString2 = parseString();
		}
		else if (ch != '[' && ch != '>')
		  syntaxError();

	} while (ch != '[' && ch != '>');

	// Raise an open doctype event, mTokbuf still has the name.
	// Careful when transfering ownership of the pubid & sysid, null
	// out the locals in case the handler throws a halt() exception,
	// and we end up down in our catch trying to clean up.

	if (mHandler != NULL) {
		char *p = mTempString1;
		char *s = mTempString2;
		mTempString1 = NULL;
		mTempString2 = NULL;
		mHandler->openDoctype(this, copyString(mTokbuf), p, s);
	}

	// set this so we know to look for "]>" later in parseLoop.
	if (ch == '[')
	  mInDoctype = 1;
	else if (mHandler != NULL)
	  mHandler->closeDoctype(this);

	// advance past '>' or '['
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseEntity
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses an <!ENTITY statement. 
 * Not supported yet.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseEntity(void)
{
	syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseNotation
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses a <!NOTATION statement.
 * Not supported yet.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseNotation(void)
{
	syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseElement
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses a <!ELEMENT statement.
 * Not supported yet.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseElement(void)
{
	syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseAttlist
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses a <!ATTLIST statement.
 * Not supported yet.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseAttlist(void)
{
	syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseConditional
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses a <!%ref;[ conditional marked section.
 * Not supported yet.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseConditional(void)
{
	syntaxError();
}

/****************************************************************************
 * XmlMiniParser::parseComment
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Parses a comment.  We've already consumed the "<!--", read position
 * is on the second '-'.  Get text till we hit "-->".  
 * 
 * Have to use shiftLookahead here since we've got a pattern that's
 * harder to parse.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseComment(void)
{
	int done, ch;

	// raise events for any pending data
	finishData();
	clearLookahead();

	done = 0;
	while (!done) {
		ch = advance();
		shiftLookahead(3);
		if (ch == '>' && compareLookahead("-->"))
		  done = 1;
	}

	if (mHandler == NULL || mFilterComments)
	  clearData();
	else
	  mHandler->comment(this, getData());

	// advance past '>'
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseCdata
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Here we've just consumed the "<![CDATA[" token, and are now ready to 
 * capture a CDATA section.  Pop text till we hit "]]>".
 * Like comments, have to use shiftLookahead since we've got a termination
 * pattern that's harder to parse.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseCdata(void)
{
	int done, ch;

	// raise event for any pending data
	finishData();
	clearLookahead();

	done = 0;
	while (!done) {
		ch = advance();
		shiftLookahead(3);
		if (ch == '>' && compareLookahead("]]>"))
		  done = 1;
	}

	if (mHandler == NULL)
	  clearData();			// no one cares
	else
	  mHandler->cdata(this, getData());

	// advance past '>'
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parsePi
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Here we've just consumed a "<?" and the read position is on the '?'.
 * Consume the text of the processing instruction and generate an event.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parsePi(void)
{
	int done, ch;

	finishData();
	clearLookahead();

	done = 0;
	while (!done) {
		ch = advance();
		shiftLookahead(2);
		if (ch == '>' && compareLookahead("?>"))
		  done = 1;
	}

	if (mHandler == NULL)
	  clearData();
	else
	  mHandler->pi(this, getData());


	// advance past '>'
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseEtag
 *
 * Arguments:
 *	none: 
 *
 * Returns: error
 *
 * Description: 
 * 
 * Here we've just consumed "</", the read position is on the '/'.
 * Consume the text of the end tag, and generate an event.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseEtag(void)
{
	finishData();

	// skip the '/' and extract the name into "mTokbuf"
	(void)advance();
	parseName();
	
	// generate an event
    if (mHandler != NULL)
	  mHandler->endTag(this, copyString(mTokbuf));

    //
    // advance past the '>'
    // This is where we're likely to throw an EOF exception, when parsing
    // the last end tag in the file.
	(void)advance();
}

/****************************************************************************
 * XmlMiniParser::parseStag
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Here the read position is on the character following the '<' and
 * we've decided this looks like a start tag.
 * 
 * We're not doing any validation on the characters that go into an 
 * element name, you could have <^%$@> if you wanted to.  
 * We're also not attempting to keep any sort of stack to ensure that
 * elements are balanced.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseStag(void)
{
	char *attval;
	int ch, empty;

	// raise data event
	finishData();

	// get the element name
	parseName();

	// raise stag event, should do some validation on the name characters
	if (mHandler != NULL)
	  mHandler->openStartTag(this, copyString(mTokbuf));

	// parse attributes
	ch = nextchar();
	while (ch != '>' && ch != '/') {

		// parse the attribute name into "mTokbuf"
		parseAttributeName();
		if (strlen((char *)mTokbuf)) {

			// parse the value
            // note that if the value contains an entity reference
            // this will trash the token buffer so have to 
            // copy it now
            strcpy(mNamebuf, mTokbuf);

			attval = parseString();

			// parseString which is built upon getData will return
			// a NULL here if the attribute was written att=''
			// since nothing ends up in the data buffer.  It is imporant
			// when we create the attribute values though that this be
			// treated as if it were the empty string.  We can't change
			// getData to do this, since its used in too many other contexts
			// where returning NULL is desired.

			if (attval == NULL)
			  attval = copyString("");

			// raise an event, mTokbuf still has name
			if (mHandler != NULL)
			  mHandler->attribute(this, copyString(mNamebuf), attval);
			else
			  delete attval;
		}

		ch = nextchar();
	}

	// if we're on the empty element marker, advance past it
	empty = 0;
	if (ch == '/') {
		empty = 1;
		ch = advance();
		if (ch != '>')
		  syntaxError();
	}
	
	// Generate close start tag event, we've lost the name
	// during attribute parsing, might want to save it and pass it again.
	if (mHandler != NULL)
	  mHandler->closeStartTag(this, empty);

	// advance past the '>'
	(void)advance();

}

/****************************************************************************
 * XmlMiniParser::parseAttributeName
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Helper for parseStag.  Here we parse what might be an attribute.
 * We trim leading whitespace until we hit a character or '>'.
 * 
 * We then load the "mTokbuf" buffer with characters until we hit whitespace, 
 * or one of the special characters '=', '>', or '/'.
 * 
 * If we hit whitespace, we consume it until we hit a special character.
 * If we hit '=', we consume that too.  
 *
 * The end result will be the attribute name in mTokbuf, and the read 
 * position after the = or on the '>'.
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseAttributeName(void)
{
	int ch;

	clearToken();

	// ignore leading whitespace
	for (ch = nextchar() ; isspace(ch) ; ch = advance());

	// extract the token
	for (ch = nextchar() ; 
		 !isspace(ch) && ch != '=' && ch != '>' && ch != '/' ; 
		 ch = advance())
	  addToken(ch);

	// consume trailing whitespace
	for (ch = nextchar() ; 
		 isspace(ch) && ch != '=' && ch != '>' && ch != '/' ; 
		 ch = advance());

	// skip equal
	if (nextchar() == '=')
	  (void)advance();

	// empty token is ok, it means there aren't any attributes
}

/****************************************************************************
 * XmlMiniParser::parseString
 *
 * Arguments:
 *
 * Returns: string extracted (without quotes)
 *
 * Description: 
 * 
 * Parses a delimited string.
 * This is used both for attribute values, as well as public and system
 * identifiers in the DOCTYPE statement.  Hmm, might have to have
 * different string parsers for each context?
 * 
 * We're expected to be on the start of the string delimiter, or on
 * some whitespace preceeding it.  We consume data until we hit
 * another delimiter of the same sex as the one we started with.  
 * 
 * We need to handle character entity references plus the built in
 * text entities &amp; etc. as we go here.
 * 
 ****************************************************************************/

PRIVATE char *XmlMiniParser::parseString(void)
{
	int ch, delim;

	// ignore leading whitespace
	for (ch = nextchar() ; isspace(ch) ; ch = advance());

	// must be a quote character
	if (ch != '\'' && ch != '"')
	  syntaxError();
	delim = ch;

	// process characters till we hit another delimiter
	clearData();
	(void)advance();

	for (ch = nextchar() ; ch != delim ; ) {

		if (ch != '&') {
			addData(ch);
            ch = advance();
		}
		else {
			// it looks like an entity reference, use the entref
			// parser which will add things to the data buffer and
			// leave us positioned over the character following the ;
			parseEntref();
            ch = nextchar();
		}
	}

	// skip over the delimiter
	advance();

	// combine the data, and return a free standing string
	return getData();
}

/****************************************************************************
 *                                                                          *
 *   							  PARSE LOOP                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlMiniParser::parseLoop
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Enters the main parse loop, over and over.  Over and over.
 * We'll eventually hit an EOF exception, and wind up back in
 * XmlMiniParser::parse.  Could try to be more orderly and detect
 * premature EOF.
 ****************************************************************************/

PRIVATE void XmlMiniParser::parseLoop(void)
{
	int ch;

	mStarted = 0;

	while (1) {
		
		ch = nextchar();

		if (ch == '<') {
			mStarted = 1;
			parseTagOpen();
		}

		else if (!mStarted)
		  syntaxError();

		else if (ch == '&')
		  parseEntref();

		else if (ch == ']') {
			if (mInDoctype)
			  parseCloseBracket();
			else {
				addData(ch);
				(void)advance();
			}
		}
		else {
			addData(ch);
			(void)advance();
		}
	}
}

/****************************************************************************
 * XmlMiniParser::parse
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Launches the parsing process.
 ****************************************************************************/

INTERFACE int XmlMiniParser::parse(void)
{
    mErrorCode = 0;
    mError[0] = 0;

	try {

		// allocate a data buffer if we don't already have one
		if (!mDatabuf)
		  mDatabuf = Vbuf::create();
	
		// allocate a parse buffer, if we need one
		if (mPbuf == NULL && mInputBuffer == NULL) {
			mPbuf = new char[MAX_XML_PARSEBUF];
			if (mPbuf == NULL)
			  throwException(ERR_MEMORY);
		}

		// initialize the input streams
		openInput();

		// start parsing
		parseLoop();

		// Formerly closed input streams with closeInput here, but
		// now we let ERR_XMLP_EOF exception handler do that so that
		// we can parse multiple objects out of one XML input source.
		// Why was this necessary ? - jsl
        // closeInput();
	}
	catch (int e) {
        // EOF is normal
        if (e != ERR_XMLP_EOF) {
            if (e == ERR_XMLP_HALT)
              fixHaltPosition(e);

            // if e != ERR_XMLP_EOF, its an odd error?
            mErrorCode = e;
            sprintf(mError, "Internal Error %d", mErrorCode);
        }
	}
    catch (AppException& e) {
        // Formerly let these out but since nothing was catching 
        // them it caused Mobius to terminate.  Use this to unwind
        // from the parser call stack, but leave errors on the parser.
        mErrorCode = e.getCode();
        CopyString(e.getMessage(), mError, sizeof(mError));
        // can't delete these, what happens to them?
        //delete e;
    }
    catch (AppException* e) {
        // Had to start using pointers for some reason, C++ sucks
        mErrorCode = e->getCode();
        CopyString(e->getMessage(), mError, sizeof(mError));
        delete e;
    }
    
    return mErrorCode;
}

/****************************************************************************
 * XmlMiniParser::fixHaltPosition
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Added some time ago, but I forget the reasons.
 * Comments were:
 * 
 * 		There seem to be parser-event handlers that throw end-of-object
 * 		exceptions right past the call to advance() in parseEtag(),
 * 		which leaves the input stream in an invalid state, as the tag
 * 		close is left as the next character to be read.  This makes the
 * 		first attempt at resuming the parser to extract the second
 * 		object from the file fail with a syntax error.  Unfortunately,
 * 		we can't just stick an advance in a catch() block and then
 * 		rethrow, because advance() itself can throw exceptions.  Ergo,
 * 		we attempt to clean up the leftover tag-close character here.
 * 
 * Not sure I understand the above comments, but in any event, 
 * advance can throw exceptions if we just happen to be at the end
 * of the stream on the ERR_XMLP_HALT condition, so we need to catch around
 * this.  Probably a cleaner way to do this...
 * 
 ****************************************************************************/

PRIVATE void XmlMiniParser::fixHaltPosition(int errcode)
{

	try {
		if (((errcode == 0) || 
			 (errcode == ERR_XMLP_HALT)) && (mPtr >= mPbuf)) {

			if ((mPtr < mEnd) && (*mPtr == '>'))
			  (void)advance();

			while ((mPtr < mEnd)
				   && ((*mPtr == '\n') || (*mPtr == '\r')))
			  (void)advance();
		}
	}
	catch (int e) {

		// here, this should be the only thing we get
		if (e != ERR_XMLP_EOF) {
			// !! what now?
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/




