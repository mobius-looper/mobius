/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 *
 * ---------------------------------------------------------------------
 *
 * An approximation of the java.util.ArrayList class because I just
 * can't stand STL.
 *
 */

#ifndef LIST_UTIL_H
#define LIST_UTIL_H

/****************************************************************************
 *                                                                          *
 *   								 LIST                                   *
 *                                                                          *
 ****************************************************************************/

#define LIST_ALLOCATION_UNIT 10

/**
 * A generic linked list because I just can't stand STL.
 * Patterned after the Java ArrayList, but we maintain our own
 * internal iterator for simplicity.  This is not thread safe!
 */
class List {

  public:

	List();
	List(int size);
	List(List* src);
	List(void** src);
	virtual ~List();

	void add(void* o);
	void add(int index, void* o);
    void addAll(List* src);
    void addAll(void** src);
	int size();
	void* get(int index);
	void set(int index, void* o);
	void remove(int index, bool deleteit);
	void remove(int index);
	bool remove(void* o, bool deleteit);
	bool remove(void* o);
    void removeAll(List* src);
	void clear();
	void clear(bool deleteit);
	void setSize(int size);
	void setSize(int size, bool deleteit);
	void reset();
	int indexOf(void* o);
    bool contains(void* o);
    bool isEmpty();
	void** toArray();
    void dump();

  protected:

	void initList(int index);
	void grow(int index);

	virtual void* copyElement(void* o);
	virtual void deleteElement(void* o);
	virtual bool compareElements(void* v1, void*v2);

	void** mElements;
	int mArraySize;
	int mSize;
};

/**
 * A simple extension of List that manages string values.
 * Here we will manage private copies of the strings and delete
 * them when the list is deleted.
 */
class StringList : public List {

  public:

	StringList();
	StringList(int size);
	StringList(const char** strings);
	StringList(const char* csv);
	StringList(StringList* src);
	~StringList();

	StringList* copy();
    void add(const char *s);
    void add(int index, const char *s);
    void set(int index, const char *s);
    const char* getString(int i);
    bool contains(const char* s);
    bool containsNoCase(const char* s);
	char* toCsv();

    void sort();

  protected:

	void* copyElement(void* src);
	void deleteElement(void* o);
	bool compareElements(void* v1, void*v2);

};

class ListElement {

  public:
	
	virtual ~ListElement(){}
};

/**
 * An extension of List that manages objects implementing the
 * ListElement interface so that they may be automatically deleted.
 */
class ObjectList : public List {

  public:

	ObjectList();
	ObjectList(int size);
	~ObjectList();

    ListElement* getObject(int index);

  protected:

	void deleteElement(void* o);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
