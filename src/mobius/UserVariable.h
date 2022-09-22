/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for a collection of user defined variables.
 *
 */

#ifndef USER_VARIABLE_H
#define USER_VARIABLE_H

// this is only for the embedded ExValue inside Variable
#include "Expr.h"

#define EL_VARIABLES "Variables"

/****************************************************************************
 *                                                                          *
 *   							   VARIABLE                                 *
 *                                                                          *
 ****************************************************************************/

#define MAX_VARIABLE_VALUE 128

/**
 * An arbitrary name/value pair that may be assigned to some Mobius objects
 * by scripts.
 */
class UserVariable {

  public:

	UserVariable();
	UserVariable(class XmlElement* e);
	~UserVariable();

	void setNext(UserVariable* v);
	UserVariable* getNext();

	void setName(const char* name);
	const char* getName();

	void setValue(class ExValue* value);
	void getValue(class ExValue* value);

	void toXml(class XmlBuffer* b);
	void parseXml(class XmlElement* e);

  private:

	void init();

	UserVariable* mNext;
	char* mName;
	ExValue mValue;

};

/**
 * Represents a collection of bound variables.
 * One of these represents a "scope" of variables, currently
 * there are three: global, track, and script.
 */
class UserVariables {

  public:

	UserVariables();
	UserVariables(class XmlElement* e);
	~UserVariables();
	
	UserVariable* getVariable(const char* name);

	void get(const char* name, class ExValue* value);
	void set(const char* name, class ExValue* value);
	bool isBound(const char* name);
    void reset();

	void parseXml(XmlElement* e);
	void toXml(XmlBuffer* b);

  private:

	UserVariable* mVariables;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif

