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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"

#include "HostConfig.h"

/****************************************************************************
 *                                                                          *
 *                                HOST CONFIG                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC HostConfig::HostConfig()
{
    init();
}

PUBLIC HostConfig::HostConfig(XmlElement* e)
{
    init();
    parseXml(e);
}

PRIVATE void HostConfig::init()
{
    mNext               = NULL;
    mVendor             = NULL;
    mProduct            = NULL;
    mVersion            = NULL;
    mStereo             = false;
    mRewindsOnResume    = false;
    mPpqPosTransport    = false;
    mSamplePosTransport = false;
}

PUBLIC HostConfig::~HostConfig()
{
	HostConfig *el, *next;

    delete mVendor;
    delete mProduct;
    delete mVersion;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC HostConfig* HostConfig::getNext()
{
    return mNext;
}

PUBLIC void HostConfig::setNext(HostConfig* c)
{
    mNext = c;
}

//
// Scope
//

PUBLIC const char* HostConfig::getVendor()
{
    return mVendor;
}

PUBLIC void HostConfig::setVendor(const char* s)
{   
    delete mVendor;
    mVendor = CopyString(s);
}

PUBLIC const char* HostConfig::getProduct()
{
    return mProduct;
}

PUBLIC void HostConfig::setProduct(const char* s)
{   
    delete mProduct;
    mProduct = CopyString(s);
}

PUBLIC const char* HostConfig::getVersion()
{
    return mVersion;
}

PUBLIC void HostConfig::setVersion(const char* s)
{   
    delete mVersion;
    mVersion = CopyString(s);
}

//
// Options
//

PUBLIC bool HostConfig::isStereo()
{
    return mStereo;
}

PUBLIC void HostConfig::setStereo(bool b)
{
    mStereo = b;
}

PUBLIC bool HostConfig::isRewindsOnResume()
{
    return mRewindsOnResume;
}

PUBLIC void HostConfig::setRewindsOnResume(bool b)
{
    mRewindsOnResume = b;
}

PUBLIC bool HostConfig::isPpqPosTransport()
{
    return mPpqPosTransport;
}

PUBLIC void HostConfig::setPpqPosTransport(bool b)
{
    mPpqPosTransport = b;
}

PUBLIC bool HostConfig::isSamplePosTransport()
{
    return mSamplePosTransport;
}

PUBLIC void HostConfig::setSamplePosTransport(bool b)
{
    mSamplePosTransport = b;
}

//
// XML
//

#define EL_HOST_CONFIG "HostConfig"
#define ATT_VENDOR "vendor"
#define ATT_PRODUCT "product"
#define ATT_VERSION "version"
#define ATT_STEREO "stereo"
#define ATT_REWINDS_ON_RESUME "rewindsOnResume"
#define ATT_PPQ_POS_TRANSPORT "ppqPosTransport"
#define ATT_SAMPLE_POS_TRANSPORT "samplePosTransport"

PRIVATE void HostConfig::parseXml(XmlElement* e)
{
	setVendor(e->getAttribute(ATT_VENDOR));
	setProduct(e->getAttribute(ATT_PRODUCT));
	setVersion(e->getAttribute(ATT_VERSION));

    mStereo = e->getBoolAttribute(ATT_STEREO);
    mRewindsOnResume = e->getBoolAttribute(ATT_REWINDS_ON_RESUME);
    mPpqPosTransport = e->getBoolAttribute(ATT_PPQ_POS_TRANSPORT);
    mSamplePosTransport = e->getBoolAttribute(ATT_SAMPLE_POS_TRANSPORT);
}

PUBLIC void HostConfig::toXml(class XmlBuffer* b)
{
	b->addOpenStartTag(EL_HOST_CONFIG);

    b->addAttribute(ATT_VENDOR, mVendor);
    b->addAttribute(ATT_PRODUCT, mProduct);
    b->addAttribute(ATT_VERSION, mVersion);

    b->addAttribute(ATT_STEREO, mStereo);
    b->addAttribute(ATT_REWINDS_ON_RESUME, mRewindsOnResume);
    b->addAttribute(ATT_PPQ_POS_TRANSPORT, mPpqPosTransport);
    b->addAttribute(ATT_SAMPLE_POS_TRANSPORT, mSamplePosTransport);

	b->closeEmptyElement();
}

/****************************************************************************
 *                                                                          *
 *                                HOST CONFIGS                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC HostConfigs::HostConfigs()
{
    init();
}

// now used when we we have a standalone host.xml file
PUBLIC HostConfigs::HostConfigs(const char *xml)
{
	init();
	parseXml(xml);
}

// used when we were embedded
PUBLIC HostConfigs::HostConfigs(XmlElement* e)
{
    init();
    parseXml(e);
}

PRIVATE void HostConfigs::init()
{
    mConfigs = NULL;
    mVendor = NULL;
    mProduct = NULL;
    mVersion = NULL;
}
    
PUBLIC HostConfigs::~HostConfigs()
{
    delete mConfigs;
    delete mVendor;
    delete mProduct;
    delete mVersion;
}

PUBLIC HostConfig *HostConfigs::getConfigs()
{
    return mConfigs;
}

PUBLIC void HostConfigs::setConfigs(HostConfig* configs)
{
    mConfigs = configs;
}

PUBLIC void HostConfigs::setHost(const char* vendor, const char* product, 
                                 const char* version)
{
    delete mVendor;
    delete mProduct;
    delete mVersion;

    // collapse empty strings to NULL for our comparisons
    mVendor = copyString(vendor);
    mProduct = copyString(product);
    mVersion = copyString(version);
}

/**
 * Collapse empty strings to NULL, so the HostInterface can give
 * us static buffers which may be empty.
 */
PRIVATE char* HostConfigs::copyString(const char* src)
{
    char* copy = NULL;
    if (src != NULL && strlen(src) > 0)
      copy = CopyString(src);
    return copy;
}

PUBLIC void HostConfigs::add(HostConfig* c) 
{
	// keep them ordered
	HostConfig *prev;
	for (prev = mConfigs ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mConfigs = c;
	else
	  prev->setNext(c);
}

/**
 * Find the most specific configuration for the currently scoped host.
 * The notion here is that there can be a HostConfig with no vendor
 * to represent the default options, one with just a vendor 
 * for everything from one company (unlikely), one with just a product
 * for all versions of a product, and one with a product and a version
 * for a specific version.  Version is relevant only if product is non-null.
 *
 * I was originally thinking that each option could have a default
 * and be overridden by more specific configs but because we're dealing
 * with bools, there is no "unset" state so we'll find the most specific
 * config for the host and use everything in it.  Since we're only dealing
 * with three flags that isn't so bad, but it also means we can't have
 * a global override.  I suppose we could make the host-less config
 * be a global override rather than a fallback default.
 * 
 */
PRIVATE HostConfig* HostConfigs::getConfig()
{
    HostConfig* found = NULL;

    for (HostConfig* c = mConfigs ; c != NULL ; c = c->getNext()) {
        
        if (isMatch(c) && (found == NULL || isMoreSpecific(found, c)))
          found = c;
    }

    return found;
}

/**
 * If any of the search fields is NULL then it can only match
 * a HostConfig that has a NULL value.  In practice this should 
 * only happen for Vendor (does AU give us that?) and Version.
 * VSTs should provide all three.  
 *
 * If a search field is non-NULL it will match a config if the
 * values are the same or the config value is NULL.  This lets
 * configs have "wildcard" values.  For example, to configure
 * all versions of Cubase just set the product and leave the vendor
 * and version blank.  In fact, I expect that vendor will be missing
 * most of the time.
 */
PRIVATE bool HostConfigs::isMatch(HostConfig* config)
{
    return ((config->getVendor() == NULL ||
             StringEqual(mVendor, config->getVendor())) &&

            (config->getProduct() == NULL ||
             StringEqual(mProduct, config->getProduct())) &&

            (config->getVersion() == NULL ||
             StringEqual(mVersion, config->getVersion()))
        );
}

/**
 * One config is more specific than another if it has a non-null value
 * for any of the fields and the previous one has a NULL value.
 */
PRIVATE bool HostConfigs::isMoreSpecific(HostConfig* prev, HostConfig* neu)
{
    return ((prev->getVendor() == NULL && neu->getVendor() != NULL) ||
            (prev->getProduct() == NULL && neu->getProduct() != NULL) ||
            (prev->getVersion() == NULL && neu->getVersion() != NULL));
}

PUBLIC bool HostConfigs::isStereo()
{
    HostConfig* config = getConfig();
    return (config != NULL) ? config->isStereo() : false;
}

PUBLIC bool HostConfigs::isRewindsOnResume()
{
    HostConfig* config = getConfig();
    return (config != NULL) ? config->isRewindsOnResume() : false;
}

PUBLIC bool HostConfigs::isPpqPosTransport()
{
    HostConfig* config = getConfig();
    return (config != NULL) ? config->isPpqPosTransport() : false;
}

PUBLIC bool HostConfigs::isSamplePosTransport()
{
    HostConfig* config = getConfig();
    return (config != NULL) ? config->isSamplePosTransport() : false;
}

//
// XML
//

PRIVATE void HostConfigs::parseXml(const char *src) 
{
    mError[0] = 0;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(src);
    XmlElement* e = NULL;

	if (d != NULL)
      e = d->getChildElement();

	if (e != NULL)
      parseXml(e);
    else
        CopyString(p->getError(), mError, sizeof(mError));

    delete d;
	delete p;
}

PRIVATE void HostConfigs::parseXml(XmlElement* e)
{
	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_HOST_CONFIG)) {
			HostConfig* c = new HostConfig(child);
			add(c);
		}
    }
}

PUBLIC void HostConfigs::toXml(XmlBuffer* b)
{
	b->addStartTag(EL_HOST_CONFIGS);
    b->incIndent();

    for (HostConfig* c = mConfigs ; c != NULL ; c = c->getNext())
      c->toXml(b);

    b->decIndent();
    b->addEndTag(EL_HOST_CONFIGS);
}

PUBLIC const char* HostConfigs::getError()
{
    return (mError[0] != 0) ? mError : NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
