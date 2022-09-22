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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>

#include "Util.h"
#include "MessageCatalog.h"

PUBLIC MessageCatalog::MessageCatalog()
{
	init();
}

PUBLIC MessageCatalog::MessageCatalog(const char* file)
{
	init();
	read(file);
}

PUBLIC void MessageCatalog::init()
{
	mMessages = NULL;
	mMessageCount = 0;
}

PUBLIC MessageCatalog::~MessageCatalog()
{
	clear();
}

PUBLIC void MessageCatalog::clear()
{
	if (mMessages != NULL) {
		for (int i = 0 ; i < mMessageCount ; i++)
		  delete mMessages[i];
		delete mMessages;
	}
	mMessages = NULL;
	mMessageCount = 0;
}

PUBLIC const char* MessageCatalog::get(int index)
{
	const char* msg = "???";
	if (index < mMessageCount)
	  msg = mMessages[index];
	return msg;
}


PUBLIC bool MessageCatalog::read(const char* file)
{
	bool success = false;

	FILE* fp = fopen(file, "r");
	if (fp != NULL) {
		success = true;

		// first extract the maximum index
		int maxIndex = read(fp, NULL);
		if (maxIndex >= 0) {
			mMessageCount = maxIndex + 1;
			mMessages = new char*[mMessageCount];
			for (int i = 0 ; i < mMessageCount ; i++)
			  mMessages[i] = NULL;
			
			// once again with FEELING
			fseek(fp, 0, 0);
			read(fp, mMessages);
		}
	}
	return success;
}

PRIVATE int MessageCatalog::read(FILE* fp, char** messages)
{
	int maxIndex = -1;
	char line[1024];

	int maxline = sizeof(line) - 1;
	while (fgets(line, maxline, fp) != NULL) {
		// skip leading whitespace
		char* ptr = line;
		for ( ; *ptr && isspace(*ptr) ; ptr++);
		if (*ptr && *ptr != '#') {
			// isolate the number
			char* end = ptr;
			for ( ; *end && !isspace(*end) ; end++);
			if (*end) {
				*end = 0;
				int index = atoi(ptr);
				if (index > maxIndex)
				  maxIndex = index;
				if (messages != NULL) {
					ptr = end + 1;
					end = ptr;
					for ( ; *end && *end != 0xA && *end != 0xD ; end++);
					if (end > ptr) {
						*end = 0;
						messages[index] = CopyString(ptr);
					}
				}
			}
		}
	}

	return maxIndex;
}

PUBLIC void MessageCatalog::dump()
{
	if (mMessageCount == 0)
	  printf("Catalog has no messages\n");
	else {
		for (int i = 0 ; i < mMessageCount ; i++) {
			const char* msg = mMessages[i];
			if (msg == NULL) msg = "";
			printf("%d %s\n", i, msg);
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
