/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 *
 * ---------------------------------------------------------------------
 * 
 * An approximation of the java.util.ArrayList class.
 *
 * The base List class provides an extensible array of object pointers.
 * Subclasses like StringList will manage the allocation of the string 
 * elements.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>
#include <math.h>

#include <vector>
#include <algorithm>

#include "Util.h"
#include "Vbuf.h"
#include "List.h"

using namespace std;

/****************************************************************************
 *                                                                          *
 *   								 LIST                                   *
 *                                                                          *
 ****************************************************************************/

List::List()
{
	initList(0);
}

List::List(int initialSize)
{
	initList(initialSize);
}

List::List(List* src)
{
    int initialSize = ((src != NULL) ? src->size() : 0);
	initList(initialSize);
    addAll(src);
}

List::List(void** array)
{
    int initialSize = 0;
    if (array != NULL) {
        while (array[initialSize] != NULL)
          initialSize++;
    }
	initList(initialSize);
    addAll(array);
}

/**
 * We don't own the things in the list, but subclasses may
 * overload the deleteElement method.
 */
List::~List()
{
	reset();
}

void List::reset()
{
	clear();
	delete mElements;
	mElements = NULL;
    mArraySize = 0;
}

PRIVATE void List::initList(int initialSize)
{
	mElements = NULL;
	mArraySize = 0;
	mSize = 0;
	grow(initialSize);
}

int List::size()
{
	return mSize;
}

void List::setSize(int size, bool deleteit)
{
	if (size > mSize) {
		grow(size);
	}
	else if (size < mSize) {
		if (deleteit) {
			for (int i = size ; i < mSize ; i++) {
				void* el = mElements[i];
				if (el != NULL) {
					deleteElement(el);
					mElements[i] = NULL;
				}
			}
		}
	}
	mSize = size;
}

void List::setSize(int size)
{
	setSize(size, true);
}

bool List::isEmpty()
{   
    return mSize == 0;
}

void List::clear()
{
	setSize(0, true);
}

void List::clear(bool deleteit)
{
	setSize(0, deleteit);
}

void List::add(void* o)
{
	grow(mSize + 1);
	mElements[mSize++] = copyElement(o);
}

void List::addAll(List* src)
{
    if (src != NULL) {
        for (int i = 0 ; i < src->size() ; i++)
          add(src->get(i));
    }
}

void List::addAll(void** array)
{
    if (array != NULL) {
        for (int i = 0 ; array[i] != NULL ; i++)
          add(array[i]);
    }
}

PRIVATE void List::grow(int index)
{
	if (index >= mArraySize) {
		int i;
		mArraySize = index + LIST_ALLOCATION_UNIT + 1;
		//void** neu = (void**)new char[sizeof(void*) * mArraySize];
		void** neu = new void*[mArraySize];
		for (i = 0 ; i < mSize ; i++)
		  neu[i] = mElements[i];
		for (i = mSize ; i < mArraySize ; i++)
		  neu[i] = NULL;
		delete mElements;
		mElements = neu;
	}
}

void List::add(int index, void* o)
{
	grow(mSize + 1);
	for (int i = mSize ; i > index ; i--)
	  mElements[i] = mElements[i-1];
	mElements[index] = copyElement(o);
	mSize++;
}

void* List::get(int index)
{
	return ((index < mSize) ? mElements[index] : NULL);
}

/**
 * Here we're going to depart from the Java List interface
 * and not return the current value at this position.  This
 * is almost always what you want and makes memory management easier.
 *
 * This will also grow the array which might not be Java, but handy.
 */
void List::set(int index, void* o)
{
	void* prev = NULL;

	if (index >= mSize) {
		grow(index + 1);
		mSize = index + 1;
	}

	prev = mElements[index];
	mElements[index] = copyElement(o);

	// Java would return this, we delete it
	if (prev != NULL)
	  deleteElement(prev);
}

/**
 * Here we're going to depart from the Java List interface
 * and not return the current value at this position.  This
 * is almost always what you want and makes memory management easier.
 */
void List::remove(int index, bool deleteit)
{
	void* prev = NULL;
	if (index < mSize) {
		prev = mElements[index];
        int last = mSize - 1;
		for (int i = index ; i < last ; i++)
		  mElements[i] = mElements[i+1];
		mElements[last] = NULL;
		mSize--;
	}

	if (prev != NULL && deleteit)
	  deleteElement(prev);
}

void List::remove(int index)
{
	remove(index, true);
}

bool List::remove(void* o, bool deleteit)
{
	bool removed = false;
	int index = 0;

    while (index < mSize) {
		if (compareElements(mElements[index], o)) {
			remove(index, deleteit);
			removed = true;
		}
        else
          index++;
    }
	return removed;
}

bool List::remove(void* o)
{
	return remove(o, true);
}

/**
 * Could be more effecient, but I don't have the energy.
 */
void List::removeAll(List* src)
{
    if (src != NULL) {
        for (int i = 0 ; i < src->size() ; i++)
          remove(src->get(i));
    }
}

/**
 * Called when an element is to be added to the list.
 * Type aware subclasses may overload this to do a true copy.
 * If you do that, then you must also overload deleteElement.
 */
void* List::copyElement(void* src)
{
	return src;
}

/**
 * This is called when an element value is no longer needed.
 * In the base List we ignore it since we don't know what it is.
 * Type aware subclasses overload this.
 */
void List::deleteElement(void* o)
{
}

/**
 * Compare two elements for equality.  The default is pointer equality
 * subclasses may overload this.
 */
bool List::compareElements(void* v1, void* v2)
{
	return (v1 == v2);
}

/**
 * Locate the index of an object in the list.
 */
int List::indexOf(void* value)
{	
	int index = -1;
	for (int i = 0 ; i < mSize ; i++) {
		if (compareElements(mElements[i], value)) {
			index = i;
			break;
		}
	}
	return index;
}

bool List::contains(void* value)
{
    return (indexOf(value) >= 0);
}

/**
 * Convert the list into a NULL terminated array of pointers.
 * Collapse empty list to NULL, is this always what we want?
 */
void** List::toArray()
{
	void** array = NULL;

	// empty list 
	if (mSize > 0) {
		array = new void*[mSize + 1];
		for (int i = 0 ; i < mSize ; i++)
		  array[i] = copyElement(mElements[i]);
		array[mSize] = NULL;
	}
	return array;
}

void List::dump()
{
    for (int i = 0 ; i < size() ; i++) 
      printf("%p\n", get(i));
}

/****************************************************************************
 *                                                                          *
 *   							  STRINGLIST                                *
 *                                                                          *
 ****************************************************************************/

StringList::StringList()
{
}

StringList::StringList(const char **strings)
{
	if (strings != NULL) {
		for (int i = 0 ; strings[i] != NULL ; i++)
		  add(strings[i]);
	}
}

StringList::StringList(StringList* src)
{
	if (src != NULL) {
		for (int i = 0 ; i < src->size() ; i++)
		  add(src->getString(i));
	}
}

/**
 * Parse a CSV string into a string list.
 * We'll trim the tokens too, might want an option?
 */
StringList::StringList(const char* csv)
{
	if (csv != NULL) {
		char* token = new char[strlen(csv) + 4];
		const char* src = csv;

		while (*src) {
			char* dest = token;
			// should we trim?
			// skip preceeding whitespace
			while (*src && isspace(*src)) src++;
			while (*src && *src != ',')
			  *dest++ = *src++;
			*dest = 0;
			if (strlen(token) > 0)
			  add(token);
			if (*src)
			  src++;
		}

		delete token;
	}
}

StringList::StringList(int initialSize) : List(initialSize)
{
}

/**
 * For some shitty C++ reason we have to define a destructor or else
 * our overloaded deleteElement method won't be called.
 */
StringList::~StringList()
{
	clear();
	delete mElements;
	mElements = NULL;
}

/**
 * Convert the string list to a CSV string.
 */
char* StringList::toCsv()
{
	char* csv = NULL;
	
	if (mSize > 0) {
		Vbuf* buf = new Vbuf();
		for (int i = 0 ; i < mSize ; i++) {
			if (i > 0)
			  buf->add(",");
			buf->add(getString(i));
		}
		csv = buf->stealString();
		delete buf;
	}
	return csv;
}

StringList* StringList::copy()
{
	StringList* copy = new StringList();
	for (int i = 0 ; i < size() ; i++) 
	  copy->add(getString(i));
	return copy;
}

void StringList::add(const char *src)
{
    // it will be copied
    List::add((void*)src);
}

void StringList::add(int index, const char *src)
{
    // it will be copied
    List::add(index, (void*)src);
}

void StringList::set(int index, const char *src)
{
    // it will be copied
    List::set(index, (void*)src);
}

void* StringList::copyElement(void* src)
{
	return CopyString((const char *)src);
}

void StringList::deleteElement(void* o)
{
	delete (char *)o;
}

bool StringList::compareElements(void* v1, void* v2)
{
	return ((v1 == v2) ||
			(v1 != NULL && v2 != NULL && 
			 !strcmp((char*)v1, (char*)v2)));
}

const char* StringList::getString(int i)
{
    return (const char*)get(i);
}

bool StringList::contains(const char* s)
{
    return List::contains((void*)s);
}

/**
 * Have to reimplement the indexOf loop for this one.
 */
bool StringList::containsNoCase(const char* s)
{
	bool contains = false;

	for (int i = 0 ; i < mSize ; i++) {
		if (StringEqualNoCase((const char*)mElements[i], s)) {
			contains = true;
			break;
		}
	}
	return contains;
}

/**
 * Return whether first element is greater than the second.
 * std probably has one of these but I don't have the energy to 
 * wade through that right now and I'm NOT using string classes.
 */
bool StringSorter(const char* o1, const char* o2)
{
    return (strcmp(o1, o2) < 0);
}

/**
 * Sort the string list in ordinary lexical order.
 * Using std library vector.
 */
void StringList::sort()
{
    if (mSize > 0) {

        vector<const char*>* v = new vector<const char*>;
        for (int i = 0 ; i < mSize ; i++) {
            const char* str = (const char*)mElements[i];
            v->push_back(str);
        }

        ::sort(v->begin(), v->end(), StringSorter);

        // now rebuild the list from the sorted vector
        vector<const char*>::iterator it;
        int psn = 0;
        for (it = v->begin() ; it != v->end() ; it++)
          mElements[psn++] = (void*)*it;
    }
}

/****************************************************************************
 *                                                                          *
 *   							 OBJECT LIST                                *
 *                                                                          *
 ****************************************************************************/

ObjectList::ObjectList()
{
}

ObjectList::ObjectList(int initialSize) : List(initialSize)
{
}

/**
 * For some shitty C++ reason we have to define a destructor or else
 * our overloaded deleteElement method won't be called.
 */
ObjectList::~ObjectList()
{
	clear();
	delete mElements;
	mElements = NULL;
}

void ObjectList::deleteElement(void* o)
{
	delete (ListElement *)o;
}

ListElement* ObjectList::getObject(int i)
{
    return (ListElement*)get(i);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
