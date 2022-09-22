/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An simple file-based message catlog.
 * 
 */

#ifndef UTIL_MSGCAT_H
#define UTIL_MSGCAT_H

#include <stdio.h>

class MessageCatalog {

  public:
	
	MessageCatalog();
	MessageCatalog(const char* file);
	~MessageCatalog();

	bool read(const char* file);

	const char* get(int index);

	void dump();

  private:

	void init();
	void clear();
	int read(FILE* fp, char** messages);

	char** mMessages;
	int mMessageCount;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
