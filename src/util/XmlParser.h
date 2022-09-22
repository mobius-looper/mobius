/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An XML mini-parser.
 * 
 * This was developed to provide a light-weight way to parse well formed
 * and valid XML files, though it does try to recover from non-well formed
 * files and raise an appropriate error.  It is often used
 * in conjuction with the "xom" memory model through the XomParser
 * subclass.
 * 
 * The parser is not SAX compliant, though it could be made so with
 * little effort.  
 */

#ifndef XMLMINI_H
#define XMLMINI_H

#include "Port.h"

/****************************************************************************
 * XMLP_ERR
 *
 * Description: 
 * 
 * Error codes that might be returned in exceptions thrown
 * by the parser.
 ****************************************************************************/

#define ERR_XMLP_INTERNAL 		ERR_BASE_XMLP + 1
#define	ERR_XMLP_MEMORY			ERR_BASE_XMLP + 2
#define ERR_XMLP_SYNTAX 		ERR_BASE_XMLP + 3
#define ERR_XMLP_FILE_OPEN 		ERR_BASE_XMLP + 4
#define ERR_XMLP_FILE_READ 		ERR_BASE_XMLP + 5
#define ERR_XMLP_EOF 			ERR_BASE_XMLP + 6
#define ERR_XMLP_HALT 			ERR_BASE_XMLP + 7
#define ERR_XMLP_NO_INPUT		ERR_BASE_XMLP + 8

/****************************************************************************
 * XmlEventHandler
 *
 * Description: 
 * 
 * A class defining the interface for an object that can be registered
 * as an event handler in the XmlMiniParser class.  Such an object
 * is notified as various things are parsed by calling the relevant
 * event method.
 * 
 * The strings passed to the handler methods are owned by the handler, and
 * must be deleted.
 ****************************************************************************/

class XmlEventHandler {
	
  public:
	
	virtual void openDoctype(class XmlMiniParser *p, 
							 char *name, 
							 char *pubid,
							 char *sysid) = 0;
	
	virtual void closeDoctype(class XmlMiniParser *p) = 0;
	
	virtual void openStartTag(class XmlMiniParser *p, 
							  char *name) = 0;
	
	virtual void attribute(class XmlMiniParser *p, 
						   char *name, 
						   char *value) = 0;
	
	virtual void closeStartTag(class XmlMiniParser *p, int empty) = 0;
	virtual void endTag(class XmlMiniParser *p, char *name) = 0;
	virtual void comment(class XmlMiniParser *p, char *text) = 0;
	virtual void pi(class XmlMiniParser *p, char *text) = 0;
	virtual void pcdata(class XmlMiniParser *p, char *text) = 0;
	virtual void entref(class XmlMiniParser *p, char *name) = 0;
	virtual void cdata(class XmlMiniParser *p, char *text) = 0;
	
	// todo: entity declarations, notations, entity references,
	// marked sections...
				 
	// still needs thought
	virtual void error(class XmlMiniParser *p, int code, const char *msg) = 0;
	
};

/****************************************************************************
 * XmlEventAdapter
 *
 * Description: 
 * 
 * Implements the XmlEventHandler interface, providing default methods
 * that do nothing but free the strings that passed in.  This allows you
 * to create classes that overload only the things you want.  It remains
 * to be seen how useful this is.
 ****************************************************************************/

class XmlEventAdapter : public XmlEventHandler {

  public:

	virtual INTERFACE void openDoctype(class XmlMiniParser *p, 
									   char *name, 
									   char *pubid,
									   char *sysid);

	virtual INTERFACE void closeDoctype(class XmlMiniParser *p);

	virtual INTERFACE void openStartTag(class XmlMiniParser *p, 
										char *name);

	virtual INTERFACE void attribute(class XmlMiniParser *p, 
									 char *name, 
									 char *value);
 
	virtual INTERFACE void closeStartTag(class XmlMiniParser *p, int empty);
	virtual INTERFACE void endTag(class XmlMiniParser *p, char *name);
	virtual INTERFACE void comment(class XmlMiniParser *p, char *text);
	virtual INTERFACE void pi(class XmlMiniParser *p, char *text);
	virtual INTERFACE void pcdata(class XmlMiniParser *p, char *text);
	virtual INTERFACE void entref(class XmlMiniParser *p, char *name);
	virtual INTERFACE void cdata(class XmlMiniParser *p, char *text);

	// still needs thought
	virtual INTERFACE void error(class XmlMiniParser *p, int code,
								 const char *msg);

};

/****************************************************************************
 * XmlMiniParser
 *
 * Description: 
 * 
 * A class that provides low overhead XML parsing.
 * Parsing "callbacks" are defined through the XmlEventHandler interface
 * class for which which the user is expected to provide an implmentation.
 * 
 ****************************************************************************/

class XmlMiniParser {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE XmlMiniParser(void);
	INTERFACE ~XmlMiniParser(void);
	
	void setEventHandler(XmlEventHandler *h) {
		mHandler = h;
	}

	// specify an input source

	INTERFACE void setFile(const char *name);
	INTERFACE void setBuffer(const char *buffer, int length);

	// options

	void setPreserveCharacterEntities(int e) {
		mPreserveCharent = e;
	}

	void setInlineEntityReferences(int e) {
		mInlineEntref = e;
	}

	void setFilterComments(int e) {
		mFilterComments = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// control
	//
	//////////////////////////////////////////////////////////////////////

	// reset to prepare for another input source
	INTERFACE void reset(void);

	// called after construction to start parsing, returns non-zero on error
	INTERFACE int parse(void);

	// called from within handlers to control execution

    INTERFACE void halt(void);		// stop parsing and reset
	INTERFACE void skip(void);		// skip over the current "object"

	// raise error condition, halt parsing
	INTERFACE void error(int code, const char *msg);

	//////////////////////////////////////////////////////////////////////
	//
	// state accessors
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE int getLine(void) {
		return mLine;
	}

	INTERFACE int getColumn(void) {
		return mColumn;
	}

	INTERFACE int getOffset(void) {
		return mOffset;
	}

    INTERFACE int getErrorCode() {
        return mErrorCode;
    }

    INTERFACE const char* getError() {
        return mError;
    }

	//////////////////////////////////////////////////////////////////////
	//
	// error codes
	// didn't have any luck with "static const int foo = 1;", why can't
    // we do that here?
	// putting #define constants here works, but I can't figure out a way
	// to make the accessible on the outside.  XmlMiniParser::ERR_SYNTAX
	// doesn't work.  
	// 	
	// Moved outside the class. C++ sucks.
	//
	//////////////////////////////////////////////////////////////////////

	void throwException(int code, const char *msg = NULL);

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	//////////////////////////////////////////////////////////////////////
	//
	// data members
	//
	//////////////////////////////////////////////////////////////////////

	// user options

	int					mPreserveCharent;
	int					mInlineEntref;
	int					mFilterComments;

	// user specified event handlers
	XmlEventHandler		*mHandler;

	// input stream(s)
	const char			*mInputBuffer;
	int					mInputBufferLength;
	char 				*mInputFile;
	FILE 				*mInputFp;

	// position within the file
	int					mLine;
	int					mColumn;
	int					mOffset;

	// transient parse state
	int 				mInDoctype;
	int					mEof;
	int					mStarted;
    int                 mInputIsOpen;

	// parse buffer
	// this defines the block size used when parsing files
	// this could be abstracted out at some point to better encapsulate UTF16
	#define MAX_XML_PARSEBUF 4086
	char *mPbuf;

	char *mPtr;		// current position in parse buffer
	char *mEnd;		//  last valid address in parse buffer plus one

	// token buffer
	#define MAX_XML_TOKEN 256
	char mTokbuf[MAX_XML_TOKEN];
	int mTokenIndex;

	// attribute name buffer
	char mNamebuf[MAX_XML_TOKEN];
 
	// lookahead buffer
	#define MAX_XML_LOOKAHEAD 16
	char mLookahead[MAX_XML_LOOKAHEAD];
	int mLookaheadIndex;

	// data buffer(s)
	class Vbuf *mDatabuf;

	// temporary string resources
	char	*mTempString1;
	char 	*mTempString2;

    // error status
    int     mErrorCode;
    char    mError[MAX_XML_TOKEN];

	//////////////////////////////////////////////////////////////////////
	//
	// Methods
	//
	//////////////////////////////////////////////////////////////////////

	void openInput(void);
	void closeInput(void);
	int  nextchar(void);
	int  advance(void);
	int  advanceToChar(void);
	void clearLookahead(void);
	void addLookahead(void);
	void shiftLookahead(int max);
	int  compareLookahead(const char *pattern);
	void saveLookahead(void);
	void clearToken(void);
	void addToken(int ch);
	void clearData();
	void addData(int ch);
	void addData(const char *str);
	char *getData(void);
	void finishData(void);
	void syntaxError(void);
	void consumeKeyword(const char *expected);
	void parseTagOpen(void);
	void parseCloseBracket(void);
	void parseEntityName(void);
	void parseEntref(void);
	void parseName(void);
	void parseDoctype(void);
	void parseEntity(void);
	void parseNotation(void);
	void parseElement(void);
	void parseAttlist(void);
	void parseConditional(void);
	void parseComment(void);
	void parseCdata(void);
	void parsePi(void);
	void parseEtag(void);
	void parseStag(void);
	void parseAttributeName(void);
	char *parseString(void);
	void parseLoop(void);
	void fixHaltPosition(int errcode);

	char *copyString(const char *src);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
