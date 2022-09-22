/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Global script execution state.  Encapsulates management of the 
 * script threads.
 * 
 * Factored out of Mobius.cpp because it was getting too big.
 *
 */

#ifndef SCRIPT_RUNTIME_H
#define SCRIPT_RUNTIME_H

class ScriptRuntime {
  public:

    ScriptRuntime(class Mobius* m);
    ~ScriptRuntime();

    void runScript(Action* action);
    void resumeScript(Track* t, Function* f);
    void cancelScripts(Action* action, Track* t);
    void doScriptMaintenance(int sampleRate, long frames);
    void doScriptNotification(Action* a);

  private:

    void startScript(Action* action, Script* script);
    void startScript(Action* action, Script* s, Track* t);
    void addScript(ScriptInterpreter* si);
    bool isInUse(Script* s);
    ScriptInterpreter* findScript(Action* action, Script* s, Track* t);
    void freeScripts();

    class Mobius* mMobius;
	class ScriptPackage* mPackage;
	class ScriptInterpreter* mScripts;

    // number of script threads launched
    int mScriptThreadCounter;

};




#endif
