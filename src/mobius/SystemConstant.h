/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Common superclass for various constant objects that are allocated
 * during static initialization.  
 *
 */

#ifndef SYSCON_H
#define SYSCON_H

/****************************************************************************
 *                                                                          *
 *                              SYSTEM CONSTANT                             *
 *                                                                          *
 ****************************************************************************/

#define MAX_CONSTANT_DISPLAY_NAME 32

/**
 * System constants all have a name and an optional display name.  
 * Some will have a catalog key with deferred localization.
 */
class SystemConstant {
  public:

	SystemConstant();
	SystemConstant(const char* name, int key);
	SystemConstant(const char* name, const char* displayName);

    virtual ~SystemConstant();

    const char* getName();
    void setName(const char* name);

    const char* getDisplayName();
    void setDisplayName(const char* name);

    int getKey();
    void setKey(int key);

    const char* getHelp();
    void setHelp(const char* name);

    void localize(class MessageCatalog* cat);

  private:

    void init();

    /**
     * This name is assumed to be a static string constant and will
     * not be copied or freed.
     */
    const char* mName;

    /**
     * This may come from a static or a message catalog so keep
     * a private copy.
     */
    char mDisplayName[MAX_CONSTANT_DISPLAY_NAME];

    /**
     * Non-zero if we initialize display name from a message catalog.
     */
    int mKey;

    /**
     * Used by functions, nothing else.
     * This is assumed to be static text.  If we're going to do
     * localization right, then this needs to be localized too.
     * I don't want to mess with that right now, but I don't want
     * to lose the old static help.
     */
    const char* mHelp;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
