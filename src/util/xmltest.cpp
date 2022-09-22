/* 
 * A little tester for the XML mini parser.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "Util.h"
#include "XmlParser.h"

/****************************************************************************
 * TestHandler
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Our XmlHandler subclass.
 ****************************************************************************/

class TestHandler : public XmlEventHandler {

  public:

	INTERFACE void openDoctype(class XmlMiniParser *p, 
							   char *name, 
							   char *pubid,
							   char *sysid);

	INTERFACE void closeDoctype(class XmlMiniParser *p);

	INTERFACE void openStartTag(class XmlMiniParser *p, 
										char *name);

	INTERFACE void attribute(class XmlMiniParser *p, 
									 char *name, 
									 char *value);

	INTERFACE void closeStartTag(class XmlMiniParser *p, int empty);
	INTERFACE void endTag(class XmlMiniParser *p, char *name);
	INTERFACE void comment(class XmlMiniParser *p, 
								   char *text);
	INTERFACE void pi(class XmlMiniParser *p, char *text);
	INTERFACE void pcdata(class XmlMiniParser *p, char *text);
	INTERFACE void entref(class XmlMiniParser *p, char *name);
	INTERFACE void cdata(class XmlMiniParser *p, char *text);

	INTERFACE void error(class XmlMiniParser *p, int code, const char *msg);

	TestHandler(void) {
		mDump = 0;
	}

	void setDump(int d) {
		mDump = d;
	}

  private:
	
	int mDump;

};

/****************************************************************************
 *                                                                          *
 *   						 TEST HANDLER METHODS                           *
 *                                                                          *
 ****************************************************************************/

INTERFACE void TestHandler::openDoctype(XmlMiniParser *p, 
											char *name, 
											char *pubid,
											char *sysid)
{
	if (mDump) {
		printf("DOCTYPE %s", name);
		if (pubid != NULL)
		  printf(" PUBID \"%s\"", pubid);
		if (sysid != NULL)
		  printf(" SYSID \"%s\"", sysid);
		printf("\n");
	}

	delete name;
	delete pubid;
	delete sysid;
}

INTERFACE void TestHandler::closeDoctype(class XmlMiniParser *p)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("DOCTYPE CLOSE\n");
}

INTERFACE void TestHandler::openStartTag(XmlMiniParser *p, 
											 char *name)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("STAGO %s\n", name);
	delete name;
}

INTERFACE void TestHandler::attribute(XmlMiniParser *p, 
										  char *name, 
										  char *value)
{
	UNUSED_VARIABLE(p);

	if (mDump)
	  printf("ATT %s = \"%s\"\n", name, value);

	delete name;
	delete value;
}

INTERFACE void TestHandler::closeStartTag(XmlMiniParser *p, int empty)
{
	UNUSED_VARIABLE(p);

	if (mDump) {
		printf("STAGC");
		if (empty)
		  printf(" empty");
		printf("\n");
	}
}

INTERFACE void TestHandler::endTag(XmlMiniParser *p, char *name)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("ETAG %s\n", name);
	delete name;
}

INTERFACE void TestHandler::comment(XmlMiniParser *p, char *text)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("COMMENT \"%s\"\n", text);
	delete text;
}

INTERFACE void TestHandler::pi(XmlMiniParser *p, char *text)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("PI \"%s\"\n", text);
	delete text;
}

INTERFACE void TestHandler::pcdata(XmlMiniParser *p, char *text)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("PCDATA \"%s\"\n", text);
	delete text;
}

INTERFACE void TestHandler::entref(XmlMiniParser *p, char *name)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("ENTREF %s\n", name);
	delete name;
}

INTERFACE void TestHandler::cdata(XmlMiniParser *p, char *text)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("CDATA \"%s\"\n", text);
	delete text;
}

INTERFACE void TestHandler::error(XmlMiniParser *p, int code, const char *msg)
{
	UNUSED_VARIABLE(p);
	if (mDump)
	  printf("ERROR %d: s\n", code, msg);
}

/****************************************************************************
 *                                                                          *
 *   								 MAIN                                   *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * main
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Parses the input file.
 ****************************************************************************/

int main(int argc, char *argv[])
{
	XmlMiniParser 	*p;
	TestHandler		*h;
	int 			status;

	status = 0;
    if (argc < 2) {
		printf("usage: xmltest <infile> [dump] \n");
		status = 1;
    }
 	else {
		// create a new parser
		p = new XmlMiniParser;
		h = new TestHandler;

		if (argc > 2)
		  h->setDump(1);

		p->setFile(argv[1]);
		p->setEventHandler(h);

        p->parse();
        if (p->getError() == NULL)
          printf("File %s parsed successfully.\n", argv[1]);
        else {
			printf("ERROR %d at line %d column %d: %s\n", 
				   p->getErrorCode(), p->getLine() + 1, p->getColumn() + 1, 
				   p->getError());
			status = 2;
		}

		delete p;
		delete h;
	}

    return status;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
