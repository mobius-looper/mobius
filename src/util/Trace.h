/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Trace utilities.
 * 
 */

#ifndef TRACE_H
#define TRACE_H

#include <stdarg.h>
#include "port.h"

extern bool TraceToDebug;
extern bool TraceToStdout;

/****************************************************************************
 *                                                                          *
 *   							 SIMPLE TRACE                               *
 *                                                                          *
 ****************************************************************************/

void trace(const char *string, ...);
void vtrace(const char *string, va_list args);


/****************************************************************************
 *                                                                          *
 *   							 TRACE LEVELS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Trace records at this level or lower are printed to the console.
 */
extern int TracePrintLevel;

/**
 * Trace records at this level or lower are sent to the debug output stream.
 */
extern int TraceDebugLevel;

/**
 * When set, trace messages will not be immediately rendnered.
 * Instead the listener is notified, and expected to call FlushTrace.
 */
extern class TraceListener* NewTraceListener;

/****************************************************************************
 *                                                                          *
 *   							 TRACE BUFFER                               *
 *                                                                          *
 ****************************************************************************/

class TraceBuffer
{
  public:

	TraceBuffer();
	~TraceBuffer();
	
	void incIndent();
	void decIndent();

	void add(const char *string, ...);
	void addv(const char *string, va_list args);

	void print();

  private:

	int mIndent;

};

/****************************************************************************
 *                                                                          *
 *   							 TRACE RECORD                               *
 *                                                                          *
 ****************************************************************************/

#define MAX_TRACE_RECORDS 10000

#define MAX_ARG 64

/**
 * Encapsulates the information necessary to format a trace message.
 * Formatting is deferred so that trace records can be captured
 * in high volume time sensitive environments like digitial audio processing.
 *
 * This isn't very flexible but it gets the job done.  We allow
 * one string argument and four long arguments.  If the string argument
 * is non-NULL it is expected to be the first argument in the message.
 */
class TraceRecord {

  public:

	/* Message level */
	int level;

	/**
	 * A number printed at the beginning of the rendered message indiciating
	 * the "context" of the record.  This will be application specific,
	 * for Mobius it will be the Loop number.
	 */
	int context;

	/**
	 * A number representing "time" within the application which will
	 * generally be a monotonically increasing number in an arbitrary
	 * time base.  For Mobius this will be the frame counter within
	 * the current loop.
	 */
	long time;

    // an sprintf format string
    const char* msg;

    // optional string arguments
    char string[MAX_ARG];
	char string2[MAX_ARG];
	char string3[MAX_ARG];

    // optional long arguments
    long long1;
    long long2;
    long long3;
    long long4;
    long long5;
};

/****************************************************************************
 *                                                                          *
 *   							TRACE CONTEXT                               *
 *                                                                          *
 ****************************************************************************/

/**
 * The interface of an object that may be registered to provide
 * application specific context for the trace records.
 */
class TraceContext {

  public:

	virtual void getTraceContext(int* context, long* time) = 0;

};

/**
 * An object that may be registered to provide context and time
 * info for all trace records.
 */
extern TraceContext* DefaultTraceContext;

/**
 * The interace of an object that may be registered to receive
 * notifications of new trace messages.
 */
class TraceListener {

  public:

	virtual void traceEvent() = 0;

};

/****************************************************************************
 *                                                                          *
 *   							BUFFERED TRACE                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC void ResetTrace();
PUBLIC void WriteTrace(const char* file);
PUBLIC void AppendTrace(const char* file);
PUBLIC void PrintTrace();
PUBLIC void FlushTrace();

/****************************************************************************
 *                                                                          *
 *   						   TRACE FUNCTIONS                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC void Trace(int level, const char* msg);
PUBLIC void Trace(TraceContext* c, int level, const char* msg);

PUBLIC void Trace(int level, const char* msg, const char* arg);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* arg);

PUBLIC void Trace(int level, const char* msg, const char* arg,
				  const char* arg2);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* arg,
				  const char* arg2);

PUBLIC void Trace(int level, const char* msg, const char* arg,
				  const char* arg2, const char* arg3);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* arg,
				  const char* arg2, const char* arg3);

PUBLIC void Trace(int level, const char* msg, const char* arg, long l1);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, long l1);

PUBLIC void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1);

PUBLIC void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2);

PUBLIC void Trace(int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, const char* arg2, long l1, long l2, long l3);

PUBLIC void Trace(int level, const char* msg, const char* arg, 
				  long l1, long l2);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  const char* arg, long l1, long l2);

PUBLIC void Trace(int level, const char* msg, long l1);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, long l1);

PUBLIC void Trace(int level, const char* msg, long l1, long l2);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2);

PUBLIC void Trace(int level, const char* msg, long l1, long l2, long l3);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3);

PUBLIC void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3);

PUBLIC void Trace(int level, const char* msg, 
				  long l1, long l2, long l3, long l4);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3, long l4);

PUBLIC void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4);
PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4);

PUBLIC void Trace(int level, const char* msg,
				  long l1, long l2, long l3, long l4, long l5);

PUBLIC void Trace(int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4, long l5);

PUBLIC void Trace(TraceContext* c, int level, const char* msg, 
				  long l1, long l2, long l3, long l4, long l5);

PUBLIC void Trace(TraceContext* c, int level, const char* msg, const char* str,
				  long l1, long l2, long l3, long l4, long l5);

#endif
