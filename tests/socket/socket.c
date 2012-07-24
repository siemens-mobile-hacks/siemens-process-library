
#include <swilib.h>
#include <task.h>
#include <mutex.h>
#include <waitcondition.h>
#include <corearray.h>
#include "socket.h"


static CSM_DESC icsmd;
static CSM_RAM *old_icsm;
int IDLECSM_onMessage(CSM_RAM *ram, GBS_MSG *msg);
int (*old_icsm_onMessage)(CSM_RAM *ram, GBS_MSG *msg);
CoreArray sockets_mess_handless;

struct HandleQueue
{
    void (*handler)(int sid, CSM_RAM *, GBS_MSG *);
    int sid;
};


void replace_idle_message_func()
{
    corearray_init(&sockets_mess_handless, 0);

    CSM_RAM *icsm = FindCSMbyID(CSM_root()->idle_id);
    memcpy(&icsmd, icsm->constr, sizeof(icsmd));
    old_icsm = (CSM_RAM *)icsm->constr;
    old_icsm_onMessage = icsmd.onMessage;
    icsmd.onMessage = IDLECSM_onMessage;
    icsm->constr = &icsmd;
}


void restore_idle_message_func()
{
    corearray_release(&sockets_mess_handless);
    CSM_RAM *icsm = FindCSMbyID(CSM_root()->idle_id);
    icsm->constr = (void *)old_icsm;
}



int IDLECSM_onMessage(CSM_RAM *ram, GBS_MSG *msg)
{
    if (msg->msg != MSG_HELPER_TRANSLATOR )
        goto end;

    CoreArray *arr = &sockets_mess_handless;
    struct HandleQueue *h = 0;
    int i = 0;

    corearray_foreach(struct HandleQueue *, h, arr, i)
    {
        if(streamBySocket(h->sid) == (int)msg->data1)
            h->handler(h->sid, ram, msg);
    }

end:
    return old_icsm_onMessage(ram, msg);
}


static int findEmpty()
{
    CoreArray *arr = &sockets_mess_handless;
    void *h = 0;
    int i = 0;
    corearray_foreach(struct HandleQueue *, h, arr, i)
    {
        if(!h)
            return i;
    }

    return corearray_push_back(arr, 0);
}


static int registerSocketMessageHandler(int sid, void (*handler)(int sid, CSM_RAM *, GBS_MSG *))
{
    struct HandleQueue *h = malloc(sizeof *h);
    h->handler = handler;
    h->sid = sid;

    int id = findEmpty();
    corearray_store_cell(&sockets_mess_handless, id, h);
    return id;
}


static int removeSocketMessageHandler(int hid)
{
    return corearray_store_cell(&sockets_mess_handless, hid, 0);
}


#define MAX_SOCKETS 256

typedef struct
{
    short id;
    int fd;
    int message_handler;

    struct {
        int wid, status;
    }connection;

    struct {
        int wid, status;
    }disconnection;

    struct {
        int wid, status;
        int stop_read, stop_write;
        int waiting_for_ready_read;

        void *rdata;
        int rsize;
        int received;

        void *sdata;
        int ssize;
        int sended;
    }io;

    char connect_state;
    int ip, port;
    char *host;
    int attempts;
    GBSTMR dnr_wait;

} CoreSocket;


static CoreMutex mutex;
static CoreSocket sockets[MAX_SOCKETS];

__attribute_constructor
void socketsInit()
{
    replace_idle_message_func();
    createMutex(&mutex);
    for(int i=0; i<MAX_SOCKETS; ++i)
        sockets[i].id = -1;
}


__attribute_destructor
void socketsFini()
{
    destroyMutex(&mutex);
    restore_idle_message_func();
}


static short emptySocket()
{
    for(int i=0; i<MAX_SOCKETS; ++i)
        if(sockets[i].id == -1)
            return i;

    return -1;
}


static CoreSocket *getSocketData(int sid)
{
    if(sid < 0 || sid > MAX_SOCKETS-1)
        return 0;

    return &sockets[sid];
}



static CoreSocket *newSocket()
{
    lockMutex(&mutex);

    short _sid;
    CoreSocket *sock = getSocketData((_sid = emptySocket()));
    if(sock) {
        sock->id = _sid;
        unlockMutex(&mutex);

    } else
        unlockMutex(&mutex);

    return sock;
}


static int freeSocket(int _sid)
{
    CoreSocket *sock = getSocketData(_sid);
    if(!sock || sock->id < 0)
        return -1;

    lockMutex(&mutex);
    sock->id = -1;
    unlockMutex(&mutex);
    return 0;
}


/******************************************************************************/
/**************************** socket implementation ***************************/
/******************************************************************************/

static void socket_open_impl(CoreSocket *mysocket);
static void socketMessageHandler(int sid, CSM_RAM *ram, GBS_MSG *msg);


int createSocket()
{
    CoreSocket *sock = newSocket();
    if(!sock)
        return -1;

    sock->fd = -1;
    sock->connection.wid = createWaitCond("connetc");
    sock->disconnection.wid = createWaitCond("disconnect");
    sock->io.wid = createWaitCond("sock_io");
    sock->connect_state = 0;
    sock->ip = -1;
    sock->port = -1;
    sock->attempts = 0;
    sock->io.status = -1;
    sock->connection.status = -1;
    sock->disconnection.status = -1;
    sock->host = 0;
    sock->message_handler = registerSocketMessageHandler(sock->id, socketMessageHandler);
    sock->io.stop_read = 0;
    sock->io.stop_write = 0;

    return sock->id;
}


int closeSocket(int sid)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    removeSocketMessageHandler(sock->message_handler);
    return freeSocket(sid);
}


int openSocketByHost(int sid, const char *host, int port)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    sock->fd = -1;
    sock->connect_state = 0;
    sock->ip = -1;
    sock->attempts = 0;
    sock->io.status = 0;
    sock->connection.status = 0;
    sock->disconnection.status = 0;
    sock->host = strdup(host);
    sock->port = port;
    sock->io.waiting_for_ready_read = 0;

    SUBPROC(socket_open_impl, sock);
    return 0;
}


int openSocketByIp(int _sock, unsigned int ip, int port)
{
    CoreSocket *sock = getSocketData(_sock);
    if(!sock || sock->id < 0)
        return -1;

    sock->fd = -1;
    sock->connect_state = 0;
    sock->ip = ip;
    sock->port = port;
    sock->io.status = 0;
    sock->connection.status = 0;
    sock->disconnection.status = 0;
    sock->io.waiting_for_ready_read = 0;

    SUBPROC(socket_open_impl, sock);
    return 0;
}


int streamBySocket(int sid)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    return sock->fd;
}


static void wait_dnr_impl(GBSTMR *tmr)
{
    SUBPROC(socket_open_impl, (void*)tmr->param4);
}

static void socket_open_impl(CoreSocket *mysocket)
{
    if(!mysocket || mysocket->connect_state) {
        goto error;
    }

    int ***p_res = NULL;
    SOCK_ADDR sa;
    int err;
    int dnr_id;

    if(mysocket->ip != -1) goto connect_by_ip;

    err = async_gethostbyname(mysocket->host, &p_res, &dnr_id);

    if (err)
    {
        if ((err==0xC9)||(err==0xD6))
        {
            if (dnr_id)
            {
                mysocket->attempts ++;
                if(mysocket->attempts > 4) {
                        mysocket->connection.status = ER_CS_DNR_ATTEMPTS_LIMIT;
                        goto dnr_error;
                }

                mysocket->dnr_wait.param4 = (int)mysocket;
                GBS_StartTimerProc(&mysocket->dnr_wait, 2*216, wait_dnr_impl);
                return;
            }
        }
        else
        {
            mysocket->connection.status = ER_CS_DNR;
dnr_error:
            wakeAllWaitConds(mysocket->connection.wid);
            return;
        }
    }

    if (p_res)
    {
        if (p_res[3])
        {
            goto connect_by_res;

connect_by_ip:
            sa.ip = mysocket->ip;
            goto doit;

connect_by_res:
            sa.ip = p_res[3][0][0];

doit:
            mysocket->fd = socket(1, 1, 0);

            if (mysocket->fd != -1)
            {
                sa.family=1;
                sa.port = htons(mysocket->port);
                if (connect(mysocket->fd, &sa, sizeof(sa))!=-1)
                {
                    //printf("Connected %d\n", mysocket->fd);
                    mysocket->fd = mysocket->fd;
                    mysocket->connection.status = 0;
                    mysocket->connect_state = 1;
                    return;
                }
                else
                {
                    //printf("Socket connection fault\n");
                    closesocket(mysocket->fd);
                    mysocket->connection.status = ER_CS_CONNECT;
                    wakeAllWaitConds(mysocket->connection.wid);
                    return;
                }
            }
            else
            {
                //printf("Create socket fault\n");
                mysocket->connection.status = ER_CS_CRT_SOCK;
                wakeAllWaitConds(mysocket->connection.wid);
                return;
            }
        }
    }

error:
    //printf("Fuck...\n");
    mysocket->connection.status = -1;
    wakeAllWaitConds(mysocket->connection.wid);
}


int waitForSocketConnected(int _sid)
{
    CoreSocket *sock = getSocketData(_sid);
    if(!sock || sock->id < 0)
        return -1;

    waitCondition(sock->connection.wid);
    return sock->connection.status;
}




/******************************************************/
/********************* read ***************************/
/******************************************************/

static void socket_write_impl(CoreSocket *mysocket)
{
    if( mysocket->connect_state != 1 ) {
        goto error;
    }


    int sended = send(mysocket->fd, mysocket->io.sdata + mysocket->io.sended,
                             mysocket->io.ssize - mysocket->io.sended, 0);

    if(sended < 0)
    {
error:
        mysocket->io.stop_write = 1;
        mysocket->io.status = ER_CS_SEND;
        wakeAllWaitConds(mysocket->io.wid);
        return;
    }else
        mysocket->io.sended += sended;

    if(mysocket->io.sended >= mysocket->io.ssize) {
        mysocket->io.stop_write = 1;
        mysocket->io.status = 0;
    }
}


int writeSocket(int sid, void *data, size_t size)
{
    CoreSocket *mysocket = getSocketData(sid);
    if(!mysocket || mysocket->id < 0)
        return -1;

    if( mysocket->connect_state != 1 )
        return -1;

    mysocket->io.sdata = data;
    mysocket->io.ssize = size;
    mysocket->io.sended = 0;
    mysocket->io.stop_write = 0;
    mysocket->io.status = 0;
    SUBPROC(socket_write_impl, mysocket);
    return 0;
}



/******************************************************/
/********************* read ***************************/
/******************************************************/

static void socket_read_impl(CoreSocket *mysocket)
{
    if(  mysocket->connect_state != 2 )
        goto error;


    int received = 0;
    received = recv(mysocket->fd, mysocket->io.rdata+mysocket->io.received, mysocket->io.rsize-mysocket->io.received, 0);

    printf("receive: %d, last: %d\n", received, mysocket->io.rsize-mysocket->io.received);

    if(received < 0)
    {
error:
        mysocket->io.stop_read = 1;
        mysocket->io.status = ER_CS_RECV;
        wakeAllWaitConds(mysocket->io.wid);
        return;
    }else
        mysocket->io.received += received;

    ((char*)mysocket->io.rdata)[mysocket->io.received] = 0;

    if(mysocket->io.received >= mysocket->io.rsize) {
        mysocket->io.stop_read = 1;
        mysocket->io.status = 0;
        wakeAllWaitConds(mysocket->io.wid);
    }
}


int readSocket(int sid, void *data, size_t size)
{
    CoreSocket *mysocket = getSocketData(sid);
    if(!mysocket || mysocket->id < 0)
        return -1;

    if(mysocket->connect_state != 2)
        return -2;

    mysocket->io.rdata = data;
    mysocket->io.rsize = size;
    mysocket->io.received = 0;
    mysocket->io.stop_read = 0;
    mysocket->io.status = 0;
    SUBPROC(socket_read_impl, mysocket);
    return 0;
}


int waitForSocketReadyRead(int sid)
{
    CoreSocket *mysocket = getSocketData(sid);
    if(!mysocket || mysocket->id < 0)
        return -1;

    if( mysocket->connect_state != 1 )
        return -2;

    mysocket->io.waiting_for_ready_read = 1;
    waitCondition(mysocket->io.wid);
    mysocket->io.waiting_for_ready_read = 0;
    return mysocket->io.status;
}


int waitForSocketReadFinished(int sid)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    if(sock->connect_state != 2)
        return -2;

    waitCondition(sock->io.wid);
    return sock->io.status;
}




/******************************************************/
/******************** close ***************************/
/******************************************************/

static void socket_close_impl(CoreSocket *mysocket)
{
    printf("socket_close_impl\n");

    shutdown(mysocket->fd, 2);
    mysocket->disconnection.status = closesocket(mysocket->fd);
    mysocket->fd = -1;

    GBS_DelTimer(&mysocket->dnr_wait);

    wakeAllWaitConds(mysocket->disconnection.wid);
}


int disconnectFromSocketHost(int sid)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    if(sock->host) {
        free(sock->host);
        sock->host = 0;
    }
    sock->io.stop_read = 1;
    sock->io.stop_write = 1;
    sock->connect_state = -1;
    sock->connect_state = 3;

    SUBPROC(socket_close_impl, sock);
    return 0;
}


int waitForSocketDisconnected(int sid)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return -1;

    waitCondition(sock->disconnection.wid);
    return sock->disconnection.status;
}


/******************************************************/
/******************* gbs messages *********************/
/******************************************************/

static void socketMessageHandler(int sid, CSM_RAM *ram, GBS_MSG *msg)
{
    CoreSocket *sock = getSocketData(sid);
    if(!sock || sock->id < 0)
        return;

    switch((int)msg->data0)
    {
        case ENIP_SOCK_CONNECTED:
            printf("ENIP_SOCK_CONNECTED\n");
            if(sock->connect_state == 1) {
                sock->connection.status = 0;
                wakeAllWaitConds(sock->connection.wid);
            }
            break;


        case ENIP_SOCK_DATA_READ:
            printf("ENIP_SOCK_DATA_READ\n");
            if(sock->connect_state == 1) {
                sock->io.status = 0;
                sock->connect_state = 2;
                wakeAllWaitConds(sock->io.wid);
                return;
            } else if(sock->connect_state != 2) {
                sock->io.status = ER_CS_STATE;
                sock->io.stop_read = 1;
                wakeAllWaitConds(sock->io.wid);
                return;
            }

            if(sock->io.waiting_for_ready_read) {
                wakeAllWaitConds(sock->io.wid);
                return;
            }

            if(sock->connect_state == 2 && !sock->io.stop_read)
                SUBPROC(socket_read_impl, sock);
            break;


        case ENIP_SOCK_REMOTE_CLOSED:
            printf("ENIP_SOCK_REMOTE_CLOSED\n");
            sock->connection.status = ER_CS_CLOSED;

            switch(sock->connect_state)
            {
                case 1:
                    wakeAllWaitConds(sock->connection.wid);
                    break;
                case 2:
                    wakeAllWaitConds(sock->io.wid);
                case 3:
                    wakeAllWaitConds(sock->disconnection.wid);
            }

            SUBPROC(socket_close_impl, sock);
            break;


        case ENIP_SOCK_CLOSED:
            printf("ENIP_SOCK_CLOSED\n");
            sock->connection.status = ER_CS_CLOSED;

            switch(sock->connect_state)
            {
                case 1:
                    wakeAllWaitConds(sock->connection.wid);
                    break;
                case 2:
                    wakeAllWaitConds(sock->io.wid);
                case 3:
                    wakeAllWaitConds(sock->disconnection.wid);
            }

            SUBPROC(socket_close_impl, sock);
            break;
    }
}








