/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A light-weight XML parser that builds a simple memory model.
 * Note that the parser is NOT reentrant.
 * 
 */

#ifndef XOMPARSER_H
#define XOMPARSER_H

#include "Port.h"
#include "XmlParser.h"
#include "XmlModel.h"

/****************************************************************************
 * XomStack
 *
 * Description: 
 * 
 * Class used to maintain a parse stack for the XomParser.
 * We origianlly just used the XmlNode parent/child relationship for the
 * stack, but it became convenient to store other things with stacked 
 * elements, like their location in the file.  This is extremely useful
 * to find elements with missing end tags.
 ****************************************************************************/

class XomStack {

  public:

	XomStack(void) {
		mStack	= NULL;
		mNode	= NULL;
		mLine	= 0;
		mColumn = 0;
	}

	~XomStack(void){}

	XomStack *push(XmlNode *node, int line, int col) {
		XomStack *neu 	= new XomStack;
		neu->mStack 	= this;
		neu->mNode 		= node;
		neu->mLine		= line;
		neu->mColumn	= col;
		return neu;
	}

	XomStack *pop(void) {
		XomStack *neu = (this) ? this->mStack : NULL;
		delete this;
		return neu;
	}

	XomStack *getStack(void) {
		return mStack;
	}

	XmlNode *getNode(void) {
		return mNode;
	}

	int getLine(void) {
		return mLine;
	}

	int getColumn(void) {	
		return mColumn;
	}

  private:

	XomStack	*mStack;
	XmlNode		*mNode;
	int			mLine;
	int			mColumn;

};

/****************************************************************************
 * XomParser
 *
 * Description: 
 * 
 * A class that provides a mechanism for parsing XML buffers and
 * building an XmlDocument object in memory.  Built upon the
 * XmlMiniParser.
 * 
 * For simplicity, we're our own XmlEventAdapter too, rather than building
 * a seperate instance for event handling.
 * 
 ****************************************************************************/

class XomParser : public XmlEventAdapter {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE XomParser(void);
	INTERFACE ~XomParser(void);
	
	// parser options, forward to the internal parser

	void setPreserveCharacterEntities(int e) {
		if (mParser != NULL)
		  mParser->setPreserveCharacterEntities(e);
	}

	void setInlineEntityReferences(int e) {
		if (mParser != NULL)
		  mParser->setInlineEntityReferences(e);
	}

	// a one shot constructor parser for simple things
	static INTERFACE XmlDocument *quickParse(const char *string);
	static INTERFACE XmlDocument *quickParse(const char *buffer, int len);

	//////////////////////////////////////////////////////////////////////
	//
	// control
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE XmlDocument *parse(const char *buffer, int length = 0);
	INTERFACE XmlDocument *parseFile(const char *name);

    INTERFACE int getErrorCode();
    INTERFACE const char* getError();

	//////////////////////////////////////////////////////////////////////
	//
	// XmlEventHandler implementations
	//
	//////////////////////////////////////////////////////////////////////

	void openDoctype(class XmlMiniParser *p, 
							 char *name, 
							 char *pubid,
							 char *sysid);
	
	void closeDoctype(class XmlMiniParser *p);
	
	void openStartTag(class XmlMiniParser *p, 
							  char *name);
	
	void attribute(class XmlMiniParser *p, 
				   char *name, 
				   char *value);
	
	void closeStartTag(class XmlMiniParser *p, int empty);
	void endTag(class XmlMiniParser *p, char *name);
	void comment(class XmlMiniParser *p, char *text);
	void pi(class XmlMiniParser *p, char *text);
	void pcdata(class XmlMiniParser *p, char *text);
	void entref(class XmlMiniParser *p, char *name);
	void cdata(class XmlMiniParser *p, char *text);
	
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

	// the underlying XML Parser
	class XmlMiniParser	*mParser;

	// the root document we're building
	XmlDocument			*mDocument;

	// stack with extra information
	XomStack			*mStack;

	// the parent node on the stack
	XmlNode				*mNode;

	//////////////////////////////////////////////////////////////////////
	//
	// methods
	//
	//////////////////////////////////////////////////////////////////////

	void pushStack(XmlNode *node);
	void popStack(void);


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
