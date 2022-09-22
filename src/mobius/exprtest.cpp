/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Tests for the expression parser and evaluator.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "util.h"
#include "vbuf.h"

#include "Expr.h"

typedef struct {
	const char* test;
	const char* result;
} Test;

Test ParseTests[] = {
	{"a + b", "+(a,b)"},
	{"a + b * c", "+(a,*(b,c))"},
	{"(a + b)", "+(a,b)"},
	{"(a + b) * (c + d)", "*(+(a,b),+(c,d))"},
	{"!a", "!(a)"},
	{"!a * b", "*(!(a),b)"},
	{"a > b && c < d", "&&(>(a,b),<(c,d))"},
	{"a && b || c", "||(&&(a,b),c)"},
	{"a < b || c > d && e <= f && g >= h", "&&(&&(||(<(a,b),>(c,d)),<=(e,f)),>=(g,h))"},
	{"a + b * c / d % e", "+(a,%(/(*(b,c),d),e))"},
	{"a", "a"},
	{"a_b", "a_b"},
	{"a.b", "a.b"},
	{"1", "i(1)"},
	{"1.2", "f(1.200000)"},
	{".5", "f(0.500000)"},
	{"1.2.3", "1.2.3"},
	{"\"a\"", "s(a)"},
	{"'a'", "s(a)"},
	{"\"a\\\"b\"", "s(a\"b)"},
	{"'a\\\"b'", "s(a\"b)"},
	{"a + \"bc\" + d", "+(+(a,s(bc)),d)"},
	{"abs(a)", "abs(a)"},
	{"rand(1,2)", "rand(i(1),i(2))"},
	{"scale(a,1,2,3,4)", "scale(a,i(1),i(2),i(3),i(4))"},
	{"a + abs(foo) * rand(1,2)", "+(a,*(abs(foo),rand(i(1),i(2))))"},
	{"$a", "$a"},
	{"a$b", "a$b"},
	{"-1", "i(-1)"},
	{"-(1)", "-(i(1))"},
	{"a-b", "-(a,b)"},
	{"(a)-b", "-(a,b)"},
	{"(a)-(b)", "-(a,b)"},
	{"(a)-1", "-(a,i(1))"},
	{"a-1", "-(a,i(1))"},
	{"a -1", "-(a,i(1))"},
	{"a- 1", "-(a,i(1))"},
	{"a--1", "-(a,i(-1))"},
	{"a- -1", "-(a,i(-1))"},
	{"a--b", "-(a,-(b))"},
    {"foo(a,b,c)", "foo(a,b,c)"},
    {"foo(a,-b,c)", "foo(a,-(b),c)"},
    // lists
    {"a,b,c", "list(a,b,c)"},
    {"a b c", "list(a,b,c)"},
    {"(a,b,c)", "list(a,b,c)"},
    {"(a b c)", "list(a,b,c)"},
    {"(a,(b,c))", "list(a,list(b,c))"},
    // this doesn't work because a is considered a function
    {"(a (b c))", "a(b,c)"},
    // but confusingly this one does
    {"(1 (2 3))", "list(i(1),list(i(2),i(3)))"},
    {"((a,b),c)", "list(list(a,b),c)"},
    {"((a b) c)", "list(list(a,b),c)"},
    {"a (a b) d e", "list(a(a,b),d,e)"},
    // not much thought behind this, just a strange one
    {"a+b*c,(1 a b c+d,(4/2,foo(a)) x) x z", 
     "list(+(a,*(b,c)),list(i(1),a,b,+(c,d),list(/(i(4),i(2)),foo(a)),x),x,z)"},
    // typical script usage
    {"mode == reset && track == 2 && autoRecord == false", 
     "&&(&&(==(mode,reset),==(track,i(2))),==(autoRecord,false))"},

    {"8 (1 2) ((1 .25) 2 (3 .25))", "?"},

	{NULL, NULL}
};

Test UnitParseTests[] = {
    //{"a,(1 a b c+d) d e", "?"},
    {"a (a b) d e", "?"},
	{NULL, NULL}
};

Test EvalTests[] = {
	{"1 + 2", "i(3)"},
	{"8 - 3", "i(5)"},
	{"1- -1", "i(2)"},
	{"1--1", "i(2)"},
	{"-(5)", "i(-5)"},
	{"8 * 3", "i(24)"},
	{"21 / 7", "i(3)"},
	{"4 % 3", "i(1)"},
	{"2 + 3 * 4", "i(14)"},
	{"0 && 1", "b(false)"},
	{"1 && 1", "b(true)"},
	{"0 || 0", "b(false)"},
	{"0 || 1", "b(true)"},
	{"!(0 || 0)", "b(true)"},
	{"!(0 || 1)", "b(false)"},
	{"!0 || 0", "b(true)"},
	{"2 * 12 / 3", "i(8)"},
	{"abs(1)", "i(1)"},
	{"abs(-2)", "i(2)"},
	{"i", "i(42)"},
	{"f", "f(123.000000)"},	// probably compiler specific
	{"b", "b(true)"},
	{"s", "s(a value)"},
	{"x", "null"},
    {"1,2,3", "[i(1),i(2),i(3)]"},
    {"1 2 3", "[i(1),i(2),i(3)]"},
    {"(1 (2 3))", "[i(1),[i(2),i(3)]]"},
    {"((1 2) (3 4) (5 6))", "[[i(1),i(2)],[i(3),i(4)],[i(5),i(6)]]"},
	{NULL, NULL}
};

Test UnitEvalTests[] = {
    {"1,2,3", "[i(1),i(2),i(3)]"},
	{NULL, NULL}
};

/**
 * ExResolver implementation with some hard coded values.
 */
class TestResolver : public ExResolver {
  public:

	TestResolver(ExSymbol* s) {
		mSymbol = s;
	}
	
	void getExValue(ExContext* context, ExValue* value) {

		const char* name = mSymbol->getName();
		if (name != NULL) {
			if (!strcmp(name, "i"))
			  value->setInt(42);
			else if (!strcmp(name, "f"))
			  value->setFloat(123.0f);
			else if (!strcmp(name, "b"))
			  value->setBool(true);
			else if (!strcmp(name, "s"))
			  value->setString("a value");
			else
			  value->setString(NULL);
		}
	}

  private:

	ExSymbol* mSymbol;
};

class TestContext : public ExContext {
  public:
	
	virtual ~TestContext() {}

	ExResolver* getExResolver(ExSymbol* symbol) {
		return new TestResolver(symbol);
	}

	ExResolver* getExResolver(ExFunction* function) {
		return NULL;
	}

};
	

void parse(const char* source, const char* expected)
{
	printf("Parsing: %s\n", source);

	ExParser* p = new ExParser();
	ExNode* node = p->parse(source);
	if (node == NULL)	
	  p->printError();
	else {
		Vbuf* buf = new Vbuf();
		node->toString(buf);
		const char* res = buf->getString();
		printf("Parsed: %s\n", res);

		if (expected != NULL) {
			if (res == NULL || strcmp(res, expected))
			  printf("!!!ERROR: expected %s\n", expected);
		}

		delete buf;
	}
}

void eval(const char* source, const char* expected)
{
	printf("Parsing: %s\n", source);

	ExParser* p = new ExParser();
	ExNode* node = p->parse(source);
	if (node == NULL)	
	  p->printError();
	else {
		Vbuf* buf = new Vbuf();
		node->toString(buf);
		const char* res = buf->getString();
		printf("Parsed: %s\n", res);

		ExContext* context = new TestContext();
		ExValue v;
		node->eval(context, &v);
		buf->clear();
		v.toString(buf);
		res = buf->getString();
		printf("Evaluated: %s\n", res);

		if (expected != NULL) {
			if (res == NULL || strcmp(res, expected))
			  printf("!!!ERROR: expected %s\n", expected);
		}

		delete buf;
		delete context;
	}
}

void parse(Test* tests)
{
    printf("-------- Parsing ------------------------------\n");
    for (int i = 0 ; tests[i].test != NULL ; i++) {
        printf("*** Parse %d ***\n", i + 1);
        parse(tests[i].test, tests[i].result);
    }
}

void eval(Test* tests)
{
    printf("-------- Evaluating ------------------------------\n");
    for (int i = 0 ; tests[i].test != NULL ; i++) {
        printf("*** Parse %d ***\n", i + 1);
        eval(tests[i].test, tests[i].result);
    }
}

int main(int argc, char** argv)
{
	if (argc == 1) {
	  printf("expr test | parse | eval | <expression>\n");
	}
	else if (!strcmp(argv[1], "test")) {
        parse(ParseTests);
        eval(EvalTests);
    }
	else if (!strcmp(argv[1], "unitparse")) {
        parse(UnitParseTests);
    }
	else if (!strcmp(argv[1], "uniteval")) {
        eval(UnitEvalTests);
    }
	else {
		Vbuf* buf = new Vbuf();
		for (int i = 1 ; i < argc ; i++) {
			buf->add(argv[i]);
			buf->add(" ");
		}
		parse(buf->getString(), NULL);
		delete buf;
	}

	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
