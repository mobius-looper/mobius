/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Utility for formatting XML text.
 *
 */

#ifndef XML_BUFFER_H
#define XML_BUFFER_H

#include "port.h"
#include "vbuf.h"

#define XML_HEADER "<?xml version='1.0' encoding='UTF-8'?>"

class XmlBuffer : public Vbuf {

  public:

	XmlBuffer();
	~XmlBuffer();

	void setPrefix(const char *s);
	void setNamespace(const char *s);
	void setAttributeNewline(bool b);
	void addNamespace(const char *name, const char *url);
	void incIndent(int i);
	void incIndent();
	void decIndent(int i);
	void decIndent();
	void addAttribute(const char *name, const char *prefix, const char *value);
	void addAttribute(const char *name, const char* value);
	void addAttribute(const char* name, bool value);
	void addAttribute(const char* name, int value);
	void addAttribute(const char* name, long value);
	void addContent(const char* s);
	void addIndent(int indent);
	void addOpenStartTag(const char* name);
	void addOpenStartTag(const char* nmspace, const char* name);
	void closeStartTag();
	void closeStartTag(bool newline);
	void closeEmptyElement();
	void addStartTag(const char* name);
	void addStartTag(const char* nmspace, const char* name);
	void addStartTag(const char * name, bool newline);
	void addStartTag(const char* nmspace, const char* name, bool newline);
	void addEndTag(const char* name);
	void addEndTag(const char* nmspace, const char* name);
	void addEndTag(const char* name, bool indent);
	void addEndTag(const char* nmspace, const char* name, bool indent);
	void addElement(const char* element, const char* content);
	void addElement(const char* nmspace, const char* element,
					const char* content);

  private:

	void checkNamespace();
	
	/**
	 * Current indentation level.
	 */
    int mIndent;

    /**
     * The namespace prefix for elements.
     */
	char* mPrefix;

    /**
     * The namespace URI.
     * When this and _prefix are set, the first time an element is 
     * added to the buffer, a namespace declaration is added.
     */
    char *mNamespace;

    /**
     * Set after the namespace declaration has been added.
     */
    bool mNamespaceDeclared;

    /**
     * List of namespaces to include.
     */
    //Map _namespaces;

	/** 
	 * Option to cause attribute to be emitted on a new line
	 * indented under the element.
	 */
	bool mAttributeNewline;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
