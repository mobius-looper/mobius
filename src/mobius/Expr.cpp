/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simple expression evaluator embedded in the Mobius scripting language.
 * 
 * Syntax Summary
 *
 * Arithmetic operators: + - * / %
 * Relational operators: ! == != < > <= >= 
 * Logical operators: && ||
 *
 * In addition to the usual C/Java operators we will also allow '=' 
 * as the equality operator, 'and' for && and 'or' for ||.
 *
 * Precence follows the C convention:
 *
 * 1 () [] -> . :: ++ -- 			Grouping 
 * 2  ! ~ ++ -- - + * & 			Logical negation 
 * 3 * / % 							Multiplication, division, modulus 
 * 4 + - 							Addition and subtraction 
 * 5 << >> 							Bitwise shift left and right 
 * 6 < <= > >= 						Comparisons: less-than, ... 
 * 7 ==  != 						Comparisons: equal and not equal 
 * 8 & 								Bitwise AND 
 * 9 ^ 								Bitwise exclusive OR 
 * 10 | 							Bitwise inclusive (normal) OR 
 * 11 && 							Logical AND 
 * 12 || 							Logical OR 
 * 13 = += -= *= /= %= &= ^= <<= >>= Assignment operators 
 *
 * LISTS
 *
 * List values are formed whenever there are adjacent terminals or 
 * complete expressions that are not separated by an operator.  The
 * comma may also be used as a list element separator though it is
 * usually (always?) optional.
 *
 *     1 2 3
 *     1,2,3
 *     a+2, b*3 4
 * 
 * List may be surrounded in parens to make it clearer though this is
 * only required to make sublists.
 *
 *     1 2 3   is the same as (1,2,3)
 *
 *     1 (2 3) 4  list with sublist
 *
 * A list value may be used with two special operators:
 *
 *      .   references qualities of the list
 *      []  references list elements
 *
 *    (1 2 3).length  --> 3
 *    (1 2 3)[1] --> 2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>

#include "Util.h"
#include "Vbuf.h"
#include "Trace.h"
#include "List.h"

#include "Expr.h"

//////////////////////////////////////////////////////////////////////
//
// ExValueList
//
//////////////////////////////////////////////////////////////////////


PUBLIC ExValueList::ExValueList()
{
    mOwner = NULL;
}

/**
 * For some shitty C++ reason we have to define a destructor or else
 * our overloaded deleteElement method won't be called.
 */
PUBLIC ExValueList::~ExValueList()
{
    reset();
}

void ExValueList::deleteElement(void* o)
{
	delete (ExValue *)o;
}

/**
 * This is called whenever an element is added to the list
 * with add() or set().  We don't actually copy but this provides
 * a hook where we can make sure ownership of embedded lists is taken.
 */
PUBLIC void* ExValueList::copyElement(void* src)
{
    if (src != NULL) {
        ExValue* srcv = (ExValue*)src;
        if (srcv->getType() == EX_LIST) {
            ExValueList* childlist = srcv->getList();   
            if (childlist->getOwner() != srcv) {
                printf("WARNING: Transfering ownership of list within list\n");
                fflush(stdout);
            }
            childlist->setOwner(srcv);
        }
    }

	return src;
}

ExValue* ExValueList::getValue(int index)
{
    return (ExValue*)get(index);
}

void ExValueList::setOwner(void* owner)
{
    mOwner = owner;
}

void* ExValueList::getOwner()
{
    return mOwner;
}

ExValueList* ExValueList::copy()
{
    ExValueList* neu = new ExValueList();

    for (int i = 0 ; i < size() ; i++) {
        ExValue* srcvalue = getValue(i);
        if (srcvalue != NULL) {
            ExValue* newvalue = new ExValue();
            neu->add(newvalue);
            if (srcvalue->getType() != EX_LIST)
              newvalue->set(srcvalue);
            else {
                ExValueList* srclist = srcvalue->getList();
                if (srclist != NULL)
                  newvalue->setOwnedList(srclist->copy());
            }
        }
    }
    return neu;
}

/****************************************************************************
 *                                                                          *
 *   								VALUE                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * We don't have an explicit NULL right now.  
 * The default value is the empty string
 */
ExValue::ExValue()
{
	mType = EX_STRING;
	mInt = 0;
	mFloat = 0.0;
	mBool = false;
	mList = NULL;
	strcpy(mString, "");
}

ExValue::~ExValue()
{
	// this will delete elements too
    releaseList();
}

void ExValue::releaseList()
{
    if (mList != NULL) {
        if (mList->getOwner() == this)
          delete mList;
        mList = NULL;
    }
}

ExType ExValue::getType()
{
	return mType;
}

void ExValue::setType(ExType t)
{
	mType = t;
}

char* ExValue::getBuffer()
{
	return mString;
}

int ExValue::getBufferMax()
{
	return EX_MAX_STRING;
}

void ExValue::setNull()
{
	mType = EX_STRING;
    mInt = 0;   
    mFloat = 0.0;
    mBool = false;
	strcpy(mString, "");
    releaseList();
}

bool ExValue::isNull()
{
	return ((mType == EX_STRING) && (strlen(mString) == 0));
}

int ExValue::getInt()
{
	int ival = mInt;

	switch (mType) {
		case EX_FLOAT: {
			ival = (int)mFloat;
		}
		break;

		case EX_BOOL: {
			ival = ((mBool) ? 1 : 0);
		}
		break;
 
		case EX_STRING: { 
			// if empty, it won't set this
            ival = 0;
			sscanf(mString, "%d", &ival);
		}
		break;
 
		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != NULL)
			  ival = el->getInt();
			else
			  ival = 0;
		}
		break;

		case EX_INT: {
			// xcode whines if we don't have this
		}
		break;
	}

	return ival; 
}

void ExValue::setInt(int i)
{
	mType = EX_INT;
	mInt = i;
    releaseList();
}

long ExValue::getLong()
{
	// !! need to have a true long internal value
	return (long)getInt();
}

void ExValue::setLong(long i)
{
	// !! need to have a true long internal value
	mType = EX_INT;
	mInt = (int)i;
    releaseList();
}

float ExValue::getFloat()
{
	float fval = mFloat;

	switch (mType) {

		case EX_INT: {
			fval = (float)mInt;
		}
		break;

		case EX_BOOL: {
			fval = ((mBool) ? 1.0f : 0.0f);
		}
		break;

		case EX_STRING: {
			sscanf(mString, "%f", &fval);
		}
		break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != NULL)
			  fval = el->getFloat();
			else
			  fval = 0.0f;
		}
		break;

		case EX_FLOAT: {
			// xcode whines if we don't have this
		}
		break;
	}

	return fval; 
}

void ExValue::setFloat(float f)
{
	mType = EX_FLOAT;
	mFloat = f;
    releaseList();
}

bool ExValue::getBool()
{
	bool bval = mBool;

	switch (mType) {

		case EX_INT: {
			bval = (mInt != 0);
		}
		break;

		case EX_FLOAT: {
			bval = (mFloat != 0.0);
		}
		break;

		case EX_STRING: {
			bval = (StringEqualNoCase(mString, "true") ||
					StringEqualNoCase(mString, "yes") ||
					StringEqualNoCase(mString, "on") ||
					StringEqualNoCase(mString, "1"));
		}
		break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != NULL)
			  bval = el->getBool();
			else
			  bval = false;
		}
		break;

		case EX_BOOL: {
			// xcode whines if we don't have this
		}
		break;
	}

	return bval; 
}

void ExValue::setBool(bool b)
{
	mType = EX_BOOL;
	mBool = b;
    releaseList();
}

const char* ExValue::getString() 
{
	switch (mType) {

		case EX_INT: {
			sprintf(mString, "%d", mInt);
		}
		break;

		case EX_FLOAT: {
			sprintf(mString, "%f", mFloat);
		}
		break;

		case EX_BOOL: {
			if (mBool)
			  strcpy(mString, "true");
			else
			  strcpy(mString, "false");
        }
        break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != NULL)
			  el->getString(mString, EX_MAX_STRING);
			else
			  strcpy(mString, "");
			break;
		}
		break;

		case EX_STRING: {
			// xcode whines if we don't have this
		}
		break;
	}

	return mString;
}

/**
 * Render the value as a string, but do not change
 * the underlying type.
 */
void ExValue::getString(char* buffer, int max)
{
	switch (mType) {

		case EX_INT: {
			sprintf(buffer, "%d", mInt);
		}
		break;

		case EX_FLOAT: {
			sprintf(buffer, "%f", mFloat);
		}
		break;

		case EX_BOOL: {
			if (mBool)
			  strcpy(buffer, "true");
			else
			  strcpy(buffer, "false");
        }
        break;

        case EX_STRING: {
            CopyString(mString, buffer, max);
		}
        break;

		case EX_LIST: {
            // in theory we should do all of them, just do the first
            // for debugging
			ExValue* el = getList(0);
			if (el != NULL)
			  el->getString(buffer, max);
			else
			  strcpy(buffer, "");
			break;
		}
		break;
	}
}

void ExValue::setString(const char* src)
{
	mType = EX_STRING;
	// coerce may call us with our own buffer
	if (src != mString)
	  CopyString(src, mString, EX_MAX_STRING);
    releaseList();
}

void ExValue::addString(const char* src)
{
    if (mType != EX_STRING)
      setString(src);

    else if (src != mString) 
      AppendString(src, mString, EX_MAX_STRING);
}

ExValueList* ExValue::getList() 
{
	ExValueList* list = NULL;
	ExValue* first = NULL;

	switch (mType) {

		case EX_INT: {
			first = new ExValue();
			first->setInt(mInt);
		}
		break;

		case EX_FLOAT: {
			first = new ExValue();
			first->setFloat(mFloat);
		}
		break;

		case EX_BOOL: {
			first = new ExValue();
			first->setBool(mBool);
		}
		break;

		case EX_STRING: {
			first = new ExValue();
			first->setString(mString);
		}
		break;

		case EX_LIST: {
			list = mList;
		}
		break;
	}

	if (list == NULL && first != NULL) {
		// it promotes so we can keep track of it
		list = new ExValueList();
		list->add(first);
        list->setOwner(this);
		mList = list;
		mType = EX_LIST;
	}

	return list;
}

ExValueList* ExValue::takeList()
{
    ExValueList* list = getList();
    if (list != NULL) {
        if (list->getOwner() != this) {
            // we weren't the owner, I guess this could happen
            // with intermediate ExValues but it's worrisome that
            // there will be a dangling reference somewhere
            printf("WARNING: takeList with someone else's list\n");
            fflush(stdout);
        }
        list->setOwner(NULL);
        mList = NULL;
        mType = EX_STRING;
    }
    return list;
}

void ExValue::setList(ExValueList* src)
{
    if (src == NULL) {
        setNull();
    }
    else {
        // if we had a list already free it
        releaseList();
        mType = EX_LIST;
        mList = src;
    }
}

void ExValue::setOwnedList(ExValueList* src)
{
    if (src != NULL) {
        if (src->getOwner() != NULL) {
            printf("WARNING: setOwnedList called with already owned list\n");
            fflush(stdout);
        }
        src->setOwner(this);
    }
    setList(src);
}

void ExValue::set(ExValue* src, bool owned)
{
    setNull();
    if (src != NULL) {
		ExType otype = src->getType();
		switch (otype) {
			case EX_INT:
				setInt(src->getInt());
				break;

			case EX_FLOAT:
				setFloat(src->getFloat());
				break;

			case EX_BOOL:
				setBool(src->getBool());
				break;

			case EX_STRING:
				setString(src->getString());
				break;

			case EX_LIST:
                if (owned)
                  setOwnedList(src->takeList());
                else
                  setList(src->getList());
				break;
		}
	}
}

/**
 * By default we do not transfer ownership of lists, same as
 * calling setList()
 */
void ExValue::set(ExValue* src)
{
    set(src, false);
}

void ExValue::setOwned(ExValue* src)
{
    set(src, true);
}

/**
 * Coerce a value to a specific type.
 */
void ExValue::coerce(ExType newtype)
{
	if (mType != newtype) {
		switch (newtype) {
			case EX_INT:
				setInt(getInt());
				break;

			case EX_FLOAT:
				setFloat(getFloat());
				break;

			case EX_BOOL:
				setBool(getBool());
				break;

			case EX_STRING:
				setString(getString());
				break;

			case EX_LIST:
				// this coerces and leaves it as a list
				getList();
				break;
		}
	}
}

/**
 * Compare two values, return 1 if they are equal, zero if inequal.
 *
 * If either side is a bool, the other is coerced to bool.
 *
 * If either side is float and the other integer, the other is coerced
 * to float.
 *
 * If either side is a string, and the other not, the string is coerced
 * to the type of the other.
 *
 * Lists aren't comparing right now, don't see a use case.
 */
int ExValue::compare(ExValue* other)
{
	int retval = 0;
	
	if (other == NULL) {
		// assume we are always larger than nothing, though
		// if we have the empty string, could consider both sides "null"?
		retval = 1;
	}
	else {
		ExType otype = other->getType();

		if (mType == EX_BOOL || otype == EX_BOOL) {
			// always a boolean 
			retval = compareBool(other);
		}
		else {
			switch (mType) {
				case EX_INT: {
					switch (otype) {
						case EX_INT:
							retval = compareInt(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareInt(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
				}
				break;
				case EX_FLOAT: {
					switch (otype) {
						case EX_INT:
							retval = compareFloat(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareFloat(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
				}
				break;
				case EX_STRING: {
					switch (otype) {
						case EX_INT:
							retval = compareInt(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareString(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
					break;
				}
				default:
					break;
			}
		}
	}

	return retval;
}

PRIVATE int ExValue::compareInt(ExValue *other)
{
	int retval = 0;
	int myval = getInt();
	int oval = other->getInt();
	
	if (myval > oval)
	  retval = 1;
	else if (myval < oval)
	  retval = -1;

	return retval;
}

PRIVATE int ExValue::compareFloat(ExValue *other)
{
	int retval = 0;
	float myval = getFloat();
	float oval = other->getFloat();
	
	if (myval > oval)
	  retval = 1;
	else if (myval < oval)
	  retval = -1;

	return retval;
}

PRIVATE int ExValue::compareBool(ExValue *other)
{
	int retval = 0;
	bool myval = getBool();
	bool oval = other->getBool();
	
	if (myval && !oval)
	  retval = 1;
	else if (!myval && oval)
	  retval = -1;

	return retval;
}

PRIVATE int ExValue::compareString(ExValue *other)
{
	int retval = 0;
	const char* myval = getString();
	const char* oval = other->getString();

	if (myval == NULL) {
		if (oval != NULL)
		  retval = -1;
	}
	else if (oval == NULL)
	  retval = 1;
	else
	  retval = strcmp(myval, oval);

	return retval;
}

void ExValue::toString(Vbuf* b)
{
    if (mType == EX_LIST) {
        // this is different than getString which is inconsistent and
        // I don't like, think more about toString and getString
        if (mList == NULL)
          b->add("null");
        else {
            b->add("[");    
            for (int i = 0 ; i < mList->size() ; i++) {
                ExValue* el = mList->getValue(i);
                if (i > 0) b->add(",");
                if (el == NULL)
                  b->add("null");
                else
                  el->toString(b);
            }
            b->add("]");
        }
    }
    else if (mType == EX_STRING && strlen(mString) == 0) {
        b->add("null");
    }
    else {
        switch (mType) {
            case EX_INT: b->add("i("); break;
            case EX_FLOAT: b->add("f("); break;
            case EX_BOOL: b->add("b("); break;
            case EX_STRING: b->add("s("); break;
            default: b->add("?("); break;
        }

        b->add(getString());
        b->add(")");
    }
}

void ExValue::dump()
{
	Vbuf* b = new Vbuf();
	toString(b);
	printf("%s\n", b->getString());
	delete b;
}

//
// List Maintenance
//

/**
 * This is used in the atomic value methods to return the first
 * list element.  Could expose these for general use but I think
 * it's better to make callers get the list directly.
 */
ExValue* ExValue::getList(int index)
{
	ExValue* value = NULL;
	if (mList != NULL)
	  value = mList->getValue(index);
	return value;
}

/****************************************************************************
 *                                                                          *
 *   								 NODE                                   *
 *                                                                          *
 ****************************************************************************/

//
// Evaluation helpers
//

int ExNode::evalToInt(ExContext * con)
{
	ExValue v;
	eval(con, &v);
	return v.getInt();
}

bool ExNode::evalToBool(ExContext * con)
{
	ExValue v;
	eval(con, &v);
	return v.getBool();
}

void ExNode::evalToString(ExContext * con, char* buffer, int max)
{
	ExValue v;
	eval(con, &v);
	CopyString(v.getString(), buffer, max);
}

/**
 * The returned list is owned by the caller and must be freed.
 */
ExValueList* ExNode::evalToList(ExContext * con)
{
	ExValue v;
	eval(con, &v);
    return v.takeList();
}

ExNode::ExNode()
{
	mNext = NULL;
	mParent = NULL;
	mChildren = NULL;
}

ExNode::~ExNode()
{
	// we delete our siblings but not the parent
	ExNode* node = mNext;
	while (node != NULL) {
		ExNode* next = node->getNext();
        node->setNext(NULL);
		delete node;
		node = next;
	}
}

ExNode* ExNode::getNext()
{
	return mNext;
}

void ExNode::setNext(ExNode* n)
{
	mNext = n;
}

ExNode* ExNode::getParent()
{
	return mParent;
}

void ExNode::setParent(ExNode* n)
{
	mParent = n;
}

ExNode* ExNode::getChildren()
{
	return mChildren;
}

ExNode* ExNode::stealChildren()
{
    ExNode* children = mChildren;
    mChildren = NULL;
	return children;
}

void ExNode::setChildren(ExNode* n)
{
	delete mChildren;
	mChildren = n;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext())
	  c->setParent(this);
}

ExNode* ExNode::getLastChild(ExNode* first)
{
	ExNode* last = first;
	while (last != NULL && last->getNext() != NULL)
	  last = last->getNext();
	return last;
}

void ExNode::addChild(ExNode* n)
{
	if (n != NULL) {
		n->setParent(this);
		if (mChildren == NULL)
		  mChildren = n;
		else {
			ExNode* last = getLastChild(mChildren);
			last->setNext(n);
		}
	}
}

void ExNode::insertChild(ExNode* n, int psn)
{
	if (n != NULL) {
		n->setParent(this);
        ExNode* prev = NULL;
        ExNode* child = mChildren;
        for (int i = 0 ; i < psn && child != NULL ; i++) {
            prev = child;
            child = child->getNext();
        }
        if (prev == NULL) {
            n->setNext(mChildren);
            mChildren = n;
        }
        else {
            n->setNext(prev->getNext());
            prev->setNext(n);
        }
    }
}

int ExNode::countChildren()
{
	int count = 0;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext())
	  count++;
	return count;
}

bool ExNode::isParent()
{
	return false;
}

bool ExNode::isOperator()
{
	return false;
}

int ExNode::getDesiredOperands()
{
	return 0;
}

bool ExNode::isBlock()
{
	return false;
}

bool ExNode::isSymbol()
{
	return false;
}

int ExNode::getPrecedence()
{
	return 0;
}

/** 
 * Return true of this node has precedence over another.
 * In our numbering system, lower numbers mean higher precedence.
 * We don't have any right-associative operators, but if we did and
 * other was right associative, the comparison would be < rather than <=.
 */
bool ExNode::hasPrecedence(ExNode* other)
{
	return (getPrecedence() <= other->getPrecedence());
}

void ExNode::eval(ExContext* context, ExValue* v)
{
	v->setString(NULL);
}

PRIVATE void ExNode::eval1(ExContext* context, ExValue* v1)
{
	v1->setString(NULL);
	if (mChildren != NULL)
	  mChildren->eval(context, v1);
}

PRIVATE void ExNode::eval2(ExContext* context, ExValue* v1, ExValue* v2)
{
	v1->setString(NULL);
	v2->setString(NULL);

	if (mChildren != NULL) {
		mChildren->eval(context, v1);
		ExNode* second = mChildren->getNext();
		if (second != NULL)
		  second->eval(context, v2);
	}
}

PRIVATE void ExNode::evaln(ExContext* context, ExValue* values, int max)
{
	int i;

	for (i = 0 ; i < max ; i++)
	  values[i].setString(NULL);

	i = 0;
	for (ExNode* c = mChildren ; c != NULL && i < max ; 
		 c = c->getNext(), i++) {
		c->eval(context, &values[i]);
	}
}

void ExNode::toString(Vbuf* b)
{
	b->add("?");
}

/****************************************************************************
 *                                                                          *
 *   							LITERAL VALUES                              *
 *                                                                          *
 ****************************************************************************/

ExLiteral::ExLiteral(int i)
{
	mValue.setInt(i);
}

ExLiteral::ExLiteral(float f)
{
	mValue.setFloat(f);
}

ExLiteral::ExLiteral(const char* str)
{
	mValue.setString(str);
}

void ExLiteral::eval(ExContext* context, ExValue* value)
{
	value->set(&mValue);
}

void ExLiteral::toString(Vbuf* b)
{
	mValue.toString(b);
}

/****************************************************************************
 *                                                                          *
 *   								SYMBOL                                  *
 *                                                                          *
 ****************************************************************************/

ExSymbol::ExSymbol(const char* name)
{
	mName = CopyString(name);
	mResolver = NULL;
	mResolved = false;
}

ExSymbol::~ExSymbol()
{
	delete mName;
	// we must be able to own these!
	delete mResolver;
}

bool ExSymbol::isSymbol()
{
	return true;
}

const char* ExSymbol::getName()
{
	return mName;
}

/**
 * If we have not looked for an ExResolver, do so now, but
 * only do this once.  If there is no resolver, the value is the
 * name of the symbol.
 */
void ExSymbol::eval(ExContext* context, ExValue* value)
{
	if (!mResolved && context != NULL) {
		mResolver = context->getExResolver(this);
		mResolved = true;
	}

	if (mResolver == NULL)
	  value->setString(mName);
	else
	  mResolver->getExValue(context, value);
}

void ExSymbol::toString(Vbuf* b)
{
	b->add(mName);
}

/****************************************************************************
 *                                                                          *
 *   							  OPERATORS                                 *
 *                                                                          *
 ****************************************************************************/

bool ExOperator::isParent()
{
	return true;
}

bool ExOperator::isOperator()
{
	return true;
}

int ExOperator::getDesiredOperands()
{
	return 2;
}

void ExOperator::toString(Vbuf* b)
{
    ExNode* child = mChildren;
    int desired = getDesiredOperands();

	b->add(getOperator());
	b->add("(");

    if (desired <= 0) {
        while (child != NULL) {
            if (child != mChildren)
              b->add(",");
            child->toString(b);
            child = child->getNext();
        }
    }
    else {
        for (int i = 0 ; i < desired ; i++) {
            if (i > 0)
              b->add(",");
            if (child == NULL)
              b->add("?");
            else {
                child->toString(b);
                child = child->getNext();
            }
        }
    }

	b->add(")");
}

/****************************************************************************
 *                                                                          *
 *   						   UNARY OPERATORS                              *
 *                                                                          *
 ****************************************************************************/

const char* ExNot::getOperator()
{
	return "!";
}

int ExNot::getDesiredOperands()
{
	return 1;
}

int ExNot::getPrecedence()
{
	return 2;
}

void ExNot::eval(ExContext* context, ExValue* value)
{
	if (mChildren == NULL) {
		// I guess ! null is true?
		value->setBool(true);
	}
	else {
		// evaluate first child as a boolean and negate
		mChildren->eval(context, value);
		value->setBool(!value->getBool());
	}
}

const char* ExNegate::getOperator()
{
	return "-";
}

int ExNegate::getDesiredOperands()
{
	return 1;
}

int ExNegate::getPrecedence()
{
	return 2;
}

void ExNegate::eval(ExContext* context, ExValue* value)
{
	if (mChildren == NULL)
	  value->setInt(0);
	else {
		// evaluate first child as an integer boolean and negate
		mChildren->eval(context, value);
		value->setInt(-value->getInt());
	}
}

/****************************************************************************
 *                                                                          *
 *   						 RELATIONAL OPERATORS                           *
 *                                                                          *
 ****************************************************************************/

const char* ExEqual::getOperator()
{
	return "==";
}

int ExEqual::getPrecedence()
{
	return 7;
}

void ExEqual::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	value->setBool(v1.compare(&v2) == 0);
}

const char* ExNotEqual::getOperator()
{
	return "!=";
}

int ExNotEqual::getPrecedence()
{
	return 7;
}

void ExNotEqual::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	value->setBool(v1.compare(&v2) != 0);
}

const char* ExGreater::getOperator()
{
	return ">";
}

int ExGreater::getPrecedence()
{
	return 6;
}

void ExGreater::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);

	// Numeric args often get stored as strings since we don't have
	// a good lexical analyzer.  Since comparison is almost always
	// assumed to be numeric, let the operator coerce the arguments.
	// we can add string comparision operators if necessary.
	v1.coerce(EX_INT);
	v2.coerce(EX_INT);

	value->setBool(v1.compare(&v2) > 0);
}

const char* ExLess::getOperator()
{
	return "<";
}

int ExLess::getPrecedence()
{
	return 6;
}

void ExLess::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	v1.coerce(EX_INT);
	v2.coerce(EX_INT);
	value->setBool(v1.compare(&v2) < 0);
}

const char* ExGreaterEqual::getOperator()
{
	return ">=";
}

int ExGreaterEqual::getPrecedence()
{
	return 6;
}

void ExGreaterEqual::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	v1.coerce(EX_INT);
	v2.coerce(EX_INT);
	value->setBool(v1.compare(&v2) >= 0);
}

const char* ExLessEqual::getOperator()
{
	return "<=";
}

int ExLessEqual::getPrecedence()
{
	return 6;
}

void ExLessEqual::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	v1.coerce(EX_INT);
	v2.coerce(EX_INT);
	value->setBool(v1.compare(&v2) <= 0);
}

/****************************************************************************
 *                                                                          *
 *   						 ARITHMETIC OPERATORS                           *
 *                                                                          *
 ****************************************************************************/

const char* ExAdd::getOperator()
{
	return "+";
}

int ExAdd::getPrecedence()
{
	return 4;
}

/**
 * For the aritmetic operators we should normally have only
 * two operands but allow more so they behave more like functions.
 * The if any child node evaluates to a floating value, the result
 * is promoted to a float.
 */
void ExAdd::eval(ExContext* context, ExValue* value)
{
	// should only have 2 values, but allow more
	int ival = 0;
	float fval = 0.0;
	bool floating = false;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (!floating && v.getType() == EX_FLOAT) {
			fval = (float)ival;
			floating = true;
		}
		if (floating)
		  fval += v.getFloat();
		else
		  ival += v.getInt();
	}
	if (floating)
	  value->setFloat(fval);
	else
	  value->setInt(ival);
}

const char* ExSubtract::getOperator()
{
	return "-";
}

int ExSubtract::getPrecedence()
{
	return 4;
}

void ExSubtract::eval(ExContext* context, ExValue* value)
{
	int ival = 0;
	float fval = 0.0;
	bool floating = false;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (!floating && v.getType() == EX_FLOAT) {
			fval = (float)ival;
			floating = true;
		}
		if (floating) {
			float fv = v.getFloat();
			if (c == mChildren)
			  fval = fv;
			else
			  fval -= fv;
		}
		else {
			int iv = v.getInt();
			if (c == mChildren)
			  ival = iv;
			else
			  ival -= iv;
		}
	}
	if (floating)
	  value->setFloat(fval);
	else
	  value->setInt(ival);
}

const char* ExMultiply::getOperator()
{
	return "*";
}

int ExMultiply::getPrecedence()
{
	return 3;
}

void ExMultiply::eval(ExContext* context, ExValue* value)
{
	int ival = 1;
	float fval = 1.0;
	bool floating = false;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (!floating && v.getType() == EX_FLOAT) {
			fval = (float)ival;
			floating = true;
		}
		if (floating)
		  fval *= v.getFloat();
		else
		  ival *= v.getInt();
	}
	if (floating)
	  value->setFloat(fval);
	else
	  value->setInt(ival);
}

const char* ExDivide::getOperator()
{
	return "/";
}

int ExDivide::getPrecedence()
{
	return 3;
}

/**
 * Unlike most conventional languages, divide by zero and modulo
 * by zero will result in a value of zero rather than throwing an 
 * exception.  
 */
void ExDivide::eval(ExContext* context, ExValue* value)
{
	// note that we consider divide by zero as zero
	int ival = 0;
	float fval = 0.0;
	bool floating = false;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (!floating && v.getType() == EX_FLOAT) {
			fval = (float)ival;
			floating = true;
		}
		if (floating) {
			float fv = v.getFloat();
			if (c == mChildren)
			  fval = fv;
			else if (fv == 0.0) 
			  fval = 0.0;
			else
			  fval /= fv;
		}
		else {
			int iv = v.getInt();
			if (c == mChildren)
			  ival = iv;
			else if (iv == 0) 
			  ival = 0;
			else
			  ival /= iv;
		}
	}
	if (floating)
	  value->setFloat(fval);
	else
	  value->setInt(ival);
}

const char* ExModulo::getOperator()
{
	return "%";
}

int ExModulo::getPrecedence()
{
	return 3;
}

void ExModulo::eval(ExContext* context, ExValue* value)
{
	// only two arguments make sense
	ExValue v1, v2;
	eval2(context, &v1, &v2);
	int ival1 = v1.getInt();
	int ival2 = v2.getInt();
	if (ival2 == 0)
	  value->setInt(0);
	else
	  value->setInt(ival1 % ival2);
}

/****************************************************************************
 *                                                                          *
 *   						  LOGICAL OPERATORS                             *
 *                                                                          *
 ****************************************************************************/

const char* ExAnd::getOperator()
{
	return "&&";
}

int ExAnd::getPrecedence()
{
	return 11;
}

void ExAnd::eval(ExContext* context, ExValue* value)
{
	// all children must be true
	// is an and of nothing also true?
	bool result = true;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (!v.getBool()) {
			result = false;
			break;
		}
	}
	value->setBool(result);
}

const char* ExOr::getOperator()
{
	return "||";
}

int ExOr::getPrecedence()
{
	return 11;
}

void ExOr::eval(ExContext* context, ExValue* value)
{
	// true if any of the children are true
	bool result = false;
	ExValue v;
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		c->eval(context, &v);
		if (v.getBool()) {
			result = true;
			break;
		}
	}
	value->setBool(result);
}

/****************************************************************************
 *                                                                          *
 *   								BLOCKS                                  *
 *                                                                          *
 ****************************************************************************/

ExBlock::ExBlock()
{
}

bool ExBlock::isBlock()
{
	return true;
}

/**
 * All blocks: {}, (), foo() have highest precedence.
 */
int ExBlock::getPrecedence()
{
	return 1;
}

bool ExBlock::isParent()
{
	return true;
}

bool ExBlock::isFunction()
{
	return false;
}

bool ExBlock::isParenthesis()
{
	return false;
}

bool ExBlock::isList()
{
	return false;
}

bool ExBlock::isArray()
{
	return false;
}

bool ExBlock::isIndex()
{
	return false;
}

/**
 * The value of a block is the value of its last child expression.
 * The others are evaluated for side effect, which we don't actually
 * have any use for yet.
 */
void ExBlock::eval(ExContext* context, ExValue* value)
{
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext())
	  c->eval(context, value);
}

/**
 * Shouldn't see any of these yet.
 */
void ExBlock::toString(Vbuf* b)
{
	b->add("{");
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add("}");
}

//
// ExParenthesis
//

bool ExParenthesis::isParenthesis()
{
	return true;
}

/**
 * Should not have any of these left in the tree after parsing!!
 */
void ExParenthesis::toString(Vbuf* b)
{
	b->add("(");
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add(")");
}

//
// ExFunction
//

bool ExFunction::isFunction()
{
	return true;
}

/**
 * Should have any of these left in the tree after parsing.
 */
void ExFunction::toString(Vbuf* b)
{
	b->add(getFunction());
	b->add("(");
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add(")");
}

//
// ExList
//

bool ExList::isList()
{
	return true;
}

void ExList::toString(Vbuf* b)
{
	b->add("list(");
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add(")");
}

/**
 * The list will be owned by the supplied value.
 */
void ExList::eval(ExContext* context, ExValue* value)
{
    value->setNull();
    if (mChildren != NULL) {
        ExValueList* list = new ExValueList();
        for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
            ExValue* el = new ExValue();
            c->eval(context, el);   
            list->add(el);
        }
        value->setOwnedList(list);
    }
}

//
// ExArray
//

bool ExArray::isArray()
{
	return true;
}

void ExArray::toString(Vbuf* b)
{
	b->add("array(");
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add(")");
}

void ExArray::eval(ExContext* context, ExValue* value)
{
    value->setNull();
    if (mChildren != NULL) {
        ExValueList* list = new ExValueList();
        for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
            ExValue* el = new ExValue();
            c->eval(context, el);   
            list->add(el);
        }
        value->setOwnedList(list);
    }
}

//
// ExIndex
//

bool ExIndex::isIndex()
{
	return true;
}

void ExIndex::toString(Vbuf* b)
{
	b->add("index(");
    // should only have one, no multi-dimensional arrays yet
	for (ExNode* c = mIndexes ; c != NULL ; c = c->getNext()) {
		if (c != mIndexes)
		  b->add(",");
		c->toString(b);
	}

    // always a placeholder between index and children
    b->add(",");
    
	for (ExNode* c = mChildren ; c != NULL ; c = c->getNext()) {
		if (c != mChildren)
		  b->add(",");
		c->toString(b);
	}
	b->add(")");
}

ExNode* ExIndex::getIndexes()
{
	return mIndexes;
}

void ExIndex::setIndexes(ExNode* n)
{
	delete mIndexes;
	mIndexes = n;
    // is this relevant?
	for (ExNode* c = mIndexes ; c != NULL ; c = c->getNext())
	  c->setParent(this);
}

void ExIndex::addIndex(ExNode* n)
{
	if (n != NULL) {
		n->setParent(this);
		if (mIndexes == NULL)
		  mChildren = n;
		else {
			ExNode* last = getLastChild(mIndexes);
			last->setNext(n);
		}
	}
}

/**
 * Evaluate the mIndexes to determine the numeric list/array indexes.
 * Then evaluate the children to produce the thing that supposedly
 * can be indexed.  We're unary left associative so there should
 * only be one child.
 */
void ExIndex::eval(ExContext* context, ExValue* value)
{
    ExValue v;

    value->setNull();

    int index = 0;
    if (mIndexes != NULL) {
        // ignore all but the first one
        mIndexes->eval(context, &v);
        index = v.getInt();
    }

    if (mChildren != NULL) {
        mChildren->eval(context, &v);
        if (v.getType() == EX_LIST) {
            ExValueList* list = v.getList();
            if (list != NULL) {
                ExValue* src = list->getValue(index);
                value->set(src);
            }
        }
        else if (v.getType() == EX_STRING) {
            const char* str = v.getString();
            if (str != NULL) {
                int len = strlen(str);
                if (index < len) {
                    char buf[4];
                    buf[0] = str[index];
                    buf[1] = 0;
                    value->setString(buf);
                }
            }
        }
    }
}

/****************************************************************************
 *                                                                          *
 *   							  FUNCTIONS                                 *
 *                                                                          *
 ****************************************************************************/

//
// int(value)
//

const char* ExInt::getFunction()
{
	return "int";
}

void ExInt::eval(ExContext* context, ExValue* value)
{
	ExValue v;
	eval1(context, &v);
	value->setInt(v.getInt());
}

//
// float(value)
//

const char* ExFloat::getFunction()
{
	return "float";
}

void ExFloat::eval(ExContext* context, ExValue* value)
{
	ExValue v;
	eval1(context, &v);
	value->setFloat(v.getFloat());
}

//
// string(value)
//

const char* ExString::getFunction()
{
	return "string";
}

void ExString::eval(ExContext* context, ExValue* value)
{
	ExValue v;
	eval1(context, &v);
	value->setString(v.getString());
}

//
// abs(value)
//

const char* ExAbs::getFunction()
{
	return "abs";
}

void ExAbs::eval(ExContext* context, ExValue* value)
{
	ExValue v;
	eval1(context, &v);
	int ival = v.getInt();
	if (ival < 0) ival = -ival;
	value->setInt(ival);
}

//
// rand(low,high)
//

const char* ExRand::getFunction()
{
	return "rand";
}

void ExRand::eval(ExContext* context, ExValue* value)
{
	ExValue v1, v2;
	eval2(context, &v1, &v2);

	int low = v1.getInt();
	int high = v2.getInt();
	int rvalue;

	if (low >= high)
	  rvalue = low;
	else {
		// Random includes both low and high in its range
		// ?? if high/low are negative could be problems
		rvalue = Random(low, high);
	}

	value->setInt(rvalue);
}

//
// scale(value,low,high,newLow,newHigh
//

const char* ExScale::getFunction()
{
	return "scale";
}

void ExScale::eval(ExContext* context, ExValue* value)
{
	int count = countChildren();
	if (count == 5) {
		ExValue values[5];
		evaln(context, values, 5);
		// TODO: 
	}
}

//
// custom(...)
//

ExCustom::ExCustom(const char* name)
{
    mName = CopyString(name);
}

ExCustom::~ExCustom()
{
    delete mName;
}

const char* ExCustom::getFunction()
{
	return mName;
}

void ExCustom::eval(ExContext* context, ExValue* value)
{
    // TODO: Need some way to resolve these!!
    printf("Unresolved expression function: %s\n", mName);
    fflush(stdout);
	value->setNull();
}

/****************************************************************************
 *                                                                          *
 *   							   PARSING                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Based on the "shunting yard" algorithm.
 */
ExParser::ExParser()
{
	mError = NULL;
	strcpy(mErrorArg, "");
	mSource = NULL;
	mSourcePsn = 0;
	mNext = 0;
	strcpy(mToken, "");
	mTokenPsn = 0;

	mOperands = NULL;
	mOperators = NULL;
    mCurrent = NULL;
	mLast = NULL;
    mLookahead = NULL;
}

ExParser::~ExParser()
{
	deleteStack(mOperators);
	deleteStack(mOperands);
}

const char* ExParser::getError()
{
	return mError;
}

const char* ExParser::getErrorArg()
{
	return ((strlen(mErrorArg) > 0) ? mErrorArg : NULL);
}

void ExParser::printError()
{
	if (mError == NULL)
	  printf("Source string empty\n");
	else if (strlen(mErrorArg) == 0)
	  printf("ERROR: %s\n", mError);
	else 
	  printf("ERROR: %s: %s\n", mError, mErrorArg);
}

/**
 * Delete nodes remaining on a stack.
 */
PRIVATE void ExParser::deleteStack(ExNode* stack)
{
	while (stack != NULL) {
		ExNode* prev = stack->getParent();
		delete stack;
		stack = prev;
	}
}

/**
 * Push a node on the operand stack.
 */
PRIVATE void ExParser::pushOperand(ExNode* n)
{
	n->setParent(mOperands);
	mOperands = n;
}

PRIVATE void ExParser::pushOperator(ExNode* n)
{
	n->setParent(mOperators);
	mOperators = n;
}

PRIVATE ExNode* ExParser::popOperator()
{
	ExNode* op = mOperators;
	if (op != NULL) {
		mOperators = op->getParent();
		op->setParent(NULL);
	}
	else {
		// this would be a bug in the parser
		mError = "Missing operator";
	}
	return op;
}

PRIVATE ExNode* ExParser::popOperand()
{
	ExNode* op = mOperands;
	if (op != NULL) {
		mOperands = op->getParent();
		op->setParent(NULL);
	}
	else {
		// this would be a syntax error
		// e.g. "a +"
		mError = "Missing operand";
	}
	return op;
}
		

/**
 * Return true if the operator on the top of the stack
 * has enough operands to be satisfied.  Used when encountering
 * adjacent non-operators which we coerce to a list constructor.
 * UPDATE: Not used...
 */
PRIVATE bool ExParser::isOperatorSatisfied()
{
    bool satisfied = false;
    if (mOperators != NULL) {
        int desired = mOperators->getDesiredOperands();
        if (desired == 1) {
            satisfied = (mOperands != NULL);
        }
        else if (desired == 2) {
            satisfied = (mOperands != NULL && mOperands->getParent() != NULL);
        }
        // else, assume block, they're never satisfied
    }
    return satisfied;
}

/**
 * Pop the top operator and its operands from the stacks
 * and move the operator node to the operand stack.
 */
PRIVATE void ExParser::shiftOperator()
{
	ExNode* op = popOperator();
    int desired = op->getDesiredOperands();
    if (desired <= 0) {
        // blocks take everything
        // must be one of our undelimited lists, consume all the operands
        // Note that the children are in reverse order on the stack
        // so use insertChild to reverse the order
        // sigh, if we bootstrapped an ExList it may already hav a child
        // which is logicaly at the head of the list so preserve
        // the order of existing children
        if (mOperands != NULL) {
            int psn = op->countChildren();
            while (mOperands != NULL)
              op->insertChild(popOperand(), psn);
        }
    }
    else {
        for (int i = 0 ; i < desired ; i++)
          op->insertChild(popOperand(), 0);
    }

	pushOperand(op);
}

/**
 * Parse a string into a node tree.
 */
ExNode* ExParser::parse(const char* src)
{
	ExNode* root = NULL;

	if (src != NULL) {

		mError = NULL;
		strcpy(mErrorArg, "");

		mSource = src;
		mSourcePsn = 0;
		mNext = mSource[0];
        strcpy(mToken, "");
		mTokenPsn = 0;

		deleteStack(mOperands);
		mOperands = NULL;
		deleteStack(mOperators);
		mOperators = NULL;

        mCurrent = NULL;
		mLast = NULL;
        mLookahead = NULL;

		while (mError == NULL && (mNext || mLookahead != NULL)) {
			ExNode *node = nextToken();

			if (mError == NULL) {
				if (node != NULL) {
					if (!node->isParent()) {
                        pushOperand(node);
                    }
					else if (mOperators == NULL) {
                        pushOperator(node);
                    }
					else {
						// Shift operators that have a higher precedence
						// than we do.  Here, a lower number means higher 
						// precedence.  We don't have right-associative 
						// operators, if we did and node was right-associative
						// the comparison would be > rather than >=
						// stop at blocks
						while (mError == NULL &&
							   mOperators != NULL && 
							   !mOperators->isBlock() &&
							   mOperators->hasPrecedence(node)) {
							shiftOperator();
						}
                        if (mError == NULL)
                          pushOperator(node);
					}
				}
				else if (!strcmp(mToken, ",")) {
					// pop till we reach the containing block
					while (mError == NULL && mOperators != NULL && 
						   !mOperators->isBlock())
                      shiftOperator();
                    
                    if (mError == NULL) {
                        if (mOperators == NULL) {
                            // unbalanced block delimiters or misplaced comma
                            // auto promote to list
                            //mError = "Misplaced comma or unbalanced parenthesis";
                            pushOperator(new ExList());
                        }

                        if (mOperators->isBlock()) {
                            // the top of the operand stack is the next 
                            // argument to the function or block
                            mOperators->addChild(popOperand());
                        }
                        else {
                            // can this happen, would we add a list wrapper?
                            mError = "Unexpected comma";
                        }
                    }
				}
				else if (!strcmp(mToken, "(")) {
					if (mLast != NULL && mLast->isSymbol()) {
						// promote the symbol to a function
						ExSymbol* s = (ExSymbol*)popOperand();
						ExNode* func = newFunction(s->getName());
						if (func != NULL)
						  pushOperator(func);
						else {
							mError = "Invalid function";
							CopyString(s->getName(), mErrorArg, EX_MAX_ERROR_ARG);
						}
						delete s;	
                        mLast = func;
					}
					else 
					  pushOperator(new ExParenthesis());
				}
				else if (!strcmp(mToken, ")")) {
					// pop until we hit a block, will either leave
                    // one expression node on the top of the operand stack
                    // or we'll get an unbalanced error
					while (mError == NULL && mOperators != NULL &&
						   !mOperators->isBlock()) {
						shiftOperator();
					}
                    
                    // The top of the operand stack is the last
                    // argument to the block.  If we had a
                    // syntax error of the form foo(a + b c + d) 
                    // we'll leave a dangling +(c,d).  Should have
                    // caught this in the lexer and converted the
                    // intervening space to a comma
                    
                    if (mError == NULL) {
                        // will be a block or NULL
                        ExBlock* block = (ExBlock*)popOperator();
                        
                        if (block == NULL) {
                            // didn't find a block
                            mError = "Unbalanced parenthesis";
                        }
                        else if (block->isArray()) {
                            mError = "Unbalanced parenthesis";
                        }
                        else if (!block->isParenthesis()) {
                            // function or list
                            if (mOperands != NULL)
                              block->addChild(popOperand());
                            pushOperand(block);
                        }
                        else if (block->getChildren() == NULL) {
                            // single element parens are simply removed
                            deleteNode(block);
                        }
                        else {
                            // promote to a list constructor    
                            ExList* list = new ExList();
                            list->setChildren(block->stealChildren());
                            if (mOperands != NULL)
                              list->addChild(popOperand());
                            pushOperand(list);
                            deleteNode(block);
                        }
                    }
				}
				else if (!strcmp(mToken, "[")) {
                    // could either make this a reference operator,
                    // a list literal, or both...that would make it a
                    // funny operator with children for the index applied
                    // to a child providing the array...
                    if (mLast == NULL || mLast->isOperator()) {
                        pushOperator(new ExArray());
                    }
                    else {
                        pushOperator(new ExIndex());
					}
				}
				else if (!strcmp(mToken, "]")) {
					// pop until we hit a block
					while (mError == NULL && mOperators != NULL && 
						   !mOperators->isBlock()) {
						shiftOperator();
					}

                    if (mError == NULL) {
                        // will be a block or NULL
                        ExBlock* block = (ExBlock*)popOperator();
                        
                        if (block == NULL) {
                            // didn't find a block
                            mError = "Unbalanced bracket";
                        }
                        else if (block->isArray()) {
                            if (mOperands != NULL)
                              block->addChild(popOperand());
                            pushOperand(block);
                        }
                        else if (block->isIndex()) {
                            ExIndex* index = (ExIndex*)block;
                            if (mOperands != NULL)
                              index->addIndex(popOperand());
                            pushOperand(block);
                            // .. must be something more
                        }
                        else {
                            mError = "Unbalanced bracket";
                        }
					}
				}
				else if (strlen(mToken) > 0) {
					mError = "Invalid token";
					CopyString(mToken, mErrorArg, EX_MAX_ERROR_ARG);
					
				}
			}
		}

		// shift any remaining operators
		while (mError == NULL && mOperators != NULL)
		  shiftOperator();

		if (mError == NULL) {
			// operand stack could be empty for special case ()
			// or an empty source string
			if (mOperands != NULL) {
				// may be more than one if a syntax error of the form
				// a + b c + d
				if (mOperands->getParent() != NULL) {
                    // formerly an error, just wrap them in a list
                    //mError = "Multiple expressions in source string";
                    root = new ExList();
                    while (mOperands != NULL)
                      root->insertChild(popOperand(), 0);
                }
				else {
					root = mOperands;
					mOperands = NULL;
				}
			}
		}
	}

	return root;
}

/**
 * Called when we're transforming a node.  Delete the original
 * and remove any potential references to avoid invalid memory refs.
 */
PRIVATE void ExParser::deleteNode(ExNode* node)
{
    delete node;
    
    // don't know if this can happen
    if (mLast == node)
      mLast = NULL;

    // but this one can
    if (mCurrent == node)
      mCurrent = NULL;
}

/**
 * Isolate the next token in the source stream, and create an ExNode.
 * Look ma, it's lexical analysis!
 */
PRIVATE ExNode* ExParser::nextToken()
{
    ExNode* node = NULL;

    // shift this 
    mLast = mCurrent;
    bool blockClose = (mLast == NULL && (!strcmp(mToken, ")") || !strcmp(mToken, "]")));

    if (mLookahead != NULL) {
        node = mLookahead;
        mLookahead = NULL;
    }
    else {
        node = nextTokenForReal();

        // hack: I like to make commas optional where possible
        // if we find adjacent operands without an operator
        // treat the "gap" as if it were a comma to force the
        // building of a block.  Same for the patterns: ) x and ] x
        if (node != NULL) {
            if (!node->isOperator() || node->getDesiredOperands() == 1) {
                // skip operators AND blocks
                bool pushLookahead = false;
                
                // if we just closed a block or shifted a non-operator
                if (blockClose || (mLast != NULL && !mLast->isParent())) {

                    if (mLookahead != NULL)
                      mError = "Lookahead token overflow";
                    else {
                        mLookahead = node;
                        node = NULL;
                        strcpy(mToken, ",");
                    }
                }
            }
            else if (mLast != NULL && mLast->isOperator()) {
                // a non-unary operator
                // something like a++b catch early
                // before we confuse the stack?
                mError = "Adjacent operators";
            }
        }
    }

    // remember this for the next call
    mCurrent = node;

    return node;
}

PRIVATE ExNode* ExParser::nextTokenForReal()
{
	ExNode* node = NULL;

    // Determine negatability based on the previous token.
    // If previous token is an operator, block start, or comma
    // then we can negate:  a--b a(-b a[-b a,-b
    // If the previous token is a non-operator or block end, then
    // we have to treat like a subtract: a-b a)-b a]-b
    // UPDATE: This sucks for script args with negative numbers,
    // "WindowMove subcycle -1" which gets converted to a subtraction
    // from the unresolved synbol subcycle.  Would it be too weird to
    // make space position significant?  

    bool negatable;
    if (mLast != NULL)
      negatable = mLast->isOperator();
    else
      negatable = (strlen(mToken) == 0 ||
                   !strcmp(mToken, "(") ||
                   !strcmp(mToken, "[") ||
                   !strcmp(mToken, ","));


	strcpy(mToken, "");
	mTokenPsn = 0;

	// skip leading space
	while (mNext && (isspace(mNext) || !isprint(mNext))) nextChar();

	if (mNext == '#') {
		// an end of line comment
		// hmm, I suppose we could support multi-line expressions?
		while (mNext && mError == NULL) {
			nextChar();
			if (mNext == '\n')
			  break;
		}
		node = nextToken();
	}
	else if (mNext == '"' || mNext == '\'') {
		// a string literal
		char quote = mNext;
		bool escape = false;
		bool terminated = false;
		nextChar();
		while (mNext && mError == NULL && !terminated) {
			if (escape) {
				// only one
				toToken();
				escape = false;
			}
			else if (mNext == '\\') {
				nextChar();
				escape = true;
			}
			else if (mNext == quote) {
				nextChar();
				terminated = true;
			}
			else
			  toToken();
		}
		
		if (!terminated)
		  mError = "Unterminated string";
		else
		  node = new ExLiteral(mToken);
	}
	else if (mNext == '-' && !negatable) {
		// a minus falling after a non-operator must be a subtract, 
		// catch it before we fall into the number parser
        // NOTE: this means the auto conversion of spaces to commas
        // won't work for negation, e.g. a -b isn't the same
        // as a,-b.  That's unfortunate because one possible
        // use for this is in shuffle patterns where negative means 
        // reverse "1 -4 3 -8"
        // we'll have to make a space sensitive lexer where:
        //    1-2 means 1 - 2
        //    1- 2 means 1 - 2
        //    1 -2 means 1,-2
		toToken();
		node = new ExSubtract();
	}
	else if (mNext && 
			 (mNext == '-' ||
			  isalnum(mNext) || 
			  strchr(SYMBOL_CHARS, mNext) != NULL)) {

		// If we get a leading minus, always try to make it a negative
		// numeric literal.  If we can't then we have to rewind and make
		// it a subtract or negate operator.
		int mLeadingMinus = (mNext == '-') ? mSourcePsn : -1;
		int chars = 0;
		int dots = 0;
		int others = 0;

		do {
			if (isalpha(mNext))
			  chars++;
			else if (mNext == '.')
			  dots++;
			else if (!isdigit(mNext) && mNext != '-')
			  others++;

			toToken();
		}
		while (mNext && (isalnum(mNext) || strchr(SYMBOL_CHARS, mNext) != NULL));

		// TODO: If we want to allow "and" and "or" to mean && and ||
		// this is the place

		if (mError == NULL) {

			// ?? should we require negation to be adjacent to the
			// operand or can there be spaces, e.g. a - - b
			// currently allowing spaces

			if (!strcmp(mToken, "-")) {
				// all we had was -, must be a negation
				node = new ExNegate();
			}
			else if (chars > 0 || others > 0 || dots > 1) {
				if (mLeadingMinus < 0) 
				  node = new ExSymbol(mToken);
				else {
					// we consumed a leading - but didn't find a number,
					// rewind and convert it to a negation
					mSourcePsn = mLeadingMinus;
					mNext = mSource[mSourcePsn];
					toToken();
					node = new ExNegate();
				}
			}
			else if (dots == 1)
			  node = new ExLiteral((float)atof(mToken));
			else
			  node = new ExLiteral(atoi(mToken));
		}
	}
	else if (mNext && strchr(OPERATOR_CHARS, mNext)) {
		char first = mNext;
		toToken();
		if (first == '!' || first == '=' || first == '<' || first == '>') {
			if (mNext == '=')
			  toToken();
		}
		else if (first == '&') {
			if (mNext == '&')
			  toToken();
		}
		else if (first == '|') {
		  if (mNext == '|')
			toToken();
		}
		node = newOperator(mToken);
	}
	else
	  toToken();

	return node;
}

/**
 * Adavnce the character position.
 */
PRIVATE void ExParser::nextChar()
{
	if (mNext)
	  mNext = mSource[++mSourcePsn];
}

/**
 * Add the next character to the token and advance the character.
 */
PRIVATE void ExParser::toToken()
{
	if (mNext) {
		if (mTokenPsn >= EX_MAX_TOKEN)
		  mError = "Token overflow";
		else {
			mToken[mTokenPsn++] = mNext;
			// mToken is actually larger than max so we can always keep
			// it terminated
			mToken[mTokenPsn] = 0;
			nextChar();
		}
	}
}

/**
 * Build the proper operator node.
 * Special tokens ( and ) won't turn into nodes, we'll
 * handle them up in parse().
 */
PRIVATE ExNode* ExParser::newOperator(const char* name)
{
	ExNode* node = NULL;

	if (name != NULL) {

		if (!strcmp(name, "!"))
		  node = new ExNot();

		else if (!strcmp(name, "=") || !strcmp(name, "=="))
		  node = new ExEqual();

		else if (!strcmp(name, "!="))
		  node = new ExNotEqual();

		else if (!strcmp(name, ">"))
		  node = new ExGreater();

		else if (!strcmp(name, "<"))
		  node = new ExLess();

		else if (!strcmp(name, ">="))
		  node = new ExGreaterEqual();

		else if (!strcmp(name, "<="))
		  node = new ExLessEqual();

		else if (!strcmp(name, "+"))
		  node = new ExAdd();

		else if (!strcmp(name, "-"))
		  node = new ExSubtract();

		else if (!strcmp(name, "*"))
		  node = new ExMultiply();

		else if (!strcmp(name, "/"))
		  node = new ExDivide();

		else if (!strcmp(name, "%"))
		  node = new ExModulo();

		else if (!strcmp(name, "&") || !strcmp(name, "&&"))
		  node = new ExAnd();

		else if (!strcmp(name, "|") || !strcmp(name, "||"))
		  node = new ExOr();
	}

	return node;
}

/**
 * Create a function node from a symbol
 */
PRIVATE ExNode* ExParser::newFunction(const char* name)
{
	ExNode* func = NULL;
	
	if (StringEqualNoCase(name, "abs"))
	  func = new ExAbs();

	else if (StringEqualNoCase(name, "rand"))
	  func = new ExRand();
	
	else if (StringEqualNoCase(name, "scale"))
	  func = new ExScale();

	else if (StringEqualNoCase(name, "int"))
	  func = new ExInt();

	else if (StringEqualNoCase(name, "float"))
	  func = new ExFloat();

	else if (StringEqualNoCase(name, "string"))
	  func = new ExString();

    else {
        // formerly returned null and made it a parse error, 
        // now we'll try to resolve it lazily
        func = new ExCustom(name);
    }

	return func;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
