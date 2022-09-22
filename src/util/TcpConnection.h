/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simple TCP client utility.
 *
 * The TcpConnection class provides a slightly higher level interface
 * for dealing with host specific libraries like WINSOCK.
 *
 */

#ifndef _TCPCONNECTION_H
#define _TCPCONNECTION_H

#ifdef _WIN32
// sigh need this for sockaddr_in
// somewhere along the line we started needing winsock2 which is 
// is included indirectly
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "port.h"

/****************************************************************************
 * TcpConnection
 *
 * Description: 
 * 
 * Class that encapsulate the stuff necessary to establish an HTTPD
 * server connection, and send and receive simple messages.
 ****************************************************************************/

class TcpConnection {

  public:

	INTERFACE TcpConnection();
	INTERFACE ~TcpConnection();

	INTERFACE void setHost(const char *host);
	INTERFACE void setPort(int port);
	INTERFACE int  connectHost(void);

	INTERFACE char *get(const char *path);

	INTERFACE char *getResponse(void);

	void setDebug(int d) {
		mDebug = 1;
	}

	// not part of HTTP, but keep here since it requires winsock
	INTERFACE const char *getLocalHost(void);

	INTERFACE void send(unsigned char *msg, int length);
	INTERFACE int receive(unsigned char *buffer, int maxlen);


  protected:

	char 	*mHost;
	char 	*mLocalHost;
	int		mPort;
	int		mTcpStarted;
	int		mSocket;
	int 	mDebug;

	struct sockaddr_in mServer;

	char 	*mBuffer;

	//
	// internal methods
	//

	int tcpStart(void);
	int tcpStop(void);
	void throwTcpError(const char *msg);
	void throwInternalError(const char *msg);
	void debug(const char *msg);
	void disconnect(void);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
