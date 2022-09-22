/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for Mobius OSC configuration.
 *
 */

#ifndef OSC_CONFIG_H
#define OSC_CONFIG_H

#include "WatchPoint.h"
#include "OscInterface.h"

//////////////////////////////////////////////////////////////////////
//
// OscConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * An object containing all OSC configuration.  There is only
 * one of these, maintained with in the MobiusConfig.  
 */
class OscConfig {
  public:

	OscConfig();
	OscConfig(const char* xml);
	OscConfig(XmlElement* e);
	~OscConfig();
    void toXml(XmlBuffer* b);

    const char* getError();
	int getInputPort();
	void setInputPort(int i);
	const char* getOutputHost();
	void setOutputHost(const char* s);
	int getOutputPort();
	void setOutputPort(int i);

	class OscBindingSet* getBindings();
    class OscWatcher* getWatchers();

  private:

    void init();
    void parseXml(XmlElement* e);

	/**
	 * The default port on which we listen for OSC messages.
	 * Each OscBindingSet can specify a different input port in 
	 * case you want different mappings for more than one of the 
	 * same device.	
	 */
	int mInputPort;

	/**
	 * The default host to which we send OSC messages.
	 * Each OscBindingSet can specify a different output host in 
	 * case you have more than one device that needs to be updated.
	 */
	char* mOutputHost;

	/**
	 * The default port to which we send OSC messages.
	 * This must be set if mOutputHost is set, there is no default.
	 */
	int mOutputPort;

	/**
	 * Binding sets.  Unlike BindinConfigs several of these can be
	 * active at a time.
	 */
	class OscBindingSet* mBindings;

    /**
     * Exports.  The definitions of things that may be exported
     * from Mobius but which aren't controls or parameters and
     * can't be bound.
     */
    class OscWatcher* mWatchers;
    
    // parser error
    char mError[256];
};

//////////////////////////////////////////////////////////////////////
//
// OscBindingSet
//
//////////////////////////////////////////////////////////////////////

/**
 * A named collection of OSC bindings.
 * These don't extend Bindable because you can't activate them in the
 * same way as BindingConfigs.  No script access at the moment, I guess
 * we would need a global variable containing a CSV of the active set names.
 */
class OscBindingSet {
  public:

	OscBindingSet();
	OscBindingSet(XmlElement* e);
	~OscBindingSet();
	void toXml(class XmlBuffer* b);

	void setNext(OscBindingSet* s);
	OscBindingSet* getNext();

    void setName(const char* s);
    const char* getName();

    void setComments(const char* s);
    const char* getComments();

    void setActive(bool b);
    bool isActive();

	int getInputPort();
	void setInputPort(int i);
	const char* getOutputHost();
	void setOutputHost(const char* s);
	int getOutputPort();
	void setOutputPort(int i);

	Binding* getBindings();
	void setBindings(Binding* b);
	void addBinding(Binding* c);
	void removeBinding(Binding* c);

  private:

    void init();
	void parseXml(class XmlElement* e);

	/**
	 * Chain link.
	 */
	OscBindingSet* mNext;

    /**
     * Sets should have names to distinguish them.
     */
    char* mName;

    /**
     * Optional comments describing the incomming messages that
     * may be bound.
     */
    char* mComments;

    /**
     * True if this is to be active. 
     * Ignored now, maybe this should be true to disable?
     */
    bool mActive;

	/**
	 * The port on which we listen for OSC messages.  This overrides
	 * the default port in the OscConfig.  This is relatively unusual but
	 * would be used if you want different mappings for more than one of the
	 * same device (e.g. two TouchOSC's controlling different sets of tracks.
	 */
	int mInputPort;

	/**
	 * The host to which we send OSC messages.
	 * This overrides the default host in the OscConfig.  You would override
	 * this if there is more than one device that needs status messages.
	 */
	char* mOutputHost;

	/**
	 * The default port to which we send OSC messages.
	 * This must be set if mOutputHost is set, there is no default.
	 */
	int mOutputPort;

	/**
	 * Bindings for this set.
	 * You can mix bindings from different devices but if you
	 * want to use bidirectional feedback you should only put
	 * bindings for one device in a set, because the set can have
	 * only one outputHost/outputPort.
	 */
	class Binding* mBindings;

};
	
//////////////////////////////////////////////////////////////////////
//
// OscBinding
//
////////////////////////////////////////////////////////////////////////

/**
 * This wraps an Action to add some extra intellence for OSC.
 * 
 * It is functionally very similar to PluginParameter with the way
 * it supports argument scaling and remembering the last value for 
 * periodic export.  Would be nice to share some of this but there
 * are enough differences to keep them distinct classes.
 *
 * One list of these will be built for every Binding in every 
 * OscBindingSet.  They will also be entered into a Map for optimized
 * searching when OSC messages come in.
 *
 * Unlike PluginParameter we don't assume that all bindings are exported
 * to a separate List is maintained of the exportable bindings.
 *
 */
class OscBinding {
  public:

    OscBinding(class MobiusInterface* m, class Binding* b, class Action* a);
	~OscBinding();

    OscBinding* getNext();
    void setNext(OscBinding* e);
    bool isResolved();
    bool isExportable();

    class TriggerType* getTriggerType();
    class Action* getAction();

    class OscDevice* getExportDevice();
    void setExportDevice(OscDevice* d);

    const char* getExportAddress();

    int getLast();
    void setLast(int f);

    bool refreshValue();
    float getExportValue();

    void setValue(float value);

  private:

    void init();

	OscBinding* mNext;
    class MobiusInterface* mMobius;
    class Action* mAction;
    class Export* mExport;
    class OscDevice* mExportDevice;
    char* mExportAddress;
    bool mExportable;
    int mMin;
    int mMax;
    int mId;

	// value state for function bindings
	int mFunctionValue;
	bool mFunctionDown;
};

//////////////////////////////////////////////////////////////////////
//
// OscWatcher
//
////////////////////////////////////////////////////////////////////////

class OscWatcher {

  public:

    OscWatcher();
    OscWatcher(class XmlElement* e);
    ~OscWatcher();

    OscWatcher* getNext();
    void setNext(OscWatcher* e);

    const char* getPath();
    void setPath(const char* path);

    const char* getName();
    void setName(const char* name);

    int getTrack();
    void setTrack(int t);

    void toXml(class XmlBuffer* b);

  private:

    void init();
    void parseXml(class XmlElement* e);

    OscWatcher* mNext;
    char* mPath;
    char* mName;
    int mTrack;
    
};

//////////////////////////////////////////////////////////////////////
//
// OscRuntimeWatcher
//
////////////////////////////////////////////////////////////////////////

/**
 * An extension of the WatchPointListener interface that is
 * registered with Mobius for each OscWatcher.
 */
class OscRuntimeWatcher : public WatchPointListener
{
  public:

    OscRuntimeWatcher(class OscConfig* config, class OscWatcher* src);
    ~OscRuntimeWatcher();

    void finish(class MobiusInterface* m, class WatchPoint* wp,
                class OscInterface* osc, class OscDevice* dev);

    // required WatchPointListener methods
    const char* getWatchPointName();
    int getWatchPointTrack();
    void watchPointEvent(int value);

    void tick();
    void setTrace(bool b);

  private:

    void send(float value);

    char* mPath;
    char* mName;
    int mTrack;

    WatchBehavior mBehavior;
    int mMin;
    int mMax;

    class OscInterface* mOsc;
    class OscDevice* mDevice;

    int mLast;
    int mSends;
    int mTicks;
    int mPendingValue;
    bool mPending;
    bool mDecaying;
    bool mTrace;
};

//////////////////////////////////////////////////////////////////////
//
// OscResolver
//
////////////////////////////////////////////////////////////////////////

/**
 * A class used to resolve incomming OSC messages and send
 * outging parameter exports.  This is built from the OscConfig
 * and owns it.  It contains all of the state that may be used 
 * by the OSC listener thread and MobiusThread.  It is encapsulated
 * so that we can reload the OscConfig file, build a new OscResolver
 * and splice it in without corrupting the OscResolver being used
 * by the other threads.
 */
class OscResolver {

  public:

    OscResolver(class MobiusInterface* mobius, class OscInterface* osc,
                OscConfig* config);
    ~OscResolver();

    OscResolver* getNext();
    void setNext(OscResolver* osc);
    void setTrace(bool b);

    void oscMessage(OscMessage* msg);
	void exportStatus(bool force);

  private:

    void addBinding(OscBindingSet* set, Binding* b);
    void addExport(OscBindingSet* set, OscBinding* ob);
    OscBinding* getBinding(const char* trigger);
    void addBinding(OscBinding* b);
    
    class MobiusInterface* mMobius;
    class OscInterface* mOsc;
    OscResolver* mNext;
    class OscConfig* mConfig;
    OscBinding* mBindings;
    class Map* mBindingMap;
    List* mExports;

    bool mTrace;
};

//////////////////////////////////////////////////////////////////////
//
// OscRuntime
//
////////////////////////////////////////////////////////////////////////

/**
 * Class used at runtime to perform OSC method translation, 
 * Mobius target loopup, and OSC status export.
 */
class OscRuntime : public OscListener {
  public:
	
	OscRuntime(class MobiusInterface* m);
	~OscRuntime();

    /**
     * Refresh parameters taken from the global MobiusConfig.
     */
    void updateGlobalConfiguration(class MobiusInterface* m);

    /**
     * Reload the configuration file which may have been edited.
     */
    void reloadConfigurationFile(class MobiusInterface* m);

    /**
     * OscListener interface to receive messages.
     */
    void oscMessage(OscMessage* msg);

    /**
     * Periodically called by MobiusThread to send out status messages.
     */
	void exportStatus();

  private:

    void updateGlobalConfiguration(class MobiusInterface* m, bool refreshExports);
    void registerWatchers(class OscConfig* config);

    class MobiusInterface* mMobius;
    class OscInterface* mOsc;
    class OscResolver* mResolver;
    class List* mWatchers;

    int mInputPort;
    int mOutputPort;
    char* mOutputHost;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
