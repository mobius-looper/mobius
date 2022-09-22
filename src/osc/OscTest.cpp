/* Copyright (c) 2009 Jeffrey S. Larson.  All rights reserved. */
/*
 * OSC message dump utility written against the OscInterface
 */

#include <stdlib.h>
#include <stdio.h>

#include "Trace.h"
#include "OscInterface.h"

class TestListener : public OscListener 
{
  public:
	TestListener() {}
	~TestListener() {}

	void oscMessage(OscMessage* msg) {

		printf("Message received: %s ", msg->getAddress());
		int nargs = msg->getNumArgs();
		for (int i = 0 ; i < nargs ; i++)
		  printf("%f ", msg->getArg(i));
		printf("\n");
		fflush(stdout);

		msg->free();
	}

};

int main(int argc, char* argv[])
{
	int port = 7000;

	TracePrintLevel = 2;

	OscInterface* osc = OscInterface::getInterface();
	osc->setReceivePort(port);
	osc->setListener(new TestListener());

    if (argc > 1) {
        const char* host = "192.168.0.110";
        int outport = 9000;
        const char* address = argv[1];
        float value = 0.0f;

        if (argc > 2) 
          value = (float)atof(argv[2]);

        printf("Sending message: %s %f\n", address, value);

        OscMessage* msg = new OscMessage();
        msg->setAddress(address);
        msg->setNumArgs(1);
        msg->setArg(0, value);

        // this takes ownership
        osc->send(host, outport, msg);
    }
    else {
        printf("Listening for input on port %d\n", port);
        printf("Press any key to exit...\n");
        fflush(stdout);

        osc->start();

        // wait for a key
        getchar();
    }

	// stop the thread and clean up
	osc->stop();
	delete osc;

    return 0;
}
