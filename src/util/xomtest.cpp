/* 
 * A little tester for the XML mini parser and memory model.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "XomParser.h"

/****************************************************************************
 * main
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Parses the input file into an XmlDocument object, then wanders
 * over the object.
 ****************************************************************************/

int main(int argc, char *argv[])
{
	XomParser		*p;
	XmlDocument		*xml;
	int 			status;

	status = 0;
    if (argc < 2) {
		printf("usage: xomtest <infile> [dump]\n");
		status = 1;
    }
 	else {
		// create parser
		p = new XomParser;

		// let it go
        xml = p->parseFile(argv[1]);
        if (xml != NULL) {
            if (argc > 2)
              xml->dump();
            else
              printf("File %s parsed successfully.\n", argv[1]);
		}
        else {
            printf("XML Parser error: %s\n", p->getError());
        }

		delete p;
		delete xml;
	}

    return status;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
