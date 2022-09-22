
#include <stdio.h>
#include <string.h>

#include "util.h"

int main(int argc, char *argv[])
{
	int status = 0;


	printf("Random 0-10 = %d\n", Random(0, 10));

	const char* path = "/Users/jeff";
	bool abs = IsAbsolute(path);
	printf("Path %s is %s\n", path, (abs) ? "absolute" : "relative");

	path = "dev";
	abs = IsAbsolute(path);
	printf("Path %s is %s\n", path, (abs) ? "absolute" : "relative");

	char* stuff = ReadFile("testdata.txt");
	printf("File contents:\n%s", stuff);
	
	char buffer[1024];
	GetWorkingDirectory(buffer, sizeof(buffer));
	printf("Working directory: %s\n", buffer);

	return status;
}

