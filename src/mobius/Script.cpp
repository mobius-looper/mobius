/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Data model, compiler and interpreter for a simple scripting language.
 *
 * We've grown a collection of "Script Internal Variables" that are
 * similar to Parameters.    A few things are represented in both
 * places (LoopFrames, LoopCycles).
 *
 * I'm leaning toward moving most of the read-only "track parameters"
 * from being ParameterDefs to script variables.  They're easier to 
 * maintain and they're really only for use in scripts anyway.
 * 
 * SCRIPT COMPILATION
 * 
 * Compilation of scripts proceeds in these phases.
 * 
 * Parse
 *   The script file is parsed and a Script object is constructed.
 *   Parsing is mostly carried out in the constructors for each 
 *   statement class.  Some statements may choose to parse their
 *   argument lists, others save the arguments for parsing during the
 *   Link phase.
 * 
 * Resolve
 *   References within the script are resolved.  This includes matching
 *   block start/end statements (if/endif, for/next) and locating
 *   referenced functions, variables, and parameters.
 * 
 * Link
 *   Call references between scripts in the ScriptEnv are resolved.
 *   Some statements may do their expression parsing and variable
 *   resolution here too.  Included in this process is the construction
 *   of a new Function array including both static functions and scripts.
 * 
 * Export
 *   The new global Functions table built during the Link phase is installed.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <ctype.h>

#include "Expr.h"
#include "List.h"
#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Export.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "MobiusThread.h"
#include "Mode.h"
#include "Project.h"
#include "Recorder.h"
#include "Script.h"
#include "Synchronizer.h"
#include "Track.h"
#include "UserVariable.h"
#include "Variable.h"

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Notification labels.
 */
#define LABEL_REENTRY "reentry"
#define LABEL_SUSTAIN "sustain"
#define LABEL_END_SUSTAIN "endSustain"
#define LABEL_CLICK "click"
#define LABEL_END_CLICK "endClick"

/**
 * Default number of milliseconds in a "long press".
 */
#define DEFAULT_SUSTAIN_MSECS 200

/**
 * Default number of milliseconds we wait for a multi-click.
 */
#define DEFAULT_CLICK_MSECS 1000

/**
 * Names of wait types used in the script.  Order must
 * correspond to the WaitType enumeration.
 */
PUBLIC const char* WaitTypeNames[] = {
	"none",
	"last",
	"function",
	"event",
	"time",			// WAIT_RELATIVE
	"until",		// WAIT_ABSOLUTE
	"up",
	"long",
	"switch",
	"script",
	"block",
	"start",
	"end",
	"externalStart",
	"driftCheck",
	"pulse",
	"beat",
	"bar",
	"realign",
	"return",
	"thread",
	NULL
};

/**
 * Names of wait unites used in the script. Order must correspond
 * to the WaitUnit enumeration.
 */
PUBLIC const char* WaitUnitNames[] = {
	"none",
	"msec",
	"frame",
	"subcycle",
	"cycle",
	"loop",
	NULL
};

/****************************************************************************
 *                                                                          *
 *   							   RESOLVER                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC void ScriptResolver::init(ExSymbol* symbol)
{
	mSymbol = symbol;
    mStackArg = 0;
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;
}

PUBLIC ScriptResolver::ScriptResolver(ExSymbol* symbol, int arg)
{
	init(symbol);
    mStackArg = arg;
}

PUBLIC ScriptResolver::ScriptResolver(ExSymbol* symbol, ScriptInternalVariable* v)
{
	init(symbol);
    mInternalVariable = v;
}

PUBLIC ScriptResolver::ScriptResolver(ExSymbol* symbol, ScriptVariableStatement* v)
{
	init(symbol);
    mVariable = v;
}

PUBLIC ScriptResolver::ScriptResolver(ExSymbol* symbol, Parameter* p)
{
	init(symbol);
    mParameter = p;
}

PUBLIC ScriptResolver::ScriptResolver(ExSymbol* symbol, const char* name)
{
	init(symbol);
    mInterpreterVariable = name;
}

PUBLIC ScriptResolver::~ScriptResolver()
{
	// we don't own the symbol, it owns us
}

/**
 * Return the value of a resolved reference.
 * The ExContext passed here will be a ScriptInterpreter.
 */
PUBLIC void ScriptResolver::getExValue(ExContext* exContext, ExValue* value)
{
	// Here is the thing I hate about the interface.  We need to implement
	// a generic context, but when we eventually call back into ourselves
	// we have to downcast to our context.
	ScriptInterpreter* si = (ScriptInterpreter*)exContext;

	value->setNull();

    if (mStackArg > 0) {
		si->getStackArg(mStackArg, value);
    }
    else if (mInternalVariable != NULL) {
		mInternalVariable->getValue(si, value);
    }
	else if (mVariable != NULL) {
        UserVariables* vars = NULL;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
		switch (scope) {
			case SCRIPT_SCOPE_GLOBAL: {
				vars = si->getMobius()->getVariables();
			}
			break;
			case SCRIPT_SCOPE_TRACK: {
                vars = si->getTargetTrack()->getVariables();
			}
			break;
			default: {
				// maybe should be doing these on the ScriptStack instead?
                vars = si->getVariables();
			}
			break;
		}
        
        if (vars != NULL)
          vars->get(name, value);
	}
    else if (mParameter != NULL) {
        // reuse an export 
        Export* exp = si->getExport();

        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            exp->setTrack(NULL);
            mParameter->getValue(exp, value); 
        }
        else {
            exp->setTrack(si->getTargetTrack());
            mParameter->getValue(exp, value);
        }
    }
    else if (mInterpreterVariable != NULL) {
        UserVariables* vars = si->getVariables();
        if (vars != NULL)
          vars->get(mInterpreterVariable, value);
    }
    else {
		// if it didn't resolve, we shouldn't have made it
		Trace(1, "ScriptResolver::getValue unresolved!\n");
	}
}

/****************************************************************************
 *                                                                          *
 *                                 ARGUMENTS                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptArgument::ScriptArgument()
{
    mLiteral = NULL;
    mStackArg = 0;
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;
}

PUBLIC const char* ScriptArgument::getLiteral()
{
    return mLiteral;
}

PUBLIC void ScriptArgument::setLiteral(const char* lit) 
{
	mLiteral = lit;
}

PUBLIC Parameter* ScriptArgument::getParameter()
{
    return mParameter;
}

PUBLIC bool ScriptArgument::isResolved()
{
	return (mStackArg > 0 ||
			mInternalVariable != NULL ||
			mVariable != NULL ||
			mParameter != NULL);
}

/**
 * Script arguments may be literal values or references to stack arguments,
 * internal variables, local script variables, or parameters.
 * If it doesn't resolve it is left as a literal.
 */
PUBLIC void ScriptArgument::resolve(Mobius* m, ScriptBlock* block, 
                                    const char* literal)
{
	mLiteral = literal;
    mStackArg = 0;
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;

    if (mLiteral != NULL) {

		if (mLiteral[0] == '\'') {
			// kludge for a universal literal quoter until
			// we can figure out how to deal with parameter values
			// that are also the names of parameters, e.g. 
			// overdubMode=quantize
			mLiteral = &literal[1];
		}
		else {
			const char* ref = mLiteral;
			if (ref[0] == '$') {
				ref = &mLiteral[1];
				mStackArg = ToInt(ref);
			}
			if (mStackArg == 0) {

				mInternalVariable = ScriptInternalVariable::getVariable(ref);
				if (mInternalVariable == NULL) {
                    if (block == NULL)
                      Trace(1, "ScriptArgument::resolve has no block!\n");
                    else {
                        mVariable = block->findVariable(ref);
                        if (mVariable == NULL) {
                            mParameter = m->getParameter(ref);
                        }
                    }
                }
			}
		}
	}
}

/**
 * Retrieve the value of the argument to a buffer.
 *
 * !! This is exactly the same as ScriptResolver::getExValue, try to 
 * merge these.
 */
PUBLIC void ScriptArgument::get(ScriptInterpreter* si, ExValue* value)
{
	value->setNull();

    if (mStackArg > 0) {
		si->getStackArg(mStackArg, value);
    }
    else if (mInternalVariable != NULL) {
        mInternalVariable->getValue(si, value);
    }
	else if (mVariable != NULL) {
        UserVariables* vars = NULL;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
		switch (scope) {
			case SCRIPT_SCOPE_GLOBAL: {
				vars = si->getMobius()->getVariables();
			}
			break;
			case SCRIPT_SCOPE_TRACK: {
				vars = si->getTargetTrack()->getVariables();
			}
			break;
			default: {
				// maybe should be doing these on the ScriptStack instead?
				vars = si->getVariables();
			}
			break;
		}

        if (vars != NULL)
          vars->get(name, value);
	}
    else if (mParameter != NULL) {
        Export* exp = si->getExport();
        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            exp->setTrack(NULL);
            mParameter->getValue(exp, value); 
        }
        else {
            exp->setTrack(si->getTargetTrack());
            mParameter->getValue(exp, value);
        }
    }
    else if (mLiteral != NULL) { 
		value->setString(mLiteral);
    }
    else {
		// This can happen for function statements with variable args
		// but is usually an error for other statement types
        //Trace(1, "Attempt to get invalid reference\n");
    }
}

/**
 * Assign a value through a reference.
 * Not all references are writable.
 */
PUBLIC void ScriptArgument::set(ScriptInterpreter* si, ExValue* value)
{
    if (mStackArg > 0) {
        // you can't set stack args
        Trace(1, "Script %s: Attempt to set script stack argument %s\n", 
              si->getTraceName(), mLiteral);
    }
	else if (mInternalVariable != NULL) {
		mInternalVariable->setValue(si, value);
    }
	else if (mVariable != NULL) {
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        UserVariables* vars = NULL;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
        if (scope == SCRIPT_SCOPE_GLOBAL) {
			Trace(2, "Script %s: setting global variable %s = %s\n", 
                  si->getTraceName(), name, traceval);
            vars = si->getMobius()->getVariables();
        }
        else if (scope == SCRIPT_SCOPE_TRACK) {
			Trace(2, "Script %s: setting track variable %s = %s\n", 
                  si->getTraceName(), name, traceval);
			vars = si->getTargetTrack()->getVariables();
		}
		else {
			// maybe should be doing these on the ScriptStack instead?
			vars = si->getVariables();
		}
        
        if (vars != NULL)
          vars->set(name, value);
	}
	else if (mParameter != NULL) {
        const char* name = mParameter->getName();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        // can resuse this unless it schedules
        Action* action = si->getAction();
        if (mParameter->scheduled)
          action = si->getMobius()->cloneAction(action);

        action->arg.set(value);

        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            Trace(2, "Script %s: setting global parameter %s = %s\n",
                  si->getTraceName(), name, traceval);
            action->setResolvedTrack(NULL);
            mParameter->setValue(action);
        }
        else {
            Trace(2, "Script %s: setting track parameter %s = %s\n", 
                  si->getTraceName(), name, traceval);
            action->setResolvedTrack(si->getTargetTrack());
            mParameter->setValue(action);
        }

        if (mParameter->scheduled)
          si->getMobius()->completeAction(action);
	}
    else if (mLiteral != NULL) {
        Trace(1, "Script %s: Attempt to set unresolved reference %s\n", 
              si->getTraceName(), mLiteral);
    }
    else {
        Trace(1, "Script %s: Attempt to set invalid reference\n",
              si->getTraceName());
    }

}


/****************************************************************************
 *                                                                          *
 *                                DECLARATION                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptDeclaration::ScriptDeclaration(const char* name, const char* args)
{
    mNext = NULL;
    mName = CopyString(name);
    mArgs = CopyString(args);
}

PUBLIC ScriptDeclaration::~ScriptDeclaration()
{
    delete mName;
    delete mArgs;
}

PUBLIC ScriptDeclaration* ScriptDeclaration::getNext()
{
    return mNext;
}

PUBLIC void ScriptDeclaration::setNext(ScriptDeclaration* next)
{
    mNext = next;
}

PUBLIC const char* ScriptDeclaration::getName()
{
    return mName;
}

PUBLIC const char* ScriptDeclaration::getArgs()
{
    return mArgs;
}

/****************************************************************************
 *                                                                          *
 *                                   BLOCK                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptBlock::ScriptBlock()
{
    mParent = NULL;
    mName = NULL;
    mDeclarations = NULL;
	mStatements = NULL;
	mLast = NULL;
}

PUBLIC ScriptBlock::~ScriptBlock()
{
    // parent is not an ownership relationship, don't delete it

    delete mName;

	ScriptDeclaration* decl = NULL;
	ScriptDeclaration* nextDecl = NULL;
	for (decl = mDeclarations ; decl != NULL ; decl = nextDecl) {
		nextDecl = decl->getNext();
		delete decl;
	}

	ScriptStatement* stmt = NULL;
	ScriptStatement* nextStmt = NULL;

	for (stmt = mStatements ; stmt != NULL ; stmt = nextStmt) {
		nextStmt = stmt->getNext();
		delete stmt;
	}
}

PUBLIC ScriptBlock* ScriptBlock::getParent()
{
    return mParent;
}

PUBLIC void ScriptBlock::setParent(ScriptBlock* parent)
{
    mParent = parent;
}

PUBLIC const char* ScriptBlock::getName()
{
    return mName;
}

PUBLIC void ScriptBlock::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

PUBLIC ScriptDeclaration* ScriptBlock::getDeclarations()
{
    return mDeclarations;
}

PUBLIC void ScriptBlock::addDeclaration(ScriptDeclaration* decl)
{
    if (decl != NULL) {
        // order doesn't matter
        decl->setNext(mDeclarations);
        mDeclarations = decl;
    }
}

PUBLIC ScriptStatement* ScriptBlock::getStatements()
{
	return mStatements;
}

PUBLIC void ScriptBlock::add(ScriptStatement* a)
{
	if (a != NULL) {
		if (mLast == NULL) {
			mStatements = a;
			mLast = a;
		}
		else {
			mLast->setNext(a);
			mLast = a;
		}

        if (a->getParentBlock() != NULL)
          Trace(1, "ERROR: ScriptStatement already has a block!\n");
        a->setParentBlock(this);
	}
}

/**
 * Resolve referenes within the block.
 */
PUBLIC void ScriptBlock::resolve(Mobius* m)
{
    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext())
      s->resolve(m);
}

/**
 * Resolve calls to other scripts within this block.
 */
PUBLIC void ScriptBlock::link(ScriptCompiler* comp)
{
    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext())
	  s->link(comp);
}

/**
 * Search for a Variable declaration.
 * These are different than other block scoped things because
 * we also allow top-level script Variables to have global scope
 * within this script.  So if we don't find it within this block
 * we walk back up the block stack and look in the top block.
 * Intermediate blocks are not searched, if you want nested Procs
 * you need to pass arguments.  Could soften this?
 */
PUBLIC ScriptVariableStatement* ScriptBlock::findVariable(const char* name) {

    ScriptVariableStatement* found = NULL;

    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {

        if (s->isVariable()) {
            ScriptVariableStatement* v = (ScriptVariableStatement*)s;
            if (name == NULL || StringEqualNoCase(name, v->getName())) {
                found = v;
                break;
            }
        }
    }

    if (found == NULL) {
        ScriptBlock* top = mParent;
        while (top != NULL && top->getParent() != NULL)
          top = top->getParent();

        if (top != NULL)
          found = top->findVariable(name);
    }

    return found;
}

/**
 * Search for a Label statement.
 */
PUBLIC ScriptLabelStatement* ScriptBlock::findLabel(const char* name)
{
	ScriptLabelStatement* found = NULL;

	for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		if (s->isLabel()) {
			ScriptLabelStatement* l = (ScriptLabelStatement*)s;
			if (name == NULL || StringEqualNoCase(name, l->getArg(0))) {
                found = l;
                break;
            }
        }
    }
	return found;
}

/**
 * Search for a Proc statement.
 * These are like Variables, we can local Procs in the block (rare)
 * or script-global procs.
 */
PUBLIC ScriptProcStatement* ScriptBlock::findProc(const char* name)
{
	ScriptProcStatement* found = NULL;

	for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		if (s->isProc()) {
			ScriptProcStatement* p = (ScriptProcStatement*)s;
			if (name == NULL || StringEqualNoCase(name, p->getArg(0))) {
                found = p;
                break;
            }
        }
    }

    if (found == NULL) {
        ScriptBlock* top = mParent;
        while (top != NULL && top->getParent() != NULL)
          top = top->getParent();

        if (top != NULL)
          found = top->findProc(name);
    }

	return found;
}

/**
 * Search for the For/Repeat statement matching a Next.
 */
PUBLIC ScriptIteratorStatement* ScriptBlock::findIterator(ScriptNextStatement* next)
{
    ScriptIteratorStatement* found = NULL;

    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		// loops can be nested so find the nearest one that isn't already
		// paired with a next statement
        if (s->isIterator() && ((ScriptIteratorStatement*)s)->getEnd() == NULL)
		  found = (ScriptIteratorStatement*)s;
        else if (s == next)
		  break;
    }
    return found;
}

/**
 * Search for the statement ending an if/else clause.  Arguent may
 * be either an If or Else statement.  Return value will be either
 * an Else or Endif statement.
 */
PUBLIC ScriptStatement* ScriptBlock::findElse(ScriptStatement* start)
{
	ScriptStatement* found = NULL;
	int depth = 0;

	for (ScriptStatement* s = start->getNext() ; s != NULL && found == NULL ;
		 s = s->getNext()) {

		// test isElse first since isIf will also true
		if (s->isElse()) {
			if (depth == 0)
			  found = s;	
		}
		else if (s->isIf()) {
			depth++;
		}
		else if (s->isEndif()) {
			if (depth == 0)
			  found = s;
			else
			  depth--;
		}
	}

	return found;
}

/****************************************************************************
 *                                                                          *
 *   							  STATEMENT                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptStatement::ScriptStatement()
{
    mParentBlock = NULL;
	mNext = NULL;
	for (int i = 0 ; i < MAX_ARGS ; i++)
	  mArgs[i] = NULL;
}

PUBLIC ScriptStatement::~ScriptStatement()
{
	for (int i = 0 ; i < MAX_ARGS ; i++)
	  delete mArgs[i];
}

PUBLIC void ScriptStatement::setParentBlock(ScriptBlock* b)
{
    if (b == (ScriptBlock*)this)
      Trace(1, "ScriptStatement::setBlock circular reference!\n");
    else
      mParentBlock = b;
}

PUBLIC ScriptBlock* ScriptStatement::getParentBlock()
{
	return mParentBlock;
}

PUBLIC void ScriptStatement::setNext(ScriptStatement* a)
{
	mNext = a;
}

PUBLIC ScriptStatement* ScriptStatement::getNext()
{
	return mNext;
}

PUBLIC void ScriptStatement::setArg(const char* arg, int psn)
{
	delete mArgs[psn];
	mArgs[psn] = NULL;
	if (arg != NULL && strlen(arg) > 0)
	  mArgs[psn] = CopyString(arg);
}

PUBLIC const char* ScriptStatement::getArg(int psn)
{
	return mArgs[psn];
}

PUBLIC void ScriptStatement::setLineNumber(int i)
{
    mLineNumber = i;
}

PUBLIC int ScriptStatement::getLineNumber()
{
    return mLineNumber;
}

PUBLIC bool ScriptStatement::isVariable()
{
    return false;
}

PUBLIC bool ScriptStatement::isLabel()
{
    return false;
}

PUBLIC bool ScriptStatement::isIterator()
{
    return false;
}

PUBLIC bool ScriptStatement::isNext()
{
    return false;
}

PUBLIC bool ScriptStatement::isEnd()
{
    return false;
}

PUBLIC bool ScriptStatement::isBlock()
{
    return false;
}

PUBLIC bool ScriptStatement::isProc()
{
    return false;
}

PUBLIC bool ScriptStatement::isEndproc()
{
    return false;
}

PUBLIC bool ScriptStatement::isParam()
{
    return false;
}

PUBLIC bool ScriptStatement::isEndparam()
{
    return false;
}

PUBLIC bool ScriptStatement::isIf()
{
    return false;
}

PUBLIC bool ScriptStatement::isElse()
{
    return false;
}

PUBLIC bool ScriptStatement::isEndif()
{
    return false;
}

PUBLIC bool ScriptStatement::isFor()
{
    return false;
}

/**
 * Called after the script has been fully parsed.
 * Overloaded by the subclasses to resolve references to 
 * things within the script such as matching block statements
 * (if/endif, for/next) and variables.
 */
PUBLIC void ScriptStatement::resolve(Mobius* m)
{
}

/**
 * Called when the entire ScriptEnv has been loaded and the
 * scripts have been exported to the global function table.
 * Overloaded by the subclasses to resolve references between scripts.
 */
PUBLIC void ScriptStatement::link(ScriptCompiler* compiler)
{
}

/**
 * Serialize a statement.  Assuming we can just emit the original
 * arguments, don't need to noramlize.
 */
PUBLIC void ScriptStatement::xwrite(FILE* fp)
{
    fprintf(fp, "%s", getKeyword());

	for (int i = 0 ; mArgs[i] != NULL ; i++) 
      fprintf(fp, " %s", mArgs[i]);

	fprintf(fp, "\n");
}

/**
 * Parse the remainder of the function line into up to 8 arguments.
 */
PRIVATE void ScriptStatement::parseArgs(char* line)
{
	parseArgs(line, 0, 0);
}

/**
 * Parse the remainder of the function line into up to 8 arguments.
 * If a maximum argument count is given, return the remainder of the
 * line after that number of arguments has been located.
 *
 * Technically this should probably go in ScriptCompiler but it's
 * easier to use if we have it here.
 */
PRIVATE char* ScriptStatement::parseArgs(char* line, int argOffset, int toParse)
{
	if (line != NULL) {

        int max = MAX_ARGS;
        if (toParse > 0) {
            max = argOffset + toParse;
			if (max > MAX_ARGS)
              max = MAX_ARGS;
		}

		while (*line && argOffset < max) {
			bool quoted = false;

			// skip preceeding whitespace
			while (*line && isspace(*line)) line++;

			if (*line == '"') {
				quoted = true;
				line++;
			}

			if (*line) {
				char* token = line;
				if (quoted)
				  while (*line && *line != '"') line++;
				else
				  while (*line && !isspace(*line)) line++;
			
				bool more = (*line != 0);
				*line = 0;
				if (strlen(token)) {
                    mArgs[argOffset] = CopyString(token);
                    argOffset++;
				}
				if (more)
				  line++;
			}
		}
	}

	return line;
}

/****************************************************************************
 *                                                                          *
 *                                    ECHO                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptEchoStatement::ScriptEchoStatement(ScriptCompiler* comp,
												char* args)
{
    // unlike most other functions, this one doesn't tokenize args
    setArg(args, 0);
}

PUBLIC const char* ScriptEchoStatement::getKeyword()
{
    return "Echo";
}

PUBLIC ScriptStatement* ScriptEchoStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    // could send this to MobiusThread, but it's rare
    // and shouldn't be that expensive
    si->expand(mArgs[0], &v);

	char* msg = v.getBuffer();

	// add a newline so we can use it with OutputDebugStream
	// note that the buffer has extra padding on the end for the nul
	int len = strlen(msg);
	if (len < MAX_ARG_VALUE) {
		msg[len] = '\n';
		msg[len+1] = 0;
	}

	// since we're sending to the debug stream don't need to trace it too
    //Trace(3, "Script: echo %s\n", msg);

	// pass this off to the MobiusThread to keep it out of the interrupt
	//OutputDebugString(msg);
	//printf("%s", msg);
	//fflush(stdout);

	ThreadEvent* te = new ThreadEvent(TE_ECHO, msg);
	si->scheduleThreadEvent(te);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   							   MESSAGE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptMessageStatement::ScriptMessageStatement(ScriptCompiler* comp,
													  char* args)
{
    // unlike most other functions, this one doesn't tokenize args
    setArg(args, 0);
}

PUBLIC const char* ScriptMessageStatement::getKeyword()
{
    return "Message";
}

PUBLIC ScriptStatement* ScriptMessageStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    // could send this to MobiusThread, but it's rare
    // and shouldn't be that expensive
    si->expand(mArgs[0], &v);
	char* msg = v.getBuffer();

    Trace(3, "Script %s: message %s\n", si->getTraceName(), msg);

	Mobius* m = si->getMobius();
	m->addMessage(msg);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   								PROMPT                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptPromptStatement::ScriptPromptStatement(ScriptCompiler* comp,
                                                    char* args)
{
    // like echo, we'll assume that the remainder is the message
	// probably want to change this to support button configs?
    setArg(args, 0);
}

PUBLIC const char* ScriptPromptStatement::getKeyword()
{
    return "Prompt";
}

PUBLIC ScriptStatement* ScriptPromptStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    // could send this to MobiusThread, but it's rare
    // and shouldn't be that expensive
    si->expand(mArgs[0], &v);
	char* msg = v.getBuffer();

	ThreadEvent* te = new ThreadEvent(TE_PROMPT, msg);
	si->scheduleThreadEvent(te);

	// we always automatically wait for this
	si->setupWaitThread(this);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    END                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptEndStatement* ScriptEndStatement::Pseudo = 
new ScriptEndStatement(NULL, NULL);

PUBLIC ScriptEndStatement::ScriptEndStatement(ScriptCompiler* comp,
											  char* args)
{
}

PUBLIC const char* ScriptEndStatement::getKeyword()
{
    return "End";
}

PUBLIC bool ScriptEndStatement::isEnd()
{
    return true;
}

PUBLIC ScriptStatement* ScriptEndStatement::eval(ScriptInterpreter* si)
{
    Trace(2, "Script %s: end\n", si->getTraceName());
    return NULL;
}


/****************************************************************************
 *                                                                          *
 *   								CANCEL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Currently intended for use only in async notification threads, though
 * think more about this, could be used to cancel an iteration?
 *
 *    Cancel for, while, repeat
 *    Cancel loop
 *    Cancel iteration
 *    Break
 */
PUBLIC ScriptCancelStatement::ScriptCancelStatement(ScriptCompiler* comp,
													char* args)
{
    parseArgs(args);
	mCancelWait = StringEqualNoCase(mArgs[0], "wait");
}

PUBLIC const char* ScriptCancelStatement::getKeyword()
{
    return "Cancel";
}

PUBLIC ScriptStatement* ScriptCancelStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = NULL;

    Trace(2, "Script %s: cancel\n", si->getTraceName());

	if (mCancelWait) {
		// This only makes sense within a notification thread, in the main
		// thread we couldn't be in a wait state
		// !! Should we set a script local variable that can be tested
		// to tell if this happened?
		ScriptStack* stack = si->getStack();
		if (stack != NULL)
		  stack->cancelWaits();
	}
	else {
		// Cancel the entire script
		// I suppose it is ok to call this in the main thread, it will
		// behave like end
		si->reset();
		next = ScriptEndStatement::Pseudo;
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *                                 INTERRUPT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Alternative to Cancel that can interrupt other scripts.
 * With no argument it breaks out of a  Wait in this thread.  With an 
 * argument it attempts to find a thread running a script with that name
 * and cancels it.
 *
 * TODO: Might be nice to set a variable in the target script:
 *
 *     Interrrupt MyScript varname foo
 *
 * But then we'll have to treat the script name as a single string constant
 * if it has spaces:
 *
 *     Interrupt "Some Script" varname foo
 *
 */
PUBLIC ScriptInterruptStatement::ScriptInterruptStatement(ScriptCompiler* comp, 
												  char* args)
{
}

PUBLIC const char* ScriptInterruptStatement::getKeyword()
{
    return "Interrupt";
}

PUBLIC ScriptStatement* ScriptInterruptStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: interrupt\n", si->getTraceName());

    ScriptStack* stack = si->getStack();
    if (stack != NULL)
      stack->cancelWaits();

    // will this work without a declaration?
    UserVariables* vars = si->getVariables();
    if (vars != NULL) {
        ExValue v;
        v.setString("true");
        vars->set("interrupted", &v);
    }

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    SET                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptSetStatement::ScriptSetStatement(ScriptCompiler* comp, 
											  char* args)
{
	mExpression = NULL;

	// isolate the first argument representing the reference
	// to the thing to set, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args == NULL) {
        Trace(1, "Malformed set statement, missing arguments\n");
    }
    else {
        // ignore = between the name and initializer
        char* ptr = args;
        while (*ptr && isspace(*ptr)) ptr++;
        if (*ptr == '=') 
          args = ptr + 1;

        // defer this to link?
        mExpression = comp->parseExpression(this, args);
    }
}

PUBLIC ScriptSetStatement::~ScriptSetStatement()
{
	delete mExpression;
}

PUBLIC const char* ScriptSetStatement::getKeyword()
{
    return "Set";
}

PUBLIC void ScriptSetStatement::resolve(Mobius* m)
{
    mName.resolve(m, mParentBlock, mArgs[0]);
}

PUBLIC ScriptStatement* ScriptSetStatement::eval(ScriptInterpreter* si)
{
	if (mExpression != NULL) {
		ExValue v;
		mExpression->eval(si, &v);
		mName.set(si, &v);
	}
	return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    USE                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptUseStatement::ScriptUseStatement(ScriptCompiler* comp, 
											  char* args) :
    ScriptSetStatement(comp, args)
    
{
}

PUBLIC const char* ScriptUseStatement::getKeyword()
{
    return "Use";
}

PUBLIC ScriptStatement* ScriptUseStatement::eval(ScriptInterpreter* si)
{
    Parameter* p = mName.getParameter();
    if (p == NULL) {
        Trace(1, "ScriptUseStatement: Not a parameter: %s\n", 
              mName.getLiteral());
    }   
    else { 
        si->use(p);
    }

    return ScriptSetStatement::eval(si);
}

/****************************************************************************
 *                                                                          *
 *                                  VARIABLE                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptVariableStatement::ScriptVariableStatement(ScriptCompiler* comp,
														char* args)
{
	mScope = SCRIPT_SCOPE_SCRIPT;
	mName = NULL;
	mExpression = NULL;

	// isolate the scope identifier and variable name

	args = parseArgs(args, 0, 1);
	const char* arg = mArgs[0];

	if (StringEqualNoCase(arg, "global"))
		mScope = SCRIPT_SCOPE_GLOBAL;
	else if (StringEqualNoCase(arg, "track"))
		mScope = SCRIPT_SCOPE_TRACK;
	else if (StringEqualNoCase(arg, "script"))
		mScope = SCRIPT_SCOPE_SCRIPT;
	else {
		// if not one of the keywords assume the name
		mName = arg;
	}

	if (mName == NULL) {
		// first arg was the scope, parse another
		args = parseArgs(args, 0, 1);
		mName = mArgs[0];
	}

	// ignore = between the name and initializer
    if (args == NULL) {
        Trace(1, "Malformed Variable statement: missing arguments\n");
    }
    else {
        char* ptr = args;
        while (*ptr && isspace(*ptr)) ptr++;
        if (*ptr == '=') 
          args = ptr + 1;

        // the remainder is the initialization expression

        mExpression = comp->parseExpression(this, args);
    }
}

PUBLIC ScriptVariableStatement::~ScriptVariableStatement()
{
	delete mExpression;
}

PUBLIC const char* ScriptVariableStatement::getKeyword()
{
    return "Variable";
}

PUBLIC bool ScriptVariableStatement::isVariable()
{
    return true;
}

PUBLIC const char* ScriptVariableStatement::getName()
{
	return mName;
}

PUBLIC ScriptVariableScope ScriptVariableStatement::getScope()
{
	return mScope;
}

/**
 * These will have the side effect of initializing the variable, depending
 * on the scope.  For variables in global and track scope, the initialization
 * expression if any is run only if there is a null value.  For script scope
 * the initialization expression is run every time.
 *
 * Hmm, if we run global/track expressions on non-null it means
 * that we can never set a global to null. 
 */
PUBLIC ScriptStatement* ScriptVariableStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: Variable %s\n", si->getTraceName(), mName);

	if (mName != NULL && mExpression != NULL) {
        
        UserVariables* vars = NULL;
        // sigh, don't have a trace sig that takes 3 strings 
        const char* tracemsg = NULL;

		switch (mScope) {
			case SCRIPT_SCOPE_GLOBAL: {
                vars = si->getMobius()->getVariables();
                tracemsg = "Script %s: initializing global variable %s = %s\n";
			}
			break;
			case SCRIPT_SCOPE_TRACK: {
                vars = si->getTargetTrack()->getVariables();
                tracemsg = "Script %s: initializing track variable %s = %s\n";
            }
			break;
			case SCRIPT_SCOPE_SCRIPT: {
                vars = si->getVariables();
                tracemsg = "Script %s: initializing script variable %s = %s\n";
			}
            break;
		}

        if (vars == NULL)  {
            Trace(1, "Script %s: Invalid variable scope!\n", si->getTraceName());
        }
        else if (mScope == SCRIPT_SCOPE_SCRIPT || !vars->isBound(mName)) {
            // script scope vars always initialize
            ExValue value;
            mExpression->eval(si, &value);

            Trace(2, tracemsg, si->getTraceName(), mName, value.getString());
            vars->set(mName, &value);
        }
	}

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   							 CONDITIONAL                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptConditionalStatement::ScriptConditionalStatement()
{
	mCondition = NULL;
}

PUBLIC ScriptConditionalStatement::~ScriptConditionalStatement()
{
	delete mCondition;
}

PUBLIC bool ScriptConditionalStatement::evalCondition(ScriptInterpreter* si)
{
	bool value = false;

	if (mCondition != NULL) {
		value = mCondition->evalToBool(si);
	}
	else {
		// unconditional
		value = true;
	}

	return value;
}

/****************************************************************************
 *                                                                          *
 *                                    JUMP                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptJumpStatement::ScriptJumpStatement(ScriptCompiler* comp, 
												char* args)
{
    mStaticLabel = NULL;

	// the label
	args = parseArgs(args, 0, 1);
    
    if (args == NULL) {
        Trace(1, "Malformed Jump statement: missing arguments\n");
    }
    else {
        // then the condition
        mCondition = comp->parseExpression(this, args);
    }
}

PUBLIC const char* ScriptJumpStatement::getKeyword()
{
    return "Jump";
}

PUBLIC void ScriptJumpStatement::resolve(Mobius* m)
{
    // try to resolve it to to a variable or stack arg for dynamic
    // jump labels
    mLabel.resolve(m, mParentBlock, mArgs[0]);
    if (!mLabel.isResolved()) {

        // a normal literal reference, try to find it now
        mStaticLabel = mParentBlock->findLabel(mLabel.getLiteral());
    }
}

PUBLIC ScriptStatement* ScriptJumpStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;
	ExValue v;

	mLabel.get(si, &v);
	const char* label = v.getString();

	Trace(3, "Script %s: Jump %s\n", si->getTraceName(), label);

	if (evalCondition(si)) {
		if (mStaticLabel != NULL)
		  next = mStaticLabel;
		else {
			// dynamic resolution
            if (mParentBlock != NULL)
              next = mParentBlock->findLabel(label);

			if (next == NULL) {
				// halt when this happens or ignore?
				Trace(1, "Script %s: unresolved jump label %s\n", 
                      si->getTraceName(), label);
			}
		}
    }

    return next;
}

/****************************************************************************
 *                                                                          *
 *   							IF/ELSE/ENDIF                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptIfStatement::ScriptIfStatement(ScriptCompiler* comp, 
											char* args)
{
    mElse = NULL;

	// ignore the first token if it is "if", it is a common error to
	// use "else if" rather than "else if"
	if (args != NULL) {
		while (*args && isspace(*args)) args++;
		if (StartsWithNoCase(args, "if "))
		  args += 3;
	}

	mCondition = comp->parseExpression(this, args);
}

PUBLIC const char* ScriptIfStatement::getKeyword()
{
    return "If";
}

PUBLIC bool ScriptIfStatement::isIf()
{
	return true;
}

PUBLIC ScriptStatement* ScriptIfStatement::getElse()
{
	return mElse;
}

PUBLIC void ScriptIfStatement::resolve(Mobius* m)
{
    // search for matching else/elseif/endif
    mElse = mParentBlock->findElse(this);
}

PUBLIC ScriptStatement* ScriptIfStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;
	ScriptIfStatement* clause = this;

	Trace(3, "Script %s: %s\n", si->getTraceName(), getKeyword());

	if (isElse()) {
		// Else conditionals are processed by the original If statement,
		// if we get here, we're skipping over the other clauses after
		// one of them has finished
		next = mElse;
	}
	else {
		// keep jumping through clauses until we can enter one
		while (next == NULL && clause != NULL) {
			if (clause->evalCondition(si)) {
				next = clause->getNext();
				if (next == NULL) {
					// malformed, don't infinite loop
                    Trace(1, "Script %s: ScriptIfStatement: malformed clause\n", si->getTraceName());
					next = ScriptEndStatement::Pseudo;
				}
			}
			else {
				ScriptStatement* nextClause = clause->getElse();
				if (nextClause == NULL) {
					// malformed
                    Trace(1, "Script %s: ScriptIfStatement: else or missing endif\n", si->getTraceName());
					next = ScriptEndStatement::Pseudo;
				}
				else if (nextClause->isIf()) {
					// try this one
					clause = (ScriptIfStatement*)nextClause;
				}
				else {
					// must be an endif
					next = nextClause;
				}
			}
		}
	}

    return next;
}

PUBLIC ScriptElseStatement::ScriptElseStatement(ScriptCompiler* comp, 
												char* args) : 
	ScriptIfStatement(comp, args)
{
}

PUBLIC const char* ScriptElseStatement::getKeyword()
{
    return (mCondition != NULL) ? "Elseif" : "Else";
}

PUBLIC bool ScriptElseStatement::isElse()
{
	return true;
}

PUBLIC ScriptEndifStatement::ScriptEndifStatement(ScriptCompiler* comp, 
												  char* args)
{
}

PUBLIC const char* ScriptEndifStatement::getKeyword()
{
    return "Endif";
}

PUBLIC bool ScriptEndifStatement::isEndif()
{
	return true;
}

/**
 * When we finally get here, just go to the next one after it.
 */
PUBLIC ScriptStatement* ScriptEndifStatement::eval(ScriptInterpreter* si)
{
	return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                   LABEL                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptLabelStatement::ScriptLabelStatement(ScriptCompiler* comp, 
												  char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptLabelStatement::getKeyword()
{
    return "Label";
}

PUBLIC bool ScriptLabelStatement::isLabel()
{   
    return true;
}

PUBLIC bool ScriptLabelStatement::isLabel(const char* name)
{
	return StringEqualNoCase(name, getArg(0));
}

PUBLIC ScriptStatement* ScriptLabelStatement::eval(ScriptInterpreter* si)
{
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   							   ITERATOR                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptIteratorStatement::ScriptIteratorStatement()
{
	mEnd = NULL;
	mExpression = NULL;
}

PUBLIC ScriptIteratorStatement::~ScriptIteratorStatement()
{
	delete mExpression;
}

/**
 * Rather than try to resolve the corresponding Next statement, let
 * the Next resolve find us.
 */
PUBLIC void ScriptIteratorStatement::setEnd(ScriptNextStatement* next)
{
	mEnd = next;
}

PUBLIC ScriptNextStatement* ScriptIteratorStatement::getEnd()
{
	return mEnd;
}

PUBLIC bool ScriptIteratorStatement::isIterator()
{
	return true;
}

/****************************************************************************
 *                                                                          *
 *                                    FOR                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptForStatement::ScriptForStatement(ScriptCompiler* comp, 
											  char* args)
{
	// there is only oen arg, let it have spaces
	// !!! support expressions?

    setArg(args, 0);
}

PUBLIC const char* ScriptForStatement::getKeyword()
{
    return "For";
}

PUBLIC bool ScriptForStatement::isFor()
{
	return true;
}

/**
 * Initialize the track target list for a FOR statement.
 * There can only be one of these active a time (no nesting).
 * If you try that, the second one takes over and the outer one
 * will complete.
 *
 * To support nesting iteratation state is maintained on a special
 * stack frame to represent a "block" rather than a call.
 */
PUBLIC ScriptStatement* ScriptForStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = NULL;
    Mobius* m = si->getMobius();
	int trackCount = m->getTrackCount();
	ExValue v;

	// push a block frame to hold iteration state
	ScriptStack* stack = si->pushStack(this);

	// this one needs to be recursively expanded at runtime
	si->expand(mArgs[0], &v);
	const char* forspec = v.getString();

	Trace(3, "Script %s: For %s\n", si->getTraceName(), forspec);
	// it's a common error to have trailing spaces so use StartsWith
	if (strlen(forspec) == 0 ||
        StartsWithNoCase(forspec, "all") ||
        StartsWithNoCase(forspec, "*")) {

		for (int i = 0 ; i < trackCount ; i++)
		  stack->addTrack(m->getTrack(i));
	}
	else if (StartsWithNoCase(forspec, "focused")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
			if (t->isFocusLock() || t == m->getTrack())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "muted")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
			Loop* l = t->getLoop();
			if (l->isMuteMode())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "playing")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
			Loop* l = t->getLoop();
			if (!l->isReset() && !l->isMuteMode())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "group")) {
		int group = ToInt(&forspec[5]);
		if (group > 0) {
			// assume for now that tracks can't be in more than one group
			// could do that with a bit mask if necessary
			for (int i = 0 ; i < trackCount ; i++) {
				Track* t = m->getTrack(i);
				if (t->getGroup() == group)
				  stack->addTrack(t);
			}
		}
	}
	else if (StartsWithNoCase(forspec, "outSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getOutSyncMaster();
        if (t != NULL)
          stack->addTrack(t);
	}
	else if (StartsWithNoCase(forspec, "trackSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getTrackSyncMaster();
        if (t != NULL)
          stack->addTrack(t);
	}
	else {
		char number[MIN_ARG_VALUE];
		int max = strlen(forspec);
		int digits = 0;

		for (int i = 0 ; i <= max ; i++) {
			char ch = forspec[i];
			if (ch != 0 && isdigit(ch))
			  number[digits++] = ch;
			else if (digits > 0) {
				number[digits] = 0;
				int tracknum = ToInt(number) - 1;
				Track* t = m->getTrack(tracknum);
				if (t != NULL)
				  stack->addTrack(t);
				digits = 0;
			}
		}
	}

	// if nothing was added, then skip it
	if (stack->getMax() == 0) {
		si->popStack();
		if (mEnd != NULL)
		  next = mEnd->getNext();

		if (next == NULL) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

/**
 * Called by the ScriptNextStatement evaluator.  
 * Advance to the next track if we can.
 */
PUBLIC bool ScriptForStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == NULL) {
		Trace(1, "Script %s: For lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		Trace(1, "Script %s: For mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else {
		Track* nextTrack = stack->nextTrack();
		if (nextTrack != NULL)
		  Trace(3, "Script %s: For track %ld\n", 
                si->getTraceName(), (long)nextTrack->getDisplayNumber());
		else {
			Trace(3, "Script %s: end of For\n", si->getTraceName());
			done = true;
		}
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *   								REPEAT                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptRepeatStatement::ScriptRepeatStatement(ScriptCompiler* comp, 
													char* args)
{
	mExpression = comp->parseExpression(this, args);
}

PUBLIC const char* ScriptRepeatStatement::getKeyword()
{
    return "Repeat";
}

/**
 * Assume for now that we an only specify a number of repetitions
 * e.g. "Repeat 2" for 2 repeats.  Eventually could have more flexible
 * iteration ranges like "Repeat 4 8" meaning iterate from 4 to 8 by 1, 
 * but I can't see a need for that yet.
 */
PUBLIC ScriptStatement* ScriptRepeatStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = NULL;
	char spec[MIN_ARG_VALUE + 4];
	strcpy(spec, "");

	if (mExpression != NULL)
	  mExpression->evalToString(si, spec, MIN_ARG_VALUE);

	Trace(3, "Script %s: Repeat %s\n", si->getTraceName(), spec);

	int count = ToInt(spec);
	if (count > 0) {
		// push a block frame to hold iteration state
		ScriptStack* stack = si->pushStack(this);
		stack->setMax(count);
	}
	else {
		// Invalid repetition count or unresolved variable, treat
		// this like an If with a false condition
		if (mEnd != NULL)
		  next = mEnd->getNext();

 		if (next == NULL) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

PUBLIC bool ScriptRepeatStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == NULL) {
		Trace(1, "Script %s: Repeat lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		// this isn't ours!
		Trace(1, "Script %s: Repeat mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else {
		done = stack->nextIndex();
		if (done)
		  Trace(3, "Script %s: end of Repeat\n", si->getTraceName());
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *   								WHILE                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptWhileStatement::ScriptWhileStatement(ScriptCompiler* comp, 
												  char* args)
{
	mExpression = comp->parseExpression(this, args);
}

PUBLIC const char* ScriptWhileStatement::getKeyword()
{
    return "While";
}

/**
 * Assume for now that we an only specify a number of repetitions
 * e.g. "While 2" for 2 repeats.  Eventually could have more flexible
 * iteration ranges like "While 4 8" meaning iterate from 4 to 8 by 1, 
 * but I can't see a need for that yet.
 */
PUBLIC ScriptStatement* ScriptWhileStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = NULL;

	if (mExpression != NULL && mExpression->evalToBool(si)) {

		// push a block frame to hold iteration state
		ScriptStack* stack = si->pushStack(this);
	}
	else {
		// while condition started off bad, just bad
		// treat this like an If with a false condition
		if (mEnd != NULL)
		  next = mEnd->getNext();

 		if (next == NULL) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

PUBLIC bool ScriptWhileStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == NULL) {
		Trace(1, "Script %s: While lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		// this isn't ours!
		Trace(1, "Script %s: While mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (mExpression == NULL) {
		// shouldn't have bothered by now
		Trace(1, "Script %s: While without conditional expression!\n",
              si->getTraceName());
		done = true;
	}
	else {
		done = !mExpression->evalToBool(si);
		if (done)
		  Trace(3, "Script %s: end of While\n", si->getTraceName());
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *                                    NEXT                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptNextStatement::ScriptNextStatement(ScriptCompiler* comp, 
												char* args)
{
    mIterator = NULL;
}

PUBLIC const char* ScriptNextStatement::getKeyword()
{
    return "Next";
}

PUBLIC bool ScriptNextStatement::isNext()
{
    return true;
}

PUBLIC void ScriptNextStatement::resolve(Mobius* m)
{
    // locate the nearest For/Repeat statement
    mIterator = mParentBlock->findIterator(this);

	// iterators don't know how to resolve the next, so tell it
	if (mIterator != NULL)
	  mIterator->setEnd(this);
}

PUBLIC ScriptStatement* ScriptNextStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;

	if (mIterator == NULL) {
		// unmatched next, ignore
	}
	else if (!mIterator->isDone(si)) {
		next = mIterator->getNext();
	}
	else {
		// we should have an iteration frame on the stack, pop it
		ScriptStack* stack = si->getStack();
		if (stack != NULL && stack->getIterator() == mIterator)
		  si->popStack();
		else {
			// odd, must be a mismatched next?
			Trace(1, "Script %s: Next no iteration frame!\n",
                  si->getTraceName());
		}
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *   								SETUP                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptSetupStatement::ScriptSetupStatement(ScriptCompiler* comp,	
												  char* args)
{
    // This needs to take the entire argument list as a literal string
    // so we can have spaces in the setup name.
    // !! need to trim
    setArg(args, 0);
}

PUBLIC const char* ScriptSetupStatement::getKeyword()
{
    return "Setup";
}

PUBLIC void ScriptSetupStatement::resolve(Mobius* m)
{
	mSetup.resolve(m, mParentBlock, mArgs[0]);
}

PUBLIC ScriptStatement* ScriptSetupStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	mSetup.get(si, &v);
	const char* name = v.getString();

    Trace(2, "Script %s: Setup %s\n", si->getTraceName(), name);

    Mobius* m = si->getMobius();
    MobiusConfig* config = m->getInterruptConfiguration();
	Setup* s = config->getSetup(name);

    // if a name lookup didn't work it may be a number, 
    // these will be zero based!!
    if (s == NULL)
      s = config->getSetup(ToInt(name));

    if (s != NULL) {
        // special interface for us to avoid queueing for the next interrupt
		m->setSetupInternal(s);
	}

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                   PRESET                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptPresetStatement::ScriptPresetStatement(ScriptCompiler* comp,
													char* args)
{
    // This needs to take the entire argument list as a literal string
    // so we can have spaces in the preset name.
    // !! need to trim
    setArg(args, 0);
}

PUBLIC const char* ScriptPresetStatement::getKeyword()
{
    return "Preset";
}

PUBLIC void ScriptPresetStatement::resolve(Mobius* m)
{
	mPreset.resolve(m, mParentBlock, mArgs[0]);
}

PUBLIC ScriptStatement* ScriptPresetStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	mPreset.get(si, &v);
	const char* name = v.getString();
    Trace(2, "Script %s: Preset %s\n", si->getTraceName(), name);

    Mobius* m = si->getMobius();
    MobiusConfig* config = m->getInterruptConfiguration();
    Preset* p = config->getPreset(name);

    // if a name lookup didn't work it may be a number, 
    // these will be zero based!
    if (p == NULL)
      p = config->getPreset(ToInt(name));

    if (p != NULL) {
		Track* t = si->getTargetTrack();
		if (t == NULL)
		  t = m->getTrack();

		// note that since we're in a script, we can set it immediately,
		// this is necessary if we have set statements immediately
		// following this that depend on the preset change
        t->setPreset(p);
    }

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                              UNIT TEST SETUP                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptUnitTestSetupStatement::ScriptUnitTestSetupStatement(ScriptCompiler* comp, 
                                                                  char* args)
{
}

PUBLIC const char* ScriptUnitTestSetupStatement::getKeyword()
{
    return "UnitTestSetup";
}

PUBLIC ScriptStatement* ScriptUnitTestSetupStatement::eval(ScriptInterpreter* si)
{
    Trace(2, "Script %s: UnitTestSetup\n", si->getTraceName());
    Mobius* m = si->getMobius();
    m->unitTestSetup();
    return NULL;
}

/**
 * An older function, shouldn't be using this any more!
 */
PUBLIC ScriptInitPresetStatement::ScriptInitPresetStatement(ScriptCompiler* comp, 
															char* args)
{
}

PUBLIC const char* ScriptInitPresetStatement::getKeyword()
{
    return "InitPreset";
}

/**
 * !! This doesn't fit with the new model for editing configurations.
 */
PUBLIC ScriptStatement* ScriptInitPresetStatement::eval(ScriptInterpreter* si)
{
    Trace(2, "Script %s: InitPreset\n", si->getTraceName());

    // Not sure if this is necessary but we always started with the preset
    // from the active track then set it in the track specified in the
    // ScriptContext, I would expect them to be the same...

    Mobius* m = si->getMobius();
    Track* srcTrack = m->getTrack();
    Preset* p = srcTrack->getPreset();
    p->reset();

	// propagate this immediately to the track (avoid a pending preset)
	// so we can start calling set statements  
	Track* destTrack = si->getTargetTrack();
	if (destTrack == NULL)
	  destTrack = srcTrack;
    else if (destTrack != srcTrack)
      Trace(1, "Script %s: ScriptInitPresetStatement: Unexpected destination track\n",
            si->getTraceName());

    destTrack->setPreset(p);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                   BREAK                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * This is used to set flags that will enable code paths where debugger
 * breakpoints may have been set.  Loop has it's own internal field
 * that it monitors, we also have a global ScriptBreak that can be used
 * elsewhere.
 */

PUBLIC bool ScriptBreak = false;

PUBLIC ScriptBreakStatement::ScriptBreakStatement(ScriptCompiler* comp, 
												  char* args)
{
}

PUBLIC const char* ScriptBreakStatement::getKeyword()
{
    return "Break";
}

PUBLIC ScriptStatement* ScriptBreakStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: break\n", si->getTraceName());
	ScriptBreak = true;
    Loop* loop = si->getTargetTrack()->getLoop();
    loop->setBreak(true);
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    LOAD                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptLoadStatement::ScriptLoadStatement(ScriptCompiler* comp,
												char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptLoadStatement::getKeyword()
{
    return "Load";
}

PUBLIC ScriptStatement* ScriptLoadStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	si->expandFile(mArgs[0], &v);
	const char* file = v.getString();

	Trace(2, "Script %s: load %s\n", si->getTraceName(), file);
	ThreadEvent* te = new ThreadEvent(TE_LOAD, file);
	si->scheduleThreadEvent(te);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    SAVE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptSaveStatement::ScriptSaveStatement(ScriptCompiler* comp,
												char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptSaveStatement::getKeyword()
{
    return "Save";
}

PUBLIC ScriptStatement* ScriptSaveStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	si->expandFile(mArgs[0], &v);
	const char* file = v.getString();

    Trace(2, "Script %s: save %s\n", si->getTraceName(), file);

    if (strlen(file) > 0) {
		ThreadEvent* e = new ThreadEvent(TE_SAVE_PROJECT, file);
		si->scheduleThreadEvent(e);
    }

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    DIFF                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptDiffStatement::ScriptDiffStatement(ScriptCompiler* comp,
												char* args)
{
	mAudio = false;
	mReverse = false;
    parseArgs(args);

	if (StringEqualNoCase(mArgs[0], "audio"))
	  mAudio = true;
	else if (StringEqualNoCase(mArgs[0], "reverse")) {
		mAudio = true;
		mReverse = true;
	}
}

PUBLIC const char* ScriptDiffStatement::getKeyword()
{
    return "Diff";
}

PUBLIC ScriptStatement* ScriptDiffStatement::eval(ScriptInterpreter* si)
{
	ExValue file1;
	ExValue file2;
	int firstarg = (mAudio) ? 1 : 0;

	si->expandFile(mArgs[firstarg], &file1);
	si->expandFile(mArgs[firstarg + 1], &file2);
    Trace(2, "Script %s: diff %s %s\n", si->getTraceName(), 
          file1.getString(), file2.getString());

	ThreadEventType event = (mAudio) ? TE_DIFF_AUDIO : TE_DIFF;
    ThreadEvent* e = new ThreadEvent(event, file1.getString());
    e->setArg(1, file2.getString());
	if (mReverse)
	  e->setArg(2, "reverse");

    si->scheduleThreadEvent(e);

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    CALL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Leave the arguments raw and resolve then dynamically at runtime.
 * Could be smarter about this, but most of the time the arguments
 * are used to build file paths and need dynamic expansion.
 */
PUBLIC ScriptCallStatement::ScriptCallStatement(ScriptCompiler* comp,
												char* args)
{
	mProc = NULL;
	mScript = NULL;
    mExpression = NULL;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != NULL)
      mExpression = comp->parseExpression(this, args);
}

PUBLIC const char* ScriptCallStatement::getKeyword()
{
    return "Call";
}

/**
 * Start by resolving within the script.
 * If we don't find a proc, then later during link()
 * we'll look for other scripts.
 */
PUBLIC void ScriptCallStatement::resolve(Mobius* m)
{
	// think locally, then globally
	mProc = mParentBlock->findProc(mArgs[0]);
	
    // TODO: I don't like deferring resolution within
    // the ExNode until the first evaluation.  Find a way to do at least
    // most of them now.
}

/**
 * Resolve a call to another script in the environment.
 */
PUBLIC void ScriptCallStatement::link(ScriptCompiler* comp)
{
	if (mProc == NULL && mScript == NULL) {

		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == NULL)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

PUBLIC ScriptStatement* ScriptCallStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;

	if (mProc != NULL) {    
        ScriptBlock* block = mProc->getChildBlock();
        if (block != NULL && block->getStatements() != NULL) {
            // evaluate the argument list
            // !! figure out a way to pool ExNodes with ExValueLists 
            // in ScriptStack 
            
            ExValueList* args = NULL;
            if (mExpression != NULL)
              args = mExpression->evalToList(si);

            si->pushStack(this, si->getScript(), mProc, args);
            next = block->getStatements();
        }
	}
	else if (mScript != NULL) {

        ScriptBlock* block = mScript->getBlock();
        if (block != NULL && block->getStatements() != NULL) {

            // !! have to be careful with autoload from another "thread"
            // if we have a call in progress, need a reference count or 
            // something on the Script

            ExValueList* args = NULL;
            if (mExpression != NULL)
              args = mExpression->evalToList(si);

            si->pushStack(this, mScript, NULL, args);
            // and start executing the child script
            next = block->getStatements();
        }
	}
	else {
		Trace(1, "Script %s: Unresolved call: %s\n", si->getTraceName(), mArgs[0]);
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *                                   START                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A variant of Call that only does scripts, and launches them
 * in a parallel thread.
 */
PUBLIC ScriptStartStatement::ScriptStartStatement(ScriptCompiler* comp,
												char* args)
{
	mScript = NULL;
    mExpression = NULL;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != NULL)
      mExpression = comp->parseExpression(this, args);
}

PUBLIC const char* ScriptStartStatement::getKeyword()
{
    return "Start";
}

/**
 * Find the referenced script.
 */
PUBLIC void ScriptStartStatement::link(ScriptCompiler* comp)
{
	if (mScript == NULL) {
		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == NULL)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

PUBLIC ScriptStatement* ScriptStartStatement::eval(ScriptInterpreter* si)
{
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                  BLOCKING                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptBlockingStatement::ScriptBlockingStatement()
{
    mChildBlock = NULL;
}

PUBLIC ScriptBlockingStatement::~ScriptBlockingStatement()
{
    delete mChildBlock;
}

/**
 * Since we are a blocking statement have to do recursive resolution.
 */
PUBLIC void ScriptBlockingStatement::resolve(Mobius* m)
{
    if (mChildBlock != NULL)
      mChildBlock->resolve(m);
}

/**
 * Since we are a blocking statement have to do recursive linking.
 */
PUBLIC void ScriptBlockingStatement::link(ScriptCompiler* compiler)
{
    if (mChildBlock != NULL)
      mChildBlock->link(compiler);
}

PUBLIC ScriptBlock* ScriptBlockingStatement::getChildBlock()
{
    if (mChildBlock == NULL)
      mChildBlock = new ScriptBlock();
    return mChildBlock;
}

/****************************************************************************
 *                                                                          *
 *   								 PROC                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptProcStatement::ScriptProcStatement(ScriptCompiler* comp, 
												char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptProcStatement::getKeyword()
{
    return "Proc";
}

PUBLIC bool ScriptProcStatement::isProc()
{
    return true;
}

PUBLIC const char* ScriptProcStatement::getName()
{
	return getArg(0);
}

PUBLIC ScriptStatement* ScriptProcStatement::eval(ScriptInterpreter* si)
{
	// no side effects, wait for a call
    return NULL;
}

//
// Endproc
//

PUBLIC ScriptEndprocStatement::ScriptEndprocStatement(ScriptCompiler* comp,
													  char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptEndprocStatement::getKeyword()
{
    return "Endproc";
}

PUBLIC bool ScriptEndprocStatement::isEndproc()
{
	return true;
}

/**
 * No side effects, in fact we normally won't even keep these
 * in the compiled script now that Proc statements are nested.
 */
PUBLIC ScriptStatement* ScriptEndprocStatement::eval(ScriptInterpreter* si)
{
	return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                 PARAMETER                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptParamStatement::ScriptParamStatement(ScriptCompiler* comp, 
                                                  char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptParamStatement::getKeyword()
{
    return "Param";
}

PUBLIC bool ScriptParamStatement::isParam()
{
    return true;
}

PUBLIC const char* ScriptParamStatement::getName()
{
	return getArg(0);
}

/**
 * Scripts cannot "call" these, the statements will be found
 * by Mobius automatically when scripts are loaded and converted
 * into Parameters.
 */
PUBLIC ScriptStatement* ScriptParamStatement::eval(ScriptInterpreter* si)
{
	// no side effects, wait for a reference
    return NULL;
}

//
// Endparam
//

PUBLIC ScriptEndparamStatement::ScriptEndparamStatement(ScriptCompiler* comp,
													  char* args)
{
    parseArgs(args);
}

PUBLIC const char* ScriptEndparamStatement::getKeyword()
{
    return "Endparam";
}

PUBLIC bool ScriptEndparamStatement::isEndparam()
{
	return true;
}

/**
 * No side effects, in fact we normally won't even keep these
 * in the compiled script.
 */
PUBLIC ScriptStatement* ScriptEndparamStatement::eval(ScriptInterpreter* si)
{
	return NULL;
}

/****************************************************************************
 *                                                                          *
 *                             FUNCTION STATEMENT                           *
 *                                                                          *
 ****************************************************************************/

/**
 * We assume arguments are expressions unless we can resolve to a static
 * function and it asks for old-school arguments.
 */
PUBLIC ScriptFunctionStatement::ScriptFunctionStatement(ScriptCompiler* comp,
														const char* name,
                                                        char* args)
{
	init();

	mFunctionName = CopyString(name);

	// This is kind of a sucky reserved argument convention...
	char* ptr = comp->skipToken(args, "up");
    if (ptr != NULL) {
        mUp = true;
        args = ptr;
    }
    else {
        ptr = comp->skipToken(args, "down");
        if (ptr != NULL) {
            // it isn't enough just to use !mUp, there is logic below that
            // needs to know if an explicit up/down argument was passed
            mDown = true;
            args = ptr;
        }
    }

    // resolve the function
    // !! should be getting this from Mobius
    mFunction = Function::getStaticFunction(mFunctionName);

    if (mFunction != NULL && 
        (!mFunction->expressionArgs && !mFunction->variableArgs)) {
        // old way
        parseArgs(args);
    }
    else {
        // parse the whole thing as an expression which may result
        // in a list
        mExpression = comp->parseExpression(this, args);
    }
}

/**
 * This is only used when script recording is enabled.
 */
PUBLIC ScriptFunctionStatement::ScriptFunctionStatement(Function* f)
{
	init();
	mFunctionName = CopyString(f->getName());
    mFunction = f;
}

void ScriptFunctionStatement::init()
{
    // the four ScriptArguments initialize themselves
	mFunctionName = NULL;
	mFunction = NULL;
	mUp = false;
    mDown = false;
    mExpression = NULL;
}

PUBLIC ScriptFunctionStatement::~ScriptFunctionStatement()
{
	delete mFunctionName;
	delete mExpression;
}

/**
 * If we have a static function, resolve the arguments if
 * the function doesn't support expressions.
 */
PUBLIC void ScriptFunctionStatement::resolve(Mobius* m)
{
    if (mFunction != NULL && 
        // if we resolved this to a script always use expressions
        // !! just change RunScriptFunction to set expressionArgs?
        mFunction->eventType != RunScriptEvent && 
        !mFunction->expressionArgs &&
        !mFunction->variableArgs) {

        mArg1.resolve(m, mParentBlock, mArgs[0]);
        mArg2.resolve(m, mParentBlock, mArgs[1]);
        mArg3.resolve(m, mParentBlock, mArgs[2]);
        mArg4.resolve(m, mParentBlock, mArgs[3]);
    }
}

/**
 * Resolve function-style references to other scripts.
 *
 * We allow function statements whose keywoards are the names of scripts
 * rather than being prefixed by the "Call" statement.  This makes
 * them behave like more like normal functions with regards to quantization
 * and focus lock.  When we find those references, we bootstrap a set
 * of RunScriptFunction objects to represent the script in the function table.
 * Eventually these will be installed in the global function table.
 *
 * Arguments have already been parsed.
 */
PUBLIC void ScriptFunctionStatement::link(ScriptCompiler* comp)
{
	if (mFunction == NULL) {

        Script* callingScript = comp->getScript();

        if (mFunctionName == NULL) {
            Trace(1, "Script %s: missing function name\n", 
                  callingScript->getTraceName());
            Trace(1, "--> File %s line %ld\n",
                  callingScript->getFilename(), (long)mLineNumber);
        }
        else {
            // look for a script
            Script* calledScript = comp->resolveScript(mFunctionName);
            if (calledScript == NULL) {
                Trace(1, "Script %s: unresolved function %s\n", 
                      callingScript->getTraceName(), mFunctionName);
                Trace(1, "--> File %s line %ld\n",
                      callingScript->getFilename(), (long)mLineNumber);
            }
            else {
                // has it been promoted?
                mFunction = calledScript->getFunction();
                if (mFunction == NULL) {
                    // promote it
                    mFunction = new RunScriptFunction(calledScript);
                    calledScript->setFunction(mFunction);
                }
            }
        }
    }
}

PUBLIC const char* ScriptFunctionStatement::getKeyword()
{
    return mFunctionName;
}

PUBLIC Function* ScriptFunctionStatement::getFunction()
{
	return mFunction;
}

PUBLIC const char* ScriptFunctionStatement::getFunctionName()
{
	return mFunctionName;
}

PUBLIC void ScriptFunctionStatement::setUp(bool b)
{
	mUp = b;
}

PUBLIC bool ScriptFunctionStatement::isUp()
{
	return mUp;
}

PUBLIC ScriptStatement* ScriptFunctionStatement::eval(ScriptInterpreter* si)
{
    // has to be resolved by now...before 2.0 did another search of the
    // Functions table but that shouldn't be necessary??
    Function* func = mFunction;

    if (func  == NULL) {
        Trace(1, "Script %s: unresolved function %s\n", 
              si->getTraceName(), mFunctionName);
    }
    else {
        Trace(3, "Script %s: %s\n", si->getTraceName(), func->getName());

        Mobius* m = si->getMobius();
        Action* a = m->newAction();

        // this is redundant because we also check for Target types, but
        // it would be simpler if we could just look at this...
        a->inInterrupt = true;

        // target

        a->setFunction(func);
        Track* t = si->getTargetTrack();
        if (t != NULL) {
            // force it into this track
            a->setResolvedTrack(t);
        }
        else {
            // something is wrong, must have a track! 
            // to make sure focus lock or groups won't be applied
            // set this special flag
            Trace(1, "Script %s: function invoked with no target track %s\n", 
                  si->getTraceName(), mFunctionName);
            a->noGroup = true;
        }

        // trigger

        a->trigger = TriggerScript;

        // this is for GlobalReset handling
        a->id = (long)si;

        // would be nice if this were just part of the Function's
        // arglist parsing?
        a->down = !mUp;

		// if there is an explicit "down" argument, assume
		// this is sustainable and there will eventually be the
		// same function with an "up" argument
        if (mUp || mDown)
          a->triggerMode = TriggerModeMomentary;
        else
          a->triggerMode = TriggerModeOnce;
		
		// Note that we are not setting a function trigger here, which
		// at the moment are only used to implement SUS scripts.  Creating
		// a unique id here may be difficult, it could be the Script address
		// but we're not guaranteed to evaluate the up transition in 
		// the same script.

        // once we start using Wait, schedule at absolute times
        a->noLatency = si->isPostLatency();

        // arguments

        if (mExpression == NULL) {
            // old school single argument
            // do full expansion on these, nice when building path names
            // for SaveFile and SaveRecordedAudio, overkill for everything else
            if (mArg1.isResolved())
              mArg1.get(si, &(a->arg));
            else
              si->expand(mArg1.getLiteral(), &(a->arg));
        }
        else {
            // Complex args, the entire line was parsed as an expression
            // may result in an ExValueList if there were spaces or commas.
            ExValue* value = &(a->arg);
            mExpression->eval(si, value);

            if (func->variableArgs) {
                // normalize to an ExValueList
                if (value->getType() == EX_LIST) {
                    // transfer the value here
                    a->scriptArgs = value->takeList();
                }
                else if (!value->isNull()) {
                    // unusual, promote to a list 
                    ExValue* copy = new ExValue();
                    copy->set(value);
                    ExValueList* list = new ExValueList();
                    list->add(copy);
                    a->scriptArgs = list;
                }
                // in all cases we don't want to leave anything here
                value->setNull();
            }
            else if (value->getType() == EX_LIST) {
                // Multiple values for a function that was only
                // expecting one.  Take the first one and ignore the others
                ExValueList* list = value->takeList();
                if (list != NULL && list->size() > 0) {
                    ExValue* first = list->getValue(0);
                    // Better not be a nested list here, ugly ownership issues
                    // could handle it but unnecessary
                    if (first->getType() == EX_LIST) 
                      Trace(1, "Script %s: Nested list in script argument!\n",
                            si->getTraceName());
                    else
                      value->set(first);
                }
                delete list;
            }
            else {
                // single value, just leave it in scriptArg
            }
        }

        // make it go!
        m->doActionNow(a);

        si->setLastEvents(a);

        // we always must be notified what happens to this, even
        // if we aren't waiting on it
		// ?? why?  if the script ends without waiting, then we have to 
		// remember to clean up this reference before deleting/pooling
		// the interpreter, I guess that's a good idea anyway
        if (a->getEvent() != NULL) {
			// TODO: need an argument like "async" to turn off
			// the automatic completion wait, probably only for unit tests.
			if (func->scriptSync)
			  si->setupWaitLast(this);
		}
		else {
			// it happened immediately
			// Kludge: Need to detect changes to the selected track and change
			// what we think the default track is.  No good way to encapsulate
			// this so look for specific function families.

			if (func->eventType == TrackEvent ||	
				func == GlobalReset) {
				// one of the track select functions, change the default track
				si->setTrack(m->getTrack());
			}
		}

        // if the event didn't take it, we can delete it
        m->completeAction(a);
    }
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                               WAIT STATEMENT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptWaitStatement::ScriptWaitStatement(WaitType type,
                                                WaitUnit unit,
                                                long time)
{
	init();
	mWaitType = type;
	mUnit = unit;
    mExpression = new ExLiteral((int)time);
}

void ScriptWaitStatement::init()
{
	mWaitType = WAIT_NONE;
	mUnit = UNIT_NONE;
    mExpression = NULL;
	mInPause = false;
}

PUBLIC ScriptWaitStatement::~ScriptWaitStatement()
{
	delete mExpression;
}

PUBLIC const char* ScriptWaitStatement::getKeyword()
{
    return "Wait";
}


/**
 * This one is awkward because of the optional keywords.
 *
 * The "time" unit is optional because it is the most common wait,
 * these lines are the same:
 *
 *     Wait time frame 100
 *     Wait frame 100
 *
 * We have even supported optional "frame" unit, this is used in many
 * of the tests:
 *
 *     Wait 100
 *
 * 
 * We used to allow the "function" keyword to be optional but
 * I dont' like that:
 *
 *     Wait function Record
 *     Wait Record
 *
 * Since this was never used I'm going to start requiring it.
 * It is messy to support if the wait time value can be an expression.
 *
 * If that weren't enough, there is an optional "inPause" argument that
 * says that the wait is allowed to proceed during Pause mode.  This
 * is only used in a few tests.  It used to be at the end but was moved
 * to the front when we started allowing value expressions.
 *
 *     Wait inPause frame 1000
 *
 */
PUBLIC ScriptWaitStatement::ScriptWaitStatement(ScriptCompiler* comp,
												char* args)
{
	init();
    
    // this one is odd because of the optional args, parse one at a time
    char* prev = args;
    char* psn = parseArgs(args, 0, 1);

    // consume optional keywords
    if (StringEqualNoCase(mArgs[0], "inPause")) {
        mInPause = true;
        prev = psn;
        psn = parseArgs(psn, 0, 1);
    }

    mWaitType = getWaitType(mArgs[0]);

	if (mWaitType == WAIT_NONE) {
        // may be a relative time wait with missing "time"
		mUnit = getWaitUnit(mArgs[0]);
        if (mUnit != UNIT_NONE) {
            // left off the type, assume "time"
            mWaitType = WAIT_RELATIVE;
        }
        else {
            // assume it's "Wait X" 
            // could sniff test the argument?  This is going to make
            // it harder to find invalid statements...
            //Trace(1, "ERROR: Invalid Wait: %s\n", args);
            mWaitType = WAIT_RELATIVE;
            mUnit = UNIT_FRAME;
            // have to rewind since the previous token was part of the expr
            psn = prev;
        }
	}

    if (mWaitType == WAIT_RELATIVE || mWaitType == WAIT_ABSOLUTE) {

        // if mUnit is none, we had the explicit "time" or "until"
        // keyword, parse the unit now
        if (mUnit == UNIT_NONE) {
            prev = psn;
            psn = parseArgs(psn, 0, 1);
            mUnit = getWaitUnit(mArgs[0]);
        }

        if (mUnit == UNIT_NONE) {
            // Allow missing unit for "Wait until"
            if (mWaitType != WAIT_ABSOLUTE)
              comp->syntaxError(this, "Invalid Wait");
            else {
                mUnit = UNIT_FRAME;
                psn = prev;
            }
        }

        if (mUnit != UNIT_NONE) {
            // whatever remains is the value expression
            mExpression = comp->parseExpression(this, psn);
        }
    }
	else if (mWaitType == WAIT_FUNCTION) {
        // next arg has the function name, leave in mArgs[0]
        parseArgs(psn, 0, 1);
	}
}

WaitType ScriptWaitStatement::getWaitType(const char* name)
{
	WaitType type = WAIT_NONE;
	for (int i = 0 ; WaitTypeNames[i] != NULL ; i++) {
		if (StringEqualNoCase(WaitTypeNames[i], name)) {
			type = (WaitType)i;
			break;
		}
	}
	return type;
}

WaitUnit ScriptWaitStatement::getWaitUnit(const char* name)
{
	WaitUnit unit = UNIT_NONE;

    // hack, it is common to put an "s" on the end such
    // as "Wait frames 1000" rather than "Wait frame 1000".
    // Since the error isn't obvious catch it here.
    if (name != NULL) {
        int last = strlen(name) - 1;
        if (last > 0 && name[last] == 's') {
            char buffer[128];
            strcpy(buffer, name);
            buffer[last] = '\0';
            name = buffer;
        }
    }

    for (int i = 0 ; WaitUnitNames[i] != NULL ; i++) {
        // KLUDGE: recognize old-style plural names for
        // backward compatibility by using StartWith rather than Compare
        // !! this didn't seem to work, had to do the hack above, why?
        if (StartsWithNoCase(name, WaitUnitNames[i])) {
            unit = (WaitUnit)i;
            break;
        }
    }

	return unit;
}

PUBLIC ScriptStatement* ScriptWaitStatement::eval(ScriptInterpreter* si)
{
    // reset the "interrupted" variable
    // will this work without a declaration?
    UserVariables* vars = si->getVariables();
    if (vars != NULL) {
        ExValue v;
        v.setNull();
        vars->set("interrupted", &v);
    }

    switch (mWaitType) {
        case WAIT_NONE: {	
            // probably an error somewhere
            Trace(1,  "Script %s: Malformed script wait statmenet\n",
                  si->getTraceName());
        }
        break;
        case WAIT_LAST: {
            Trace(2,  "Script %s: Wait last\n", si->getTraceName());
            si->setupWaitLast(this);
        }
        break;
        case WAIT_THREAD: {
            Trace(2,  "Script %s: Wait thread\n", si->getTraceName());
            si->setupWaitThread(this);
        }
        break;
        case WAIT_FUNCTION: {
            // !! not sure if this actually works anymore, it was
            // never used...
            const char* name = mArgs[0];
            Function* f = si->getMobius()->getFunction(name);
            if (f == NULL)
			  Trace(1, "Script %s: unresolved wait function %s!\n", 
                    si->getTraceName(), name);
			else {
				Trace(2,  "Script %s: Wait function %s\n", si->getTraceName(), name);
				ScriptStack* frame = si->pushStackWait(this);
				frame->setWaitFunction(f);
			}
        }
        break;
		case WAIT_EVENT: {
			// wait for a specific event
            Trace(1,  "Script %s: Wait event not implemented\n", si->getTraceName());
		}
		break;
        case WAIT_UP: {
            Trace(1,  "Script %s: Wait up not implemented\n", si->getTraceName());
        }
        break;
        case WAIT_LONG: {
            Trace(1,  "Script %s: Wait long not implemented\n", si->getTraceName());
        }
        break;
        case WAIT_BLOCK: {
            // wait for the start of the next interrupt
            Trace(3, "Script %s: waiting for next block\n", si->getTraceName());
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitBlock(true);
        }
        break;
        case WAIT_SWITCH: {
            // no longer have the "fundamenatal command" concept
            // !! what is this doing?
            Trace(1, "Script %s: wait switch\n", si->getTraceName());
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitFunction(Loop1);
        }
        break;
        case WAIT_SCRIPT: {
            // wait for any events we've sent to MobiusThread to complete
            // !! we don't need this any more now that we have "Wait thread"
			ThreadEvent* te = new ThreadEvent(TE_WAIT);
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitThreadEvent(te);
			si->scheduleThreadEvent(te);
			Trace(3, "Script %s: wait script event\n", si->getTraceName());
		}
        break;

		case WAIT_START:
		case WAIT_END:
		case WAIT_EXTERNAL_START:
		case WAIT_DRIFT_CHECK:
		case WAIT_PULSE:
		case WAIT_BEAT:
		case WAIT_BAR:
		case WAIT_REALIGN:
		case WAIT_RETURN: {

			// Various pending events that wait for Loop or 
			// Synchronizer to active them at the right time.
            // !! TODO: Would be nice to wait for a specific pulse

            Trace(2, "Script %s: wait %s\n", 
                  si->getTraceName(), WaitTypeNames[mWaitType]);
			Event* e = setupWaitEvent(si, 0);
			e->pending = true;
			e->fields.script.waitType = mWaitType;
		}
		break;

        default: {
            // relative, absolute, and audio
			Event* e = setupWaitEvent(si, getWaitFrame(si));
			e->fields.script.waitType = mWaitType;

			// special option to bring us out of pause mode
			// Should really only allow this for absolute millisecond waits?
			// If we're waiting on a cycle should wait for the loop to be
			// recorded and/or leave pause.  Still it could be useful
			// to wait for a loop-relative time.
			e->pauseEnabled = mInPause;

            // !! every relative UNIT_MSEC wait should be implicitly
            // enabled in pause mode.  No reason not to and it's what
            // people expect.  No one will remember "inPause"
            if (mWaitType == WAIT_RELATIVE && mUnit == UNIT_MSEC)
              e->pauseEnabled = true;

            Trace(2, "Script %s: Wait\n", si->getTraceName());
        }
        break;
    }

    // set this to prevent the addition of input latency
    // when scheduling future functions from the script
    si->setPostLatency(true);

    return NULL;
}

/**
 * Setup a Script event on a specific frame.
 */
PRIVATE Event* ScriptWaitStatement::setupWaitEvent(ScriptInterpreter* si, 
												   long frame)
{
    Track* track = si->getTargetTrack();
    EventManager* em = track->getEventManager();
	Loop* l = track->getLoop();
	Event* e = em->newEvent();

	e->type = ScriptEvent;
	e->frame = frame;
	e->setScript(si);
	Trace(3, "Script %s: wait for frame %ld\n", si->getTraceName(), e->frame);
	em->addEvent(e);

	ScriptStack* stack = si->pushStackWait(this);
	stack->setWaitEvent(e);

	return e;
}

/**
 * Return the number of frames represented by a millisecond.
 * Adjusted for the current playback rate.  
 * For accurate waits, you have to ensure that the rate can't
 * change while we're waiting.
 */
PRIVATE long ScriptWaitStatement::getMsecFrames(ScriptInterpreter* si, long msecs)
{
	float rate = si->getTargetTrack()->getEffectiveSpeed();
	// should we ceil()?
	long frames = (long)(MSEC_TO_FRAMES(msecs) * rate);
	return frames;
}

/**
 * Calculate the frame at which to schedule a ScriptEvent event after 
 * the desired wait.
 *
 * If we're in the initial record, only WAIT_AUDIO or WAIT_ABSOLULTE with 
 * UNIT_MSEC and UNIT_FRAME are meaningful.  Since it will be a common 
 * error, also recognize WAIT_RELATIVE with UNIT_MSEC and UNIT_FRAME. 
 * If any other unit is specified assume 1 second.
 */
PRIVATE long ScriptWaitStatement::getWaitFrame(ScriptInterpreter* si)
{
	long frame = 0;
    Track* track = si->getTargetTrack();
	Loop* loop = track->getLoop();
	WaitType type = mWaitType;
	WaitUnit unit = mUnit;
	long current = loop->getFrame();
	long loopFrames = loop->getFrames();
	long time = getTime(si);

	if (loopFrames == 0) {
		// initial record
		if (type == WAIT_RELATIVE || type == WAIT_ABSOLUTE) {
			if (unit != UNIT_MSEC && unit != UNIT_FRAME) {
                // !! why have we done this?
                Trace(1, "Script %s: ERROR: Fixing malformed wait during initial record\n",
                      si->getTraceName());
				unit = UNIT_MSEC;
				time = 1000;
			}
		}
	}

	switch (type) {
		
		case WAIT_RELATIVE: {
			// wait some number of frames after the current frame	
			switch (unit) {
				case UNIT_MSEC: {
					frame = current + getMsecFrames(si, time);
				}
				break;

				case UNIT_FRAME: {
					frame = current + time;
				}
				break;

				case UNIT_SUBCYCLE: {
					// wait for the start of a subcycle after the current frame
					frame = getQuantizedFrame(loop, 
											  Preset::QUANTIZE_SUBCYCLE, 
											  current, time);
				}
				break;

				case UNIT_CYCLE: {
					// wait for the start of a cycle after the current frame
					frame = getQuantizedFrame(loop, Preset::QUANTIZE_CYCLE, 
											  current, time);
				}
				break;

				case UNIT_LOOP: {
					// wait for the start of a loop after the current frame
					frame = getQuantizedFrame(loop, Preset::QUANTIZE_LOOP, 
											  current, time);
				}
				break;
			
				case UNIT_NONE: break;
			}
		}
		break;

		case WAIT_ABSOLUTE: {
			// wait for a particular frame within the loop
			switch (unit) {
				case UNIT_MSEC: {
					frame = getMsecFrames(si, time);
				}
				break;

				case UNIT_FRAME: {
					frame = time;
				}
				break;

				case UNIT_SUBCYCLE: {
					// Hmm, should the subcycle be relative to the
					// start of the loop or relative to the current cycle?
					// Start of the loop feels more natural.
					// If there aren't this many subcycles in a cycle, do 
					// we spill over into the next cycle or round?
					// Spill.
					frame = loop->getSubCycleFrames() * time;
				}
				break;

				case UNIT_CYCLE: {
					frame = loop->getCycleFrames() * time;
				}
				break;

				case UNIT_LOOP: {
					// wait for the start of a particular loop
					// this is meaningless since there is only one loop, though
					// I supposed we could take this to mean whenever the
					// loop is triggered, that would be inconsistent with the
					// other absolute time values though.
					// Let this mean to wait for n iterations of the loop
					frame = loop->getFrames() * time;
				}
				break;

				case UNIT_NONE: break;
			}
		}
		break;

		default:
			// need this or xcode 5 whines
			break;
	}

	return frame;
}

/**
 * Evaluate the time expression and return the result as a long.
 */
PRIVATE long ScriptWaitStatement::getTime(ScriptInterpreter* si)
{
    long time = 0;
    if (mExpression != NULL) {
		ExValue v;
		mExpression->eval(si, &v);
        time = v.getLong();
    }
    return time;
}

/**
 * Helper for getWaitFrame.
 * Calculate a quantization boundary frame.
 * If we're finishing recording of the initial loop, 
 * don't quantize to the end of the loop, go to the next.
 */
PRIVATE long ScriptWaitStatement::getQuantizedFrame(Loop* loop,
                                                    Preset::QuantizeMode q, 
                                                    long frame, int count)
{
	long loopFrames = loop->getFrames();

	// special case for the initial record, can only get here after
	// we've set the loop frames, but before receiving all of them
	if (loop->getMode() == RecordMode)
	  frame = loopFrames;

	// if count is unspecified it defaults to 1, for the next whatever
	if (count == 0) 
	  count = 1;

    EventManager* em = loop->getTrack()->getEventManager();

	for (int i = 0 ; i < count ; i++) {
		// if we're on a boundary the first time use it, otherwise advance?
		// no, always advance
		//bool after = (i > 0);
		frame = em->getQuantizedFrame(loop, frame, q, true);
	}

	return frame;
}

/****************************************************************************
 *                                                                          *
 *   								SCRIPT                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Script::Script()
{
	init();
}

PUBLIC Script::Script(ScriptEnv* env, const char* filename)
{
	init();
	mEnv = env;
    setFilename(filename);
}

PUBLIC void Script::init()
{
	mEnv = NULL;
	mNext = NULL;
    mFunction = NULL;
    mName = NULL;
	mDisplayName = NULL;
	mFilename = NULL;
    mDirectory = NULL;

	mAutoLoad = false;
	mButton = false;
	mFocusLockAllowed = false;
	mQuantize = false;
	mSwitchQuantize = false;
    mExpression = false;
	mContinuous = false;
    mParameter = false;
	mSpread = false;
    mHide = false;
	mSpreadRange = 0;
	mSustainMsecs = DEFAULT_SUSTAIN_MSECS;
	mClickMsecs = DEFAULT_CLICK_MSECS;

    mBlock = NULL;

	mReentryLabel = NULL;
	mSustainLabel = NULL;
	mEndSustainLabel = NULL;
	mClickLabel = NULL;	
    mEndClickLabel = NULL;
}

PUBLIC Script::~Script()
{
	Script *el, *next;

	clear();
    delete mFunction;
	delete mName;
	delete mDisplayName;
	delete mFilename;
	delete mDirectory;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC void Script::setEnv(ScriptEnv* env)
{
    mEnv = env;
}

PUBLIC ScriptEnv* Script::getEnv()
{
    return mEnv;
}

PUBLIC void Script::setNext(Script* s)
{
	mNext = s;
}

PUBLIC Script* Script::getNext()
{
	return mNext;
}

PUBLIC void Script::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

PUBLIC const char* Script::getName()
{
	return mName;
}

PUBLIC const char* Script::getDisplayName()
{
	const char* name = mName;
	if (name == NULL) {
		if (mDisplayName == NULL) {
            if (mFilename != NULL) {
                // derive a display name from the file path
                char dname[1024];
                GetLeafName(mFilename, dname, false);
                mDisplayName = CopyString(dname);
            }
            else {
                // odd, must be an anonymous memory script?
                name = "???";
            }
		}
		name = mDisplayName;
	}
    return name;
}

PUBLIC const char* Script::getTraceName()
{
    // better to always return the file name?
    return getDisplayName();
}

PUBLIC void Script::setFilename(const char* s)
{
    delete mFilename;
    mFilename = CopyString(s);
}

PUBLIC const char* Script::getFilename()
{
    return mFilename;
}

PUBLIC void Script::setDirectory(const char* s)
{
    delete mDirectory;
    mDirectory = CopyString(s);
}

PUBLIC void Script::setDirectoryNoCopy(char* s)
{
    delete mDirectory;
    mDirectory = s;
}

PUBLIC const char* Script::getDirectory()
{
    return mDirectory;
}

//
// Statements
//

PUBLIC void Script::clear()
{
    delete mBlock;
    mBlock = NULL;
	mReentryLabel = NULL;
	mSustainLabel = NULL;
	mEndSustainLabel = NULL;
	mClickLabel = NULL;
	mEndClickLabel = NULL;
}

PUBLIC ScriptBlock* Script::getBlock()
{
    if (mBlock == NULL)
      mBlock = new ScriptBlock();
    return mBlock;
}

//
// parsed options
//

PUBLIC void Script::setAutoLoad(bool b)
{
	mAutoLoad = b;
}

PUBLIC bool Script::isAutoLoad()
{
	return mAutoLoad;
}

PUBLIC void Script::setButton(bool b)
{
	mButton = b;
}

PUBLIC bool Script::isButton()
{
	return mButton;
}

PUBLIC void Script::setHide(bool b)
{
	mHide = b;
}

PUBLIC bool Script::isHide()
{
	return mHide;
}

PUBLIC void Script::setFocusLockAllowed(bool b)
{
	mFocusLockAllowed = b;
}

PUBLIC bool Script::isFocusLockAllowed()
{
	return mFocusLockAllowed;
}

PUBLIC void Script::setQuantize(bool b)
{
	mQuantize = b;
}

PUBLIC bool Script::isQuantize()
{
	return mQuantize;
}

PUBLIC void Script::setSwitchQuantize(bool b)
{
	mSwitchQuantize = b;
}

PUBLIC bool Script::isSwitchQuantize()
{
	return mSwitchQuantize;
}

PUBLIC void Script::setContinuous(bool b)
{
	mContinuous = b;
}

PUBLIC bool Script::isContinuous()
{
	return mContinuous;
}

PUBLIC void Script::setParameter(bool b)
{
	mParameter = b;
}

PUBLIC bool Script::isParameter()
{
	return mParameter;
}

PUBLIC void Script::setSpread(bool b)
{
	mSpread = b;
}

PUBLIC bool Script::isSpread()
{
	return mSpread;
}

PUBLIC void Script::setSpreadRange(int i)
{
	mSpreadRange = i;
}

PUBLIC int Script::getSpreadRange()
{
	return mSpreadRange;
}

PUBLIC void Script::setSustainMsecs(int msecs)
{
	if (msecs > 0)
	  mSustainMsecs = msecs;
}

PUBLIC int Script::getSustainMsecs()
{
	return mSustainMsecs;
}

PUBLIC void Script::setClickMsecs(int msecs)
{
	if (msecs > 0)
	  mClickMsecs = msecs;
}

PUBLIC int Script::getClickMsecs()
{
	return mClickMsecs;
}

//
// Cached labels
//

PUBLIC void Script::cacheLabels()
{
    if (mBlock != NULL) {
        for (ScriptStatement* s = mBlock->getStatements() ; 
             s != NULL ; s = s->getNext()) {

            if (s->isLabel()) {
                ScriptLabelStatement* l = (ScriptLabelStatement*)s;
                if (l->isLabel(LABEL_REENTRY))
                  mReentryLabel = l;
                else if (l->isLabel(LABEL_SUSTAIN))
                  mSustainLabel = l;
                else if (l->isLabel(LABEL_END_SUSTAIN))
                  mEndSustainLabel = l;
                else if (l->isLabel(LABEL_CLICK))
                  mClickLabel = l;
                else if (l->isLabel(LABEL_END_CLICK))
                  mEndClickLabel = l;
            }
        }
    }
}

PUBLIC ScriptLabelStatement* Script::getReentryLabel()
{
	return mReentryLabel;
}

PUBLIC ScriptLabelStatement* Script::getSustainLabel()
{
	return mSustainLabel;
}

PUBLIC ScriptLabelStatement* Script::getEndSustainLabel()
{
	return mEndSustainLabel;
}

PUBLIC bool Script::isSustainAllowed()
{
	return (mSustainLabel != NULL || mEndSustainLabel != NULL);
}

PUBLIC ScriptLabelStatement* Script::getClickLabel()
{
	return mClickLabel;
}

PUBLIC ScriptLabelStatement* Script::getEndClickLabel()
{
	return mEndClickLabel;
}

PUBLIC bool Script::isClickAllowed()
{
	return (mClickLabel != NULL || mEndClickLabel != NULL);
}

PUBLIC void Script::setFunction(Function* f)
{
    mFunction = f;
}

PUBLIC Function* Script::getFunction()
{
    return mFunction;
}

//////////////////////////////////////////////////////////////////////
//
// Compilation
//
//////////////////////////////////////////////////////////////////////

/**
 * Resolve references in a script after it has been fully parsed.
 */
PUBLIC void Script::resolve(Mobius* m)
{
    if (mBlock != NULL)
      mBlock->resolve(m);
    
    // good place to do this too
    cacheLabels();
}

/**
 * Resolve references between scripts after the entire environment
 * has been loaded.  This will do nothing except for ScriptCallStatement
 * and ScriptStartStatement which will call back to resolveScript to find
 * the referenced script.  Control flow is a bit convoluted but the
 * alternatives aren't much better.
 */
PUBLIC void Script::link(ScriptCompiler* comp)
{
    if (mBlock != NULL)
      mBlock->link(comp);
}

/**
 * Can assume this is a full path 
 *
 * !!! This doesn't handle blocking statements, Procs won't write
 * properly.  Where is this used?
 */
PUBLIC void Script::xwrite(const char* filename)
{
	FILE* fp = fopen(filename, "w");
	if (fp == NULL) {
		// need a configurable alerting mechanism
		Trace(1, "Script %s: Unable to open file for writing: %s\n", 
              getDisplayName(), filename);
	}
	else {
        // !! write the options
        if (mBlock != NULL) {
            for (ScriptStatement* a = mBlock->getStatements() ; 
                 a != NULL ; a = a->getNext())
              a->xwrite(fp);
            fclose(fp);
        }
	}
}

//////////////////////////////////////////////////////////////////////
//
// ScriptEnv
//
//////////////////////////////////////////////////////////////////////

PUBLIC ScriptEnv::ScriptEnv()
{
    mNext = NULL;
    mSource = NULL;
	mScripts = NULL;
}

PUBLIC ScriptEnv::~ScriptEnv()
{
	ScriptEnv *el, *next;

    delete mSource;
    delete mScripts;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC ScriptEnv* ScriptEnv::getNext()
{
    return mNext;
}

PUBLIC void ScriptEnv::setNext(ScriptEnv* env)
{
    mNext = env;
}

PUBLIC ScriptConfig* ScriptEnv::getSource()
{
    return mSource;
}

PUBLIC void ScriptEnv::setSource(ScriptConfig* config)
{
    delete mSource;
    mSource = config;
}

PUBLIC Script* ScriptEnv::getScripts()
{
	return mScripts;
}

PUBLIC void ScriptEnv::setScripts(Script* scripts)
{
    delete mScripts;
    mScripts = scripts;
}

/**
 * Return a list of Functions for the scripts that are allowed to be bound.
 * This is used by Function::initFunctions when building the global function
 * table.
 */
PUBLIC List* ScriptEnv::getScriptFunctions()
{
	List* functions = NULL;

    for (Script* s = mScripts ; s != NULL ; s = s->getNext()) {
        if (!s->isHide()) {
            // may already have a function if we had a cross reference
            Function* f = s->getFunction();
            if (f == NULL) {
                f = new RunScriptFunction(s);
                s->setFunction(f);
            }

            if (functions == NULL)
              functions = new List();
            functions->add(f);
        }
    }
	return functions;
}

/**
 * Detect differences after editing the script config.
 * We assume the configs are the same if the same names appear in both lists
 * ignoring order.
 *
 * Since our mScripts list can contain less than what was in the original
 * ScriptConfig due to filtering out invalid names, compare with the
 * original ScriptConfig which we saved at compilation.
 */
PUBLIC bool ScriptEnv::isDifference(ScriptConfig* config)
{
    bool difference = false;

    if (mSource == NULL) {
        // started with nothing
        if (config != NULL && config->getScripts() != NULL)
          difference = true;
    }
    else {
        // let the configs compare themselves
        difference = mSource->isDifference(config);
    }

    return difference;
}

/**
 * Search for a new version of the given script.
 * This is used to refresh previously resolved ResolvedTarget after
 * the scripts are reloaded.  
 *
 * We search using the same name that was used in the binding,
 * which is the script "display name". This is either the !name
 * if it was specified or the base file name.
 * Might want to search on full path to be safe?
 */
PUBLIC Script* ScriptEnv::getScript(Script* src)
{
    Script* found = NULL;

    for (Script* s = mScripts ; s != NULL ; s = s->getNext()) {
        if (StringEqual(s->getDisplayName(), src->getDisplayName())) {
            found = s;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// ScriptCompiler
//
//////////////////////////////////////////////////////////////////////

PUBLIC ScriptCompiler::ScriptCompiler()
{
    mMobius     = NULL;
    mParser     = NULL;
    mEnv        = NULL;
    mScripts    = NULL;
    mLast       = NULL;
    mScript     = NULL;
    mBlock      = NULL;
    mLineNumber = 0;
    strcpy(mLine, "");
}

PUBLIC ScriptCompiler::~ScriptCompiler()
{
    delete mParser;
}

/**
 * Compile a ScriptConfig into a ScriptEnv.
 * 
 * The returned ScriptEnv is completely self contained and only has
 * references to static objects like Functions and Parameters.
 * 
 * Mobius is only necessary to get the configuration directory out of the
 * MobiusContext.
 */
PUBLIC ScriptEnv* ScriptCompiler::compile(Mobius* m, ScriptConfig* config)
{
    // should not try to use this more than once
    if (mEnv != NULL)
      Trace(1, "ScriptCompiler: dangling environment!\n");

    mMobius = m;
    mEnv = new ScriptEnv();
    mScripts = NULL;
    mLast = NULL;

    // give it a copy of the config for later diff detection
    mEnv->setSource(config->clone());

	if (config != NULL) {
		for (ScriptRef* ref = config->getScripts() ; ref != NULL ; 
			 ref = ref->getNext()) {

            // allow relative paths so we can distribute examples
            char path[1024];
			const char* file = ref->getFile();
            if (IsAbsolute(file))
              strcpy(path, file);
            else {
                MobiusContext* con = m->getContext();

                strcpy(path, "");
                // check configuration directory first
                const char* srcdir = con->getConfigurationDirectory();
                bool found = false;
                if (srcdir != NULL) {
                    sprintf(path, "%s/%s", srcdir, file);
                    found = IsFile(path) || IsDirectory(path);
                }

                // fall back to installation directory
                if (!found) {
                    srcdir = con->getInstallationDirectory();
                    if (srcdir != NULL)
                      sprintf(path, "%s/%s", srcdir, file);
                    else
                      strcpy(path, file);
                }
            }

			if (IsFile(path)) {
				parse(path);
			}
			else if (IsDirectory(path)) {
				Trace(2, "Reading Mobius script directory: %s\n", path);
				StringList* files = GetDirectoryFiles(path, ".mos");
				if (files != NULL) {
					for (int i = 0 ; i < files->size() ; i++) {
						parse(files->getString(i));
					}
				}
			}
            else {
                Trace(1, "Invalid script path: %s\n", file);
            }
		}
	}

    // Link Phase
    for (Script* s = mScripts ; s != NULL ; s = s->getNext())
      link(s);
    
    mEnv->setScripts(mScripts);

    return mEnv;
}

/**
 * Recompile one script declared with !autoload
 * Keep the same script object so we don't have to mess
 * with substitution in the ScriptEnv and elsewhere.
 * This should only be called if the script is not currently
 * running, ScriptInterpreter checks that.
 * 
 * NOTE: If we ever start allowing references to Variables
 * and Procs defined between scripts, this will need to be more 
 * complicated.  Sharing Variable declarations could be easily
 * done just by duplicating them in every file with some kind of
 * !include directive, but sharing Procs is not as nice since they
 * can be large and we don't want to duplicate them in every file.
 * Either we have to disallow !autoload of Proc libraries or we
 * need to indirect through a Proc lookup table.
 * 
 */
PUBLIC void ScriptCompiler::recompile(Mobius* m, Script* script)
{
    mMobius = m;

	if (script->isAutoLoad()) {
        const char* filename = script->getFilename();
        if (filename != NULL) {
            FILE* fp = fopen(filename, "r");
            if (fp == NULL) {
                Trace(1, "Unable to refresh script %s\n", filename);
                // just leave it as it was or empty it out?
            }
            else {
                Trace(2, "Re-reading Mobius script %s\n", filename);

                // Get the environment for linking script references
                mEnv = script->getEnv();

                if (!parse(fp, script)) {
                    // hmm, could try to splice it out of everywhere
                    // but just leave it, we don't have a mechanism
                    // for returning inner script errors anyway
                }
                else {
                    // relink just this script
                    // NOTE: if we get around to supporting Proc refs
                    // then we may need to relink all the *other* scripts
                    // as well
                    link(script);
                }

                fclose(fp);
            }
        }
    }
}

/**
 * Final link phase for one script.
 */
PRIVATE void ScriptCompiler::link(Script* s)
{
    // zero means we're in the link phase
    mLineNumber = 0;
    strcpy(mLine, "");

    // save for callbacks to parseExpression and other utilities
    mScript = s;

    s->link(this);
}

/**
 * Internal helper used when processing something from the script config
 * we know is an individual file.
 */
PRIVATE void ScriptCompiler::parse(const char* filename)
{
    if (!IsFile(filename)) {
		Trace(1, "Unable to locate script file %s\n", filename);
	}
	else {
        FILE* fp = fopen(filename, "r");
        if (fp == NULL) {
			Trace(1, "Unable to open file: %s\n", filename);
		}
        else {
			Trace(2, "Reading Mobius script %s\n", filename);

            Script* script = new Script(mEnv, filename);
            
			// remember the directory, for later relative references
			// within the script
			// !! don't need this any more?
            int psn = strlen(filename) - 1;
            while (psn > 0 && filename[psn] != '/' && filename[psn] != '\\')
                psn--;

            if (psn == 0) {
                // should only happen if we're using relative paths
            }
            else {
                // leave the trailing slash
                script->setDirectoryNoCopy(CopyString(filename, psn));
            }

            if (parse(fp, script)) {
                if (mScripts == NULL)
                  mScripts = script;
                else
                  mLast->setNext(script);
                mLast = script;
            }
            else {
                delete script;
            }

            fclose(fp);
        }
    }
}

PRIVATE bool ScriptCompiler::parse(FILE* fp, Script* script)
{
	char line[SCRIPT_MAX_LINE + 4];
	char arg[SCRIPT_MAX_LINE + 4];

    mScript = script;
    mLineNumber = 0;
    
    if (mParser == NULL)
      mParser = new ExParser();

    // if here on !authload, remove current contents
    // NOTE: If this is still being interpreted someone has to wait
    // for all threads to finish...how does THAt work?
	mScript->clear();

    // start by parsing in to the script block
    mBlock = mScript->getBlock();

	while (fgets(line, SCRIPT_MAX_LINE, fp) != NULL) {

        if (mLineNumber == 0 && 
            (line[0] == (char)0xFF || line[0] == (char)0xFE)) {
            // this looks like a Unicode Byte Order Mark, we could
            // probably handle these but skip them for now
            Trace(1, "Script %s: Script appears to contain multi-byte unicode\n", 
                  script->getTraceName());
            break;
        }

        // we may be tokenizing the line, save a copy here
        // in case the statements need it
		strcpy(mLine, line);
		mLineNumber++;

		char* ptr = line;

		// fix? while (*ptr && isspace(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		int len = strlen(ptr);

		if (len > 0) {
			if (ptr[0] == '!') {
				// Script directives
				ptr++;

                if (StartsWithNoCase(ptr, "name")) {
					parseArgument(ptr, "name", arg);
                    script->setName(arg);
                }
                else if (StartsWithNoCase(ptr, "hide") ||
                         StartsWithNoCase(ptr, "hidden"))
                  script->setHide(true);

                else if (StartsWithNoCase(ptr, "autoload")) {
                    // until we work out the dependencies, autoload
                    // and parameter are mutually exclusive
                    if (!script->isParameter())
                      script->setAutoLoad(true);
                }

                else if (StartsWithNoCase(ptr, "button"))
				  script->setButton(true);

                else if (StartsWithNoCase(ptr, "focuslock"))
				  script->setFocusLockAllowed(true);
                
                else if (StartsWithNoCase(ptr, "quantize"))
				  script->setQuantize(true);

                else if (StartsWithNoCase(ptr, "switchQuantize"))
				  script->setSwitchQuantize(true);

                // old name
                else if (StartsWithNoCase(ptr, "controller"))
				  script->setContinuous(true);

                // new preferred name
                else if (StartsWithNoCase(ptr, "continous"))
				  script->setContinuous(true);

                else if (StartsWithNoCase(ptr, "parameter")) {
                    script->setParameter(true);
                    // make sure this stays out of the binding windows
                    script->setHide(true);
                    // issues here, ignore autoload
                    script->setAutoLoad(false);
                }

                else if (StartsWithNoCase(ptr, "sustain")) {
					// second arg is the sustain unit in msecs
					parseArgument(ptr, "sustain", arg);
					int msecs = ToInt(arg);
                    if (msecs > 0)
					  script->setSustainMsecs(msecs);
				}
                else if (StartsWithNoCase(ptr, "multiclick")) {
					// second arg is the multiclick unit in msecs
					parseArgument(ptr, "multiclick", arg);
					int msecs = ToInt(arg);
					if (msecs > 0)
					  script->setClickMsecs(msecs);
				}
                else if (StartsWithNoCase(ptr, "spread")) {
					// second arg is the range in one direction,
					// e.g. 12 is an octave up and down, if not specified
					// use the global default range
					script->setSpread(true);
					parseArgument(ptr, "spread", arg);
					int range = ToInt(arg);
					if (range > 0)
					  script->setSpreadRange(range);
				}
			}
			else if (ptr[0] != '#' && len > 1) {
				if (ptr[len-1] == '\n')
				  ptr[len-1] = 0;
				else {
					// actually this is common on the last line of the file
					// if it wasn't terminated
					//Trace(1, "Script line too long!\n");
					//Trace(1, "--> '%s'\n", ptr);
				}

                ScriptStatement* s = parseStatement(ptr);
                if (s != NULL) {
                    s->setLineNumber(mLineNumber);

                    if (s->isEndproc() || s->isEndparam()) {
                        // pop the stack
                        // !! hey, should check to make sure we have the
                        // right ending, currently Endproc can end a Param
                        if (mBlock != NULL && mBlock->getParent() != NULL) {
                            mBlock = mBlock->getParent();
                        }
                        else {
                            // error, mismatched block ending
                            const char* msg;
                            if (s->isEndproc())
                              msg = "Script %s: Mismatched Proc/Endproc line %ld\n";
                            else
                              msg = "Script %s: Mismatched Param/Endparam line %ld\n";

                            Trace(1, msg, script->getTraceName(), (long)mLineNumber);
                        }

                        // we don't actually need these since the statements 
                        // are nested
                        delete s;
                    }
                    else {
                        // add the statement to the block
                        mBlock->add(s);

                        if (s->isProc() || s->isParam()) {
                            // push a new block 
                            ScriptBlockingStatement* bs = (ScriptBlockingStatement*)s;
                            ScriptBlock* block = bs->getChildBlock();
                            block->setParent(mBlock);
                            mBlock = block;
                        }
                    }
                }
            }
		}
	}

    // do internal resolution
    script->resolve(mMobius);

    // TODO: do some sanity checks, like looking for Param
    // statements in a script that isn't declared with !parameter

    // don't have an error return case yet, safer to ignore harmless
    // parse errors and let the script do the best it can?
    return true;
}

/**
 * Helper for script declaration argument parsing.
 * Given a line with a declaration and the keyword, skip over the keyword 
 * and copy the argument into the given buffer.
 * Be sure to trim both leading and trailing whitespace, this is especially
 * important when moving scripts from Windows to Mac because Widnows
 * uses the 0xd 0xa newline convention but Mac file streams only strip 0xa.
 * Failing to strip the 0xd can cause script name mismatches.
 */
PRIVATE void ScriptCompiler::parseArgument(char* line, const char* keyword, 
                                           char* buffer)
{
	char* ptr = line + strlen(keyword);

	ptr = TrimString(ptr);

	strcpy(buffer, ptr);
}

PRIVATE ScriptStatement* ScriptCompiler::parseStatement(char* line)
{
    ScriptStatement* stmt = NULL;
	char* keyword;
	char* args;

    // parse the initial keyword
	keyword = parseKeyword(line, &args);

	if (keyword != NULL) {

        if (StartsWith(keyword, "!") || EndsWith(keyword, ":"))
          parseDeclaration(keyword, args);

        else if (StringEqualNoCase(keyword, "echo"))
          stmt = new ScriptEchoStatement(this, args);

        else if (StringEqualNoCase(keyword, "message"))
          stmt = new ScriptMessageStatement(this, args);

        else if (StringEqualNoCase(keyword, "prompt"))
          stmt = new ScriptPromptStatement(this, args);

        else if (StringEqualNoCase(keyword, "end"))
          stmt = new ScriptEndStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "cancel"))
          stmt = new ScriptCancelStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "wait"))
          stmt = new ScriptWaitStatement(this, args);

        else if (StringEqualNoCase(keyword, "set"))
          stmt = new ScriptSetStatement(this, args);

        else if (StringEqualNoCase(keyword, "use"))
          stmt = new ScriptUseStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "variable"))
          stmt = new ScriptVariableStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "jump"))
          stmt = new ScriptJumpStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "label"))
          stmt = new ScriptLabelStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "for"))
          stmt = new ScriptForStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "repeat"))
          stmt = new ScriptRepeatStatement(this, args);

        else if (StringEqualNoCase(keyword, "while"))
          stmt = new ScriptWhileStatement(this, args);

        else if (StringEqualNoCase(keyword, "next"))
          stmt = new ScriptNextStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "setup"))
          stmt = new ScriptSetupStatement(this, args);

        else if (StringEqualNoCase(keyword, "preset"))
          stmt = new ScriptPresetStatement(this, args);

        else if (StringEqualNoCase(keyword, "unittestsetup"))
          stmt = new ScriptUnitTestSetupStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "initpreset"))
          stmt = new ScriptInitPresetStatement(this, args);

        else if (StringEqualNoCase(keyword, "break"))
          stmt = new ScriptBreakStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "interrupt"))
          stmt = new ScriptInterruptStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "load"))
          stmt = new ScriptLoadStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "save"))
          stmt = new ScriptSaveStatement(this, args);

        else if (StringEqualNoCase(keyword, "call"))
          stmt = new ScriptCallStatement(this, args);

        else if (StringEqualNoCase(keyword, "start"))
          stmt = new ScriptStartStatement(this, args);

        else if (StringEqualNoCase(keyword, "proc"))
          stmt = new ScriptProcStatement(this, args);

        else if (StringEqualNoCase(keyword, "endproc"))
            stmt = new ScriptEndprocStatement(this, args);

        else if (StringEqualNoCase(keyword, "param"))
          stmt = new ScriptParamStatement(this, args);

        else if (StringEqualNoCase(keyword, "endparam"))
            stmt = new ScriptEndparamStatement(this, args);

        else if (StringEqualNoCase(keyword, "if"))
          stmt = new ScriptIfStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "else"))
          stmt = new ScriptElseStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "elseif"))
          stmt = new ScriptElseStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "endif"))
          stmt = new ScriptEndifStatement(this, args);
        
        else if (StringEqualNoCase(keyword, "diff"))
          stmt = new ScriptDiffStatement(this, args);
        
        else {
            // assume it must be a function reference
            stmt = new ScriptFunctionStatement(this, keyword, args);
        }

    }

    return stmt;
}

/**
 * Isolate the initial keyword token.
 * The line buffer is modified to insert a zero between the
 * keyword and the arguments.  Also return the start of the arguments.
 */
PRIVATE char* ScriptCompiler::parseKeyword(char* line, char** retargs)
{
	char* keyword = NULL;
	char* args = NULL;

	// skip preceeding whitespace
	while (*line && isspace(*line)) line++;
	if (*line) {
		keyword = line;
		while (*line && !isspace(*line)) line++;
		if (*line != 0) {
			*line = 0;
			args = line + 1;
		}
		if (strlen(keyword) == 0)
		  keyword = NULL;
	}

	// remove trailing carriage-return from Windows
	args = TrimString(args);
	
	*retargs = args;

	return keyword;
}

/**
 * Parse a declaration found within a block.
 * Should eventually use this for parsing the script declarations?
 */
PRIVATE void ScriptCompiler::parseDeclaration(const char* keyword, const char* args)
{
    // mBlock has the containing ScriptBlock
    if (mBlock != NULL) {
        // let's defer complicated parsing since it may be block specific?
        // well it shouldn't be...
        ScriptDeclaration* decl = new ScriptDeclaration(keyword, args);
        mBlock->addDeclaration(decl);
    }
    else {
        // error, mismatched block end
        Trace(1, "Script %s: Declaration found outside block, line %ld\n",
              mScript->getTraceName(), (long)mLineNumber);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parse/Link Callbacks
//
//////////////////////////////////////////////////////////////////////

PUBLIC Mobius* ScriptCompiler::getMobius()
{   
    return mMobius;
}

/**
 * Return the script currently being compiled or linked.
 */
PUBLIC Script* ScriptCompiler::getScript()
{
    return mScript;
}

/** 
 * Consume a reserved token in an argument list.
 * Returns NULL if the token was not found.  If found it returns
 * a pointer into "args" after the token.
 *
 * The token may be preceeded by whitespace and MUST be followed
 * by whitespace or be on the end of the line.
 *
 * For example when looking for "up" these are valid tokens
 *
 *     something up
 *     something up arg
 *
 * But this is not
 *
 *     something upPrivateVariable
 * 
 * Public so it may be used by the ScriptStatement constructors.
 */
PUBLIC char* ScriptCompiler::skipToken(char* args, const char* token)
{
    char* next = NULL;

    if (args != NULL) {
        char* ptr = args;
        while (*ptr && isspace(*ptr)) ptr++;
        int len = strlen(token);

        if (StringEqualNoCase(args, token, len)) {
            ptr += len;
            if (*ptr == '\0' || isspace(*ptr)) 
              next = ptr;
        }
    }

    return next;
}

/**
 * Internal utility to parse an expression.  This may be called during both
 * the parse and link phases.  During the parse phase mLine and mLineNumber
 * will be valid.  During the link phase these are obtained from the 
 * ScriptStatement which must have saved them.
 *
 * This is only called during the link phase for ScriptFunctionStatement
 * and I'm not sure why. 
 */
PUBLIC ExNode* ScriptCompiler::parseExpression(ScriptStatement* stmt,
                                               const char* src)
{
	ExNode* expr = NULL;

    // should have one by now
    if (mParser == NULL)
      mParser = new ExParser();

	expr = mParser->parse(src);

    const char* error = mParser->getError();
	if (error != NULL) {
		// !! need a console or something for these
		char buffer[1024];
		const char* earg = mParser->getErrorArg();
		if (earg == NULL || strlen(earg) == 0)
		  strcpy(buffer, error);
		else 
		  sprintf(buffer, "%s (%s)", error, earg);

        int line = mLineNumber;
        if (line <= 0) {
            // we must be linking get it from the statement
            line = stmt->getLineNumber();
        }

		Trace(1, "ERROR: %s at line %ld\n", buffer, line);
		Trace(1, "--> file: %s\n", mScript->getFilename());
        // we'll have this during parsing but not linking!
        if (strlen(mLine) > 0)
          Trace(1, "--> line: %s", mLine);
		Trace(1, "--> expression: %s\n", src);
	}

	return expr;
}

/**
 * Generic syntax error callback.
 */
PUBLIC void ScriptCompiler::syntaxError(ScriptStatement* stmt, const char* msg)
{
    int line = mLineNumber;
    if (line <= 0) {
        // we must be linking get it from the statement
        line = stmt->getLineNumber();
    }

    Trace(1, "ERROR: %s at line %ld\n", msg, (long)line);
    Trace(1, "--> file: %s\n", mScript->getFilename());
    // we'll have this during parsing but not linking!
    if (strlen(mLine) > 0)
      Trace(1, "--> line: %s", mLine);
}

/**
 * Resolve references to other scripts during the link phase.
 *
 * !! Would also be interesting to resolve to procs in other scripts
 * so we could have "library" scripts.  This will be a problem for 
 * autoload though because we would have a reference to something
 * within the Script that will become invalid if it loads again.
 * Need an indirect reference on the Script.  Maybe a hashtable
 * keyed by name?
 *
 * Originally it assumed the argument was either an absolute or relative
 * file name.  If relative we would look in the directory containing the
 * parent script.  
 *
 * Now scripts may be referenced in these ways:
 *
 *    - leaf file name without extension: foo
 *    - leaf file name with extension: foo.mos
 *    - value of !name
 *
 * We don't care which directory the referenced script came from, this
 * makes it slightly easier to have duplicate names but usually there
 * will be only one directory. 
 *
 * This can be called in two contexts: when compiling an entire ScriptConfig
 * and when recompiling an individual script with !autoload.  In the
 * first case we only look at the local mScripts list since mEnv
 * may have things that are no longer configured.  In the second
 * case we must use what is in mEnv.
 */
PUBLIC Script* ScriptCompiler::resolveScript(const char* name)
{
	Script* found = NULL;

    if (mScripts != NULL) {
        // must be doing a full ScriptConfig compile
        found = resolveScript(mScripts, name);
    }
    else if (mEnv != NULL) {
        // fall back to ScriptEnv
        found = resolveScript(mEnv->getScripts(), name);
    }

    return found;
}

PRIVATE Script* ScriptCompiler::resolveScript(Script* scripts, const char* name)
{
	Script* found = NULL;

    if (name != NULL)  {
        for (Script* s = scripts ; s != NULL ; s = s->getNext()) {

            // check the !name
            // originally case sensitive but since we're insensntive
            // most other places be here too
            const char* sname = s->getName();
            if (StringEqualNoCase(name, sname))
              found = s;
            else {
                // check leaf filename
                const char* fname = s->getFilename();
                if (fname != NULL) {
                    char lname[1024];
                    GetLeafName(fname, lname, true);
						
                    if (StringEqualNoCase(name, lname)) {
                        // exact name match
                        found = s;
                    }
                    else if (EndsWithNoCase(lname, ".mos") && 
                             !EndsWithNoCase(name, ".mos")) {

                        // tolerate missing extensions in the call
                        int dot = LastIndexOf(lname, ".");
                        lname[dot] = 0;
							
                        if (StringEqualNoCase(name, lname))
                          found = s;
                    }
                }
            }
        }
    }

    if (found != NULL) {
		Trace(2, "ScriptEnv: Reference %s resolved to script %s\n",
			  name, found->getFilename());
	}

	return found;
}

/****************************************************************************
 *                                                                          *
 *                                   STACK                                  *
 *                                                                          *
 ****************************************************************************/

ScriptStack::ScriptStack()
{
    // see comments in init() for why this has to be set NULL first
    mArguments = NULL;
	init();
}

/**
 * Called to initialize a stack frame when it is allocated
 * for the first time and when it is removed from the pool.
 * NOTE: Handling of mArguments is special because we own it, 
 * everything else is just a reference we can NULL.  The constructor
 * must set mArguments to NULL before calling this.
 */
void ScriptStack::init()
{
    mStack = NULL;
	mScript = NULL;
    mCall = NULL;
	mIterator = NULL;
    mLabel = NULL;
    mSaveStatement = NULL;
	mWait = NULL;
	mWaitEvent = NULL;
	mWaitThreadEvent = NULL;
	mWaitFunction = NULL;
	mWaitBlock = false;
	mMax = 0;
	mIndex = 0;

	for (int i = 0 ; i < MAX_TRACKS ; i++)
	  mTracks[i] = NULL;

    // This is the only thing we own
    delete mArguments;
    mArguments = NULL;
}

ScriptStack::~ScriptStack()
{
    // !! need to be pooling these
    delete mArguments;
}

void ScriptStack::setScript(Script* s)
{
    mScript = s;
}

Script* ScriptStack::getScript()
{
    return mScript;
}

void ScriptStack::setProc(ScriptProcStatement* p)
{
    mProc = p;
}

ScriptProcStatement* ScriptStack::getProc()
{
    return mProc;
}

void ScriptStack::setStack(ScriptStack* s)
{
    mStack = s;
}

ScriptStack* ScriptStack::getStack()
{
    return mStack;
}

void ScriptStack::setCall(ScriptCallStatement* call)
{
    mCall = call;
}

ScriptCallStatement* ScriptStack::getCall()
{
    return mCall;
}

void ScriptStack::setArguments(ExValueList* args)
{
    delete mArguments;
    mArguments = args;
}

ExValueList* ScriptStack::getArguments()
{
    return mArguments;
}

void ScriptStack::setIterator(ScriptIteratorStatement* it)
{
    mIterator = it;
}

ScriptIteratorStatement* ScriptStack::getIterator()
{
    return mIterator;
}

void ScriptStack::setLabel(ScriptLabelStatement* it)
{
    mLabel = it;
}

ScriptLabelStatement* ScriptStack::getLabel()
{
    return mLabel;
}

void ScriptStack::setSaveStatement(ScriptStatement* it)
{
    mSaveStatement = it;
}

ScriptStatement* ScriptStack::getSaveStatement()
{
    return mSaveStatement;
}

ScriptStatement* ScriptStack::getWait()
{
	return mWait;
}

void ScriptStack::setWait(ScriptStatement* wait)
{
	mWait = wait;
}

Event* ScriptStack::getWaitEvent()
{
	return mWaitEvent;
}

void ScriptStack::setWaitEvent(Event* e)
{
	mWaitEvent = e;
}

ThreadEvent* ScriptStack::getWaitThreadEvent()
{
	return mWaitThreadEvent;
}

void ScriptStack::setWaitThreadEvent(ThreadEvent* e)
{
	mWaitThreadEvent = e;
}

Function* ScriptStack::getWaitFunction()
{
	return mWaitFunction;
}

void ScriptStack::setWaitFunction(Function* e)
{
	mWaitFunction = e;
}

bool ScriptStack::isWaitBlock()
{
	return mWaitBlock;
}

void ScriptStack::setWaitBlock(bool b)
{
	mWaitBlock = b;
}

/**
 * Called by ScriptForStatement to add a track to the loop.
 */
PUBLIC void ScriptStack::addTrack(Track* t)
{
    if (mMax < MAX_TRACKS)
      mTracks[mMax++] = t;
}

/**
 * Called by ScriptForStatement to advance to the next track.
 */
PUBLIC Track* ScriptStack::nextTrack()
{
    Track* next = NULL;

    if (mIndex < mMax) {
        mIndex++;
        next = mTracks[mIndex];
    }
    return next;
}

/**
 * Called by ScriptRepeatStatement to set the iteration count.
 */
PUBLIC void ScriptStack::setMax(int max)
{
	mMax = max;
}

PUBLIC int ScriptStack::getMax()
{
	return mMax;
}

/**
 * Called by ScriptRepeatStatement to advance to the next iteration.
 * Return true if we're done.
 */
PUBLIC bool ScriptStack::nextIndex()
{
	bool done = false;

	if (mIndex < mMax)
	  mIndex++;

	if (mIndex >= mMax)
	  done = true;

	return done;
}

/**
 * Determine the target track if we're in a For statement.
 * It is possible to have nested iterations, so search upward
 * until we find a For.  Nested fors don't make much sense, but
 * a nested for/repeat might be useful.  
 */
PUBLIC Track* ScriptStack::getTrack()
{
	Track* track = NULL;
	ScriptStack* stack = this;
	ScriptStack* found = NULL;

	// find the innermost For iteration frame
	while (found == NULL && stack != NULL) {
		ScriptIteratorStatement* it = stack->getIterator();
		if (it != NULL && it->isFor())
		  found = stack;
		else
		  stack = stack->getStack();
	}

	if (found != NULL && found->mIndex < found->mMax)
	  track = found->mTracks[found->mIndex];

	return track;
}

/**
 * Notify wait frames on the stack of the completion of a function.
 * 
 * Kludge for Wait switch, since we no longer have a the "fundamental"
 * command concept, assume that waiting for a function with
 * the SwitchEvent event type will end the wait on any of them,
 * need a better way to declare this.
 */
PUBLIC bool ScriptStack::finishWait(Function* f)
{
	bool finished = false;

	if (mWaitFunction != NULL && 
		(mWaitFunction == f ||
		 (mWaitFunction->eventType == SwitchEvent && 
		  f->eventType == SwitchEvent))) {
			
		Trace(3, "Script end wait function %s\n", f->getName());
		mWaitFunction = NULL;
		finished = true;
	}

	// maybe an ancestor is waiting
	// this should only happen if an async notification frame got pushed on top
	// of the wait frame
	// only return true if the current frame was waiting, not an ancestor
	// becuase we're still executing in the current frame and don't want to 
	// recursively call run() again
	if (mStack != NULL) 
	  mStack->finishWait(f);

	return finished;
}

/**
 * Notify wait frames on the stack of the completion of an event.
 * Return true if we found this event on the stack.  This used when
 * canceling events so we can emit some diagnostic messages.
 */
PUBLIC bool ScriptStack::finishWait(Event* e)
{
	bool finished = false;

	if (mWaitEvent == e) {
		mWaitEvent = NULL;
		finished = true;
	}

	if (mStack != NULL) {
		if (mStack->finishWait(e))
		  finished = true;
	}

	return finished;
}

/**
 * Called as events are rescheduled into new events.
 * If we had been waiting on the old event, have to start wanting on the new.
 */
PUBLIC bool ScriptStack::changeWait(Event* orig, Event* neu)
{
	bool found = false;

	if (mWaitEvent == orig) {
		mWaitEvent = neu;
		found = true;
	}

	if (mStack != NULL) {
		if (mStack->changeWait(orig, neu))
		  found = true;
	}

	return found;
}

/**
 * Notify wait frames on the stack of the completion of a thread event.
 */
PUBLIC bool ScriptStack::finishWait(ThreadEvent* e)
{
	bool finished = false;
 
	if (mWaitThreadEvent == e) {
		mWaitThreadEvent = NULL;
		finished = true;
	}

	if (mStack != NULL) {
		if (mStack->finishWait(e))
		  finished = true;
	}

	return finished;
}

PUBLIC void ScriptStack::finishWaitBlock()
{
	mWaitBlock = false;
	if (mStack != NULL)
	  mStack->finishWaitBlock();
}

/**
 * Cancel all wait blocks.
 *
 * How can there be waits on the stack?  Wait can only
 * be on the bottom most stack block, right?
 */
PUBLIC void ScriptStack::cancelWaits()
{
	if (mWaitEvent != NULL) {
        // will si->getTargetTrack always be right nere?
        // can't get to it anyway, assume the Event knows
        // the track it is in
        Track* track = mWaitEvent->getTrack();
        if (track == NULL) {
            Trace(1, "Wait event without target track!\n");
        }
        else {
            mWaitEvent->setScript(NULL);

            EventManager* em = track->getEventManager();
            em->freeEvent(mWaitEvent);

            mWaitEvent = NULL;
        }
    }

	if (mWaitThreadEvent != NULL)
	  mWaitThreadEvent = NULL;

	mWaitFunction = NULL;
	mWaitBlock = false;

	if (mStack != NULL) {
		mStack->cancelWaits();
		mStack->finishWaitBlock();
	}
}

/****************************************************************************
 *                                                                          *
 *                                 SCRIPT USE                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptUse::ScriptUse(Parameter* p)
{
    mNext = NULL;
    mParameter = p;
    mValue.setNull();
}

PUBLIC ScriptUse::~ScriptUse()
{
	ScriptUse *el, *next;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC void ScriptUse::setNext(ScriptUse* next) 
{
    mNext = next;
}

PUBLIC ScriptUse* ScriptUse::getNext()
{
    return mNext;
}

PUBLIC Parameter* ScriptUse::getParameter()
{
    return mParameter;
}

PUBLIC ExValue* ScriptUse::getValue()
{
    return &mValue;
}

/****************************************************************************
 *                                                                          *
 *   							 INTERPRETER                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptInterpreter::ScriptInterpreter()
{
	init();
}

PUBLIC ScriptInterpreter::ScriptInterpreter(Mobius* m, Track* t)
{
	init();
	mMobius = m;
	mTrack = t;
}

PUBLIC void ScriptInterpreter::init()
{
	mNext = NULL;
    mNumber = 0;
    mTraceName[0] = 0;
	mMobius = NULL;
	mTrack = NULL;
	mScript = NULL;
    mUses = NULL;
    mStack = NULL;
    mStackPool = NULL;
	mStatement = NULL;
	mVariables = NULL;
    mTrigger = NULL;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mLastEvent = NULL;
	mLastThreadEvent = NULL;
	mReturnCode = 0;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;
    mAction = NULL;
    mExport = NULL;
}

PUBLIC ScriptInterpreter::~ScriptInterpreter()
{
	if (mStack != NULL)
	  mStack->cancelWaits();

    // do this earlier?
    restoreUses();

    delete mUses;
    delete mAction;
    delete mExport;

}

PUBLIC void ScriptInterpreter::setNext(ScriptInterpreter* si)
{
	mNext = si;
}

PUBLIC ScriptInterpreter* ScriptInterpreter::getNext()
{
	return mNext;
}

PUBLIC void ScriptInterpreter::setNumber(int n) 
{
    mNumber = n;
}

PUBLIC int ScriptInterpreter::getNumber()
{
	return mNumber;
}

PUBLIC void ScriptInterpreter::setMobius(Mobius* m)
{
	mMobius = m;
}

PUBLIC Mobius* ScriptInterpreter::getMobius()
{
	return mMobius;
}

/**
 * Allocation an Action we can use when setting parameters.
 * We make one for function invocation too but that's more 
 * complicated and can schedule events.
 *
 * These won't have a ResolvedTarget since we've already
 * got the Parameter and we're calling it directly.
 */
PUBLIC Action* ScriptInterpreter::getAction()
{
    if (mAction == NULL) {
        mAction = mMobius->newAction();
        mAction->trigger = TriggerScript;
        
        // this is redundant because we also check for Target types, but
        // it would be simpler if we could just look at this...
        mAction->inInterrupt = true;

        // function action needs this for GlobalReset handling
        // I don't think Parameter actions do
        mAction->id = (long)this;
    }
    return mAction;
}

/**
 * Allocate an Export we can use when reading parameters.
 * We'll set the resolved track later.  This won't
 * have a ResolvedTarget since we've already got the
 * Parameter and will be using that directly.
 */
PUBLIC Export* ScriptInterpreter::getExport()
{
    if (mExport == NULL) {
        mExport = new Export(mMobius);
    }
    return mExport;
}

/**
 * Find a suitable name to include in trace messages so we have
 * some idea of what script we're dealing with.
 */
PUBLIC const char* ScriptInterpreter::getTraceName()
{
    if (mTraceName[0] == 0) {
        const char* name = "???";
        if (mScript != NULL)
          name = mScript->getDisplayName();

        sprintf(mTraceName, "%d:", mNumber);
        int len = strlen(mTraceName);

        AppendString(name, &mTraceName[len], MAX_TRACE_NAME - len - 1);
    }
    return mTraceName;
}

PUBLIC void ScriptInterpreter::setTrack(Track* m)
{
	mTrack = m;
}

PUBLIC Track* ScriptInterpreter::getTrack()
{
	return mTrack;
}

PUBLIC Track* ScriptInterpreter::getTargetTrack()
{
	Track* target = mTrack;
	if (mStack != NULL) {
		Track* t = mStack->getTrack();
		if (t != NULL)
		  target = t;
	}
	return target;
}

ScriptStack* ScriptInterpreter::getStack()
{
    return mStack;
}

PUBLIC bool ScriptInterpreter::isPostLatency()
{
	return mPostLatency;
}

PUBLIC void ScriptInterpreter::setPostLatency(bool b)
{
	mPostLatency = b;
}

PUBLIC int ScriptInterpreter::getSustainedMsecs()
{
	return mSustainedMsecs;
}

PUBLIC void ScriptInterpreter::setSustainedMsecs(int c)
{
	mSustainedMsecs = c;
}

PUBLIC int ScriptInterpreter::getSustainCount()
{
	return mSustainCount;
}

PUBLIC void ScriptInterpreter::setSustainCount(int c)
{
	mSustainCount = c;
}

PUBLIC bool ScriptInterpreter::isSustaining()
{
	return mSustaining;
}

PUBLIC void ScriptInterpreter::setSustaining(bool b)
{
	mSustaining = b;
}

PUBLIC int ScriptInterpreter::getClickedMsecs()
{
	return mClickedMsecs;
}

PUBLIC void ScriptInterpreter::setClickedMsecs(int c)
{
	mClickedMsecs = c;
}

PUBLIC int ScriptInterpreter::getClickCount()
{
	return mClickCount;
}

PUBLIC void ScriptInterpreter::setClickCount(int c)
{
	mClickCount = c;
}

PUBLIC bool ScriptInterpreter::isClicking()
{
	return mClicking;
}

PUBLIC void ScriptInterpreter::setClicking(bool b)
{
	mClicking = b;
}

/**
 * Save some things about the trigger that we can reference
 * later through ScriptVariables.
 *
 * TODO: Should we just clone the whole damn action?
 */
PUBLIC void ScriptInterpreter::setTrigger(Action* action)
{
    if (action == NULL) {
        mTrigger = NULL;
        mTriggerId = 0;
        mTriggerValue = 0;
        mTriggerOffset = 0;
    }
    else {
        mTrigger = action->trigger;
        mTriggerId = action->id;
        mTriggerValue = action->triggerValue;
        mTriggerOffset = action->triggerOffset;
    }
}

PUBLIC Trigger* ScriptInterpreter::getTrigger()
{
    return mTrigger;
}

PUBLIC int ScriptInterpreter::getTriggerId()
{
    return mTriggerId;
}

PUBLIC int ScriptInterpreter::getTriggerValue()
{
    return mTriggerValue;
}

PUBLIC int ScriptInterpreter::getTriggerOffset()
{
    return mTriggerOffset;
}

PUBLIC bool ScriptInterpreter::isTriggerEqual(Action* action)
{
    return (action->trigger == mTrigger && action->id == mTriggerId);
}

PUBLIC void ScriptInterpreter::reset()
{
	mStatement = NULL;
    mTrigger = NULL;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;

	delete mVariables;
	mVariables = NULL;

    while (mStack != NULL)
      popStack();

	if (mScript != NULL) {
        ScriptBlock* block = mScript->getBlock();
        if (block != NULL)
          mStatement = block->getStatements();
    }

    // this?
    restoreUses();
}

PUBLIC void ScriptInterpreter::setScript(Script* s, bool inuse)
{
	reset();
	mScript = s;

	// kludge, do not refesh if the script is currently in use
	if (!inuse && s->isAutoLoad()) {
        ScriptCompiler* comp = new ScriptCompiler();
        comp->recompile(mMobius, s);
        delete comp;
    }

    ScriptBlock* block = s->getBlock();
    if (block != NULL)
      mStatement = block->getStatements();
}

/**
 * Formerly have been assuming that the Script keeps getting pushed
 * up the stack, but that's unreliable.  We need to be looking down the stack.
 */
Script* ScriptInterpreter::getScript()
{
	// find the first script on the stack
	Script* stackScript = NULL;

	for (ScriptStack* stack = mStack ; stack != NULL && stackScript == NULL ; 
		 stack = stack->getStack()) {
		stackScript = stack->getScript();
	}

	return (stackScript != NULL) ? stackScript : mScript;
}

PUBLIC bool ScriptInterpreter::isFinished()
{
	return (mStatement == NULL && !mSustaining && !mClicking);
}

/**
 * Return code accessor for the "returnCode" script variable.
 */
PUBLIC int ScriptInterpreter::getReturnCode()
{
	return mReturnCode;
}

PUBLIC void ScriptInterpreter::setReturnCode(int i) 
{
	mReturnCode = i;
}

/**
 * Add a use rememberance.  Only do this once.
 */
PUBLIC void ScriptInterpreter::use(Parameter* p)
{
    ScriptUse* found = NULL;

    for (ScriptUse* use = mUses ; use != NULL ; use = use->getNext()) {
        if (StringEqual(use->getParameter()->getName(), p->getName())) {
            found = use;
            break;
        }
    }

    if (found == NULL) {
        ScriptUse* use = new ScriptUse(p);
        ExValue* value = use->getValue();
        getParameter(p, value);
        use->setNext(mUses);
        mUses = use;
    }
}

/**
 * Restore the uses when the script ends.
 */
PRIVATE void ScriptInterpreter::restoreUses()
{
    for (ScriptUse* use = mUses ; use != NULL ; use = use->getNext()) {

        Parameter* p = use->getParameter();
        const char* name = p->getName();
        ExValue* value = use->getValue();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        // can resuse this unless it schedules
        Action* action = getAction();
        if (p->scheduled)
          action = getMobius()->cloneAction(action);

        action->arg.set(value);

        if (p->scope == PARAM_SCOPE_GLOBAL) {
            Trace(2, "Script %s: restoring global parameter %s = %s\n",
                  getTraceName(), name, traceval);
            action->setResolvedTrack(NULL);
            p->setValue(action);
        }
        else {
            Trace(2, "Script %s: restoring track parameter %s = %s\n", 
                  getTraceName(), name, traceval);
            action->setResolvedTrack(getTargetTrack());
            p->setValue(action);
        }

        if (p->scheduled)
          getMobius()->completeAction(action);
	}

    delete mUses;
    mUses = NULL;

}

/**
 * Get the value of a parameter.
 */
PUBLIC void ScriptInterpreter::getParameter(Parameter* p, ExValue* value)
{
    Export* exp = getExport();

    if (p->scope == PARAM_SCOPE_GLOBAL) {
        exp->setTrack(NULL);
        p->getValue(exp, value); 
    }
    else {
        exp->setTrack(getTargetTrack());
        p->getValue(exp, value);
    }
}

/****************************************************************************
 *                                                                          *
 *                            INTERPRETER CONTROL                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods called by Track to control the interpreter.
 * Other than the constructors, these are the only true "public" methods.
 * The methods called by all the handlers should be protected, but I 
 * don't want to mess with a billion friends.
 */

/**
 * Called by Track during event processing and at various points when a
 * function has been invoked.  Advance if we've been waiting on this function.
 *
 * Function may be null here for certain events like ScriptEvent.
 * Just go ahread and run.
 *
 * !! Need to sort out whether we want on the invocation of the
 * function or the event that completes the function.
 * 
 * !! Should this be waiting for event types?  The function
 * here could be an alternate ending which will confuse the script
 * 
 * !! The combined waits could address this though in an inconvenient
 * way, would be nice to have something like "Wait Switch any"
 * 
 */
PUBLIC void ScriptInterpreter::resume(Function* func)
{
	// if we have no stack, then can't be waiting
	if (mStack != NULL) {
		// note that we can't run() unless we were actually waiting, otherwise
		// we'll be here for most functions we actually *call* from the script
		// which causes a stack overflow
		if (mStack->finishWait(func))
		  run(false);
	}
}

/**
 * Called by MobiusThread when it finishes processing events we scheduled.
 * Note we don't run here since we're not in the audio interrupt thread.
 * Just remove the reference, the script will advance on the next interrupt.
 */
PUBLIC void ScriptInterpreter::finishEvent(ThreadEvent* te)
{
	bool ours = false;

	if (mStack != NULL)
	  ours = mStack->finishWait(te);

	// Since we're dealing with another thread, it is possible
	// that the thread could notify us before the interpreter gets
	// to a "Wait thread", it is important that we NULL out the last
	// thread event so the Wait does't try to wait for an invalid event.

	if (mLastThreadEvent == te) {
		mLastThreadEvent = NULL;
		ours = true;
	}

	// If we know this was our event, capture the return code for
	// later use in scripts.  
	if (ours)
	  mReturnCode = te->getReturnCode();
}

/**
 * Called by Loop after it processes any Event that has an attached
 * interpreter.  Check to see if we've met an event wait condition.
 * Can get here with ScriptEvents, but we will have already handled
 * those in the scriptEvent method below.
 */
void ScriptInterpreter::finishEvent(Event* event)
{
	if (mStack != NULL) {
		mStack->finishWait(event);

		// Make sure the last function state no longer references this event,
		// just in case there is another Wait last
		if (mLastEvent == event)
		  mLastEvent = NULL;

		// Kludge: Need to detect changes to the selected track and change
		// what we think the default track is.  No good way to encapsulate
		// this so look for specific function families.
		if (event->type == TrackEvent || event->function == GlobalReset) {
			// one of the track select functions, change the default track
			setTrack(mMobius->getTrack());
		}

		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Must be called when an event is canceled. So any waits can end.
 */
bool ScriptInterpreter::cancelEvent(Event* event)
{
	bool canceled = false;

	if (mStack != NULL)
	  canceled = mStack->finishWait(event);

	// Make sure the last function state no longer references this event,
	// just in case there is another Wait last.
 	if (mLastEvent == event)
	  mLastEvent = NULL;

	return canceled;
}

/**
 * Handler for a ScriptEvent scheduled in a track.
 */
void ScriptInterpreter::scriptEvent(Loop* l, Event* event)
{
	if (mStack != NULL) {
		mStack->finishWait(event);
		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Called when a placeholder event has been rescheduled.
 * If there was a Wait for the placeholder event, switch the wait event
 * to the new event.
 */
PUBLIC void ScriptInterpreter::rescheduleEvent(Event* src, Event* neu)
{
	if (neu != NULL) {

		if (mStack != NULL) {
			if (mStack->changeWait(src, neu))
			  neu->setScript(this);
		}

		// this should only be the case if we did a Wait last, not
		// sure this can happen?
		if (mLastEvent == src) {
			mLastEvent = neu;
			neu->setScript(this);
		}
	}
}

/**
 * Called by Track at the beginning of each interrupt.
 * Factored out so we can tell if we're exactly at the start of a block,
 * or picking up in the middle.
 */
PUBLIC void ScriptInterpreter::run()
{
	run(true);
}

/**
 * Called at the beginning of each interrupt, or after
 * processing a ScriptEvent event.  Execute script statements in the context
 * of the parent track until we reach a wait state.
 *
 * Operations are normally performed on the parent track.  If
 * the script contains a FOR statement, the operations within the FOR
 * will be performed for each of the tracks specified in the FOR.  
 * But note that the FOR runs serially, not in parallel so if there
 * is a Wait statement in the loop, you will suspend in that track
 * waiting for the continuation event.
 */
PRIVATE void ScriptInterpreter::run(bool block)
{
	if (block && mStack != NULL)
	  mStack->finishWaitBlock();

	// remove the wait frame if we can
	checkWait();

	while (mStatement != NULL && !isWaiting()) {

        ScriptStatement* next = mStatement->eval(this);

		// evaluator may return the next statement, otherwise follow the chain
        if (next != NULL) {
            mStatement = next;
        }
        else if (mStatement == NULL) {
            // evaluating the last statement must have reset the script,
            // this isn't supposed to happen, but I suppose it could if
            // we allow scripts to launch other script threads and the
            // thread we launched immediately reset?
            Trace(1, "Script: Script was reset during execution!\n");
        }
        else if (!isWaiting()) {

			if (mStatement->isEnd())
			  mStatement = NULL;
			else
			  mStatement = mStatement->getNext();

			// if we hit an end statement, or fall off the end of the list, 
			// pop the stack 
			while (mStatement == NULL && mStack != NULL) {
				mStatement = popStack();
				// If we just exposed a Wait frame that has been satisfied, 
				// can pop it too.  This should only come into play if we just
				// finished an async notification.
				checkWait();
			}
		}
	}
    
    // !! if mStatement is NULL should we restoreUses now or wait
    // for Mobius to do it?  Could be some subtle timing if several
    // scripts use the same parameter

}

/**
 * If there is a wait frame on the top of the stack, and all the
 * wait conditions have been satisfied, remove it.
 */
PRIVATE void ScriptInterpreter::checkWait()
{
	if (isWaiting()) {
		if (mStack->getWaitFunction() == NULL &&
			mStack->getWaitEvent() == NULL &&
			mStack->getWaitThreadEvent() == NULL &&
			!mStack->isWaitBlock()) {

			// nothing left to live for...
			do {
				mStatement = popStack();
			}
			while (mStatement == NULL && mStack != NULL);
		}
	}
}

/**
 * Advance to the next ScriptStatement, popping the stack if necessary.
 */
PRIVATE void ScriptInterpreter::advance()
{
	if (mStatement != NULL) {

		if (mStatement->isEnd())
		  mStatement = NULL;
		else
          mStatement = mStatement->getNext();

        // when finished with a called script, pop the stack
        while (mStatement == NULL && mStack != NULL) {
			mStatement = popStack();
		}
    }
}

/**
 * Called when the script is supposed to unconditionally terminate.
 * Currently called by Track when it processes a GeneralReset or Reset
 * function that was performed outside the script.  Will want a way
 * to control this using script directives?
 */
PUBLIC void ScriptInterpreter::stop()
{
    // will also restore uses...
	reset();
	mStatement = NULL;
}

/**
 * Jump to a notification label.
 * These must happen while the interpreter is not running!
 */
PUBLIC void ScriptInterpreter::notify(ScriptStatement* s)
{
	if (s == NULL)
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a statement!\n",
            getTraceName());

	else if (!s->isLabel())
	  // restict this to labels for now, though should support procs
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a label!\n",
            getTraceName());

	else {
		pushStack((ScriptLabelStatement*)s);
		mStatement = s;
	}
}

/****************************************************************************
 *                                                                          *
 *                             INTERPRETER STATE                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods that control the state of the interpreter called
 * by the stament evaluator methods.
 */

/**
 * Return true if any of the wait conditions are set.
 * If we're in an async notification return false so we can proceed
 * evaluating the notification block, leaving the waits in place.
 */
PUBLIC bool ScriptInterpreter::isWaiting()
{
	return (mStack != NULL && mStack->getWait() != NULL);
}

PUBLIC UserVariables* ScriptInterpreter::getVariables()
{
    if (mVariables == NULL)
      mVariables = new UserVariables();
    return mVariables;
}

/**
 * Schedule a Mobius ThreadEvent.
 * Add ourselves as the interested script interpreter so that we
 * may be notified when the event has been processed.
 *
 */
PUBLIC void ScriptInterpreter::scheduleThreadEvent(ThreadEvent* e)
{
	// this is now the "last" thing we can wait for
	// do this before passing to the thread so we can get notified
	mLastThreadEvent = e;
    
    MobiusThread* t = mMobius->getThread();
    t->addEvent(e);
}

/**
 * Called after we've processed a function and it scheduled an event.
 * Since events may not be scheduled, be careful not to trash state
 * left behind by earlier functions.
 */
PUBLIC void ScriptInterpreter::setLastEvents(Action* a)
{
	if (a->getEvent() != NULL) {
		mLastEvent = a->getEvent();
		mLastEvent->setScript(this);
	}

	if (a->getThreadEvent() != NULL) {
		mLastThreadEvent = a->getThreadEvent();
		// Note that ThreadEvents don't point back to the ScriptInterpreter
		// because the interpreter may be gone by the time the thread
		// event finishes.  Mobius will forward thread event completion
		// to all active interpreters.
	}
}

/**
 * Initialize a wait for the last function to complete.
 * Completion is determined by waiting for either the Event or
 * ThreadEvent that was scheduled by the last function.
 */
PUBLIC void ScriptInterpreter::setupWaitLast(ScriptStatement* src)
{
	if (mLastEvent != NULL) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitEvent(mLastEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// This can often happen if there is a "Wait last" after
		// an Undo or another function that has the scriptSync flag
		// which will cause an automatic wait.   Just ignore it.
	}
}

PUBLIC void ScriptInterpreter::setupWaitThread(ScriptStatement* src)
{
	if (mLastThreadEvent != NULL) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitThreadEvent(mLastThreadEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// not sure if there are common reasons for this, but
		// if you try to wait for something that isn't there,
		// just return immediately
	}
}

/**
 * Allocate a stack frame, from the pool if possible.
 */
PRIVATE ScriptStack* ScriptInterpreter::allocStack()
{
    ScriptStack* s = NULL;
    if (mStackPool == NULL)
      s = new ScriptStack();
    else {
        s = mStackPool;
        mStackPool = s->getStack();
        s->init();
    }
	return s;
}

/**
 * Push a call frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptCallStatement* call, 
										  Script* sub,
                                          ScriptProcStatement* proc,
                                          ExValueList* args)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setCall(call);
    s->setScript(sub);
    s->setProc(proc);
    s->setArguments(args);
    mStack = s;

    return s;
}

/**
 * Push an iteration frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptIteratorStatement* it)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setIterator(it);
	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a notification frame on the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptLabelStatement* label)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setLabel(label);
    s->setSaveStatement(mStatement);

	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a wait frame onto the stack.
 * !! can't we consistently use pending events for waits?
 */
ScriptStack* ScriptInterpreter::pushStackWait(ScriptStatement* wait)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setWait(wait);

	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);

    mStack = s;

    return s;
}

/**
 * Pop a frame from the stack.
 * Return the next statement to evaluate if we know it.
 */
ScriptStatement* ScriptInterpreter::popStack()
{
    ScriptStatement *next = NULL;

    if (mStack != NULL) {
        ScriptStack* parent = mStack->getStack();

        ScriptStatement* st = mStack->getCall();
		if (st != NULL) {
			// resume after the call
			next = st->getNext();
		}
		else {
			st = mStack->getSaveStatement();
			if (st != NULL) {
				// must have been a asynchronous notification, return to the 
				// previous statement
				next = st;
			}
			else {
				st = mStack->getWait();
				if (st != NULL) {
					// resume after the wait
					next = st->getNext();
				}
				else {
					// iterators handle the next statement themselves
				}
			}
		}

        mStack->setStack(mStackPool);
        mStackPool = mStack;
        mStack = parent;
    }

    return next;
}

/**
 * Called by ScriptArgument and ScriptResolver to derive the value of
 * a stack argument.
 *
 * Recurse up the stack until we see a frame for a CallStatement,
 * then select the argument that was evaluated when the frame was pushed.
 */
PUBLIC void ScriptInterpreter::getStackArg(int index, ExValue* value)
{
	value->setNull();

    getStackArg(mStack, index, value);
}

/**
 * Inner recursive stack walker looking for args.
 */
PRIVATE void ScriptInterpreter::getStackArg(ScriptStack* stack,
                                            int index, ExValue* value)
{
    if (stack != NULL && index >= 1 && index <= MAX_ARGS) {
        ScriptCallStatement* call = stack->getCall();
		if (call == NULL) {
			// must be an iteration frame, recurse up
            getStackArg(stack->getStack(), index, value);
		}
		else {
            ExValueList* args = stack->getArguments();
            if (args != NULL) {
                // arg indexes in the script are 1 based
                ExValue* arg = args->getValue(index - 1);
                if (arg != NULL) {
                    // copy the stack argument to the return value
                    // if the arg contains a list (rare) the reference is
                    // transfered but it is not owned by the new value
                    value->set(arg);
                }
            }
        }
    }
}

/**
 * Run dynamic expansion on file path.
 * After expansion we prefix the base directory of the current script
 * if the resulting path is not absolute.
 *
 * TODO: Would be nice to have variables to get to the
 * installation and configuration directories.
 */
void ScriptInterpreter::expandFile(const char* value, ExValue* retval)
{
	retval->setNull();

    // first do basic expansion
    expand(value, retval);
	
	char* buffer = retval->getBuffer();
	int curlen = strlen(buffer);

    if (curlen > 0 && !IsAbsolute(buffer)) {
		if (StartsWith(buffer, "./")) {
			// a signal to put it in the currrent working directory
			for (int i = 0 ; i <= curlen - 2 ; i++)
			  buffer[i] = buffer[i+2];
		}
		else {
            // relative to the script directory
			Script* s = getScript();
			const char* dir = s->getDirectory();
			if (dir != NULL) {
				int insertlen = strlen(dir);
                int shiftlen = insertlen;
                bool needslash = false;
                if (insertlen > 0 && dir[insertlen] != '/' &&
                    dir[insertlen] != '\\') {
                    needslash = true;
                    shiftlen++;
                }

				int i;
				// shift down, need to check overflow!!
				for (i = curlen ; i >= 0 ; i--)
				  buffer[shiftlen+i] = buffer[i];
			
				// insert the prefix
				for (i = 0 ; i < insertlen ; i++)
				  buffer[i] = dir[i];

                if (needslash)
                  buffer[insertlen] = '/';
			}
		}
	}
}

/**
 * Called during statement evaluation to do dynamic reference expansion
 * for a statement argument, recursively walking up the call stack
 * if necessary.
 * 
 * We support multiple references in the string provided they begin with $.
 * Numeric references to stack arguments look like $1 $2, etc.
 * References to variables may look like $foo or $(foo) depending
 * on whether you have surrounding content that requries the () delimiters.
 */
PRIVATE void ScriptInterpreter::expand(const char* value, ExValue* retval)
{
    int len = (value != NULL) ? strlen(value) : 0;
	char* buffer = retval->getBuffer();
    char* ptr = buffer;
    int localmax = retval->getBufferMax() - 1;
	int psn = 0;

	retval->setNull();

    // keep this terminated
    if (localmax > 0) *ptr = 0;
    
	while (psn < len && localmax > 0) {
        char ch = value[psn];
        if (ch != '$') {
            *ptr++ = ch;
			psn++;
            localmax--;
        }
        else {
            psn++;
            if (psn < len) {
                // assume that variables can't start with numbers
                // so if we find one it is a numeric argument ref
				// acctually this breaks for "8thsPerCycle" so it has
				// to be surrounded by () but that's an alias now
                char digit = value[psn];
                int index = (int)(digit - '0');
                if (index >= 1 && index <= MAX_ARGS) {
					ExValue v;
					getStackArg(index, &v);
					CopyString(v.getString(), ptr, localmax);
					psn++;
                }
                else {
                    // isolate the reference name
                    bool delimited = false;
                    if (value[psn] == '(') {
                        delimited = true;
                        psn++;
                    }
                    if (psn < len) {
                        ScriptArgument arg;
                        char refname[MIN_ARG_VALUE];
                        const char* src = &value[psn];
                        char* dest = refname;
                        // !! bounds checking
                        while (*src && !isspace(*src) && 
							   (delimited || *src != ',') &&
                               (!delimited || *src != ')')) {
                            *dest++ = *src++;
                            psn++;
                        }	
                        *dest = 0;
                        if (delimited && *src == ')')
						  psn++;
                            
                        // resolution logic resides in here
                        arg.resolve(mMobius, mStatement->getParentBlock(), refname);
						if (!arg.isResolved())
						  Trace(1, "Script %s: Unresolved reference: %s\n", 
                                getTraceName(), refname);	

						ExValue v;
						arg.get(this, &v);
						CopyString(v.getString(), ptr, localmax);
                    }
                }
            }

            // advance the local buffer pointers after the last
            // substitution
            int insertlen = strlen(ptr);
            ptr += insertlen;
            localmax -= insertlen;
        }

        // keep it termianted
        *ptr = 0;
    }
}

/****************************************************************************
 *                                                                          *
 *                                 EXCONTEXT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * An array containing names of variables that may be set by the interpreter
 * but do not need to be declared.
 */
const char* InterpreterVariables[] = {
    "interrupted",
    NULL
};

/**
 * ExContext interface.
 * Given the a symbol in an expression, search for a parameter,
 * internal variable, or stack argument reference with the same name.
 * If one is found return an ExResolver that will be called during evaluation
 * to retrieve the value.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 *
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 * 
 */
PUBLIC ExResolver* ScriptInterpreter::getExResolver(ExSymbol* symbol)
{
	ExResolver* resolver = NULL;
	const char* name = symbol->getName();
	int arg = 0;

	// a leading $ is required for numeric stack argument references,
	// but must also support them for legacy symbolic references
	if (name[0] == '$') {
		name = &name[1];
		arg = ToInt(name);
	}

	if (arg > 0)
	  resolver = new ScriptResolver(symbol, arg);

    // next try internal variables
	if (resolver == NULL) {
		ScriptInternalVariable* iv = ScriptInternalVariable::getVariable(name);
		if (iv != NULL)
		  resolver = new ScriptResolver(symbol, iv);
	}
    
    // next look for a Variable in the innermost block
	if (resolver == NULL) {
        // we should only be called during evaluation!
        if (mStatement == NULL)
          Trace(1, "Script %s: getExResolver has no statement!\n", getTraceName());
        else {
            ScriptBlock* block = mStatement->getParentBlock();
            if (block == NULL)
              Trace(1, "Script %s: getExResolver has no block!\n", getTraceName());
            else {
                ScriptVariableStatement* v = block->findVariable(name);
                if (v != NULL)
                  resolver = new ScriptResolver(symbol, v);
            }
        }
    }

	if (resolver == NULL) {
		Parameter* p = mMobius->getParameter(name);
		if (p != NULL)
		  resolver = new ScriptResolver(symbol, p);
	}

    // try some auto-declared system variables
    if (resolver == NULL) {
        for (int i = 0 ; InterpreterVariables[i] != NULL ; i++) {
            if (StringEqualNoCase(name, InterpreterVariables[i])) {
                resolver = new ScriptResolver(symbol, name);
                break;
            }
        }
    }

	return resolver;
}

/**
 * ExContext interface.
 * Given the a symbol in non-built-in function call, search for a Proc
 * with a matching name.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 */
PUBLIC ExResolver* ScriptInterpreter::getExResolver(ExFunction* function)
{
	return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
