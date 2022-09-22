
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "Thread.h"

class TestThread : public Thread {

  public:

	TestThread() {
	}

	void eventTimeout() {
		printf("timeout\n");
        fflush(stdout);
	}

};

int main(int argc, char *argv[])
{
	Thread* t = new TestThread();
	t->start();

	SleepSeconds(10);

	t->stopAndWait();
	delete t;
    return 0;
}
