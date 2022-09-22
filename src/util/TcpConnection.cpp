/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Utilities for TCP connections.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
//#include <winsock.h>
#else
// Windows apparently gets this from winsock.h?
#include <errno.h>

// required for gethostname on OSX, windows seems to get it from winsock.h
#include <unistd.h>

// required for gethostbyname on OSX
#include <netdb.h>

// a windows convention we want on OSC
#define SOCKET_ERROR -1

#endif

#include "util.h"
#include "Trace.h"
#include "TcpConnection.h"
 
/****************************************************************************
 *                                                                          *
 *   						TCP CONNECTION CLASS                           *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * TcpConnection::TcpConnection
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Initializes a connection object.
 ****************************************************************************/

INTERFACE TcpConnection::TcpConnection(void)
{
	mHost		= NULL;
	mLocalHost	= NULL;
	mPort		= 80;
	mTcpStarted	= 0;
	mSocket		= 0;
	mDebug		= 0;
	
	// buffer for messages
	mBuffer		= new char[1024];

}

/****************************************************************************
 * TcpConnection::~TcpConnection
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destroys a connection object.
 ****************************************************************************/

INTERFACE TcpConnection::~TcpConnection(void)
{
	disconnect();

	// shutdown TCP ? 

	delete mHost;
	delete mLocalHost;
	delete mBuffer;
}

/****************************************************************************
 * TcpConnection::tcpStart
 *
 * Arguments:
 *
 * Returns: error 
 *
 * Description: 
 * 
 * Used on Windows to get Winsock started.
 * Should be called before any sockets API functions are called.
 ****************************************************************************/

PRIVATE int TcpConnection::tcpStart(void)
{
	int error = 0;

	if (!mTcpStarted) {
#ifdef _WIN32
		WSADATA wsdata;
		WSAStartup(2, &wsdata);
#endif
		// error conditions?
		mTcpStarted = 1;
	}

	return error;
}

/****************************************************************************
 * TcpConnection::tcpStop
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Called when the process is ready to stop using sockets.
 * Not sure if this is really necessary?
 ****************************************************************************/

PRIVATE int TcpConnection::tcpStop(void)
{
	int error = 0;

	if (mTcpStarted) {
#ifdef _WIN32
		WSACleanup();
#endif
		mTcpStarted = 0;
	}

	return error;
}

/****************************************************************************
 * TcpConnection::setHost
 *
 * Arguments:
 *	host: host name to contact
 *
 * Returns: none
 *
 * Description: 
 * 
 * Sets the host name to contact.
 * We could try to map the name and build the mServer here too, but
 * we wait till connect() time.
 ****************************************************************************/

INTERFACE void TcpConnection::setHost(const char *host)
{
	if (host && mHost && !strcmp(host, mHost)) {
		// names are the same, ignore
	}
	else {
		// must close current connection if any
		disconnect();

		delete mHost;
		mHost = CopyString(host);
	}
}

INTERFACE void TcpConnection::setPort(int port)
{
	if (mPort == port) {
		// port the same, ingore
	}
	else {
		// must close current connection if any
		disconnect();

		mPort = port;
	}
}

/****************************************************************************
 * TcpConnection::getLocalHost
 *
 * Arguments:
 *	none: 
 *
 * Returns: local host name
 *
 * Description: 
 * 
 * Returns the local host name of this machine.
 * This doesn't really belong here, but it requires Winsock initialization
 * so it was a convenient place to put it.  So we can behave more
 * like gethostname(), we retain ownership of the string, the caller
 * does not need to free it.
 ****************************************************************************/

INTERFACE const char *TcpConnection::getLocalHost(void)
{
	if (!mLocalHost) {
		char hostbuf[1024];

		tcpStart();
		int status = gethostname(hostbuf, sizeof(hostbuf));
		if (status == SOCKET_ERROR)
          throwTcpError("Couldn't use gethostname");

		mLocalHost = CopyString(hostbuf);
	}

	return mLocalHost;
}

/****************************************************************************
 * TcpConnection::throwTcpError
 *
 * Arguments:
 *	msg: message to add
 *
 * Returns: error 
 *
 * Description: 
 * 
 * Formats a TCP related error mesage, and includes any information we
 * can get from the OS.
 ****************************************************************************/

PRIVATE void TcpConnection::throwTcpError(const char *msg)
{
#ifdef _WIN32	
	sprintf(mBuffer, "Winsock error %d: %s\n", WSAGetLastError(), msg);
#else
	sprintf(mBuffer, "TCP error %d: %s\n", errno, msg);
#endif

    Trace(1, "TcpConnection: %s\n", mBuffer);

	throw new AppException(1, mBuffer);
}

/****************************************************************************
 * TcpConnection::throwInternalError
 *
 * Arguments:
 *	msg: message to add
 *
 * Returns: error 
 *
 * Description: 
 * 
 * Formats an internal error mesage.
 * Not much to do now, could let error code be passed in?
 ****************************************************************************/

PRIVATE void TcpConnection::throwInternalError(const char *msg)
{
    Trace(1, "TcpConnection: internal error: %s\n", msg);
	throw new AppException(1, msg);
}

/****************************************************************************
 * TcpConnection::debug
 *
 * Arguments:
 *	msg: message to display
 *
 * Returns: error 
 *
 * Description: 
 * 
 * Sends a message to the debug stream.
 ****************************************************************************/

PRIVATE void TcpConnection::debug(const char *msg)
{
	if (msg && strlen(msg)) {

		printf("%s", msg);

#ifdef _WIN32
		OutputDebugString(msg);
		if (msg[strlen(msg)-1] != '\n') {
			OutputDebugString("\n");
			printf("\n");
		}
#endif

	}
}

/****************************************************************************
 * TcpConnection::connect
 *
 * Arguments:
 *
 * Returns: error
 *
 * Description: 
 * 
 * Establishes a connection to the server, using previously
 * specified connection parameters.
 ****************************************************************************/

INTERFACE int TcpConnection::connectHost(void)
{
	int error = 0;

	// check if already connected
	if (mSocket)
	  return 0;

	// make sure Winsock is up
	error = tcpStart();

	if (mHost == NULL)
	  mHost = CopyString("localhost");

	// get server information from use supplied host name
	if (!error) {
		struct hostent *he;
		he = gethostbyname(mHost);
		if (!he)
		  throwTcpError("invalid host name");
		else {
			mServer.sin_family = he->h_addrtype;
			mServer.sin_port = htons((unsigned short) mPort);
			mServer.sin_addr.s_addr = 
				((unsigned long *) (he->h_addr_list[0]))[0];
		}
	}

	// create a socket
	if (!error) {
		mSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (mSocket < 0)
		  throwTcpError("couldn't create socket");
		else {
			// apachebench would do this, but we want synchronous IO
			// nonblock(mSocket);
			// how about TCP_NODELAY?

			// jsl - contrary to what the documentation says, 
			// this is almost always what you want on Windows
#ifdef _WIN32
			int value = 1;
			int n = setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY,
							   (const char *)&value, sizeof(value));
			if (n < 0)
			  throwTcpError("couldn't enable TCP_NODELAY");
#endif

		}
	}

	// connect, need to add a retry loop here!
	if (!error) {
		int n = connect(mSocket, (struct sockaddr *) &mServer, 
						sizeof(mServer));

		if (n < 0)
		  throwTcpError("couldn't connect");
	}

	// this is our connected flag, make sure its valid
	if (error && mSocket) {
#ifdef _WIN32
		closesocket(mSocket);
#else
		close(mSocket);
#endif	
		mSocket = NULL;
	}

	return error;
}

/****************************************************************************
 * TcpConnection::disconnect
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Disconnects from the TCPD server if we have a socket open.
 ****************************************************************************/

PRIVATE void TcpConnection::disconnect(void)
{
	if (mSocket) {
#ifdef _WIN32
		closesocket(mSocket);
#else
		close(mSocket);
#endif
		mSocket = NULL;
	}
}

/****************************************************************************
 * TcpConnection::send
 *
 * Arguments:
 *	     msg: message to send
 *
 * Returns: none
 *
 * Description: 
 * 
 * Sends a message to the server, does not wait for a response.
 * 
 ****************************************************************************/

INTERFACE void TcpConnection::send(unsigned char *msg, int length)
{
	if (!connectHost()) {

		if (mDebug)
		  printf("Sending message.");

		// should check errors !
		::send(mSocket, (char*)msg, length, 0);
	}
}

/****************************************************************************
 * TcpConnection:receive
 *
 * Arguments:
 *	buffer: buffer to put message in
 *  maxlength: length of the buffer
 *
 * Returns: length of message
 *
 * Description: 
 * 
 * Reads a response from the server.
 * 
 ****************************************************************************/

PRIVATE int TcpConnection::receive(unsigned char *buffer, int maxlen)
{

	int r = recv(mSocket, (char*)buffer, maxlen, 0);

	if (r == 0) {
		// socket has been "gracefully" closed by the server, what now?
		throwInternalError("HTTP socket closed by server");
	}
	else if (r == SOCKET_ERROR) {
		// disconnect?
		throwTcpError("couldn't read from socket");
	}

	return r;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
