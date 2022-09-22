Tue Jan 06 22:32:20 2009

This a slightly hacked older version of PortAudio v19.  I'm not
exactly sure where what date this was captured, it was probably around
2005.  This was used with Mobius through December 2008.

The sources have been consolodated into one directory including
the ASIO SDK.  I made a few changes to fix a suggested latency
bug and to return the final host buffer size in PaStreamInfo.  
These changes were not carried into the 20071207 sources but may
want to do that.


