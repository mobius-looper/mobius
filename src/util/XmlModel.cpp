/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A simple memory model for parsed XML documents.
 * Conceptually similar to DOM but meets my simplicity standards.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
 
#include "Trace.h"
#include "Vbuf.h"
#include "Util.h"

#include "XmlModel.h"

/****************************************************************************
 *                                                                          *
 *   							 XML PROPERTY                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * ~XmlProperty
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destructor for a property object.
 * We free all properties on the list.
 ****************************************************************************/

INTERFACE XmlProperty::~XmlProperty(void)
{
	XmlProperty *n, *next;

	delete mName;
	delete mValue;

	// remainder of the list
	for (n = mNext, next = NULL ; n != NULL ; n = next) {
		next = n->getNext();
		n->setNext(NULL);			// stop recursion
		delete n;
	}
}

/****************************************************************************
 *                                                                          *
 *   							   XML NODE                                 *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * ~XmlNode
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Destructor for a node object
 * Note that we free all the right siblings too, so you have to NULL
 * out that pointer if you're only freeing this object.
 * Children are freed too.
 ****************************************************************************/

INTERFACE XmlNode::~XmlNode(void)
{
	XmlNode *n, *next;

	delete mProperties;
	delete mChildren;

	// remainder of the list
	for (n = mNext, next = NULL ; n != NULL ; n = next) {
		next = n->getNext();
		n->setNext(NULL);		// stop recursion
		delete n;
	}
}

/****************************************************************************
 * XmlNode::getPropertyObject
 *
 * Arguments:
 *	name: property name
 *
 * Returns: property value
 *
 * Description: 
 * 
 * Searches the property list for the named property and returns
 * the property object, or NULL if the property wasn't found.
 * 
 * This isn't a public method, though I suppose it wouldn't hurt.
 * You generally want the one that returns the value as a string.
 ****************************************************************************/

INTERFACE XmlProperty *XmlNode::getPropertyObject(const char *name)
{
	XmlProperty *found, *p;

	found = NULL;
	for (p = mProperties ; p && !found ; p = p->getNext()) {
		if (!strcmp(p->getName(), name))
		  found = p;
	}

	return found;
}

/****************************************************************************
 * XmlNode::getProperty
 *
 * Arguments:
 *	name: property name
 *
 * Returns: property value
 *
 * Description: 
 * 
 * Searches the property list for the named property and returns
 * its value, or NULL if the property wasn't found.
 ****************************************************************************/

INTERFACE const char *XmlNode::getProperty(const char *name)
{
	XmlProperty *p = getPropertyObject(name);
	return (p) ? p->getValue() : NULL;
}

/****************************************************************************
 * XmlNode::setProperty
 *
 * Arguments:
 *	 name: property name
 *	value: property value
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a new property to the object
 * If a property with this name already exists, the value will be
 * replaced.  If the value is NULL, then the property will be removed
 * if it exists.
 ****************************************************************************/

INTERFACE void XmlNode::setProperty(const char *name, const char *value)
{
	XmlProperty *p, *prev;

	prev = NULL;
	for (p = mProperties ; p != NULL && strcmp(p->getName(), name) ; 
		 p = p->getNext())
	  prev = p;

	if (p != NULL) {
		if (value != NULL) {
			// just modify the value
			char *vcopy = new char[strlen(value) + 1];	
			strcpy(vcopy, value);
			p->setValue(vcopy);
		}
		else {
			// remove this object
			if (prev == NULL)
			  mProperties = p->getNext();
			else
			  prev->setNext(p->getNext());
			p->setNext(NULL);
			delete p;
		}
	}
	else if (value != NULL) {
		// create a new property
		char *ncopy, *vcopy;

		p = new XmlProperty;
		ncopy = new char[strlen(name) + 1];
		strcpy(ncopy, name);
		vcopy = new char[strlen(value) + 1];
		strcpy(vcopy, value);

		p->setName(ncopy);
		p->setValue(vcopy);
		p->setNext(mProperties);
		mProperties = p;
	}
	else {
		// assigning null to a property that does't exist
		// ignore
	}
}

/****************************************************************************
 * XmlNode::setChildren
 *
 * Arguments:
 *	c: child list to set
 *
 * Returns: none
 *
 * Description: 
 * 
 * Methods related to maintenance of the child list.
 ****************************************************************************/

INTERFACE void XmlNode::setChildren(XmlNode *c) 
{
	XmlNode *n;

	mChildren = c;
	mLastChild = c;

	for (n = mChildren ; n != NULL ; n = n->getNext()) {
		n->setParent(this);
		mLastChild = n;
	}

}

// assumes mLastChild is being maintained!

INTERFACE void XmlNode::addChild(XmlNode *c) 
{
	if (c != NULL) {
		c->setParent(this);
		if (mLastChild == NULL)
		  mChildren = c;
		else
		  mLastChild->setNext(c);
		mLastChild = c;
	}
}

// not very effecient

INTERFACE void XmlNode::deleteChild(XmlNode *c)
{
	XmlNode *n, *prev;

	for (n = mChildren ; n && n != c ; n = n->getNext())
	  prev = n;

	if (n) {
		if (prev)
		  prev->setNext(n->getNext());
		else
		  mChildren = n->getNext();

		n->setNext(NULL);
		delete n;

		if (mLastChild == n)
		  mLastChild = prev;
	}
}

INTERFACE XmlNode *XmlNode::stealChildren(void) 
{
	XmlNode *ret = mChildren;
	mChildren = NULL;
	mLastChild = NULL;
	return ret;
}

/****************************************************************************
 * XmlNode::dump
 *
 * Arguments:
 *	level: optional indent level
 *
 * Returns: none
 *
 * Description: 
 * 
 * Debugging function to dump an XmlNode structure to stdout.  
 * Would be prettier to use polymorphism here, but its a hack so keep
 * it all in one place.
 ****************************************************************************/

#define INDENT(level) {int i; for (i = 0 ; i < level ; i++) printf(" "); }

#define SAFESTR(str) (str) ? str : ""

INTERFACE void XmlNode::dump(int level)
{
	switch (mClass) {

		case XML_DOCUMENT: {
			XmlDocument *doc = (XmlDocument *)this;
			XmlNode *p = doc->getPreamble();
			
			INDENT(level);
			printf("DOCUMENT\n");

			// preamble
			if (p != NULL) {
				INDENT(level);
				printf("Preamble:\n");	
				for ( ; p != NULL ; p = p->getNext())
				  p->dump(level + 2);
			}

			// doctype
			if (doc->getDoctype())
			  doc->getDoctype()->dump(level + 2);
		}
		break;

		case XML_DOCTYPE: {
			XmlDoctype *dt = (XmlDoctype *)this;
			INDENT(level);
			printf("DOCTYPE %s \"%s\" \"%s\"\n",
				   SAFESTR(dt->getName()),
				   SAFESTR(dt->getPubid()),
				   SAFESTR(dt->getSysid()));
		}
		break;

		case XML_ELEMENT: {
			XmlElement *el = (XmlElement *)this;
			XmlAttribute *att = el->getAttributes();
			INDENT(level);
			printf("ELEMENT %s %s\n", SAFESTR(el->getName()),
				   (el->isEmpty()) ? "(empty)" : "");
			for (att = el->getAttributes() ; att != NULL ; 
				 att = att->getNext()) {
				INDENT(level + 2);
				printf("ATTRIBUTE %s = %s\n", 
					   SAFESTR(att->getName()),
					   SAFESTR(att->getValue()));
			}
				   
		}
		break;

		case XML_PI: {
			XmlPi *node = (XmlPi *)this;
			INDENT(level);
			printf("PI \"%s\"\n", SAFESTR(node->getText()));
		}
		break;
		
		case XML_COMMENT: {
			XmlComment *node = (XmlComment *)this;
			INDENT(level);
			printf("COMMENT \"%s\"\n", SAFESTR(node->getText()));
		}
		break;

		case XML_MSECT: {
			XmlMsect *node = (XmlMsect *)this;
			INDENT(level);
			printf("MSECT \"%s\"\n", SAFESTR(node->getText()));
		}
		break;
		
		case XML_PCDATA: {
			XmlPcdata *node = (XmlPcdata *)this;
			INDENT(level);
			printf("PCDATA \"%s\"\n", SAFESTR(node->getText()));
		}
		break;

		default:
			printf("Unknown node class %d\n", (int)mClass);
			break;
	}
	
	// recurse on children
	if (mChildren != NULL)
	  mChildren->dump(level + 2);

	// do our siblings
	if (mNext != NULL)
	  mNext->dump(level);
}

/****************************************************************************
 * XmlNode::findElement
 *
 * Arguments:
 *	name: name of element
 *
 * Returns: element object or NULL if not found
 *
 * Description: 
 * 
 * Convenience methods to find the first ocurrence of the element with
 * the given name under the given node.  The node itself is considered
 * in the search, might not want that?
 * 
 * Might want to support some sort of rudimentary path expression here.
 * Searches depth-first-left-to-right.  
 * 
 ****************************************************************************/

INTERFACE XmlElement *XmlNode::findElement(const char *name)
{
	XmlElement *e = isElement();

	if (e) {
		if (strcmp(e->getName(), name)) {
			// not the one
			e = NULL;
		}
	}
	
	if (!e) {
		// recurse on children
		XmlNode *c;
		for (c = getChildren() ; c != NULL && !e ; c = c->getNext())
		  e = c->findElement(name);
	}

	return e;
}

// another finder that takes an attribute/value pair

INTERFACE XmlElement *XmlNode::findElement(const char *elname, 
										   const char *attname,
										   const char *attval)
{
	XmlElement *found, *e;

	found = NULL;
	e = isElement();

	if (e && !strcmp(e->getName(), elname)) {
		const char *v = e->getAttribute(attname);
		if (v && !strcmp(v, attval))
		  found = e;
	}
	
	if (!found) {
		// recurse on children
		XmlNode *c;
		for (c = getChildren() ; c != NULL && !found ; c = c->getNext())
		  found = c->findElement(elname, attname, attval);
	}

	return found;
}

/****************************************************************************
 * XmlNode::getElementContent
 *
 * Arguments:
 *
 * Returns: element content or NULL if none
 *
 * Description: 
 * 
 * Convenience method to find the first ocurrence of the element with
 * the given name and return the first piece of PCDATA that it contains.
 * Useful for parsing XML data structures.
 ****************************************************************************/

INTERFACE const char *XmlNode::getElementContent(const char *name)
{
	XmlElement *el;
	const char *content;

	content = NULL;
	el = findElement(name);
	if (el != NULL) {
		XmlNode *n = el->getChildren();
		if (n != NULL) {
			XmlPcdata *p = n->isPcdata();
			if (p != NULL) {
				content = p->getText();
			}
		}
	}
	
	return content;
}

/****************************************************************************
 * XmlNode::getChildElement
 *
 * Arguments:
 *	none: 
 *
 * Returns: first element child
 *
 * Description: 
 * 
 * Returns the first child of this element which is also an element.
 * This is often used to get the "document element" which is always the first, 
 * and supposedly only, element after the <!DOCTYPE...> statement.  
 * This isn't simply doc->getChildren() since comments, PI's and marked
 * sections can preceed the element.
 *
 * Defined on XmlNode, since a similar operation is convenient to have
 * on all nodes, not just the document root.
 * 
 * I think technically we would have to handle top level marked sections
 * that contain the document element.  That isn't being done now.
 ****************************************************************************/

INTERFACE XmlElement *XmlNode::getChildElement(void)
{
	XmlNode *node;
	XmlElement *el;

	el = NULL;
	for (node = getChildren() ; node && !el ; node = node->getNext())
	  el = node->isElement();

	return el;
}

/****************************************************************************
 * XmlNode::getNextElement
 *
 * Arguments:
 *
 * Returns: next element
 *
 * Description: 
 * 
 * Returns the "next" element in the document after "this" one.
 * This just looks at siblings, we're not doing any fancy iteration
 * here.  Should define some real iterators for this.
 ****************************************************************************/

INTERFACE XmlElement *XmlNode::getNextElement(void) 
{
	XmlNode *n;
	XmlElement *el;

	el = NULL;
	for (n = mNext ; n && !el ; n = n->getNext())
	  el = n->isElement();

	return el;
}

/****************************************************************************
 *                                                                          *
 *   							XML ATTRIBUTE                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * ~XmlAttribute
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destructor for an attribute object.
 * We free all attributes on the list.
 ****************************************************************************/

INTERFACE XmlAttribute::~XmlAttribute(void)
{
	XmlAttribute *n, *next;

	delete mName;
	delete mValue;

	// remainder of the list
	for (n = mNext, next = NULL ; n != NULL ; n = next) {
		next = n->getNext();
		n->setNext(NULL);			// stop recursion
		delete n;
	}
}

/****************************************************************************
 *                                                                          *
 *   							 XML ELEMENT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Return true if the element has a given tag name.
 */
INTERFACE bool XmlElement::isName(const char *name)
{
	return (name != NULL && mName != NULL) ? !strcmp(name, mName) : false;
}

/****************************************************************************
 * XmlElement::getAttribute
 *
 * Arguments:
 *   name: attribute name
 *
 * Returns: attribute value
 *
 * Description: 
 * 
 * Returns the value of an XML attribute on an element.
 * 
 ****************************************************************************/

INTERFACE XmlAttribute *XmlElement::getAttributeObject(const char *name)
{
	XmlAttribute *att, *found;

	found = NULL;
	for (att = mAttributes ; att && !found ; att = att->getNext()) {
		if (!strcmp(att->getName(), name))
		  found = att;
	}

	return found;
}

INTERFACE const char *XmlElement::getAttribute(const char *name)
{
	XmlAttribute *att = getAttributeObject(name);
	return (att) ? att->getValue() : NULL;
}

INTERFACE int XmlElement::getIntAttribute(const char *name, int dflt)
{
	int value = dflt;
	XmlAttribute *att = getAttributeObject(name);
	if (att != NULL) {
		const char* v = att->getValue();
		if (v != NULL)
		  value = atoi(v);
	}
	return value;
}

INTERFACE int XmlElement::getIntAttribute(const char *name)
{
	return getIntAttribute(name, 0);
}

INTERFACE bool XmlElement::getBoolAttribute(const char *name)
{
	bool value = false;
	XmlAttribute *att = getAttributeObject(name);
	if (att != NULL) {
		const char* v = att->getValue();
		value = (v != NULL && !strcmp(v, "true"));
	}
	return value;
}

/****************************************************************************
 * XmlElement::setAttribute
 *
 * Arguments:
 *   name: 
 *  value:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Assigns an attribute to an element.
 * If the attribute already exists, the value is replaced.
 * If the value is NULL the attribute is removed.  If you want
 * to have an "empty" attribute, you will have to assign it an
 * empty string, not a NULL pointer.
 * 
 * This is almost identical to XmlNode::setProperty, could consider
 * a template here, but its not that much.
 ****************************************************************************/

INTERFACE void XmlElement::setAttribute(const char *name, 
										const char *value)
{
	XmlAttribute *p, *prev;

	prev = NULL;
	for (p = mAttributes ; p != NULL && strcmp(p->getName(), name) ; 
		 p = p->getNext())
	  prev = p;

	if (p != NULL) {
		if (value != NULL) {
			// just modify the value
			char *vcopy = new char[strlen(value) + 1];	
			strcpy(vcopy, value);
			p->setValue(vcopy);
		}
		else {
			// remove this object
			if (prev == NULL)
			  mAttributes = p->getNext();
			else
			  prev->setNext(p->getNext());
			p->setNext(NULL);
			delete p;
		}
	}
	else if (value != NULL) {
		// create a new property
		char *ncopy, *vcopy;

		p = new XmlAttribute;
		ncopy = new char[strlen(name) + 1];
		strcpy(ncopy, name);
		vcopy = new char[strlen(value) + 1];
		strcpy(vcopy, value);

		p->setName(ncopy);
		p->setValue(vcopy);
		p->setNext(mAttributes);
		mAttributes = p;
	}
	else {
		// assigning null to a property that does't exist
		// ignore
	}
}

// convenience
INTERFACE void XmlElement::setAttributeInt(const char *name, int value)
{
	char buf[80];
	sprintf(buf, "%d", value);
	setAttribute(name, buf);
}

/****************************************************************************
 * XmlElement::getContent
 *
 * Arguments:
 *
 * Returns: content string
 *
 * Description: 
 * 
 * Convenience to return the first pcdata under this element.
 * Intended for use with elements that will always have just one
 * XmlPcdata object, though could be smarter and traverse.
 ****************************************************************************/

INTERFACE const char *XmlElement::getContent(void)
{
	const char *content = NULL;

	if (mChildren) {
		XmlPcdata *pcd = mChildren->isPcdata();
		if (pcd)
		  content = pcd->getText();
	}
	return content;
}

/****************************************************************************
 * XmlElement::findNextElement
 *
 * Arguments:
 *
 * Returns: content string
 *
 * Description: 
 * 
 * Hack to make it easy to write simple iterations over the a document.
 * This is a kludge, we only look at the siblings of this element, 
 * should define some simple iterators for this sort of thing.
 ****************************************************************************/

INTERFACE XmlElement *XmlElement::findNextElement(const char *name)
{
	XmlElement *found;
	XmlNode *sib;

	found = NULL;
	for (sib = mNext ; sib != NULL && !found ; sib = sib->getNext())
	  found = sib->findElement(name);

	return found;
}

/****************************************************************************
 *                                                                          *
 *   								 COPY                                   *
 *                                                                          *
 ****************************************************************************/
/* 
 * Copy could be implemented as an XmlVisitor, but its a little
 * more awkward, and is arguably somethign the nodes should
 * support natively.
 * 
 * Note that we do NOT copy properties or attachments, though that
 * could be added.
 *
 */

INTERFACE XmlNode *XmlDocument::copy(void)
{
	XmlDocument *copy;
	XmlNode *nodes, *last, *child;
	
	copy = new XmlDocument;
	
	// preamble, would be nice to have addPreamble...
	nodes = last = NULL;
	for (child = mPreamble ; child ; child = child->getNext()) {
		XmlNode *c = child->copy();
		if (last == NULL)
		  nodes = c;
		else
		  last->setNext(c);
		last = c;
	}
	copy->setPreamble(nodes);

	// doctype
	if (mDoctype)
	  copy->setDoctype(mDoctype->copy()->isDoctype());

	// children
	for (child = mChildren ; child ; child = child->getNext())
	  copy->addChild(child->copy());

	return copy;
}

INTERFACE XmlNode *XmlDoctype::copy(void)
{
	XmlDoctype *copy;
	XmlNode *child;

	copy = new XmlDoctype;

	copy->setName(CopyString(mName));
	copy->setPubid(CopyString(mPubid));
	copy->setSysid(CopyString(mSysid));

	// children
	for (child = mChildren ; child ; child = child->getNext())
	  copy->addChild(child->copy());

	return copy;
}

INTERFACE XmlNode *XmlElement::copy(void)
{
	XmlElement *copy;
	XmlAttribute *a;
	XmlNode *c;

	copy = new XmlElement;

	copy->setName(CopyString(mName));
	copy->setEmpty(mEmpty);

	for (a = mAttributes ; a ; a = a->getNext())
	  copy->addAttribute(a->copy());

	for (c = mChildren ; c ; c = c->getNext())
	  copy->addChild(c->copy());
	
	return copy;
}

INTERFACE XmlAttribute *XmlAttribute::copy(void)
{
	XmlAttribute *copy;

	copy = new XmlAttribute;
	copy->setName(CopyString(mName));
	copy->setValue(CopyString(mValue));

	return copy;
}

INTERFACE XmlNode *XmlPi::copy(void)
{
	XmlPi *copy = new XmlPi;

	copy->setText(CopyString(mText));

	return copy;
}

INTERFACE XmlNode *XmlComment::copy(void)
{
	XmlComment *copy = new XmlComment;

	copy->setText(CopyString(mText));

	return copy;
}

INTERFACE XmlNode *XmlMsect::copy(void)
{
	XmlMsect *copy = new XmlMsect;
	XmlNode *c;

	copy->setType(mType);
	copy->setEntity(CopyString(mEntity));
	copy->setText(CopyString(mText));

	for (c = mChildren ; c ; c = c->getNext())
	  copy->addChild(c->copy());

	return copy;
}

INTERFACE XmlNode *XmlPcdata::copy(void)
{
	XmlPcdata *copy = new XmlPcdata;

	copy->setText(CopyString(mText));

	return copy;
}

INTERFACE XmlNode *XmlEntref::copy(void)
{
	XmlEntref *copy = new XmlEntref;

	copy->setName(CopyString(mName));
	copy->setParameter(mParameter);

	return copy;
}

INTERFACE XmlProperty *XmlProperty::copy(void)
{
	XmlProperty *copy = new XmlProperty;

	copy->setName(CopyString(mName));
	copy->setValue(CopyString(mValue));

	return copy;
}

/****************************************************************************
 *                                                                          *
 *   							   VISITORS                                 *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlFoo::visit
 *
 * Description: 
 * 
 * Implementations of the various visitor acceptance methods.
 * Defined in here rather than inline to avoid ugly declaration dependency
 * problems in the .h file.
 ****************************************************************************/

INTERFACE void XmlDocument::visit(XmlVisitor *v)
{
	v->visitDocument(this);
}

INTERFACE void XmlDoctype::visit(XmlVisitor *v)
{
	v->visitDoctype(this);
}

INTERFACE void XmlElement::visit(XmlVisitor *v)
{
	v->visitElement(this);
}

INTERFACE void XmlPi::visit(XmlVisitor *v)
{
	v->visitPi(this);
}

INTERFACE void XmlComment::visit(XmlVisitor *v)
{
	v->visitComment(this);
}

INTERFACE void XmlMsect::visit(XmlVisitor *v)
{
	v->visitMsect(this);
}

INTERFACE void XmlPcdata::visit(XmlVisitor *v)
{
	v->visitPcdata(this);
}

INTERFACE void XmlEntref::visit(XmlVisitor *v)
{
	v->visitEntref(this);
}

INTERFACE void XmlAttribute::visit(XmlVisitor *v)
{
	v->visitAttribute(this);
}

INTERFACE void XmlProperty::visit(XmlVisitor *v)
{
	v->visitProperty(this);
}

/****************************************************************************
 *                                                                          *
 *   							SERIALIZATION                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlWriter::XmlWriter
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Constructor/destructor for the XmlWriter.
 ****************************************************************************/

INTERFACE XmlWriter::XmlWriter(void)
{
	mBuf = NULL;
}

INTERFACE XmlWriter::~XmlWriter(void)
{
	mBuf->free();	// these can be pooled
}

/****************************************************************************
 * XmlWriter::exec
 *
 * Arguments:
 *	node: node to traverse
 *
 * Returns: serialized XML string
 *
 * Description: 
 * 
 * Traverses the given node, generating the corresponding XML text.
 ****************************************************************************/

INTERFACE char *XmlWriter::exec(XmlNode *node)
{
	// allocate an output buffer
	if (mBuf == NULL)
	  mBuf = Vbuf::create();
	mBuf->clear();

	indent();

	// traverse
	node->visit(this);

	// return the xml
	return mBuf->copyString();
}

/****************************************************************************
 * XmlWriter::visitFoo
 *
 * Arguments:
 *	obj: object to ponder
 *
 * Returns: none
 *
 * Description: 
 * 
 * A set of visitor methods for the XmlWriter.
 * We do the traversal as part of the visitation.
 * Be careful with newlines, they are normally part of the surrounding
 * pcdata, but when outside of elements, we need to add them for
 * readability.
 ****************************************************************************/

INTERFACE void XmlWriter::visitDocument(XmlDocument *obj)
{
	XmlNode *n;

	// preamble, normally the XML header PI, and maybe comments
	for (n = obj->getPreamble() ; n != NULL ; n = n->getNext())
	  n->visit(this);

	// doctype
	if (obj->getDoctype())
	  obj->getDoctype()->visit(this);

	// children
	for (n = obj->getChildren() ; n != NULL ; n = n->getNext())
	  n->visit(this);
}

INTERFACE void XmlWriter::visitDoctype(XmlDoctype *obj)
{
	mBuf->add("<!DOCTYPE ");
	mBuf->add(obj->getName());

	if (obj->getPubid()) {

		mBuf->add(" PUBLIC \"");
		mBuf->add(obj->getPubid());
		mBuf->add("\"");
		
		// Valid XML documents are supposed to have a sysid too.
		// If don't have one, we'll put an empty string in its place
		// which is valid.
		
		mBuf->add(" \"");
		mBuf->add(obj->getSysid());
		mBuf->add("\"");
	}
	else if (obj->getSysid()) {

		mBuf->add(" SYSTEM \"");
		mBuf->add(obj->getSysid());
		mBuf->add("\"");
	}

	if (obj->getChildren()) {
		// an internal subset
		XmlNode *n;
		mBuf->add(" [\n");
		for (n = obj->getChildren() ; n != NULL ; n = n->getNext())
		  n->visit(this);
		mBuf->add("]>\n");
	}
	else
	  mBuf->add(">\n");
}

INTERFACE void XmlWriter::visitElement(XmlElement *obj)
{
	XmlAttribute *a;

	mBuf->add("<");
	mBuf->add(obj->getName());
	
	for (a = obj->getAttributes() ; a != NULL ; a = a->getNext()) {
		mBuf->add(" ");
		a->visit(this);
	}

	if (obj->isEmpty())
	  mBuf->add("/>");
	else {
		XmlNode *n;
		mBuf->add(">");

		for (n = obj->getChildren() ; n != NULL ; n = n->getNext())
		  n->visit(this);

		mBuf->add("</");
		mBuf->add(obj->getName());

		// note that we do not put newlines after end tags, 
		// when inside the document, we assume that the pcdta
		// carries the original newlines
		mBuf->add(">");
	}
}

INTERFACE void XmlWriter::visitAttribute(XmlAttribute *obj)
{
	const char *value, *ptr;
	char delim;
	int singles, doubles;

	// assume we're using single quote delimiters
	doubles = 0;
	singles = 0;
	delim   = '\'';
	value	= obj->getValue();
	
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

	mBuf->add(obj->getName());
	mBuf->add("=");
	mBuf->addChar(delim);

	// add the value, converting embedded quotes if necessary
	for (ptr = value ; ptr && *ptr ; ptr++) {
		if (*ptr != delim)
		  mBuf->addChar(*ptr);
		else if (delim == '\'')
		  mBuf->add("&#39;");	     // convert '
		else
		  mBuf->add("&#34;");	     // convert "
	}

	mBuf->addChar(delim);
}

INTERFACE void XmlWriter::visitPi(XmlPi *obj)
{
	mBuf->add("<?");
	mBuf->add(obj->getText());
	mBuf->add("?>");

	// if we're not in element content, add a newline
	if (obj->getParent() && !obj->getParent()->isElement())
	  mBuf->add("\n");
}

INTERFACE void XmlWriter::visitComment(XmlComment *obj)
{
	mBuf->add("<!--");
	mBuf->add(obj->getText());
	mBuf->add("-->");

	// if we're not in element content, add a newline
	if (obj->getParent() && !obj->getParent()->isElement())
	  mBuf->add("\n");
}

INTERFACE void XmlWriter::visitMsect(XmlMsect *obj)
{
	XmlNode *n;

	mBuf->add("<![");
	
	if (obj->getEntity()) {
		// defined by parameter entity reference
		mBuf->add("%");
		mBuf->add(obj->getEntity());
		mBuf->add(";");
	}
	else {
		switch(obj->getType()) {
			case XmlMsect::MS_IGNORE: 
				mBuf->add("IGNORE");
				break;
			case XmlMsect::MS_INCLUDE:
				mBuf->add("INCLUDE");
				break;
			case XmlMsect::MS_CDATA:
				mBuf->add("CDATA");
				break;
		}
	}

	mBuf->add("[");

	for (n = obj->getChildren() ; n != NULL ; n = n->getNext())
	  n->visit(this);
	
	mBuf->add("]]>");

	// if we're not in element content, add a newline
	if (obj->getParent() && !obj->getParent()->isElement())
	  mBuf->add("\n");
}

INTERFACE void XmlWriter::visitPcdata(XmlPcdata *obj)
{
	mBuf->add(obj->getText());
}

INTERFACE void XmlWriter::visitEntref(XmlEntref *obj)
{
	if (obj->isParameter())
	  mBuf->add("%");
	else
	  mBuf->add("&");

	mBuf->add(obj->getName());
	mBuf->add(";");

	// if we're not in element content, add a newline
	if (obj->getParent() && !obj->getParent()->isElement())
	  mBuf->add("\n");
}

INTERFACE void XmlWriter::visitProperty(XmlProperty *obj)
{
	// these don't have XML renderings
}

PRIVATE void XmlWriter::indent(void)
{
	int i;
	for (i = 0 ; i < mIndent ; i++)
	  mBuf->add(" ");
}

/****************************************************************************
 * XmlNode::serialize
 *
 * Arguments:
 *
 * Returns: XML text for the node
 *
 * Description: 
 * 
 * A convenience method on nodes that returns their XML text representation.
 * This sort of violates the visitor separation, but its a pretty 
 * obvious operation to want on all nodes.
 ****************************************************************************/

INTERFACE char *XmlNode::serialize(int indent)
{
	XmlWriter *w;
	char *xml;

	w = new XmlWriter;
	w->setIndent(indent);
	
	xml = w->exec(this);
	delete w;

	return xml;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
