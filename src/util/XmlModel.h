/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A very simple XML object model.  Used with the XML mini parser
 * for fast instantiation of XML streams into C++ objects.
 * 
 * This is conceptually similar to DOM, but is simpler and less functional,
 * which can be a good or bad thing depending on your point of view.  
 * 
 */

#ifndef XOM_H
#define XOM_H

#include "Port.h"

/****************************************************************************
 *                                                                          *
 *   							 XML OBJECTS                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * ERR_XOM
 *
 * Description: 
 * 
 * Xom parser error codes, extend those of XmlMiniParser.
 ****************************************************************************/

#define ERR_XOM_BASE 100

#define ERR_XOM_UNBALANCED_TAGS 	ERR_XOM_BASE + 0
#define ERR_XOM_DANGLING_TAGS 		ERR_XOM_BASE + 1

/****************************************************************************
 * XmlProperty
 *
 * Description: 
 * 
 * These are objects that can be hanging off any node in an XML tree. 
 * They allow for the attachment of arbitrary "properties" or "metadata" 
 * to nodes that aren't considered part of the XML source.  They are 
 * similar in behavior to XML attributes (represented by the 
 * XmlAttribute class) except that any node may have them, not just elements.
 * 
 * NOTE: After the addition of the "attachment" to nodes and attributes, 
 * the original use of properties is now gone.  Still, it might be useful
 * to keep around.
 *
 ****************************************************************************/

class XmlProperty {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty *getNext(void) {
		return mNext;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getValue(void) {
		return mValue;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty(void) {
		mNext		= NULL;
		mName		= NULL;
		mValue		= NULL;
	}

	INTERFACE XmlProperty *copy(void);

	INTERFACE ~XmlProperty(void);

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setValue(char *v) {
		delete mValue;
		mValue = v;
	}

	void setNext(XmlProperty *n) {
		mNext = n;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlProperty 	*mNext;
	char 			*mName;
	char 			*mValue;

};
	
/****************************************************************************
 * XmlNode
 *
 * Description: 
 * 
 * The base class for most XML objects.
 * Defines the basic tree node interface and implementation.
 *  
 * Unlike more complex Composites, we don't try to push implementations
 * of things like child maintenance down into the non-leaf subclasses.  
 * Could do that if necessary.
 * 
 * Note that deleting a node deletes all the right siblings as well.
 * 
 * All nodes have a class code so we can do type checks quickly.
 * Various isFoo methods provided for safe downcasting.
 * 
 * We're optimizing for construction speed, vs. memory usage.
 * For this reason, we keep a tail pointer on the child list so 
 * we can append quickly.  This is used only by the XomParser.
 * 
 * All nodes may have a list of XmlProperty objects that allows the 
 * attachment of user defined state to each node.
 * 
 * All nodes also have a void* "attachment" that can be used in controlled
 * conditions to associate user defined objects with nodes.  
 * 
 ****************************************************************************/

class XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	// 
	// class codes
	//
	//////////////////////////////////////////////////////////////////////

	typedef enum {

		XML_UNKNOWN,
		XML_DOCUMENT,
		XML_DOCTYPE,
		XML_ELEMENT,
		XML_PI,
		XML_COMMENT,
		XML_MSECT,
		XML_PCDATA,
		XML_ENTREF

	} XML_CLASS;

	//////////////////////////////////////////////////////////////////////
	// 
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XML_CLASS getClass(void) {
		return mClass;
	}
	
	int isCLass(XML_CLASS c) {
		return mClass == c;
	}

	XmlNode *getParent(void) {
		return mParent;
	}

	XmlNode *getChildren(void) {
		return mChildren;
	}

	XmlNode *getNext(void) {
		return mNext;
	}

	// getChildElement and getNextElement are convenience methods
	// that skip over non-element nodes like comments, pi's etc.

	INTERFACE class XmlElement *getChildElement(void);
	INTERFACE class XmlElement *getNextElement(void);

	// debug only
	INTERFACE void dump(int level = 0);

	void *getAttachment(void) {
		return mAttachment;
	}

	void setAttachment(void *a) {
		mAttachment = a;
	}

	//////////////////////////////////////////////////////////////////////
	// 
	// downcasting/typechecking 
	//
	//////////////////////////////////////////////////////////////////////
 
	virtual class XmlDocument *isDocument(void) {
		return NULL;
	}

	virtual class XmlDoctype *isDoctype(void) {
		return NULL;
	}

	virtual class XmlElement* isElement(void) {
		return NULL;
	}

	virtual class XmlPi *isPi(void) {
		return NULL;
	}
	
	virtual class XmlComment *isComment(void) {
		return NULL;
	}

	virtual class XmlMsect *isMsect(void) {
		return NULL;
	}

	virtual class XmlPcdata *isPcdata(void) {
		return NULL;
	}

	virtual class XmlEntref *isEntref(void) {
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////
	// 
	// properties
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty *getProperties(void) {
		return mProperties;
	}

	void setProperties(XmlProperty *props) {
		delete mProperties;
		mProperties = props;
	}

	INTERFACE const char *getProperty(const char *name);
	INTERFACE void setProperty(const char *name, const char *value);
	INTERFACE XmlProperty *getPropertyObject(const char *name);

	//////////////////////////////////////////////////////////////////////
	// 
	// convenience utilities
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE XmlElement *findElement(const char *name);
	INTERFACE XmlElement *findElement(const char *elname, const char *attname,
									  const char *attval);
	INTERFACE const char *getElementContent(const char *name);

	//////////////////////////////////////////////////////////////////////
	// 
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	// normally you don't create one of these, create a subclass

	XmlNode(void) {
		mParent 	= NULL;
		mChildren 	= NULL;
		mLastChild	= NULL;
		mNext 		= NULL;
		mProperties	= NULL;
		mAttachment	= NULL;
	}

	virtual INTERFACE ~XmlNode(void);

	// this performs a deep copy, I decided not to define this
	// as a copy constructor in case we wanted to use that for shallow copy
	virtual INTERFACE XmlNode *copy(void) = 0;

	void setParent(XmlNode *p) {
		mParent = p;
	}

	void setNext(XmlNode *n) {
		mNext = n;
	}

	INTERFACE void setChildren(XmlNode *c);
	INTERFACE void addChild(XmlNode *c);
	INTERFACE void deleteChild(XmlNode *c);
	INTERFACE XmlNode *stealChildren(void);

	// visitor interface

	virtual INTERFACE void visit(class XmlVisitor *v) = 0;

	// convenient interface to the xml writer visitor

	INTERFACE char *serialize(int indent = 0);

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XML_CLASS 	mClass;
	XmlNode 	*mNext;
	XmlNode 	*mParent;
	XmlNode 	*mChildren;
	XmlNode		*mLastChild;
	XmlProperty	*mProperties;
	void 		*mAttachment;

};

/****************************************************************************
 * XmlDoctype
 *
 * Description: 
 * 
 * Encapsultes the contents of a <!DOCTYPE statement.
 * These are XmlNode's since they can have children (the internal subset),
 * but they're not currently on the child list of the XmlDocument,
 * since its usually invonvenient to deal with them there.
 ****************************************************************************/

class XmlDoctype : public XmlNode 
{

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlDoctype *isDoctype(void) {
		return this;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getPubid(void) {
		return mPubid;
	}

	const char *getSysid(void) {
		return mSysid;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlDoctype(void) {
		mClass	= XML_DOCTYPE;
		mName	= NULL;	
		mPubid	= NULL;
		mSysid	= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlDoctype(void) {
		delete mName;
		delete mPubid;
		delete mSysid;
	}

	void setName(char *name) {
		delete mName;
		mName = name;
	}

	void setPubid(char *pubid) {
		delete mPubid;
		mPubid = pubid;
	}

	void setSysid(char *sysid) {
		delete mSysid;
		mSysid = sysid;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char *mName;
	char *mPubid;
	char *mSysid;

};

/****************************************************************************
 * XmlDocument
 *
 * Description: 
 * 
 * Class that forms the root of an XML document in memory.
 * We keep the preamble PI's and the DOCTYPE statement out of the child 
 * list since that's usually the what you want.  The preamble normally
 * consists of only processing instructions, but we let it be
 * an XmlNode list in case there are comments.
 * 
 ****************************************************************************/

class XmlDocument : public XmlNode 
{

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlDocument *isDocument(void) {
		return this;
	}

	XmlNode *getPreamble(void) {
		return mPreamble;
	}

	XmlDoctype *getDoctype(void) {
		return mDoctype;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlDocument(void) {
		mClass		= XML_DOCUMENT;
		mPreamble	= NULL;
		mDoctype	= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlDocument(void) {
		delete mPreamble;
		delete mDoctype;
	}

	void setPreamble(XmlNode *n) {

		delete mPreamble;
		mPreamble = n;

		for (n = mPreamble ; n != NULL ; n = n->getNext())
		  n->setParent(this);
	}

	void setDoctype(XmlDoctype *d) {
		delete mDoctype;
		mDoctype = d;
		if (d != NULL)
		  d->setParent(this);
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlNode			*mPreamble;
	XmlDoctype		*mDoctype;

};

/****************************************************************************
 * XmlAttribute
 *
 * Description: 
 * 
 * Class representing an element attribute.
 * Note that unlike DOM these aren't XmlNodes.  They could be, but
 * since they're not tree structured, its a bit of a waste of space.
 * I personally don't find treating attributes as nodes a useful thing.
 * 
 * Added an attachment pointer like XmlNode.  Should have properties
 * here too?
 * 
 ****************************************************************************/

class XmlAttribute {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XmlAttribute *getNext(void) {
		return mNext;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getValue(void) {
		return mValue;
	}

	void *getAttachment(void) {
		return mAttachment;
	}

	void setAttachment(void *a) {
		mAttachment = a;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlAttribute(void) {
		mNext		= NULL;
		mName		= NULL;
		mValue		= NULL;
		mAttachment	= 0;
	}

	INTERFACE ~XmlAttribute(void);

	INTERFACE XmlAttribute *copy(void);

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setValue(char *v) {
		delete mValue;
		mValue = v;
	}

	void setNext(XmlAttribute *n) {
		mNext = n;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlAttribute 	*mNext;
	char 			*mName;
	char 			*mValue;
	void			*mAttachment;

};
	
/****************************************************************************
 * XmlElement
 *
 * Description: 
 * 
 * Class representing an XML element.
 * Attributes are kept on a special list.
 ****************************************************************************/

class XmlElement : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlElement *isElement(void) {
		return this;
	}

	int isEmpty(void) {
		return mEmpty;
	}

	const char *getName(void) {
		return mName;
	}
	
	INTERFACE bool isName(const char *name);

	XmlAttribute *getAttributes(void) {
		return mAttributes;
	}

	INTERFACE const char *getAttribute(const char *name);
	INTERFACE int getIntAttribute(const char *name, int dflt);
	INTERFACE int getIntAttribute(const char *name);
	INTERFACE bool getBoolAttribute(const char *name);
	INTERFACE void setAttribute(const char *name, const char *value);
	INTERFACE void setAttributeInt(const char *name, int value);
	INTERFACE XmlAttribute *getAttributeObject(const char *name);

	INTERFACE const char *getContent(void);
	INTERFACE XmlElement *findNextElement(const char *name);

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlElement(void) {
		mClass			= XML_ELEMENT;
		mName			= NULL;
		mAttributes		= NULL;
		mLastAttribute	= NULL;
		mEmpty			= 0;
	}

	INTERFACE XmlNode *copy(void);

	~XmlElement(void) {
		delete mName;
		delete mAttributes;
	}

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setAttributes(XmlAttribute *a) {
		delete mAttributes;
		mAttributes = a;
		mLastAttribute = a;
	}

	void addAttribute(XmlAttribute *a) {
		if (a != NULL) {
			if (mAttributes == NULL)
			  mAttributes = a;
			else
		  mLastAttribute->setNext(a);
			mLastAttribute = a;
		}
	}

	void setEmpty(int e) {
		mEmpty = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mName;
	XmlAttribute	*mAttributes;
	XmlAttribute 	*mLastAttribute;
	int 			mEmpty;

};

/****************************************************************************
 * XmlPi
 *
 * Description: 
 * 
 * Class representing an XML processing instruction.
 ****************************************************************************/

class XmlPi : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlPi *isPi(void) {
		return this;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlPi(void) {
		mClass		= XML_PI;
		mText		= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlPi(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlComment
 *
 * Description: 
 * 
 * Class representing an XML comment.
 ****************************************************************************/

class XmlComment : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlComment *isComment(void) {
		return this;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlComment(void) {
		mClass		= XML_COMMENT;
		mText		= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlComment(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlMsect
 *
 * Description: 
 * 
 * Class representing an XML marked section.
 * Three types, CDATA, INCLUDE, IGNORE.
 ****************************************************************************/

class XmlMsect : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	typedef enum {

		MS_IGNORE,
		MS_INCLUDE,
		MS_CDATA

	} MSECT_TYPE;

	MSECT_TYPE getType(void) {
		return mType;
	}

	const char *getText(void) {
		return mText;
	}

	const char *getEntity(void) {
		return mEntity;
	}

	XmlMsect *isMsect(void) {
		return this;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////
	
	XmlMsect(void) {
		mClass		= XML_MSECT;
		mType		= MS_CDATA;
		mText		= NULL;
		mEntity		= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlMsect(void) {
		delete mText;
		delete mEntity;
	}

	void setType(MSECT_TYPE t) {
		mType = t;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	void setEntity(char *e) {
		delete mEntity;
		mEntity = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;
	char			*mEntity; 	// when using parameter keywords
	MSECT_TYPE		mType;

};

/****************************************************************************
 * XmlPcdata
 *
 * Description: 
 * 
 * Class representing a string of pcdata.  
 ****************************************************************************/

class XmlPcdata : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlPcdata *isPcdata(void) {
		return this;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlPcdata(void) {
		mClass		= XML_PCDATA;
		mText		= NULL;
	}

	INTERFACE XmlNode *copy(void);

	~XmlPcdata(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlEntref
 *
 * Description: 
 * 
 * Class representing an XML entity reference.
 * If had a representation for <!ENTITY statements in the doctype, 
 * we could try to resolve the name to an object.
 ****************************************************************************/

class XmlEntref : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getName(void) {
		return mName;
	}

	XmlEntref *isEntref(void) {
		return this;
	}

	int isParameter(void) {
		return mParameter;
	}

	INTERFACE void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlEntref(void) {
		mClass		= XML_ENTREF;
		mName		= NULL;
		mParameter	= 0;
	}

	INTERFACE XmlNode *copy(void);

	~XmlEntref(void) {
		delete mName;
	}

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setParameter(int p) {
		mParameter = p;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mName;
	int				mParameter;

};

/****************************************************************************
 *                                                                          *
 *   							  VISITORS                                  *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlVisitor
 *
 * Description: 
 * 
 * Base class of a visitor for XmlNode composites.
 * 
 ****************************************************************************/

class XmlVisitor {

  public:
	
	// XmlNode hierachy

	virtual INTERFACE void visitDocument(XmlDocument *obj){}
	virtual INTERFACE void visitDoctype(XmlDoctype *obj){}
	virtual INTERFACE void visitElement(XmlElement *obj){}
	virtual INTERFACE void visitPi(XmlPi *obj){}
	virtual INTERFACE void visitComment(XmlComment *obj){}
	virtual INTERFACE void visitMsect(XmlMsect *obj){}
	virtual INTERFACE void visitPcdata(XmlPcdata *obj){}
	virtual INTERFACE void visitEntref(XmlEntref *obj){}

	// Not part of the XmlNode hierarchy, but some iterators may
	// be able to visit them.

	virtual INTERFACE void visitAttribute(XmlAttribute *obj){}
	virtual INTERFACE void visitProperty(XmlProperty *obj){}


  protected:

	XmlVisitor(void){}
	virtual ~XmlVisitor(void){}

};

/****************************************************************************
 * XmlWriter
 *
 * Description: 
 * 
 * An XmlVisitor that renders the XmlNode tree as a string of XML text.
 * Iteration is done by the visitor itself right now, might want
 * an iterator someday.
 * 
 ****************************************************************************/

class XmlWriter : private XmlVisitor {

  public:

	INTERFACE XmlWriter(void);
	INTERFACE ~XmlWriter(void);

	INTERFACE char *exec(XmlNode *node);

	void setIndent(int i) {
		mIndent = i;
	}

  private:

	class Vbuf *mBuf;		// buffer we're accumulating
	int mIndent;

	//
	// visitor implementations
	//

	INTERFACE void visitDocument(XmlDocument *obj);
	INTERFACE void visitDoctype(XmlDoctype *obj);
	INTERFACE void visitElement(XmlElement *obj);
	INTERFACE void visitPi(XmlPi *obj);
	INTERFACE void visitComment(XmlComment *obj);
	INTERFACE void visitMsect(XmlMsect *obj);
	INTERFACE void visitPcdata(XmlPcdata *obj);
	INTERFACE void visitEntref(XmlEntref *obj);
	INTERFACE void visitAttribute(XmlAttribute *obj);
	INTERFACE void visitProperty(XmlProperty *obj);

	void indent(void);
};

/****************************************************************************
 * XmlCopier
 *
 * Description: 
 * 
 * An XmlVisitor that copes an XmlNode tree.
 * 
 ****************************************************************************/

class XmlCopier : private XmlVisitor {

  public:

	INTERFACE XmlCopier(void);
	INTERFACE ~XmlCopier(void);

	INTERFACE char *exec(XmlNode *node);

  private:

	XmlNode	*mParent;		// parent node of the copy

	//
	// visitor implementations
	//

	INTERFACE void visitDocument(XmlDocument *obj);
	INTERFACE void visitDoctype(XmlDoctype *obj);
	INTERFACE void visitElement(XmlElement *obj);
	INTERFACE void visitPi(XmlPi *obj);
	INTERFACE void visitComment(XmlComment *obj);
	INTERFACE void visitMsect(XmlMsect *obj);
	INTERFACE void visitPcdata(XmlPcdata *obj);
	INTERFACE void visitEntref(XmlEntref *obj);
	INTERFACE void visitAttribute(XmlAttribute *obj);
	INTERFACE void visitProperty(XmlProperty *obj);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
