/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Helper class for Mobius, handles distribution of Actions.
 *
 */

#ifndef ACTION_DISPATCHER_H
#define ACTION_DISPATCHER_H

class ActionDispatcher {
  public:

    ActionDispatcher(class Mobius* m, class CriticalSection* csect, class ScriptRuntime* scripts);
    ~ActionDispatcher();

    void doAction(Action* a);
    void doActionNow(Action* a);
    void startInterrupt(long frames);

  private:

    void doPreset(Action* a);
    void doSetup(Action* a);
    void doBindings(Action* a);
    void doFunction(Action* a);
    void doFunction(Action* action, Function* f, Track* t);
    void doParameter(Action* a);
    void doParameter(Action* a, Parameter* p, Track* t);
    void doUIControl(Action* a);

    class Mobius* mMobius;
    class CriticalSection* mCsect;
    class ScriptRuntime* mScripts;
    class TriggerState* mTriggerState;
    class Action *mActions;
    class Action *mLastAction;
        
};




#endif
