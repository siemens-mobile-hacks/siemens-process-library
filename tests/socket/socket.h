
#ifndef __SOCKET_H__
#define __SOCKET_H__


enum {
    ER_CS_DNR = -1,
    ER_CS_DNR_ATTEMPTS_LIMIT = -2,
    ER_CS_CRT_SOCK = -3,
    ER_CS_CONNECT = -4,
    ER_CS_SEND = -5,
    ER_CS_RECV = -6,
    ER_CS_STATE = -7,
    ER_CS_REMOTE_CLOSED = -8,
    ER_CS_CLOSED = -9
};


int createSocket();
int closeSocket(int sid);
int openSocketByHost(int sid, const char *host, int port);
int openSocketByIp(int sid, unsigned int ip, int port);
int streamBySocket(int sid);

int disconnectFromSocketHost(int sid);
int waitForSocketConnected(int sid);

int writeSocket(int sid, void *data, size_t size);

int waitForSocketReadyRead(int sid);
int readSocket(int sid, void *data, size_t size);
int waitForSocketReadFinished(int sid);

int disconnectFromSocketHost(int sid);
int waitForSocketDisconnected(int sid);
#endif
