/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Data model for internal variables.
 * Factored out of Script.h so we can make them more general.
 *
 */

#ifndef VARIABLE_H
#define VARIABLE_H

/****************************************************************************
 *                                                                          *
 *                             INTERNAL VARIABLES                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Static instances of this class define the internal variables
 * that may be referenced from scripts.
 */
class ScriptInternalVariable {

  public:

    static ScriptInternalVariable* getVariable(const char* name);

    ScriptInternalVariable();
    ~ScriptInternalVariable();

    void setName(const char* name);
    const char* getName();

    void setAlias(const char* name);
	bool isMatch(const char* name);

	virtual void getValue(ScriptInterpreter* si, class ExValue* value);
    virtual void getTrackValue(Track* t, class ExValue* value);
    
    virtual void setValue(ScriptInterpreter* si, class ExValue* value);
  
  protected:

    char* mName;
	char* mAlias;
	ParameterType mType;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
