/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An imlementation of the PluginInterface defined in 
 * HostInterface.h for the Mobius VST and AU plugins.
 *
 * This must have NO dependencies on the AU, CoreAudio, Carbon APIs, 
 * or VST APIs.  
 *
 */

#include "Thread.h"


#include "AudioInterface.h"
#include "HostInterface.h"
#include "HostMidiInterface.h"
#include "MidiInterface.h"
#include "MidiEvent.h"

#include "Qwin.h"
#include "Palette.h"

#include "Action.h"
#include "Export.h"
#include "Expr.h"
#include "Function.h"
#include "MobiusInterface.h"
#include "MobiusConfig.h"
#include "Parameter.h"
#include "Script.h"
#include "Track.h"

#include "UI.h"
#include "UIConfig.h"

QWIN_USE_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * The suggested size of the VST window when in dual window
 * mode.  Typically the Mobius icon will be streatched to fit.  
 * If this is too wide it looks like a blue oval.
 */
#define VST_WINDOW_WIDTH 300
#define VST_WINDOW_HEIGHT 40

//////////////////////////////////////////////////////////////////////
//
// MobiusPluginParameter
//
//////////////////////////////////////////////////////////////////////

class MobiusPluginParameter : public PluginParameter {
  public:

	MobiusPluginParameter(MobiusInterface* m, Action* a);
	virtual ~MobiusPluginParameter();

    bool isResolved();
	const char** getValueLabels();
	float getValueInternal();
	void setValueInternal(float neu);

  private:

	void calcName(Action* a);

	MobiusInterface* mMobius;
    Action* mAction;
    Export* mExport;
	char** mValueLabels;
	
	// value state for parameters bound to momentary targets
	bool mFunctionDown;
};

MobiusPluginParameter::~MobiusPluginParameter()
{
    if (mAction != NULL) {
        mAction->setRegistered(false);
        delete mAction;
    }

    delete mExport;

	if (mValueLabels != NULL) {
		for (int i = 0 ; mValueLabels[i] != NULL ; i++)
		  delete mValueLabels[i];
		delete mValueLabels;
	}
}

#define MAX_ENUMERATED_INTEGER_RANGE 16

/**
 * NOTE: TrackParameter advertises a theoretical maximum
 * (16)  not the actual maximum which is almost always 8.
 * To get the actual maximum we have to use the getEffectiveMaximum
 * method rather than assuming p->high is accurate.
 */
MobiusPluginParameter::MobiusPluginParameter(MobiusInterface* m, 
                                             Action* a)
{
	mMobius = m;
    mAction = a;
    mExport = NULL;
	mValueLabels = NULL;
	mFunctionDown = false;
	mId = a->id;

    // set this to ensure that we don't use it by accident
    mAction->setRegistered(true);

    calcName(a);

	// NOTE: mDefault defaults to 0.0 which is what we use when
	// publishing AU parameters for the first time.  It is important
	// that the initial values for mDefault and mLast be the same so if
	// you change mDefault here (we don't) change mLast too.

    Target* target = mAction->getTarget();
    if (target == TargetParameter) {

        // we'll be momentary if we have a binding arg
        if (mAction->triggerMode == TriggerModeMomentary) {
            setType(PluginParameterButton);
        }
        else {
            mExport = m->resolveExport(mAction);

            ExportType extype = mExport->getType();

            // TODO: if Parameter::zeroCenter is true return it has
            // a different type so we can take advantage of 
            // host rendering (only for AU I think, it has a "pan" type)
            // !! for the two "bend" parameters the range is the PB range
            // we may want to shorten that for host parameters?
            // Bend controls can be negative, will this work??

            if (extype == EXP_INT) {
            
                int low = mExport->getMinimum();
                int high = mExport->getMaximum();
                int range = high - low + 1;

                setMinimum((float)low);
                setMaximum((float)high);

                if (range > MAX_ENUMERATED_INTEGER_RANGE) {
                    setType(PluginParameterContinuous);
                }
                else {
                    // make this look like an enum so we 
                    // can have a menu
                    setType(PluginParameterEnumeration);
                }
            }
            else if (extype == EXP_BOOLEAN) {
                setType(PluginParameterBoolean);
            }
            else if (extype == EXP_ENUM) {
                setType(PluginParameterEnumeration);
                int max = mExport->getMaximum();
                setMaximum((float)max);
            }
            else if (extype == EXP_STRING) {
                // these are okay as long as they support ordinals
                // should not be making random strings available for plugin
                // parameter bindings
                setType(PluginParameterEnumeration);
                int max = mExport->getMaximum();
                setMaximum((float)max);
            }
        }
    }
    else if (target == TargetFunction) {
        // Mobius::resolveTarget will have selected a TriggerMode.
        // Normally it is TriggerModeMomentary, but if the
        // function is a script with !controller it can behave
        // like a CC.  
        if (mAction->triggerMode == TriggerModeContinuous) {
            setType(PluginParameterContinuous);
            // scripts currently assume this, could pass the float
            // for a larger range!
            setMinimum(0);
            setMaximum(127);
        }
        else {
            // PluginParameterButton is treated like a boolean with a 
            // range of 0-1 and labels "Up" and "Down"
            setType(PluginParameterButton);
        }
    }
    else if (target == TargetSetup || 
             target == TargetPreset ||
             target == TargetBindings) {
        // PluginParameterButton is treated like a boolean with a 
        // range of 0-1 and labels "Up" and "Down"
        setType(PluginParameterButton);
    }
    else {
        // shouldn't be here now that we check for resolution first?
        const char* name = a->getResolvedTarget()->getName();
        if (name == NULL)
          name = "*unknown*";

        Trace(1, "MobiusPluginParameter: Unable to bind target to Host parameter: %s\n",
              name);
		// leave a name so we don't crash later
		setName(name);
	}
}

/**
 * Determine the name to expose for a parameter.
 */
PRIVATE void MobiusPluginParameter::calcName(Action* a)
{
    char buffer[1024];
    a->getDisplayName(buffer, sizeof(buffer));
    setName(buffer);
}

PUBLIC bool MobiusPluginParameter::isResolved()
{
    return mAction->isResolved();
}

PUBLIC const char** MobiusPluginParameter::getValueLabels()
{
	// use our own derived labels if set
	const char** labels = (const char**)mValueLabels;

    if (labels == NULL && mExport != NULL) {
        // enumerated parameters have their own labels
        labels = mExport->getValueLabels();

        if (labels == NULL && 
            (mExport->getType() == EXP_INT || 
             mExport->getType() == EXP_STRING)) {

            // We allow this for short range integer parameters
            // and strings behaving as enums.  Why bother even
            // checking types?  Just get the min/max and go?
            int low = mExport->getMinimum();
            int high = mExport->getMaximum();
            int range = high - low + 1;

            if (range <= MAX_ENUMERATED_INTEGER_RANGE) {

                mValueLabels = new char*[range + 1];
                char buffer[128];
                // todo: make these 1 based?
                // works for string enums, not for ints
                for (int i = 0 ; i < range ; i++) {
                    sprintf(buffer, "%d", low + i);
                    mValueLabels[i] = CopyString(buffer);
                }
                mValueLabels[range] = NULL;
                labels = (const char**)mValueLabels;
            }
        }
    }

	return labels;
}

/**
 * Return the current value of the parameter.
 */
PUBLIC float MobiusPluginParameter::getValueInternal()
{
	float value = 0.0f;

    if (mExport != NULL) {
        int ivalue = mExport->getOrdinalValue();
        value = (float)ivalue;
    }
    else {
        // it's a function or other momentary target without
        // state, return the last value set
        value = mLast;
    }

	return value;
}

/**
 * Set the current value of the parameter.
 * The value is supposed to obey the range we told the host
 * so we don't need any further scaling other than converting
 * float to int.
 */
PUBLIC void MobiusPluginParameter::setValueInternal(float v)
{
    if (mAction->triggerMode == TriggerModeContinuous) {
        Action* a = mMobius->cloneAction(mAction);
        a->arg.setInt((int)v);
        mMobius->doAction(a);
    }
    else {
        // must be Momentary
        // Non-zero is "down" and zero is "up".
        // Do not trigger a down if the value were tracking is already
        // non-zero.  This is for hosts that display boolean parameters
        // with value sliders.  We don't want the slider to trigger for
        // each value, just once when it leaves zero and once
        // when it returns.
        // NOTE: Alternative is to make down be exactly 1.0 so we don't
        // trigger throughoug the range, VST effectively does that 
        // when we give it a range of 0 and 1
        bool down = (v > 0.0);
        if (down != mFunctionDown) {
            mFunctionDown = down;
            Action* a = mMobius->cloneAction(mAction);
            a->down = down;
            mMobius->doAction(a);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MobiusPlugin
//
//////////////////////////////////////////////////////////////////////

class MobiusPlugin : public PluginInterface, public HostMidiInterface,
                     public WindowAdapter, public MouseInputAdapter {

  public:

	MobiusPlugin(HostInterface* host);
	~MobiusPlugin();

	// PluginInterface

    HostConfigs* getHostConfigs();
	int getPluginPorts();
	void start();
    void resume();
    void suspend();
	void midiEvent(int status, int channel, int data1, int data2, long frame);
    MidiEvent* getMidiEvents();
    void getWindowRect(int* left, int* top, int* width, int* height);
	void openWindow(void* window, void* pane);
	void closeWindow();
	PluginParameter* getParameters();
	PluginParameter* getParameter(int id);

    // HostMidiInterface
    void send(MidiEvent* event);

	// WindowAdapter

	void windowOpened(class WindowEvent* e);
	void windowClosing(class WindowEvent* e);

	// MouseInputAdapter

	void mousePressed(MouseEvent* e);
	void mouseReleased(MouseEvent* e);

	// KeyListener

	void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);
	void keyTyped(KeyEvent* e);

    // for PluginThread
    Context* getContext();
    MobiusInterface* getMobius();
    void openStandaloneWindow();

  private:

    bool isDualWindowMode();
    void getWindowBounds(Bounds* b);
	PluginParameter* addParameter(PluginParameter* last, Parameter* p);

	bool mTrace;
	HostInterface* mHost;
	MobiusInterface* mMobius;
	MidiInterface* mMidi;
	PluginParameter* mParameters;
	PluginParameter** mParameterTable;
	int mParameterTableMax;

    // MIDI messages queued for the next render cycle
    MidiEvent* mMidiEvents;
    MidiEvent* mLastMidiEvent;

	// diagnostics
	int mFrameLogs;

	// view

    /**
     * The plugin will always manage a HostFrame wrapping the 
     * native window given to us by the host.  Inside this frame
     * we will either open the UI panel if in single window mode,
     * or a LaunchPanel if in dual window mode.
     */
	class HostFrame* mFrame;

    /**
     * When running in dual window mode, we'll start a thread
     * that opens a standalone UIFrame which manages it's own UI.
     */
    class PluginThread* mThread;

    /**
     * When running in single window mode, we manage a UI that
     * installs itself in mFrame.  This is NULL in dual window mode.
     */
	class UI* mUI;

    /**
     * Previous versions of the UI and HostFrame that we're deferring
     * the deletion of until threads that popped up modal dialogs can 
     * unwind.
     */
    class UI* mOldUI;
    class HostFrame* mOldFrame;

};

/**
 * This is the static factory method we must implement.
 */
PluginInterface* PluginInterface::newPlugin(HostInterface* host)
{
	return new MobiusPlugin(host);
}

PUBLIC MobiusPlugin::MobiusPlugin(HostInterface* host)
{
	mTrace = false;
	mHost = host;
	mMobius = NULL;
	mMidi = NULL;
    mMidiEvents = NULL;
    mLastMidiEvent = NULL;
	mParameters = NULL;
	mParameterTable = NULL;
	mParameterTableMax = 0;
	mFrameLogs = 0;
	mFrame = NULL;
    mThread = NULL;
	mUI = NULL;
    mOldUI = NULL;
    mOldFrame = NULL;

	//strcpy(mError, "");

	// todo, need overridable fields for name etc....
	if (mTrace)
	  trace("MobiusPlugin::MobiusPlugin\n");

    // Need this so we can allocate MidiEvents, this is also passed
    // to Mobius which may make it open MIDI devices if there are plugin
    // devices configured.  We MUST release this when the plugin is closed.
	mMidi = MidiInterface::getInterface("MobiusPlugin");

	// have to convert some things so Mobius doesn't depend on qwin
	Context* con = mHost->getContext();
	if (con == NULL) {
		printf("ERROR: MobiusPlugin: Context is NULL!\n");
		fflush(stdout);
	}
			  
	MobiusContext* mcon = new MobiusContext();
    mcon->setPlugin(true);
	mcon->setCommandLine(con->getCommandLine());
	mcon->setInstallationDirectory(con->getInstallationDirectory());
	mcon->setConfigurationDirectory(con->getConfigurationDirectory());

    // Host replaces the audio streams
	mcon->setAudioInterface(mHost->getAudioInterface());

    

    // Mobius uses this to determine if it is being controlled by a plugin,
    // not elegant.  
	mcon->setMidiInterface(mMidi);

    // New kludge to give Mobius a handle to the host's MIDI output port
    // need to rethink this
    mcon->setHostMidiInterface(this);     //#014 - Cas:Here set MidiInterface and also HostMidiInterface

	// will read the config file but won't open devices yet
    // this is the only place that the 
	mMobius = MobiusInterface::getMobius(mcon);

	if (mTrace)
	  trace("MobiusPlugin::MobiusPlugin finished\n");
}

PUBLIC MobiusPlugin::~MobiusPlugin()
{
	if (mTrace)
	  trace("MobiusPlugin::~MobiusPlugin %p\n", this);

	closeWindow();

	SleepMillis(100);

	delete mParameters;
	mParameters = NULL;

	delete mParameterTable;
	mParameterTable = NULL;

	// note that deleting this will also delete the ResolvedTargets
    // we interned and are still be referenced by the MobiusPluginParameter
    // objects on the mParameters list
	delete mMobius;

    // be sure to release pooled MidiEvents, before we release
    // the MidiInterface that owns them
    MidiEvent* next = NULL;
    for (MidiEvent* event = mMidiEvents ; event != NULL ; event = next) {
        next = event->getNext();
        event->setNext(NULL);
        event->free();
    }

	// shouldn't have to do this but leaving a thread behind causes
	// Live and other hosts to crash
	//ObjectPoolManager::exit(false);

	// Leaving MIDI devices open with their monitor threads causes 
    // host crashes.  Unfortunately Usine and possibly others (SawStudio?) 
    // like to create several instances of the plugin and delete them at
    // random so we have to maintain a reference count on the MidiInterface
	// MidiInterface::exit();

    if (mMidi != NULL)
      MidiInterface::release(mMidi);

    // hopefully safe to delete these now
    delete mOldUI;
    delete mOldFrame;

    // probably not safe to delete these if we didn't go through
    // closeWindow properly, let them leak
    if (mUI != NULL)
      Trace(1, "MobiusPlugin: mUI lingering in destructor\n");

    if (mFrame != NULL)
      Trace(1, "MobiusPlugin: mFrame lingering in destructor\n");

    if (mThread != NULL)
      Trace(1, "MobiusPlugin: mThread lingering in destructor\n");

	if (mTrace)
	  trace("MobiusPlugin::~MobiusPlugin finished\n");
}

/**
 * Called by the plugin wrapper to get the configuration objects that can be 
 * used to adjust the way the plugin interacts with the host.
 * !! we should be able to do getPluginPorts this way now?
 */
PUBLIC HostConfigs* MobiusPlugin::getHostConfigs()
{
    return mMobius->getHostConfigs();
}

/**
 * Accessor for PluginThread.
 */
PUBLIC Context* MobiusPlugin::getContext()
{
    return mHost->getContext();
}

/**
 * Accessor for PluginThread.
 */
PUBLIC MobiusInterface* MobiusPlugin::getMobius()
{
    return mMobius;
}

/**
 * Return the number of stereo ports supported by this plugin.
 * We've got an older parameter "pluginPins" that we can use
 * here but divide by 2.
 * !! we should be able to do this in HostConfig now?
 */
PUBLIC int MobiusPlugin::getPluginPorts()
{
	int ports = 8;

	MobiusConfig* config = mMobius->getConfiguration();
	int pins = config->getPluginPins();

	if (pins > 0) {
		ports = pins / 2;
		if (ports == 0)
		  ports = 1;
	}

	return ports;
}

/**
 * Called at an appropriate time after the initial quick opening.
 * Mobius creates a Recorder and registers it as the AudioHandler
 * for the AudioStream it gets from the AudioInterface we got 
 * from the HostInterface...whew!  Flow is:
 *
 *   - HostInterface calls PluginInterface::newPlugin passing in itself
 *   - PluginInterface::newPlugin instantiates MobiusPlugin
 *   - MobiusPlugin asks HostInterface for an AudioInterface
 *   - MobiusPlugin puts the AudioInterface into the MobiusContext and
 *     creates Mobius
 *   - Mobius::start asks the AudioInterface for an AudioStream and registers
 *     the Recorder as the handler.
 */
PUBLIC void MobiusPlugin::start()
{
	mMobius->start();
	// wait for resume
	mMobius->setCheckInterrupt(false);

    // KLUDGE: Refresh the parameter values since mLast will 
    // still have 0.0 and periodic parameter exporting isn't working
    // in Ableton.  This still doesn't fix Ableton but at least the
    // parmeters get initial values.
	for (PluginParameter* p = mParameters ; p != NULL ; p = p->getNext())
      p->refreshValue();
}

/**
 * Called when the host knows that buffers will be comming in.
 * VST calls this on startProcess and maybe setBypass.
 * AU does not call this.
 *
 * We use this as a signal to start monitoring "stuck" interrupts.
 * 
 * !! this isn't accurate, we need to treat this like a pause
 * mute and adjust the frame counters
 */
PUBLIC void MobiusPlugin::resume()
{
	mMobius->setCheckInterrupt(true);
}

/**
 * Called when the host knows that buffers will no longer be comming in.
 * VST calls this on suspend and stopProcess.
 * AU does not call this.
 * 
 * We use this as a signal to stop monitoring "stuck" interrupts.
 */
PUBLIC void MobiusPlugin::suspend()
{
	mMobius->setCheckInterrupt(false);
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

PUBLIC PluginParameter* MobiusPlugin::getParameters()
{
	if (mParameters == NULL) {

		// Need to force population of the function tables for
		// parameter binding.  Unfortunately this also loads scripts
		// which is a potentially heavyweight thing to do in plugin
		// initialization but there isn't an easy alternative.
		// Could at least whip over the parameters and skip this  
		// if none of them are function bingings.
        // !! or just let them be unresolved, we won't auto-filter
        // things but they won't do anything
        mMobius->preparePluginBindings();

		// covert bindings for TriggerHost into PluginParameters
		MobiusConfig* config = mMobius->getConfiguration();
		BindingConfig *bconfig = config->getBaseBindingConfig();
        if (bconfig != NULL) {
            PluginParameter* last = NULL;
            Binding* bindings = bconfig->getBindings();
            if (bindings != NULL) {
                for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
                    if (b->getTrigger() == TriggerHost) {

                        Action* a = mMobius->resolveAction(b);
                        if (a != NULL) {
                            MobiusPluginParameter* p = new MobiusPluginParameter(mMobius, a);
                            if (last == NULL)
                              mParameters = p;
                            else
                              last->setNext(p);
                            last = p;
                        }
                    }
                }
            }
        }
    }

	return mParameters;
}

/**
 * !! not sure we need this any more now that we do parameter
 * sync in bulk.
 */
PUBLIC PluginParameter* MobiusPlugin::getParameter(int id)
{
	PluginParameter* found = NULL;
	if (mParameterTable == NULL) {
		PluginParameter* params = getParameters();
		if (params != NULL) {
			int max = 0;
			PluginParameter* p;
			for (p = params ; p != NULL ; p = p->getNext()) {
				if (p->getId() > max)
				  max = p->getId();
			}
			
			mParameterTable = new PluginParameter*[max+1];
			mParameterTableMax = max;

			for (int i = 0 ; i < max ; i++)
			  mParameterTable[i] = NULL;

			for (p = params ; p != NULL ; p = p->getNext())
				mParameterTable[p->getId()] = p;
		}
	}
	
	if (id >= 0 && id <= mParameterTableMax)
	  found = mParameterTable[id];

	return found;
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Events
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by HostInterface when a MIDI event comes in.
 *
 * Wrap the raw MIDI message bytes in a MidiEvent structure and pass through
 * to Mobius.  The caller is expected to have separated status and channel.
 * 
 * In AU, "frame" represents for a buffer offset.
 * When sending events to a unit "If non-zero, specifies that the event should
 * be rendered at this sample frame offset within the next buffer to be rendered.
 * Otherwise, the event will occur at the beginning of the next buffer."
 * 
 * When running under Bidule this seems to be usually 0 and occasionally 1.
 * It is unclear when we get these, probably the host sends these all at once
 * before the next render?
 * 
 * To handle offsets properly we would have to save them until the next 
 * render, then slice the buffer on MIDI event boundaries, similar to the way
 * AUEffectBase does for parameter change events.  If the buffer is small enough
 * the quantization shouldn't be that bad.
 * 
 * In VST, frame will currenly be zero.
 */
void MobiusPlugin::midiEvent(int status, int channel, int data1, int data2, 
							 long frame)
{
	MidiEvent* event = NULL;

	// I'd like to know this
	if (frame > 10) {
		if (mFrameLogs < 10) {
			printf("MobiusPlugin::midiEvent frame %ld\n", frame);
			fflush(stdout);
			mFrameLogs++;
		}
	}

	if (mTrace) {
		printf("MIDI: %d %d %d %d %ld\n", 	
			   status, channel,data1, data2, frame);
		fflush(stdout);
	}
	
	if (status >= 0xF0) {
		// a non-channel event, always filter out active 
		// sense garbage WindowsMidiInterface also allows
		// filtering of all realtime events
		// do "commons" come in here??

		if (status != 0xFE)
		  event = mMidi->newEvent(status, 0, data1, data2);
	}
	else {
		event = mMidi->newEvent(status, channel, data1, data2);
	}

	if (event != NULL) {
		mMobius->doMidiEvent(event);
		event->free();
	}
}

/**
 * Called by HostInterface to return the MIDI events to process in this cycle.
 * Both this and send(MidiEvent*) are called during the render cycle
 * so we don't have to worry about concurrency.
 *
 * Ownership transfers to the caller which must call MidiEvent::free() 
 * on each event.
 */
MidiEvent* MobiusPlugin::getMidiEvents()
{
    //trace("MobiusPlugin::getMidiEvents()");
    MidiEvent* events = mMidiEvents;
    
    if(events != NULL)
    {    
        // Debug (count midi event in queue)
        // int i = 0;
        // for (MidiEvent* event = events ; event != NULL ; event = event->getNext()) {
        //     i++;
        // }

        // Trace(3,"MobiusPlugin::getMidiEvents() | count : %i", i); //#014 Cas
        
    }
    else
    {
        //Proof Bug VSTHost  #014!!
        //MidiEvent* mevent = mMidi->newEvent(0xF0, 3, 4, 127);
        //MidiEvent* mevent = mMidi->newEvent(0x90, 0x3C, 0x40, 127);
        //return mevent;
         
        // MidiEvent* mevent = mMidi->newEvent(0x90, 0, 2, 3);
        // MidiEvent* mevent2 = mMidi->newEvent(0x90, 1, 3, 4);
        // mevent->setNext(mevent2);
        // return mevent;


    }

    mMidiEvents = NULL;     //#014 Cas  || disperato riattivare
    mLastMidiEvent = NULL;  //#014 Cas  || disperato riattivare
    return events;
}

/**
 * HostMidiInterface implementation called by Mobius to register
 * events to send on the next cycle.
 *
 * We'll just queue these on a list and expect HostInterface to 
 * call getMidiEvents periodically.
 *
 * !! If the audio stream is bypassed which is common with Reaper until
 * you arm the track for recording, these can potentially queue up a long
 * way, leading to an explosion when the track is eventually armed.  
 * Might want a governor on this...
 */
void MobiusPlugin::send(MidiEvent* event)
{
    //#014 | Create a new object from event, someone destry it before VST send...
    //MidiEvent* nEv =  mMidi->newEvent(event->getStatus(), event->getChannel(),event->getValue(), event->getVelocity());   BUG
    MidiEvent* queueEvent = event->copy(); //#014b

    //trace("send!--------------------------------------------------------------------------------------");
    //Trace(3,"MobiusPlugin::send");
    if (mLastMidiEvent != NULL)   //Se già presente l'ultimo evento, concateno next
    {
      //mLastMidiEvent->setNext(event);
      mLastMidiEvent->setNext(queueEvent);
    }
    else
    {
      //mMidiEvents = event;  //Altrimenti lo metto come primo della lista...
      mMidiEvents = queueEvent;
    }
    //mLastMidiEvent = event; //e poi ultimo :)  [bug]  //#014b | bugfix wrong reference
    mLastMidiEvent = queueEvent;
    //Trace(3,"MobiusPlugin::send |%i|%i|%i|%i|",mLastMidiEvent->getStatus(), mLastMidiEvent->getChannel(),mLastMidiEvent->getValue(), mLastMidiEvent->getVelocity());
}

//////////////////////////////////////////////////////////////////////
//
// UI Thread
//
//////////////////////////////////////////////////////////////////////

/**
 * Used in cases where we launch our own editor window rather than
 * using the one the host provides.
 *
 * Tried to keep the host window and this one in sync, but we don't
 * get reliable close/open/activate/deactivate messages in the
 * child window.  Have to hook into the parent window?
 *
 * For now, keep them independent, but if the child window get's
 * any paint messages, make sure we have a Mobius window active.
 */
class PluginThread : public Thread 
{
  public:

    PluginThread(MobiusPlugin* plugin);
    ~PluginThread();

	class Window* getWindow();
	void run();
	void stop();
	void toFront();

  private:

	MobiusPlugin* mPlugin;
	class UIFrame* mFrame;
};


PluginThread::PluginThread(MobiusPlugin* plugin)
{
    setName("Mobius");
	mPlugin = plugin;
    mFrame = NULL;

    // this does a fair bit of work, defer it to the thread run 
    // method to avoid host UI thread entanglements
	//mFrame = new UIFrame(plugin->getContext(), plugin->getMobius());
}

PUBLIC Window* PluginThread::getWindow()
{
	return mFrame;
}

PUBLIC void PluginThread::run()
{
    if (mFrame == NULL)
      mFrame = new UIFrame(mPlugin->getContext(), mPlugin->getMobius());

    int result = mFrame->run();

	Trace(2, "PluginThread: frame no longer running\n");

	// I've seen occasional crashes in MobiusThread when it
	// tells the listener about a time boundary, the UIFrame
	// destructor should be doing this, but make sure it happens
	// be fore we start deleting the window hierarchy
    MobiusInterface* m = mPlugin->getMobius();
 	m->setListener(NULL);

	// Pause a moment to make sure MobiusThread is finished with
	// the listener, this isn't really safe!
	SleepMillis(100);

	Trace(2, "PluginThread: deleting frame\n");

    // Divided this into prepare/sleep/delete phases debugging 
    // a memory problem with Reaper.  This isn't necessary any more
    // but it can't hurt.
    mFrame->prepareToDelete();
	SleepMillis(100);
    delete mFrame;
	mFrame = NULL;

	Trace(2, "PluginThread: frame deleted\n");
}

void PluginThread::stop()
{
	if (mFrame != NULL) {

		Trace(2, "PluginThread: stopping\n");

		// in case we're being probed, wait until the window is fully opened
		// and we're running the event loop
		int i;
		for (i = 0 ; i < 50 ; i++) {
			if (!mFrame->isRunning())
			  SleepMillis(100);
			else
			  break;
		}

		if (!mFrame->isRunning())
		  Trace(1, "PluginThread::stop Waited too long for Mobius window to open!\n");

		// this should eventually cause Frame::run to return 
		// in the run method above which will delete the frame

		// kludge, need to figure out better control flow
		mFrame->close();

		// wait for the run method to delete the frame
		for (i = 0 ; i < 50 && mFrame != NULL ; i++) 
		  SleepMillis(100);

		if (mFrame != NULL)
		  Trace(1, "PluginThread::stop Unable to close Mobius frame from plugin thread!\n");
	}
}

void PluginThread::toFront()
{
	if (mFrame != NULL) {
	}
}

PluginThread::~PluginThread()
{
    if (mFrame != NULL)
      Trace(1, "PluginThread: Deleting thread with lingering UIFrame!");
}

//////////////////////////////////////////////////////////////////////
//
// LaunchPanel
//
//////////////////////////////////////////////////////////////////////

/**
 * Component placed inside the HostFrame if running in dual window mode.
 * Used just to detect activity and bring up our standalone
 * Mobius window.
 */
class LaunchPanel : public Panel, public MouseInputAdapter
{
  public:

	LaunchPanel(MobiusPlugin* plugin);
	~LaunchPanel();

	void mousePressed(MouseEvent* e);

  private:

	class MobiusPlugin* mPlugin;

};

LaunchPanel::LaunchPanel(MobiusPlugin* plugin)
{
	mPlugin = plugin;

    Color* black = GlobalPalette->getColor(COLOR_SPACE_BACKGROUND, Color::Black); 
	setPreferredSize(new Dimension(VST_WINDOW_WIDTH, VST_WINDOW_HEIGHT));
	setLayout(new BorderLayout());
    setBackground(black);
    addMouseListener(this);

    Static* s = new Static();
    s->setBackground(black);
    s->setIcon("Mobius");

    // centering these is awkward would be nice to have a general 
    // CenteredLayout that handled both dimensions
    Panel* p = new Panel();
    LinearLayout* l = new VerticalLayout();
    l->setCenterY(true);
    p->setLayout(l);
    p->add(s);

    add(p, BORDER_LAYOUT_WEST);

    Label* t = new Label("Click to open Mobius window...");
    t->setBackground(black);
    t->setForeground(GlobalPalette->getColor(COLOR_BUTTON, Color::White)); 

    p = new Panel();
    l = new VerticalLayout();
    l->setCenterY(true);
    p->setLayout(l);
    p->add(t);

    add(p, BORDER_LAYOUT_CENTER);
}

LaunchPanel::~LaunchPanel()
{
}

void LaunchPanel::mousePressed(MouseEvent* e)
{
	trace("LaunchPanel::mousePressed\n");
	mPlugin->openStandaloneWindow();
}

//////////////////////////////////////////////////////////////////////
//
// Editor Window
//
//////////////////////////////////////////////////////////////////////

PRIVATE bool MobiusPlugin::isDualWindowMode()
{
    bool dual = false;

	// Let this be configurable on Mac too?
	// Not yet, it comes up but there are timeouts on the popup
	// window thread so something is going wrong with communication between
	// the two windows.  Since we can't display bitmaps yet, the 
	// "Click to bring up..." window looks less pretty, and it doesn't fill
	// the Bidule window so it looks stupid.  Keys seem to be comming in
	// but it doesn't replace the menu bar so there is little use
	// for this on Mac.

#ifdef _WIN32
	MobiusConfig* config = mMobius->getConfiguration();
    dual = config->isDualPluginWindow();
#endif

    return dual;
}

/**
 * PluginInterface method to get the desired bounds of the editor window.
 * This is only used on VST.  For AU we just create one
 * of the size we want and the host deals with it.
 */
PUBLIC void MobiusPlugin::getWindowRect(int* left, int* top, 
                                        int* width, int* height)
{
    Bounds b;
    getWindowBounds(&b);

    *left = b.x;
    *top = b.y;
    *width = b.width;
    *height = b.height;
}

/**
 * Get the bounds of the editor window into a Bounds object.
 * This is used both by getWindowRect and openWindow.
 */
PRIVATE void MobiusPlugin::getWindowBounds(Bounds* ret)
{
    if (isDualWindowMode()) {
		// VST controlled window is just used for a little splash screen
		ret->x = 0;
		ret->y = 0;
		ret->width = VST_WINDOW_WIDTH;
		ret->height = VST_WINDOW_HEIGHT;
    }
    else {
        // single window mode, we have to create a UI to read
        // bounds from the config file

		if (mUI == NULL)
		  mUI = new UI(mMobius);

		UIConfig* config = mUI->getUIConfig();
		Bounds* b = config->getBounds();

		// should we try to position this?
        ret->x = 0;
        ret->y = 0;

		// had problems once when ui.xml wasn't available
		// try to not crash
		if (b != NULL) {
			ret->width = b->width;
			ret->height = b->height;
		}
		else {
			ret->width = 600;
			ret->height = 480;
		}
	}
}

/**
 * Open the editing window.
 * For AU both the window and pane should be passed.
 * For VST only the window is passed.
 */
PUBLIC void MobiusPlugin::openWindow(void* window, void* pane)
{
	if (mFrame == NULL) {

        // determine the frame size for either single or dual mode
        Bounds b;
        getWindowBounds(&b);

		Context* con = mHost->getContext();
		mFrame = new HostFrame(con, window, pane, &b);
		mFrame->setBackground(Color::Black);

        // intercept window close/open
		mFrame->addWindowListener(this);
		mFrame->addMouseListener(this);
		//mFrame->addKeyListener(this);

        // UI expects border layout
        mFrame->setLayout(new BorderLayout());

		// kludge for AudioMulch, don't verify the host frame size
		// after opening
		if (StringEqualNoCase(mHost->getHostName(), "AudioMulch"))
		  ((HostFrame*)mFrame)->setNoBoundsCapture(true);

        if (isDualWindowMode()) {
			// this one does not contain a UI, we wait for a signal
            // and launch a PluginThread

			LaunchPanel* lp = new LaunchPanel(this);
            mFrame->add(lp, BORDER_LAYOUT_CENTER);

            // open the components and repaint
            mFrame->open();

            // auto-open standalone window
            openStandaloneWindow();
        }
        else {
            // getWindowBounds should already have bootstrapped one of these
            if (mUI == NULL)
              mUI = new UI(mMobius);
			
			// complete the opening
			mUI->open(mFrame, true);

            // open the components and repaint
            mFrame->open();
		}
	}
}

/**
 * Called from LaunchPanel when it receives an event 
 * to open the primary window.
 * Used only in dual window mode.
 */
PUBLIC void MobiusPlugin::openStandaloneWindow()
{
	// may no longer be running if we manually shut down the main window?
	if (mThread != NULL) {
		if (!mThread->isRunning()) {
            Trace(2, "MobiusPlugin::openStandaloneWindow cleaning up thread\n");
			delete mThread;
			mThread = NULL;
		}
	}

	if (mThread == NULL) {
        Trace(2, "MobiusPlugin::openStandaloneWindow starting thread\n");
		mThread = new PluginThread(this);
		mThread->start();
	}
	else {
		// already open, but make sure we're on top!
        Trace(2, "MobiusPlugin::openStandaloneWindow thread already running\n");
		mThread->toFront();
	}
}

PUBLIC void MobiusPlugin::closeWindow()
{
    // this raised issues with strange crashes at shutdown of Live 5.2.2
    bool deferUIDelete = false;

	if (mUI != NULL) {
        Trace(2, "MobiusPlugin::closeWindow closing single window frame\n");

        // should only be here in single window mode
        if (mThread != NULL)
          Trace(1, "MobiusPlugin::closeWindow Both UI and PluginThread active");

        // stop the refresh timer and anything else that might be 
        // sending events to the UI or the Component hierarchy
        // we used to just delete it here, but I'm worried about
        // stray events that may come in while we're closing the host frame
        // that may end up back in mUI
        mUI->prepareToDelete();

        if (mFrame == NULL)
          Trace(1, "MobiusPlugin::closeWindow: UI without a HostFrame!");
        else {
            // this breaks the links between the native
            // components and the Component hierarchy, necessary
            // because we don't have control over when the native window
            // will be deleted and it can still send us events
            mFrame->close();

            // delete the Component hierarchy
            // !! If you left a dialog up, it will be running in another
            // thread (typically in a menu item handler that launched
            // the dialog) and deleting the frame deletes the world out
            // from under it.  This isn't an issue for standalone since
            // modal dialogs prevent the window from being closed, but 
            // here the host can close it any time.  Deferring the
            // delete might work but it feels like there will be a race
            // condition, we don't know when exactly the menu threads will end.
            // Better than nothing...

            if (deferUIDelete) {
                if (mOldFrame != NULL)
                  delete mOldFrame;
                mOldFrame = mFrame;
                mFrame = NULL;
            }
            else {
                delete mFrame;
                mFrame = NULL;
            }
        }

        // same issue with unclosed modal dialogs here
        if (deferUIDelete) {
            if (mOldUI != NULL)
              delete mOldUI;
            mOldUI = mUI;
            mUI = NULL;
        }
        else {
            delete mUI;
            mUI = NULL;
        }
	}

	if (mThread != NULL) {
        Trace(2, "MobiusPlugin::closeWindow closing dual window thread\n");

        // in theory we could try to keep the thread running
        // and leave the main UI window up and let it be closed
        // manually?

        if (mThread->isRunning()) {
            // main window still open
            // stop() will block until it shuts down
            mThread->stop();
            if (mTrace)
              trace("MobiusPlugin::close thread stopped\n");
        }

        delete mThread;
        mThread = NULL;
    }

    if (mFrame != NULL) {
        Trace(2, "MobiusPlugin::closeWindow closing dual window frame\n");

        // should only be here in dual window mode, mUI must have been NULL
        // which means that mFrame is surrounding the child launch window

        // break the links between the native window and the Component
        // hierararchy
        mFrame->close();

        // here we don't have to worry about the lingering modal dialog
        // problem?
        delete mFrame;
        mFrame = NULL;
	}
}

/**
 * Called when we are registered as a mouse listener
 * for the HostFrame in dual-window mode.
 */
void MobiusPlugin::mousePressed(MouseEvent* e)
{
	trace("MobiusPlugin::mousePressed %d %d\n", e->getX(), e->getY());
}

void MobiusPlugin::mouseReleased(MouseEvent* e)
{
	trace("MobiusPlugin::mouseReleased %d %d\n", e->getX(), e->getY());
}

/**
 * WindowListener for VST host window.
 * We care about the opened event because this is when the UI
 * will start the timer and begin periodic refreshes.
 * If we don't have a UI it means that PluginThread created
 * a UIFrame which has it's own open listener.
 */
void MobiusPlugin::windowOpened(WindowEvent* e)
{
	trace("MobiusPlugin::windowOpened\n");
	if (mUI != NULL)
	  mUI->opened();
}

/**
 * This is where UIFrame would save the ending locations but
 * we don't need to since you can't resize a VST host window.
 * If this is the child window in dual-window mode, we'll keep the
 * thread with the main UI running.
 */
void MobiusPlugin::windowClosing(WindowEvent* e)
{
	trace("MobiusPlugin::windowClosing\n");
}

void MobiusPlugin::keyPressed(KeyEvent* e)
{
	trace("MobiusPlugin::keyPressed %d\n", e->getKeyCode());
}

void MobiusPlugin::keyReleased(KeyEvent* e)
{
	trace("MobiusPlugin::keyReleased %d\n", e->getKeyCode());
}

void MobiusPlugin::keyTyped(KeyEvent* e)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
