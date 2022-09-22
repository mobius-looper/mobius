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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
 
#include "Util.h"
#include "Trace.h"
#include "Vbuf.h"
#include "XmlModel.h"
#include "XomParser.h"

/****************************************************************************
 *                                                                          *
 *   						  XML PARSER EVENTS                             *
 *                                                                          *
 ****************************************************************************/
/*
 * These are callback methods called by the XmlMiniParser as it
 * finds things.
 *
 */

/****************************************************************************
 * XomParser::pushStack
 *
 * Arguments:
 *	n: node to push
 *
 * Returns: none
 *
 * Description: 
 * 
 * Pushes a node on the parse stack.
 * We build an XomStack object to capture the file position, and also
 * leave the node in the mNode variable.
 ****************************************************************************/

PRIVATE void XomParser::pushStack(XmlNode *n)
{
	mStack = mStack->push(n, mParser->getLine(), mParser->getColumn());
	mNode = n;
}

/****************************************************************************
 * XomParser::popStack
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Pops the node stack. 
 * Sets mNode as a side effect.
 ****************************************************************************/

PRIVATE void XomParser::popStack(void)
{
	mStack = mStack->pop();
	mNode = (mStack) ? mStack->getNode() : NULL;
}

/****************************************************************************
 * XomParser::openDoctype
 *
 * Arguments:
 *	    p: parser
 *	 name: doctype name
 *	pubid: public identifier
 *	sysid: system identifier
 *
 * Returns: none
 *
 * Description: 
 * 
 * We've just started parsing the DOCTYPE statment.  If the XmlDocument
 * at the top of the stack has any children, then these represent
 * objects in the "preamble" above the doctype statement (not sure
 * if there's an official name for this region.  Normally this consists
 * of only processing instructions like <?XML...?>.
 *
 ****************************************************************************/

void XomParser::openDoctype(XmlMiniParser *p, 
							char *name, 
							char *pubid,
							char *sysid)
{
	XmlDoctype *dt;

	// If the document has any children at this point, 
	// then they become preamble objects.

	mDocument->setPreamble(mDocument->stealChildren());

	// create a new doctype object
	dt = new XmlDoctype;
	dt->setName(name);	
	dt->setPubid(pubid);
	dt->setSysid(sysid);
	mDocument->setDoctype(dt);

	// this becomes the parent node for any children
	// in the internal subset

	pushStack(dt);
}

/****************************************************************************
 * XomParser::closeDoctype
 *
 * Arguments:
 *	p: parser
 *
 * Returns: none
 *
 * Description: 
 * 
 * Done parsing the doctype statement, put the document root back 
 * on the top of the stack.
 ****************************************************************************/

void XomParser::closeDoctype(class XmlMiniParser *p)
{
	// restore the document as the parent
	popStack();
}
	
/****************************************************************************
 * XomParser::openStartTag
 *
 * Arguments:
 *	   p: parser
 *	name: element name
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an element start tag up through the element name.
 * Create a new XmlElement object and push it on the stack.
 ****************************************************************************/

void XomParser::openStartTag(XmlMiniParser *p, char *name)
{
	XmlElement *el;
	
	// create a new element
	el = new XmlElement;
	el->setName(name);

	// add it to the parent on top of the stack
	mNode->addChild(el);

	// push the node on the stack
	pushStack(el);
}

/****************************************************************************
 * XomParser::closeStartTag
 *
 * Arguments:
 *	    p: parser
 *	empty: non-zero if the /> empty element syntax was used
 *
 * Returns: none
 *
 * Description: 
 * 
 * We've just finished parsing the element start tag.  
 * If the element is empty, pop it from the stack.
 ****************************************************************************/

void XomParser::closeStartTag(XmlMiniParser *p, int empty) 
{
	XmlElement *el = mNode->isElement();

	// top of the stack must be an element, else its a syntax error
	if (el != NULL && empty) {
		el->setEmpty(empty);
		// pop node since we're empty
		popStack();
	}
}

/****************************************************************************
 * XomParser::endTag
 *
 * Arguments:
 *	   p: parser
 *	name: element name
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an element end tag, remove it from the stack.
 ****************************************************************************/

void XomParser::endTag(class XmlMiniParser *p, char *name)
{
	// Top of stack better be an element with the same name.
	// Mini parser doesn't catch this, since it isn't maintaining
    // a stack.

    XmlElement *pel = mNode->isElement();
	if (!pel || strcmp(pel->getName(), name)) {

		char buf[1024];

		if (!pel)
		  sprintf(buf, "Unexpected end tag %s at line %d column %d, "
				  "expecting none.\n",
				  name, mParser->getLine() + 1, mParser->getColumn() + 1);
		else
		  sprintf(buf, "Unexpected end tag %s at line %d column %d.\n"
				  "Expecting %s started at line %d column %d.\n",
				  name, 
				  mParser->getLine() + 1, mParser->getColumn() + 1,
				  pel->getName(),
				  mStack->getLine() + 1, mStack->getColumn() + 1);

		delete name;

        // Trace needs a static format string, you can't pass buf
        // just print the damn thing...
        //Trace(1, buf);
        printf("%s\n", buf);
        fflush(stdout);
		throw new AppException(ERR_XOM_UNBALANCED_TAGS, buf, false);
	}
	else {
		// pop stack
		popStack();

		// don't need this, might want an option to avoid allocating it
		delete name;
	}
}

/****************************************************************************
 * XomParser::attribute
 *
 * Arguments:
 *	    p: parser
 *	 name: XML attribute name
 *	value: attribute value
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an XML attribute within the start tag of an element.
 ****************************************************************************/

void XomParser::attribute(XmlMiniParser *p, char *name, char *value)
{
	XmlElement *el = mNode->isElement();

	if (el == NULL) {
		// top of stack wasn't an element, shouldn't happen
		delete name;
		delete value;
	}
	else {
		XmlAttribute *att = new XmlAttribute;
		att->setName(name);
		att->setValue(value);
		el->addAttribute(att);
	}
}

/****************************************************************************
 * XomParser::comment
 *
 * Arguments:
 *	   p: parser
 *	text: comment text
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an XML comment: <!-- ... -->
 * The text does not include the surrounding markup.
 ****************************************************************************/

void XomParser::comment(XmlMiniParser *p, char *text)
{
	XmlComment *com;

	com = new XmlComment;
	com->setText(text);

	// add as a child of the stack top
	mNode->addChild(com);

	// these aren't parent nodes, so don't push the stack
}

/****************************************************************************
 * XomParser::pi
 *
 * Arguments:
 *	   p: parser
 *	text: PI text
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an XML processing instruction: <?...?>
 * The text does not include the surrounding markup.
 ****************************************************************************/

void XomParser::pi(XmlMiniParser *p, char *text)
{
	XmlPi *node;

	node = new XmlPi;
	node->setText(text);
	mNode->addChild(node);
	// these aren't parent nodes, so don't push the stack
}

/****************************************************************************
 * XomParser::pcdata
 *
 * Arguments:
 *	   p: parser
 *	text: pcdata
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed a piece of pcdata content.  Should try to be smart and
 * merge adajdacent pcdata nodes should they arise, XmlMiniParser shouldn't
 * let that happen.
 ****************************************************************************/

void XomParser::pcdata(XmlMiniParser *p, char *text)
{
	// hack, ignore any pcdata that comes in while
	// we're processing the preamble objects before the DOCTYPE
	// if we've already got children, then this must have been
	// a fragment without a DOCTYPE, not really valid, but we allow it

	if (mDocument->getDoctype() == NULL && 
		mDocument->getChildren() == NULL) {
		delete text;
	}
	else {
		XmlPcdata *node;
		node = new XmlPcdata;
		node->setText(text);
		mNode->addChild(node);
		// these aren't parent nodes, so don't push the stack
	}
}

/****************************************************************************
 * XomParser::entref
 *
 * Arguments:
 *	   p: parser
 *	name: entity name
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed an entity reference: &foo;
 * The name does not include the & or ;
 * We might try to resolve these to <!ENTITY definitions in the
 * doctype, but since we're not representing those yet, we just save
 * the name.
 * 
 ****************************************************************************/

void XomParser::entref(XmlMiniParser *p, char *name)
{
	XmlEntref *node;

	node = new XmlEntref;
	node->setName(name);
	mNode->addChild(node);
	// these aren't parent nodes, so don't push the stack
}

/****************************************************************************
 * XomParser::cdata
 *
 * Arguments:
 *	   p: parser
 *	text: marked section text
 *
 * Returns: none
 *
 * Description: 
 * 
 * Just parsed a marked section: <![CDATA[...]]>
 * 
 ****************************************************************************/

void XomParser::cdata(XmlMiniParser *p, char *text)
{
	XmlMsect *node;

	node = new XmlMsect;
	node->setText(text);
	mNode->addChild(node);
	// these aren't parent nodes, so don't push the stack
}

/****************************************************************************
 *                                                                          *
 *   						 XOM PARSER OPERATION                           *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XomParser::XomParser
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Constructor for an XomParser object.
 * After construction, call setBuffer() and parse().
 ****************************************************************************/

INTERFACE XomParser::XomParser(void)
{
	// need an XML mini parser
	mParser 	= new XmlMiniParser;

	// wire us as the event handler
	mParser->setEventHandler(this);

	// initialize parse state
	mDocument 	= NULL;
	mNode		= NULL;
	mStack		= NULL;
}

/****************************************************************************
 * ~XomParser
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destruct an XOM parser.
 * Gets rid of the XML parser, and cleans up any residual parse state if any.
 ****************************************************************************/

INTERFACE XomParser::~XomParser(void)
{
	delete mParser;
	delete mDocument;
	while (mStack)
	  popStack();
}

/****************************************************************************
 * XomParser::quickParse
 *
 * Arguments:
 *	xml: xml text
 *
 * Returns: parsed document
 *
 * Description: 
 * 
 * Convenient parsing interface for xml documents that are expected
 * to be well formed. This is a static class method that handles
 * creation of the parser, parsing, and destroying the parser.
 * 
 * Throws an exception if there was an error.
 ****************************************************************************/

INTERFACE XmlDocument *XomParser::quickParse(const char *xml)
{
	XomParser *parser = new XomParser;
	XmlDocument *doc = NULL;

	try {
		doc = parser->parse(xml);
		delete parser;
	}
	catch (...) {
		delete parser;
        Trace(1, "XomParser::quickParse exception thrown\n");
	}

	return doc;
}

INTERFACE XmlDocument *XomParser::quickParse(const char *buffer, int len)
{
	XomParser *parser = new XomParser;
	XmlDocument *doc = NULL;

	try {
		doc = parser->parse(buffer, len);
		delete parser;
	}
	catch (...) {
		delete parser;
        Trace(1, "XomParser::quickParse exception thrown\n");
	}

	return doc;
}

/****************************************************************************
 * parse
 *
 * Arguments:
 *	buffer: buffer to parse
 *
 * Returns: xml document
 *
 * Description: 
 * 
 * Parses a string expected to contain a valid well formed XML document,
 * and returns an XmlDocument object that represents that document.
 * Throws an exception on error.
 ****************************************************************************/

INTERFACE XmlDocument *XomParser::parse(const char *buffer, int length)
{
	XmlDocument *retval = NULL;

	// toss parse state (but leave the XML parser initialized)
	while (mStack)
	  popStack();
	delete mDocument;

	// initialize a new root document
	mDocument = new XmlDocument;
	pushStack(mDocument);

	// initialize the parser
	if (!length && buffer)
	  length = strlen(buffer);
	mParser->setBuffer(buffer, length);

	// let it go
	mParser->parse();
	
	// if we're here, then there were no errors, check for dangling elements
	if (mNode != mDocument) {
		XomStack *s;
		char buf[1024];

		for (s = mStack ; s != NULL ; s = s->getStack()) {
			XmlElement *el = s->getNode()->isElement();
			if (el != NULL) {
				sprintf(buf, "Element %s at line %d column %d was "
						"unterminated at end of file.\n",
						el->getName(), s->getLine() + 1, s->getColumn() + 1);
				break;
			}
		}

		// cleanup the stack
		while (mStack)
		  popStack();

        // Trace must have a static format string
        // just print the damn thing
        printf("%s\n", buf);
        fflush(stdout);

        // Formerly threw here but no one was expecting that
        // so just return null or what we were able to parse?
		// throw new AppException(ERR_XOM_DANGLING_TAGS, buf, false);
        delete mDocument;
        mDocument = NULL;
	}

	retval = mDocument;
	mDocument = NULL;
	return retval;
}

/**
 * Parse a file and return the model.
 * Returns NULL on error.
 */
INTERFACE XmlDocument *XomParser::parseFile(const char *name)
{
	XmlDocument *retval = NULL;

	// toss parse state (but leave the XML parser initialized)
	delete mDocument;
	while(mStack)
	  popStack();

	// initialize a new root document
	mDocument = new XmlDocument;
	pushStack(mDocument);

	// initialize the parser
	mParser->setFile(name);

	// let it go
	if (mParser->parse()) {
        // errors, return null
        delete mDocument;
        mDocument = NULL;
    }

	retval = mDocument;
	mDocument = NULL;
	return retval;
}

/**
 * Return the error code of the wrapped parser.
 */
INTERFACE int XomParser::getErrorCode()
{
    return mParser->getErrorCode();
}

INTERFACE const char* XomParser::getError()
{
    return mParser->getError();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
