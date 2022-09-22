/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Data model, compiler and interpreter for a simple scripting language.
 *
 */

#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdio.h>

#include "Expr.h"
#include "Function.h"
#include "Parameter.h"
#include "Preset.h"

/**
 * For debugging, will become true when the Break statement is
 * evaluated in a script.
 */
extern bool ScriptBreak;

/****************************************************************************
 *                                                                          *
 *                                ENUMERATIONS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum length of the buffer for ScriptInterpreter trace names.
 * This combines the thread number, usually below a thousand with the
 * Script display name.
 */
#define MAX_TRACE_NAME 128

/**
 * Maximum number of tracks you can possibly have
 */
#define MAX_TRACKS 32

/**
 * Maximum number of arguments a ScriptStatement may have.
 */
#define MAX_ARGS 8

/**
 * Maximum length an referenced value may be, including all 
 * recursive expansions.
 */
#define MAX_ARG_VALUE 1024 * 8

/**
 * Average length an referenced value may be.
 */
#define MIN_ARG_VALUE 1024

/**
 * Types of waiting.
 */
typedef enum {

    WAIT_NONE,
	WAIT_LAST,
    WAIT_FUNCTION,
	WAIT_EVENT,
	WAIT_RELATIVE,
	WAIT_ABSOLUTE,
	WAIT_UP,
	WAIT_LONG,
	WAIT_SWITCH,
	WAIT_SCRIPT,
	WAIT_BLOCK,

	/**
	 * Wait for the "start point" this is similar to to
	 * Wait until 0 except that it is displayed as pending
	 * rather than at the left, and if we're already on frame zero,
	 * we'll wait for the next start point.
	 */
	WAIT_START,

	/**
	 * Wait for the "end point", this is done by creating a pending
	 * event that Loop knows to activate when it reaches the loop frame.
	 */
	WAIT_END,
	
	/**
	 * Wait for the "external start point", when synchronizing this
	 * is when the external pulse count wraps to zero.
	 */
	WAIT_EXTERNAL_START,

	/** 
	 * Wait for the "drift check point", when synchronizing this
	 * is when we compare the pulseFrame calculated from external
	 * pulses with the loop frame to determine drift.
	 */
	WAIT_DRIFT_CHECK,

	/**
	 * Wait for the next sync pulse.  The nature of the pulse depends
	 * on the sync mode of the loop.
	 */
	WAIT_PULSE,

	/**
	 * Wait for the next logical beat.  The nature of this depends
	 * on the sync mode.  For MIDI in/out there will be 24 pulses (clocks)
	 * in a beat.  For host sync, a pulse and a beat are the same thing.
	 */
	WAIT_BEAT,

	/**
	 * Wait for the next logical bar.  The nature of this depends
	 * on the sync mode.
	 */
	WAIT_BAR,

	/**
	 * Wait for the realign point.  This will always be on a pulse
	 * boundary, but the loop location may vary depending on 
	 * the RealignTime parameter.
	 */
	WAIT_REALIGN,

	/**
	 * Wait for the completion of a ReturnEvent, scheduled
	 * for SamplePlay and SwitchStyle=Once.
	 */
	WAIT_RETURN,

	/**
	 * Wait for the last MobiusThread event to finish.
	 * These are mostly related to file handling, but "echo" also
	 * schedules a thread event.  "Wait last" ignores these because
	 * it is common to have an echo between a function and the wait
	 * for that function, and we don't want the echo to override
	 * the function event.
	 */
	WAIT_THREAD

} WaitType;

/**
 * For WAIT_RELATIVE and WAIT_ABSOLUTE the unit of time to wait.
 */
typedef enum {

	UNIT_NONE,
	UNIT_MSEC,
	UNIT_FRAME,
	UNIT_SUBCYCLE,
	UNIT_CYCLE,	
	UNIT_LOOP

} WaitUnit;

//
// Errors returned by script parsing
//

#define SCRIPT_ERR_INVALID_FILE 1
#define SCRIPT_ERR_OPEN_FILE 2
#define SCRIPT_ERR_SYNTAX 3

//////////////////////////////////////////////////////////////////////
//
// ScriptEnv
//
//////////////////////////////////////////////////////////////////////

/**
 * A collection of compiled scripts.
 * This is created by the ScriptCompiler from a ScriptConfig.
 */
class ScriptEnv {
  public:

	ScriptEnv();
	~ScriptEnv();

    ScriptEnv* getNext();
    void setNext(ScriptEnv* env);

    class ScriptConfig* getSource();
    void setSource(ScriptConfig* config);

	class Script* getScripts();
    void setScripts(Script* scripts);

	class List* getScriptFunctions();

    bool isDifference(ScriptConfig* config);

    Script* getScript(Script* src);

  private:
	
    /**
     * Link for the script environment history.
     */
    ScriptEnv* mNext;

    /**
     * A copy of the ScriptConfig from which this environmment
     * was compiled.  This is used later for change detection.
     */
    class ScriptConfig* mSource;

    /**
     * A list of compiled Script objects.
     */
	class Script* mScripts;

};

/****************************************************************************
 *                                                                          *
 *                                DECLARATION                               *
 *                                                                          *
 ****************************************************************************/

/**
 * A block declaration.  These are unordered, unevaluated statements
 * that define characterics of the block.
 */
class ScriptDeclaration {

  public:
    ScriptDeclaration(const char* name, const char* args);
    ~ScriptDeclaration();

    ScriptDeclaration* getNext();
    void setNext(ScriptDeclaration* next);

    const char* getName();
    const char* getArgs();

  private:
    
    ScriptDeclaration* mNext;

    // the name of the declaration
    char* mName;

    // the unparsed arguments
    char* mArgs;

};

/****************************************************************************
 *                                                                          *
 *                                   BLOCK                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A block is a collection of statements with tools for searching them.
 *
 */
class ScriptBlock {

  public:

    ScriptBlock();
    virtual ~ScriptBlock();

    ScriptBlock* getParent();
    void setParent(ScriptBlock* block);

    // for named blocks like top-level scripts, Procs, and Params
    const char* getName();
    void setName(const char* name);

    void add(class ScriptStatement* stmt);
    void addDeclaration(class ScriptDeclaration* decl);
    class ScriptDeclaration* getDeclarations();
    class ScriptStatement* getStatements();
    void resolve(class Mobius* m);
    void link(class ScriptCompiler* comp);

    // for resolve()

    class ScriptVariableStatement* findVariable(const char* name);
    class ScriptLabelStatement* findLabel(const char* name);
    class ScriptProcStatement* findProc(const char* name);
    class ScriptIteratorStatement* findIterator(class ScriptNextStatement* next);
	class ScriptStatement* findElse(class ScriptStatement* start);

  private:

    ScriptBlock* mParent;
    char *mName;
    class ScriptDeclaration* mDeclarations;
	class ScriptStatement* mStatements;
	class ScriptStatement* mLast;
};

/****************************************************************************
 *                                                                          *
 *   								SCRIPT                                  *
 *                                                                          *
 ****************************************************************************/

class Script {

  public:

	Script();
    Script(ScriptEnv* env, const char* filename);
	~Script();

    void setEnv(ScriptEnv* env);
    ScriptEnv* getEnv();

	void setNext(Script* s);
    Script* getNext();

    void setName(const char* name);
    const char* getName();
    const char* getDisplayName();
    const char* getTraceName();

    void setFilename(const char* s);
    const char* getFilename();

    void setDirectory(const char* s);
    void setDirectoryNoCopy(char* s);
    const char* getDirectory();

	void clear();
	void add(class ScriptStatement* a);

	bool isAutoLoad();
	void setAutoLoad(bool b);

    bool isButton();
    void setButton(bool b);

	bool isHide();
	void setHide(bool b);

	bool isFocusLockAllowed();
	void setFocusLockAllowed(bool b);

	bool isQuantize();
	void setQuantize(bool b);

	bool isSwitchQuantize();
	void setSwitchQuantize(bool b);

	bool isContinuous();
	void setContinuous(bool b);

	bool isParameter();
	void setParameter(bool b);

	bool isSpread();
	void setSpread(bool b);
	int getSpreadRange();
	void setSpreadRange(int i);

	int getSustainMsecs();
	void setSustainMsecs(int i);

	int getClickMsecs();
	void setClickMsecs(int i);

	bool isSustainAllowed();
	bool isClickAllowed();

    void setFunction(Function* f);
    Function* getFunction();

	// notification labels
    void cacheLabels();
	class ScriptLabelStatement* getReentryLabel();
	class ScriptLabelStatement* getSustainLabel();
	class ScriptLabelStatement* getEndSustainLabel();
	class ScriptLabelStatement* getClickLabel();
	class ScriptLabelStatement* getEndClickLabel();

    // compilation

    ScriptBlock* getBlock();
	void resolve(Mobius* m);
	void link(class ScriptCompiler* comp);
	void xwrite(const char* filename);
    
    Function* getScriptFunction();
    void setScriptFunction(Function* f);

  private:

	void init();

	/**
	 * Environment we're a part of.  
	 * Necessary to resolve calls when !autoload is enabled.
	 */
	ScriptEnv* mEnv;

    /**
     * Chain pointer within the ScriptEnv.
     */
    Script* mNext;

    /** 
     * RunScriptFunction object we create to wrap this Script when 
     * it needs to be installed in the global function table.
     */
    Function* mFunction;

    char* mName;
    char* mDisplayName;
	char* mFilename;
	char* mDirectory;

	bool mAutoLoad;
	bool mButton;
	bool mFocusLockAllowed;
	bool mQuantize;
	bool mSwitchQuantize;
	bool mExpression;
	bool mContinuous;
	bool mParameter;
	bool mSpread;
	bool mHide;

	int mSpreadRange;
	int mSustainMsecs;
	int mClickMsecs;

    // outer block containing script statements
    ScriptBlock* mBlock;

	// cached notifcation labels
	class ScriptLabelStatement* mReentryLabel;
	class ScriptLabelStatement* mSustainLabel;
	class ScriptLabelStatement* mEndSustainLabel;
	class ScriptLabelStatement* mClickLabel;
	class ScriptLabelStatement* mEndClickLabel;
};

/****************************************************************************
 *                                                                          *
 *                              SCRIPT COMPILER                             *
 *                                                                          *
 ****************************************************************************/

#define SCRIPT_MAX_LINE 1024

/**
 * Parses script files and builds Script objects.
 * 
 * Encapsulates state necessary for script compilation.
 * This is used during both the parse and link phases.  
 *
 * During linking the script, lineNumber, and line fields are invalid.
 * The ExParser is always available.
 *
 * The compiler is normally built once when a ScriptConfig is loaded
 * and converted into a ScriptEnv.  It may also be built to 
 * incrementally compile scripts that use the !autoload option.
 *
 */
class ScriptCompiler {
  public:

	ScriptCompiler();
	~ScriptCompiler();

    /**
     * Compile a ScriptConfig into a ScriptEnv.
     */
    class ScriptEnv* compile(class Mobius* m, class ScriptConfig* config);
    
    /**
     * Incrementally recompile one script.
     * This is what happens for !autoload.  
     */
    void recompile(class Mobius* m, Script* script);

    // Utilities for the ScriptStatement constructors and linkers

    Mobius *getMobius();
    Script* getScript();
    char* skipToken(char* args, const char* token);
    class ExNode* parseExpression(ScriptStatement* stmt, const char* src);
    Script* resolveScript(const char* name);
    void syntaxError(ScriptStatement* stmt, const char* msg);

  private:

    void parse(const char* filename);
    bool parse(FILE* fp, Script* script);
    void parseArgument(char* line, const char* keyword, char* buffer);
    ScriptStatement* parseStatement(char* line);
    char* parseKeyword(char* line, char** retargs);
    void parseDeclaration(const char* line, const char* keyword);
    void link(Script* s);
    Script* resolveScript(Script* scripts, const char* name);

    /**
     * Supplies resolution for some references.
     */
    class Mobius* mMobius;

	/**
     * A parser for expressions, null if we're disabling expressions.
     */
	class ExParser* mParser;

    // Environment we're compiling into
    ScriptEnv* mEnv;

    // scripts we have parsed
    Script* mScripts;
    Script* mLast;
    
	// the script we're currently parsing or linking
	Script* mScript;

    // the script/proc block we're parsing
    ScriptBlock* mBlock;

	// line number of the file we're currently parsing (0 during linking)
	int mLineNumber;

	// the unmodified line we're parsing
	char mLine[SCRIPT_MAX_LINE + 4];


};

/****************************************************************************
 *                                                                          *
 *                                  RESOLVER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Class implementing the ExResolver interface for returning values
 * of parameters, internal variables, and stack arguments back to the
 * expression evaluator.
 *
 * This is functionally similar to ScriptArgument used by statements
 * that have simple argument lists that don't require expressions.
 * 
 */
class ScriptResolver : public ExResolver {
  public:

	ScriptResolver(ExSymbol* symbol, int arg);
	ScriptResolver(ExSymbol* symbol, class ScriptInternalVariable* v);
	ScriptResolver(ExSymbol* symbol, class ScriptVariableStatement* v);
	ScriptResolver(ExSymbol* symbol, Parameter* p);
	ScriptResolver(ExSymbol* symbol, const char* name);
	~ScriptResolver();

	void getExValue(ExContext* exContext, ExValue* value);

  private:

	void init(ExSymbol* symbol);

	ExSymbol* mSymbol;

	// this is now much like ScriptArgument, can we share?

	const char* mLiteral;
    int mStackArg;
    class ScriptInternalVariable* mInternalVariable;
    class ScriptVariableStatement* mVariable;
	Parameter* mParameter;
    const char* mInterpreterVariable;
};

/****************************************************************************
 *                                                                          *
 *                                  ARGUMENT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Represents a script statement argument, which may be a literal value,
 * or a reference that resolves to a stack argument, internal variable, 
 * script variable, or parameter.
 *
 * These are used by functions that have a small number of simple
 * arguments that don't need to be expressions.  I would like to phase
 * these out in most functions in favor of expressions returning ExValueLists.
 * 
 * There is no automatic resolution to global or track variables
 * that aren't declared within the script.  I think that's okay,
 * it's sort of like requiring an "extern" declaration in C, but
 * it does make it more cumbersome to use a large number of global variables.
 *
 * An "include" directive would make it easier to build libraries of
 * Variable and Proc declarations.  We could try to resolve Variables
 * within the entire ScriptEnv like we do for Call to other scripts,
 * this makes it hard to see how a variable is going to behave.
 *
 * Proc libraries would be very useful for the unit tests, but have
 * to be careful about !autoload.
 *
 */
class ScriptArgument {

  public:

	ScriptArgument();

	void resolve(class Mobius* m, class ScriptBlock* block, 
                 const char* literal);
	bool isResolved();
    const char* getLiteral();
	void setLiteral(const char* lit);

    void get(class ScriptInterpreter* si, ExValue* value);
    void set(class ScriptInterpreter* si, ExValue* value);

    // only for ScriptUseStatement
    Parameter* getParameter();

 private:

	const char* mLiteral;
    int mStackArg;
    ScriptInternalVariable* mInternalVariable;
    class ScriptVariableStatement* mVariable;
	Parameter* mParameter;

};

/****************************************************************************
 *                                                                          *
 *                                 STATEMENT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Base class for all script statements.
 */
class ScriptStatement {

  public:

	ScriptStatement();
	virtual ~ScriptStatement();

    virtual const char* getKeyword() = 0;
    virtual void resolve(Mobius* m);
    virtual void link(class ScriptCompiler* compiler);
    virtual ScriptStatement* eval(class ScriptInterpreter* si) = 0;

    virtual bool isVariable();
    virtual bool isLabel();
    virtual bool isIterator();
    virtual bool isNext();
    virtual bool isEnd();
    virtual bool isBlock();
    virtual bool isProc();
    virtual bool isEndproc();
    virtual bool isParam();
    virtual bool isEndparam();
	virtual bool isIf();
	virtual bool isElse();
	virtual bool isEndif();
	virtual bool isFor();

    void setParentBlock(ScriptBlock* b);
    ScriptBlock* getParentBlock();

	void setNext(ScriptStatement* a);
	ScriptStatement* getNext();

	void setArg(const char* s, int psn);
	const char* getArg(int psn);

    void setLineNumber(int i);
    int getLineNumber();

	void xwrite(FILE* fp);

  protected:

	void init();
    void parseArgs(char* line);
    char* parseArgs(char* line, int offset, int count);

    /**
     * The block we're in.
     */
    ScriptBlock* mParentBlock;

	/**
	 * Chain pointer.
	 */
	ScriptStatement* mNext;

	/**
	 * Functions may have up to 8 arguments.  Subclasses may further
	 * parse these into specific fields below.
	 */
	char* mArgs[MAX_ARGS];

    /**
     * Line number from the source file.
     */
    int mLineNumber;

};

/****************************************************************************
 *                                                                          *
 *                            FUNCTION STATEMENT                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Function call statement.
 */
class ScriptFunctionStatement : public ScriptStatement {

  public:

	ScriptFunctionStatement(ScriptCompiler* pcon, const char* name, 
							char* args);
	ScriptFunctionStatement(Function* f);
	~ScriptFunctionStatement();

	Function* getFunction();
	const char* getFunctionName();

	void setUp(bool b);
	bool isUp();

    const char* getKeyword();
    void resolve(Mobius* m);
    void link(class ScriptCompiler* comp);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	void init();

	/**
	 * Name of function to execute.
	 */
	char* mFunctionName;

	/**
	 * Resolved function object.
	 */
	Function* mFunction;

	/**
	 * True if "up" was the first token, used to represent the up
     *  transition of a sustained function.
	 */
	bool mUp;

	/**
	 * True if "down" was the first token, used to represent the down
     * transition of a sustained function.  Note that !mUp isn't
     * enough we need to know if either of them was explicitly specified.
	 */
	bool mDown;

    /**
     * Unparsed argument list.
     * This is captured during construction and later parsed into
     * either ScriptArguments or an ExNode during the link phase.
     */
    char* mUnparsedArgs;

	/**
	 * Old style resolved arguments to the function.
	 */
	ScriptArgument mArg1;
	ScriptArgument mArg2;
	ScriptArgument mArg3;
	ScriptArgument mArg4;

    /**
     * New style argument expression.
     */
    class ExNode* mExpression;

};

/****************************************************************************
 *                                                                          *
 *                               WAIT STATEMENT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Wait statement.
 */
class ScriptWaitStatement : public ScriptStatement {

  public:

	ScriptWaitStatement(ScriptCompiler* pcon, char* args);
	ScriptWaitStatement(WaitType type, WaitUnit unit, long time);
	~ScriptWaitStatement();

	const char* getKeyword();

	ScriptStatement* eval(ScriptInterpreter* si);

  private:

	WaitType getWaitType(const char* name);
	WaitUnit getWaitUnit(const char* name);
	
	void init();
    long getTime(ScriptInterpreter* si);
	class Event* setupWaitEvent(ScriptInterpreter* si, long frame);
	long getMsecFrames(ScriptInterpreter* si, long msecs);
	long getWaitFrame(ScriptInterpreter* si);
	long getQuantizedFrame(class Loop* loop, Preset::QuantizeMode q, 
						   long frame, int count);

	/**
	 * Type of wait. 
	 */
	WaitType mWaitType;

	/**
	 * Relative or absolute wait unit.
	 */
	WaitUnit mUnit;

    /**
     * Expression to calculate the wait time.
     */
	ExNode* mExpression;

	/**
	 * True if the wait advances during pause mode.
	 */
	bool mInPause;

};

/****************************************************************************
 *                                                                          *
 *                              MINOR STATEMENTS                            *
 *                                                                          *
 ****************************************************************************/

class ScriptEchoStatement : public ScriptStatement {

  public:

	ScriptEchoStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptMessageStatement : public ScriptStatement {

  public:

	ScriptMessageStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptEndStatement : public ScriptStatement {

  public:

	// special internal statement returned by some block evaluators
    // static initializer, safe for multiple plugins
	static ScriptEndStatement* Pseudo;

	ScriptEndStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isEnd();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptCancelStatement : public ScriptStatement {

  public:

	ScriptCancelStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	bool mCancelWait;

};

class ScriptInterruptStatement : public ScriptStatement {

  public:

	ScriptInterruptStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

/****************************************************************************
 *                                                                          *
 *   							  ITERATORS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Base class for statements that end with "next".
 */
class ScriptIteratorStatement : public ScriptStatement {

  public:

	ScriptIteratorStatement();
	virtual ~ScriptIteratorStatement();

	void setEnd(class ScriptNextStatement* end);
	ScriptNextStatement* getEnd();

	bool isIterator();
	virtual bool isDone(ScriptInterpreter* si) = 0;

  protected:

	// matching next, be careful because ScriptStatement already has mNext
    class ScriptNextStatement* mEnd;

	// various expressions that determine the iteration count
	class ExNode* mExpression;

};

class ScriptForStatement : public ScriptIteratorStatement {

  public:

	ScriptForStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isFor();

    ScriptStatement* eval(ScriptInterpreter* si);
	bool isDone(ScriptInterpreter* si);

  private:

};

class ScriptRepeatStatement : public ScriptIteratorStatement {

  public:

	ScriptRepeatStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);
	bool isDone(ScriptInterpreter* si);

  private:

};

class ScriptWhileStatement : public ScriptIteratorStatement {

  public:

	ScriptWhileStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);
	bool isDone(ScriptInterpreter* si);

  private:

};

class ScriptNextStatement : public ScriptStatement {

  public:

	ScriptNextStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isNext();
    void resolve(Mobius* m);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

    ScriptIteratorStatement* mIterator;

};

/****************************************************************************
 *                                                                          *
 *                                    SET                                   *
 *                                                                          *
 ****************************************************************************/

class ScriptSetStatement : public ScriptStatement {

  public:

	ScriptSetStatement(ScriptCompiler* con, char* args);
	virtual ~ScriptSetStatement();

    const char* getKeyword();
    void resolve(Mobius* m);
    ScriptStatement* eval(ScriptInterpreter* si);

  protected:

	ScriptArgument mName;
	class ExNode* mExpression;
};

class ScriptUseStatement : public ScriptSetStatement {

  public:

	ScriptUseStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

};

/****************************************************************************
 *                                                                          *
 *                                  VARIABLE                                *
 *                                                                          *
 ****************************************************************************/

typedef enum {

	SCRIPT_SCOPE_SCRIPT,
	SCRIPT_SCOPE_TRACK,
	SCRIPT_SCOPE_GLOBAL

} ScriptVariableScope;

class ScriptVariableStatement : public ScriptStatement {

  public:

	ScriptVariableStatement(ScriptCompiler* con, char* args);
	~ScriptVariableStatement();

    const char* getKeyword();
	bool isVariable();
	const char* getName();
	ScriptVariableScope getScope();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	ScriptVariableScope mScope;
	const char* mName;
	class ExNode* mExpression;
};

typedef enum {

    OP_NONE,
	OP_ISNULL,
	OP_NOTNULL,
    OP_EQ,
    OP_NEQ

} ScriptOperator;

/****************************************************************************
 *                                                                          *
 *                                CONDITIONALS                              *
 *                                                                          *
 ****************************************************************************/

class ScriptConditionalStatement : public ScriptStatement {

  public:

	ScriptConditionalStatement();
	virtual ~ScriptConditionalStatement();

    bool evalCondition(ScriptInterpreter* si);

  protected:

	class ExNode* mCondition;

};

class ScriptJumpStatement : public ScriptConditionalStatement {

  public:

	ScriptJumpStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void resolve(Mobius* m);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	ScriptArgument mLabel;
    class ScriptLabelStatement* mStaticLabel;

};

class ScriptIfStatement : public ScriptConditionalStatement {

  public:

	ScriptIfStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void resolve(Mobius* m);
	bool isIf();

	ScriptStatement* eval(ScriptInterpreter* si);

	ScriptStatement* getElse();

  private:

	// Either a ScriptElseStatement or ScriptEndifStatement
    ScriptStatement* mElse;

};

class ScriptElseStatement : public ScriptIfStatement {

  public:

	ScriptElseStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isElse();

};

class ScriptEndifStatement : public ScriptStatement {

  public:

	ScriptEndifStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isEndif();

	ScriptStatement* eval(ScriptInterpreter* si);
};

class ScriptLabelStatement : public ScriptStatement {

  public:

	ScriptLabelStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isLabel();
	bool isLabel(const char* name);

    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

/****************************************************************************
 *                                                                          *
 *                               CONFIG OBJECTS                             *
 *                                                                          *
 ****************************************************************************/

class ScriptSetupStatement : public ScriptStatement {

  public:

	ScriptSetupStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void resolve(Mobius* m);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	ScriptArgument mSetup;

};

class ScriptPresetStatement : public ScriptStatement {

  public:

	ScriptPresetStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void resolve(Mobius* m);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	ScriptArgument mPreset;

};

class ScriptUnitTestSetupStatement : public ScriptStatement {

  public:

	ScriptUnitTestSetupStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptInitPresetStatement : public ScriptStatement {

  public:

	ScriptInitPresetStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

/****************************************************************************
 *                                                                          *
 *                                    CALL                                  *
 *                                                                          *
 ****************************************************************************/

class ScriptCallStatement : public ScriptStatement {

  public:

	ScriptCallStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void resolve(Mobius* m);
    void link(class ScriptCompiler* comp);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	class ScriptProcStatement* mProc;
	Script* mScript;
    class ExNode* mExpression;

};

class ScriptStartStatement : public ScriptStatement {

  public:

	ScriptStartStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    void link(class ScriptCompiler* comp);
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	Script* mScript;
    class ExNode* mExpression;

};

/****************************************************************************
 *                                                                          *
 *                                    PROC                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Base class for statements that define a statement block.
 */
class ScriptBlockingStatement : public ScriptStatement {

  public:

	ScriptBlockingStatement();
	virtual ~ScriptBlockingStatement();

    void resolve(Mobius* m);
    void link(ScriptCompiler* compiler);

    ScriptBlock* getChildBlock();

  protected:

    /**
     * Block containing the proc statements.
     * Note that mBlock is inherited from ScriptStatement and has
     * the block the Prox statement is in.
     */
    ScriptBlock* mChildBlock;

};

class ScriptProcStatement : public ScriptBlockingStatement {

  public:

	ScriptProcStatement(ScriptCompiler* con, char* args);

	bool isProc();
    const char* getKeyword();
	const char* getName();

    ScriptStatement* eval(ScriptInterpreter* si);

};

class ScriptEndprocStatement : public ScriptStatement {

  public:

	ScriptEndprocStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isEndproc();
    ScriptStatement* eval(ScriptInterpreter* si);

};

/****************************************************************************
 *                                                                          *
 *                                   PARAM                                  *
 *                                                                          *
 ****************************************************************************/

class ScriptParamStatement : public ScriptBlockingStatement {

  public:

	ScriptParamStatement(ScriptCompiler* con, char* args);

	bool isParam();
    const char* getKeyword();
	const char* getName();

    ScriptStatement* eval(ScriptInterpreter* si);

};

class ScriptEndparamStatement : public ScriptStatement {

  public:

	ScriptEndparamStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
	bool isEndparam();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

/****************************************************************************
 *                                                                          *
 *                                   FILES                                  *
 *                                                                          *
 ****************************************************************************/

class ScriptLoadStatement : public ScriptStatement {

  public:

	ScriptLoadStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptSaveStatement : public ScriptStatement {

  public:

	ScriptSaveStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

/****************************************************************************
 *                                                                          *
 *                                   DEBUG                                  *
 *                                                                          *
 ****************************************************************************/

class ScriptBreakStatement : public ScriptStatement {

  public:

	ScriptBreakStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptPromptStatement : public ScriptStatement {

  public:

	ScriptPromptStatement(ScriptCompiler* pcon, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

};

class ScriptDiffStatement : public ScriptStatement {

  public:

	ScriptDiffStatement(ScriptCompiler* con, char* args);

    const char* getKeyword();
    ScriptStatement* eval(ScriptInterpreter* si);

  private:

	bool mAudio;
	bool mReverse;
};

/****************************************************************************
 *                                                                          *
 *   							 INTERPRETER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * State maintained for each stack frame created when procs and scripts 
 * are called.
 *
 * Overloaded with several "types" of stack frame but don't want to mess
 * with subclasses so we can use a single pool.
 */
class ScriptStack {

  public:

    ScriptStack();
    ~ScriptStack();
    void init();

    void setStack(ScriptStack* s);
    ScriptStack* getStack();

    void setScript(Script* s);
    Script* getScript();

    void setProc(ScriptProcStatement* s);
    ScriptProcStatement* getProc();
    
    void setParam(ScriptParamStatement* s);
    ScriptParamStatement* getParam();
    
	void setCall(ScriptCallStatement* call);
	ScriptCallStatement* getCall();

	void setArguments(ExValueList* args);
	ExValueList* getArguments();

	void setIterator(ScriptIteratorStatement* it);
	ScriptIteratorStatement* getIterator();

	void setLabel(ScriptLabelStatement* label);
	ScriptLabelStatement* getLabel();

    void setSaveStatement(ScriptStatement* s);
    ScriptStatement* getSaveStatement();

	void setWait(ScriptStatement* s);
	ScriptStatement* getWait();

	class Event* getWaitEvent();
	void setWaitEvent(class Event* e);

	class ThreadEvent* getWaitThreadEvent();
	void setWaitThreadEvent(class ThreadEvent* e);

	Function* getWaitFunction();
	void setWaitFunction(Function* f);
	
	bool isWaitBlock();
	void setWaitBlock(bool b);

    void addTrack(class Track* t);
	Track* getTrack();
	Track* nextTrack();

	void setMax(int max);
	int getMax();
	bool nextIndex();

	// wait stack maintenance

	bool finishWait(class Event* e);
	bool changeWait(class Event* orig, Event* neu);
	bool finishWait(class ThreadEvent* e);
	bool finishWait(class Function* f);
	void finishWaitBlock();
	void cancelWaits();

  private:

    /**
     * Pointer to the previous frame on the stack, or chain pointer
     * for the stack pool.
     */
    ScriptStack* mStack;

    /**
     * The Call statement that pushed the stack frame.
     * Required later when resolving argument references.
     * Control resumes after the CallStatement.
     */
    ScriptCallStatement* mCall;

    /**
     * The For/Repeat statement that pushed the stack frame.
     * Used for several sanity checks, don't really need 
     * an InteratorStatement, could be just a generic Statement.
     * Required to locate the target track in case there are
     * nested blocks inside a For statement.
     * Control resumes after the NextStatement paired
     * to this iterator.
     */
    ScriptIteratorStatement* mIterator;

    /**
     * The Script we are "in".  For Call frames this will be the
	 * script that was called, for other blocks this will be
	 * the script containing the block.
     */
    Script* mScript;

    /**
     * The Proc we are in, null if we're at the top-level of the
     * script.  Necessary for resolving variable references with
     * proper scoping.
     */
    ScriptProcStatement* mProc;

    /**
     * Arguments for a Call statement.
     * This typically contains an ExValueList representing the
     * positional arguments passed in the call.
     * !! Figure out a way to pool these.  We went through all the work
     * of pooling ScriptStack objects, but now that we're using expressions
     * rather than static ScriptArguments, most of the memory churn is
     * with expression evaluation.  
     */
    ExValueList* mArguments;

	/**
	 * Track list for For statements.
	 */
	Track* mTracks[MAX_TRACKS];

	/**
	 * Number of tracks in mTracks for For statements
	 * or the target value for a Repeat statement.
	 */
	int mMax;

	/** 
	 * Current index into mTracks for For statements, 
	 * or current iteration count for Repeat statements
	 */
	int mIndex;

    /**
     * The Label statement for notifications.
     * Control resumes with mSavedStatement.
     */
    ScriptLabelStatement* mLabel;

    /**
     * For notification frames, the statement we were
     * previously on.  This is normally a Wait statement
     * since we don't do notifications until the scripts
     * have advanced all they can.
     */
    ScriptStatement* mSaveStatement;

    /**
     * Set when waiting for something.  Normally this is a Wait statement, but
	 * can also be a Function statement if we're doing an automatic completion wait.
     */
    ScriptStatement* mWait;

	/**
	 * For WaitStatement frames, the loop event we're waiting on.
	 */
	Event* mWaitEvent;

    /**
     * When mWaitEvent is set, this is the Track the event was scheduled in.
     */
    Track* mWaitTrack;

	/**
	 * For WaitStatement frames, the thread event we're waiting on.
	 */
	ThreadEvent* mWaitThreadEvent;

	/**
	 * For WaitStatement frames, the function event we're waiting on.
	 */
	Function* mWaitFunction;

	/**
	 * For WaitStatement frames, a flag indiciating to wait till the beginning
	 * of the next interrupt.
	 */
	bool mWaitBlock;

};

/**
 * State maintained for "use" statements.
 * This will hold the original value of a parameter which will be
 * restored when the script ends.  Note that uses are script global, 
 * meaning they do not have block scope.  You can't use inside
 * a for statement, or inside a called script and have the previous
 * use restored.
 */
class ScriptUse {

  public:

    ScriptUse(Parameter* p);   
    ~ScriptUse();

    void setNext(ScriptUse *use);
    ScriptUse* getNext();

    Parameter* getParameter();
    ExValue* getValue();

  private:

    ScriptUse* mNext;
    Parameter* mParameter;
    ExValue mValue;
    
};

/**
 * Script interpreter.
 * 
 * We extend ExContext so we can provide symbol resolution for
 * Expr.h expressions.
 */
class ScriptInterpreter : public ExContext {

  public:

	ScriptInterpreter();
	ScriptInterpreter(Mobius* mob, Track* t);
	~ScriptInterpreter();

	void setMobius(Mobius* m);
	void setTrack(Track* t);
    void setNumber(int i);
    int getNumber();
	Track* getTrack();
	Track* getTargetTrack();
	void setScript(Script* s, bool inuse);
	void reset();
	bool isFinished();

	// active script chain
	void setNext(ScriptInterpreter* si);
	ScriptInterpreter* getNext();

    // uses
    void use(Parameter* p);
    void getParameter(Parameter* p, ExValue* value);

	// control methods called by Mobius

	void setSustaining(bool b);
	bool isSustaining();
	int getSustainedMsecs();
	void setSustainedMsecs(int msecs);
	int getSustainCount();
	void setSustainCount(int c);

	void setClicking(bool b);
	bool isClicking();
	int getClickedMsecs();
	void setClickedMsecs(int msecs);
	int getClickCount();
	void setClickCount(int c);

	void notify(ScriptStatement* s);
	int getReturnCode();
	void setReturnCode(int i);

	// Called by Mobius to keep track of a sustainable script trigger,
	// and save trigger info for range and control scripts.
	void setTrigger(class Action* action);
    Trigger* getTrigger();
    int getTriggerId();
    int getTriggerValue();
    int getTriggerOffset();
    bool isTriggerEqual(class Action* action);

	// control methods called by Track

	void finishEvent(class Event* event);
	void finishEvent(class ThreadEvent* event);
	bool cancelEvent(class Event* event);
	void rescheduleEvent(class Event* src, class Event* neu);
	void scriptEvent(class Loop* l, class Event* event);
	void resume(class Function* func);
	void run();
	void stop();

	// state methods called by statement handlers
	// would be nice if we could protect these but we'll have a billion friends

	Mobius* getMobius();
    Action* getAction();
    Export* getExport();
	Script* getScript();
    const char* getTraceName();
    class UserVariables* getVariables();
	bool isWaiting();
	bool isPostLatency();
	void setPostLatency(bool b);
	void scheduleThreadEvent(ThreadEvent* te);
    void setLastEvents(Action* a);
    Track* nextTrack();
	void setupWaitLast(ScriptStatement* wait);
	void setupWaitThread(ScriptStatement* wait);
    ScriptStack* allocStack();
    ScriptStack* pushStack(ScriptCallStatement* call, Script* subscript, ScriptProcStatement* proc, ExValueList* args);
    ScriptStack* pushStack(ScriptIteratorStatement* it);
    ScriptStack* pushStack(ScriptLabelStatement* label);
    ScriptStack* pushStackWait(ScriptStatement* src);
	ScriptStack* getStack();
    ScriptStatement* popStack();

	void getStackArg(int arg, ExValue* retval);
	void expand(const char* value, ExValue* retval);
	void expandFile(const char* value, ExValue* retval);

    // ExContext interface

	ExResolver* getExResolver(ExSymbol* symbol);
	ExResolver* getExResolver(ExFunction* symbol);
	
  private:

	void init();
	void run(bool block);
	void checkWait();
    void advance();
    void getStackArg(ScriptStack* stack, int index, ExValue* value);
    void restoreUses();

	ScriptInterpreter* mNext;
    int mNumber;
    char mTraceName[MAX_TRACE_NAME];
	Mobius* mMobius;
	Track* mTrack;
	Script* mScript;
    ScriptUse* mUses;
    ScriptStack* mStack;
    ScriptStack* mStackPool;
	ScriptStatement* mStatement;
	class UserVariables *mVariables;
    Action* mAction;
    Export* mExport;
    Trigger* mTrigger;
    int mTriggerId;
    int mTriggerValue;
    int mTriggerOffset;
	bool mSustaining;
	bool mClicking;
	Event* mLastEvent;
	ThreadEvent* mLastThreadEvent;
	int mReturnCode;
	bool mPostLatency;
	int mSustainedMsecs;
	int mSustainCount;
	int mClickedMsecs;
	int mClickCount;
	
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
