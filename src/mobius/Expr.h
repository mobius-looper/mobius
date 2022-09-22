/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simple expression evaluator embedded in the Mobius scripting language.
 *
 * TODO:
 *
 * isnull <op>
 * notnull <op>
 *
 */

#ifndef EXPR_H
#define EXPR_H

#include "List.h"

/****************************************************************************
 *                                                                          *
 *   								VALUE                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * The maximum length of a string value returned by an expression node.
 * This can be used for paths so it needs to be healthy.
 * Originally this was 2K but I started using them embedded in Action
 * and that was way too large.  Paths are only used for testing, so just
 * be sure to test with short paths.
 */
#define EX_MAX_STRING 128

/**
 * An enumeration of the types of values we may hold in an ExValue.
 */
typedef enum {

    EX_INT,
	EX_FLOAT,
	EX_BOOL,
	EX_STRING,
	EX_LIST

} ExType;

/**
 * Expressions generate values.
 * String values have an upper bound so we don't have to deal with
 * dynamic allocation during evaluation.
 */
class ExValue {

  public:
	
	ExValue();
	~ExValue();

	ExType getType();
	void setType(ExType t);
	void coerce(ExType newtype);
	
	void setNull();
	bool isNull();

	int getInt();
	void setInt(int i);

	long getLong();
	void setLong(long i);
	
	float getFloat();
	void setFloat(float f);

	bool getBool();
	void setBool(bool b);

	const char* getString();
	void getString(char *buffer, int max);
	void setString(const char* src);
    void addString(const char* src);

	class ExValueList* getList();
	class ExValueList* takeList();
	void setList(class ExValueList* l);
	void setOwnedList(class ExValueList* l);

	char* getBuffer();
	int getBufferMax();

	int compare(ExValue* other);
    void set(ExValue* src, bool owned);
	void set(ExValue* other);
	void setOwned(ExValue* other);

	void toString(class Vbuf* b);
	void dump();

  private:
    
    void releaseList();
	void copyList(class ExValueList* src, class ExValueList* dest);
	ExValue* getList(int index);

	int compareInt(ExValue *other);
	int compareFloat(ExValue *other);
	int compareBool(ExValue *other);
	int compareString(ExValue *other);

	ExType mType;
	int mInt;
	float mFloat;
	bool mBool;
	char mString[EX_MAX_STRING + 4];
	class ExValueList* mList;
};

//////////////////////////////////////////////////////////////////////
//
// ExValueList
//
//////////////////////////////////////////////////////////////////////

/**
 * A list of ExValues.
 *
 * These are a little weird because we don't have formal support
 * in the scripting languge for "pass by value" or "pass by reference".
 * Most things are pass by value, each ExValue has it's own char buffer
 * for strings.  But lists are more complicated, we generally want to 
 * use pass by reference so the reciever can modify the list.
 *
 * I don't like reference counting so we're going to try a marginally
 * more stable notion of a list "owner".  
 * 
 * The rules for referencing ExValueList in an ExValue
 *
 *   - setting a list in an ExValue sets the reference it does not copy
 * 
 *   - returning a list in a caller supplied ExValue returns a reference
 *     to the list, not a copy
 *
 *   - when an ExValue contains an ExValueList and the ExValue is deleted,
 *     the ExValueList is deleted only if the owner pointer in the
 *     ExValueList is equal to the ExValue address
 *
 *   - to transfer a list from one ExValue to another you can either
 *     use the copy() method or use takeList() that returns the list
 *     clears the owner pointer, and removes the reference from the 
 *     original ExValue
 * 
 * The rules for ExValueList elements are:
 *
 *   - deleting the list deletes the ExValues in it
 *   - adding or setting an ExValue takes ownership of the ExValue
 *   - if an ExValue in a list contains an ExValueList, the contained
 *     list is deleted when the parent list is deleted, 
 *     this means the contained ExValueList will have it's owner set
 *
 */
class ExValueList : public List {

  public:

	ExValueList();
	~ExValueList();

    // List overloads
    void deleteElement(void* o);
    void* copyElement(void* src);

    ExValue* getValue(int i);
    void* getOwner();
    void setOwner(void* v);

    ExValueList* copy();

  protected:

    void* mOwner;

};

/****************************************************************************
 *                                                                          *
 *   							   CONTEXT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The interface of an object that may resolve the value of a symbol.
 * These are requested of the ExContext for each symbol, and if returned
 * are cached for reuse every time the expression is evaluated.
 *
 * This contortion is done to keep Expr independent of Script in case
 * we want to use expressions in some other application.
 */
class ExResolver {
  public:

	virtual ~ExResolver() {}

	/**
	 * Get the value of a symbol or function call.
	 */
	virtual void getExValue(class ExContext* context, ExValue *value) = 0;
};

/**
 * The interface of an object that provides links to external symbols
 * and functions.
 */
class ExContext {
  public:

	virtual ~ExContext() {}

	/**
	 * Locate a resolver for a symbol reference.
	 * The expression evaluator will ensure that the ExSymbol remains
	 * valid so the ExResolver may cache a copy.
	 */
	virtual ExResolver* getExResolver(class ExSymbol* symbol) = 0;

	/**
	 * Locate a resolver for a function reference.
	 * The expression evaluator will ensure that the ExFunction remains
	 * valid so the ExResolver may cache a copy.
	 */
	virtual ExResolver* getExResolver(class ExFunction* function) = 0;

};

/****************************************************************************
 *                                                                          *
 *   								 NODE                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * The base class for all script nodes.
 * Nodes are fundamentally organized into trees.
 */
class ExNode {

  public:

	ExNode();
	~ExNode();

	ExNode* getNext();
	void setNext(ExNode* n);

	ExNode* getParent();
	void setParent(ExNode* n);
	
	ExNode* getChildren();
    ExNode* stealChildren();
	void setChildren(ExNode* n);
	void addChild(ExNode* n);
	void insertChild(ExNode* n, int i);
	int countChildren();
	
	// parsing
	virtual bool isParent();
	virtual bool isOperator();
	virtual bool isBlock();
	virtual bool isSymbol();
	virtual int getPrecedence();
    virtual int getDesiredOperands();

	bool hasPrecedence(ExNode* other);

	// runtime evaluation

	virtual void toString(class Vbuf* b);
	virtual void eval(ExContext* context, ExValue *value);

	int evalToInt(ExContext* context);
	bool evalToBool(ExContext* context);
	void evalToString(ExContext* context, char* buffer, int max);
    ExValueList* evalToList(ExContext * con);

  protected:
	
    ExNode* getLastChild(ExNode* first);

	void eval1(ExContext* context, ExValue* v1);
	void eval2(ExContext* context, ExValue* v1, ExValue* v2);
	void evaln(ExContext* context, ExValue* values, int max);

	ExNode* mNext;
	ExNode* mParent;
	ExNode* mChildren;

};

/****************************************************************************
 *                                                                          *
 *   							   LITERAL                                  *
 *                                                                          *
 ****************************************************************************/

class ExLiteral : public ExNode {

  public:

	ExLiteral(int i);
	ExLiteral(float f);
	ExLiteral(const char* str);

	void toString(class Vbuf* b);
	void eval(ExContext* context, ExValue *value);

  private:

	ExValue mValue;
};

/****************************************************************************
 *                                                                          *
 *   								SYMBOL                                  *
 *                                                                          *
 ****************************************************************************/

class ExSymbol : public ExNode {

  public:

	ExSymbol(const char* name);
	~ExSymbol();

	const char* getName();
	bool isSymbol();
	void toString(class Vbuf* b);
	void eval(ExContext* context, ExValue *value);

  private:

	char* mName;
	bool mResolved;
	ExResolver* mResolver;

};

/****************************************************************************
 *                                                                          *
 *   							   OPERATOR                                 *
 *                                                                          *
 ****************************************************************************/

class ExOperator : public ExNode {
  public:
	virtual const char* getOperator() = 0;
	bool isOperator();
	bool isParent();
	void toString(class Vbuf* b);
    virtual int getDesiredOperands();
};

/****************************************************************************
 *                                                                          *
 *   						 RELATIONAL OPERATORS                           *
 *                                                                          *
 ****************************************************************************/

class ExNot : public ExOperator {
  public:
	const char* getOperator();
	int getDesiredOperands();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExEqual : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExNotEqual : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExGreater : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExLess : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExGreaterEqual : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExLessEqual : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

/****************************************************************************
 *                                                                          *
 *   						 ARITHMETIC OPERATORS                           *
 *                                                                          *
 ****************************************************************************/

class ExAdd : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExSubtract : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExNegate : public ExOperator {
  public:
	const char* getOperator();
	int getDesiredOperands();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExMultiply : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExDivide : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExModulo : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

/****************************************************************************
 *                                                                          *
 *   						  LOGICAL OPERATORS                             *
 *                                                                          *
 ****************************************************************************/

class ExAnd : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

class ExOr : public ExOperator {
  public:
	const char* getOperator();
	int getPrecedence();
	void eval(ExContext* context, ExValue* value);
};

/****************************************************************************
 *                                                                          *
 *   								BLOCKS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A container of other nodes, the value of the block is the value
 * of the last node.  We don't actually have block syntax in the
 * language yet, but some of the subclasses do.  Leave support in place
 * in case we want to add multi-expression blocks where the first
 * expressions are evaluated for side effect.
 */
class ExBlock : public ExNode {
  public:
	ExBlock();
	virtual ~ExBlock(){}
	bool isParent();
	bool isBlock();
	int getPrecedence();

	virtual bool isFunction();
    virtual bool isParenthesis();
	virtual bool isList();
	virtual bool isArray();
	virtual bool isIndex();

    virtual void toString(class Vbuf* b);
	virtual void eval(ExContext* context, ExValue *value);
};

class ExParenthesis : public ExBlock {
  public:
    // C++ shit necessary for virtual overloads to work
	ExParenthesis(){}
	virtual ~ExParenthesis(){}
	bool isParenthesis();
	void toString(class Vbuf* b);
};
	
class ExFunction : public ExBlock {
  public:
	ExFunction(){}
	virtual ~ExFunction(){}
	virtual const char* getFunction() = 0;
	bool isFunction();
	void toString(class Vbuf* b);
};

class ExList : public ExBlock {
  public:
	ExList(){}
	virtual ~ExList(){}
    bool isList();
	void toString(class Vbuf* b);
    void eval(ExContext* context, ExValue* value);
};

/**
 * Not sure we need this, it is identical to ExList
 * except we render it differently after parasing and
 * force the delimiters to be a balanced [].
 */
class ExArray : public ExBlock {
  public:
	ExArray(){}
	virtual ~ExArray(){}
    bool isArray();
	void toString(class Vbuf* b);
    void eval(ExContext* context, ExValue* value);
};

class ExIndex : public ExBlock {
  public:
	ExIndex(){}
	virtual ~ExIndex(){}
    bool isIndex();
	void toString(class Vbuf* b);
    void eval(ExContext* context, ExValue* value);

	ExNode* getIndexes();
	void setIndexes(ExNode* n);
	void addIndex(ExNode* n);

  private:
    ExNode* mIndexes;

};

/****************************************************************************
 *                                                                          *
 *   							  FUNCTIONS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Cast a value to an int.
 */
class ExInt : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Cast a value to a float.
 */
class ExFloat : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Cast a value to a string.
 */
class ExString : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Generate a random integer between two given integers.
 *   rand(lowValue, highValue)
 */
class ExRand : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Take the absolute value of one integer.
 *   abs(value)
 */
class ExAbs : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Scale a value from one range to another.
 *   scale(value, low, high, newLow, newHigh)
 */
class ExScale : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/**
 * Reference to a custom function.
 */
class ExCustom : public ExFunction {
  public:
    ExCustom(const char* name);
    ~ExCustom();
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
  private:
    char* mName;
};

/**
 * Used for testing.
 *   foo(...)
 */
class ExFoo : public ExFunction {
  public:
	const char* getFunction();
	void eval(ExContext* context, ExValue* value);
};

/****************************************************************************
 *                                                                          *
 *   								PARSER                                  *
 *                                                                          *
 ****************************************************************************/

#define EX_MAX_TOKEN 1024
#define EX_MAX_ERROR_ARG 1024

#define OPERATOR_CHARS "!=<>+-*/%&|()"
#define SYMBOL_CHARS "_.$"

class ExParser {
  public:

	ExParser();
	~ExParser();

	ExNode* parse(const char* src);

	const char* getError();
	const char* getErrorArg();
	void printError();

  private:

	void deleteStack(ExNode* stack);
	void pushOperand(ExNode* n);
	void pushOperator(ExNode* n);
	ExNode* popOperator();
	ExNode* popOperand();
    bool isOperatorSatisfied();
	void shiftOperator();
    void deleteNode(ExNode* node);

	ExNode* nextToken();
	ExNode* nextTokenForReal();
	void nextChar();
	void toToken();
	ExNode* newOperator(const char* name);
	ExNode* newFunction(const char* name);

	const char* mError;
	char mErrorArg[EX_MAX_ERROR_ARG + 4];

	const char* mSource;
	int mSourcePsn;
	char mNext;
	char mToken[EX_MAX_TOKEN + 4];
	int mTokenPsn;

	ExNode* mOperands;
	ExNode* mOperators;
    ExNode* mCurrent;
	ExNode* mLast;
    ExNode* mLookahead;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
