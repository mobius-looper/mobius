#include "asiosys.h"

#if DEBUG
#if MAC
#include <TextUtils.h>
void DEBUGGERMESSAGE(char *string)
{
	c2pstr(string);
	DebugStr((unsigned char *)string);
}
#else
// jsl - didn't have an implementation originally
//#error debugmessage
#include <windows.h>
void DEBUGGERMESSAGE(char *string)
{
	OutputDebugString(string);
}
#endif
#endif
