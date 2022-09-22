/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for describing the idiosyncrasies of plugin hosts.
 *
 */

#ifndef HOST_CONFIG_H
#define HOST_CONFIG_H

/****************************************************************************
 *                                                                          *
 *                                HOST CONFIG                               *
 *                                                                          *
 ****************************************************************************/

/**
 * A collection of options keyed by vendor and version.
 * A list of these will be managed in one HostConfigs objects, 
 * normally stored within MobiusConfig.
 */
class HostConfig {

  public:

    HostConfig();
    HostConfig(class XmlElement* e);
    ~HostConfig();

    HostConfig* getNext();
    void setNext(HostConfig* c);

    // Host Identification

    const char* getVendor();
    void setVendor(const char* s);

    const char* getProduct();
    void setProduct(const char* s);

    const char* getVersion();
    void setVersion(const char* s);

    // Host Options

    bool isStereo();
    void setStereo(bool b);

    bool isRewindsOnResume();
    void setRewindsOnResume(bool b);
    
    bool isPpqPosTransport();
    void setPpqPosTransport(bool b);

    bool isSamplePosTransport();
    void setSamplePosTransport(bool b);

    void toXml(class XmlBuffer* b);


  private:

    void init();
    void parseXml(class XmlElement* e);

    HostConfig* mNext;

    /**
     * Name of the host vendor: "Steinberg", etc.
     * For VSTs this must match what is returned by the
     * getHostVendorString VST interface method.
     */
    char* mVendor;

    /**
     * Name of the prodict: "Cubase", etc.
     * For VSTs this must match what is returned by the
     * getHostProductString VST interface method.
     */
    char* mProduct;

    /**
     * Version of the product.  For VSTs this will be the
     * string representation of a number.  If we ever use
     * this for AU hosts it may need to be more than a number?
     */
    char* mVersion;

    /**
     * When true forces the plugin to advertise a single pair 
     * of stereo pins no matter what else is in the MobiusConfig.
     * This was necessary for older versions of Cubase and Orion that
     * didn't like plugins with lots of pins, not sure if it's relevant
     * for newer versions.
     */
    bool mStereo;

    /**
     * When true it means that the host transport rewinds a bit after a resume.
     * This was noticed in an old version of Cubase...
     *
     * "Hmm, Cubase as usual throws a wrench into this.  Because of it's odd
     * pre-roll, ppqPos can actually go negative briefly when starting from
     * zero. But it is -0.xxxxx which when you truncate is just 0 so we can't
     * tell when the beat changes given the lastBeat formula above."
     *
     * When this is set it tries to compensate for this pre-roll, not sure
     * if modern versions of Cubase do this.
     */
    bool mRewindsOnResume;

    /**
     * When true, we check for stop/play by monitoring the ppqPos
     * rather than expecting kVstTransportChanged events.
     * This was originally added for Usine around 2006, not sure if
     * it's still necessary.
     */
    bool mPpqPosTransport;

    /**
     * When true we check for stop/play by monitoring the samplePos.
     * rather than expecting kVstTransportChanged events.
     * This was added a long time ago and hasn't been enabled for several
     * releases.
     */
    bool mSamplePosTransport;

};

/****************************************************************************
 *                                                                          *
 *                                HOST CONFIGS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Constant MobiusConfig uses to recognize us.
 */
#define EL_HOST_CONFIGS "HostConfigs"

/**
 * Object encapsulating a list of Hostconfig objects, and providing
 * methods for resolving options.
 */
class HostConfigs {
  public:

    HostConfigs();
    HostConfigs(const char* xml);
    HostConfigs(class XmlElement* e);
    ~HostConfigs();
    
    const char* getError();
    HostConfig *getConfigs();
    void setConfigs(HostConfig* configs);
    void add(HostConfig* c);

    void setHost(const char* vendor, const char* product, const char* version);

    bool isStereo();
    bool isRewindsOnResume();
    bool isPpqPosTransport();
    bool isSamplePosTransport();

    void toXml(class XmlBuffer* b);

  private:

    void init();
    void parseXml(const char* xml);
    void parseXml(class XmlElement* e);
    char* copyString(const char* src);
    HostConfig* getConfig();
    bool isMatch(HostConfig* config);
    bool isMoreSpecific(HostConfig* prev, HostConfig* neu);

    HostConfig* mConfigs;

    // todo: need trace flags in here so we don't have
    // to hard code them in the plugins
    //bool mTrace;

    // Scope = this object is unusual in that we can give it a scope
    // at runtime that then influences how we look up options.  Normally
    // the scope is set once when the plugin is instantiated.
    
    char* mVendor;
    char* mProduct;
    char* mVersion;

    // parser error
    char mError[256];
};


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
