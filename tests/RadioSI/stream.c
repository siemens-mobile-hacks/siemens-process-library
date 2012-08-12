
#include <stdio.h>
#include <swilib.h>
#include <unistd.h>
#include <fcntl.h>
#include <spl/memctl.h>
#include <spl/coreevent.h>
#include "stream.h"
#include <limits.h>
#include <ctype.h>
#include "socket.h"

void parse_info(const char *info);
void updateStreamTitle(const char *);


FILE *StreamFD = 0;
int local_stream_type = -1;
int icy_metaint = 0, block = 0;
extern char icy_track[256];


struct unget_data
{
    char *data;
    size_t size,
           flushed,
           seek;
};


#define stream_read(d, s) fread(d, 1, s, StreamFD)
#define stream_close() fclose(StreamFD)
/*
struct unget_data udata = {0, 0, 0, 0};

ssize_t stream_read(void *data, size_t size)
{
    int readed = 0;
    if(udata.flushed > 0 && udata.seek < udata.flushed) {
        if(udata.flushed-udata.seek > size) {
            memcpy(data, udata.data+udata.seek, size);
            udata.seek += size;
            if(udata.seek == udata.flushed) {
                udata.seek = 0;
                udata.flushed = 0;
            }
            return size;
        }

        memcpy(data, udata.data+udata.seek, udata.flushed-udata.seek);
        readed += udata.flushed-udata.seek;

        udata.flushed = 0;
        udata.seek = 0;

        int r = fread(data+readed, 1, size-readed, StreamFD);

        return r > 0? r+readed : readed;
    }

    return fread(data, 1, size-readed, StreamFD);
}


int stream_unget_data(const void *data, size_t size)
{
    if(udata.flushed > 0)
    {
        int seek = udata.flushed - udata.seek;
        memmove(udata.data, udata.data+udata.seek, seek);
        udata.seek = 0;


        if(udata.size < size) {
            udata.data = realloc(udata.data, size+56);
            udata.size = size+56;
        }

        memcpy(udata.data+seek, data, size);
        udata.flushed = seek+size;
        return size;
    }

    if(udata.size > 0) {
        if(udata.size < size) {
            udata.data = realloc(udata.data, size+56);
            udata.size = size+56;
        }

        memcpy(udata.data, data, size);
        udata.flushed = size;
        udata.seek = 0;
        return size;
    }

    udata.data = malloc(size+56);
    udata.size = size+56;
    udata.flushed = size;
    udata.seek = 0;
    memcpy(udata.data, data, size);
    return size;
}


int stream_close()
{
    if(udata.data)
        free(udata.data);

    memset(&udata, 0, sizeof udata);
}*/


int parse_url(const char *url, char **host, unsigned int *ip, char **request_location, unsigned int *port)
{
    const char *s = url;
    if(!memcmp(s, "http://", 7))
        s += 7;

    const char *host_begin = s;
    while(*s != ':' && *s != '/' && *s) ++s;
    if(*s != ':' && *s != '/')
        return -1;
    char *host_end = (char*)s;

    if( isdigit(*host_begin) ) {
        printf("Parseurl: ip detected\n");
        *host = 0;

        unsigned int _ip[4];
        sscanf(host_begin, "%d.%d.%d.%d", &_ip[0], &_ip[1], &_ip[2], &_ip[3]);
        *ip = htonl(IP_ADDR(_ip[0],_ip[1],_ip[2],_ip[3]));
        printf("ip: %d.%d.%d.%d\n", _ip[0],_ip[1],_ip[2],_ip[3]);

    } else {
        *ip = -1;
        *host = memoryAlloc(getpid(), s - host_begin + 1);
        memcpy(*host, host_begin, s - host_begin);
        (*host)[s - host_begin] = 0;
    }

    char *request_location_start = 0;
    if(*host_end == ':') {
        request_location_start = strchr(host_end+1, '/');
        printf("getting port...\n");
        if(!request_location_start)
            request_location_start = host_end+strlen(host_end);

        sscanf(host_end+1, "%d", port);
        printf("port: %d\n", *port);
    } else {
        request_location_start = (char*)host_end;
        *port = 80;
    }

    if(request_location_start) {
        *request_location = strdup(request_location_start);
    } else
        *request_location = "/";


    return 0;
}


static int open_witch_redirections(const char *location, char *data, int *sz)
{
    char *host, *req;
    unsigned int ip = -1;
    unsigned int port = -1;

again:

    printf("location: %s\n", location);
    if( parse_url(location, &host, &ip, &req, &port) ) {
        return -1;
    }

    printf("parse done\n");

    int sock;
    if(host) {
        printf("Host: %s\n", host);
        sock = socket_open_by_host(host, port);
        free(host);
    }
    else if((int)ip != -1) {
        printf("Ip: %d\n", ip);
        sock = socket_open_by_ip(ip, port);
    } else {
        ShowMSG(1, (int)"Unknow url");
        return -1;
    }


    if(sock < 0) {
        return -1;
    }



    printf("req: %s\n", req);
    char request[1024*2] = {0}, ansver[1024*4] = {0};
    int len = sprintf(request, "GET %s HTTP/1.0\r\n"
                     "Host: online-hitfm.tavrmedia.ua\r\n"
                     "User-Agent: Siemens HitFM player\r\n"
                     "Referer: http://online-hitfm.tavrmedia.ua\r\n"
                     "Icy-MetaData: 1\r\n"
                     "Connection: keep_alive\r\n\r\n", req);

    free(req);

    printf("Write\n");
    if( write(sock, request, len) < 1 ) {
        close(sock);
        return -1;
    }

    printf("Read\n");
    if( (len = read(sock, ansver, 512)) < 1 ) {
        printf("Fail\n");
        close(sock);
        return -1;
    }
    printf("Readed\n");


    if(!memcmp(ansver+9, "302 Moved Temporarily", 21) || !memcmp(ansver+9, "301 Moved Permanently", 21)) {

        printf(ansver);
        close(sock);
        sock = -1;

        printf(" ====> Moved Temporarily\n");
        char *loc = strstr(ansver, "Location: ");
        if(!loc || loc >= ansver+len) return -1;

        loc += 10;
        char *end = strstr(loc, "\r\n");
        if(!end)
            return -1;

        memcpy(request, loc, end-loc);
        request[end-loc] = 0;
        location = request;
        goto again;
    }

    if(!memcmp(ansver+9, "200 OK", 6)) {
        printf(" ====> 200 OK\n");
        memcpy(data, ansver, len);
        data[len] = 0;
        *sz = len;
        return sock;
    }

    close(sock);
    return -1;
}


static FILE *open_stream_socket(const char *location)
{
    char ansver[1024];
    int len = 0;
    int sock = open_witch_redirections(location, ansver, &len);

    if(sock < 0) {
        ShowMSG(1, (int)"socket failed");
        return 0;
    }

    printf("fdopen..\n");
    FILE *fp = fdopen(sock, "r");
    if(!fp) {
        ShowMSG(1, (int)"fdopen failed");
        close(sock);
        return 0;
    }


    if(!memcmp(ansver+9, "200 OK", 6)) {
        parse_info(ansver);
        //printf(ansver);

        const char *metaint = strstr(ansver, "icy-metaint:");

        if(metaint)
        {
            metaint += 12;
            sscanf(metaint, "%d", &icy_metaint);
            printf("icy_metaint: %d\n", icy_metaint);
        }

        const char *ansver_end = strstr(ansver, "\r\n\r\n");
        if(ansver_end) {
            ansver_end += 4;
            if(ansver_end < ansver+len){
                //fprintf(out_stream, "\nneed ungetc\n");
                char *s = ansver+len;

                /*for(int i=0; i<s-ansver_end; ++i)
                    fprintf(out_stream, "%X ", ((char*)ansver_end)[i]);*/

                //stream_unget_data(ansver_end, s-ansver_end);
                //fprintf(out_stream, "\n\n");
            }
        }


        return fp;
    }

    fclose(fp);
    return 0;
}



int open_stream(int type, const char *location)
{
    StreamFD = 0;
    local_stream_type = -1;
    icy_metaint = 0, block = 0;

    switch(type)
    {
        case FS_STREAM:
            StreamFD = fopen(location, "rb");
            if(!StreamFD)
                return -1;
            local_stream_type = FS_STREAM;
            return FS_STREAM;

        case INET_STREAM:
        {
            StreamFD = open_stream_socket(location);
            if(!StreamFD)
                return -1;

            local_stream_type = INET_STREAM;
            return INET_STREAM;
        }
    }

    return -1;
}


int read_stream(void *_data, size_t size)
{
    char *data = (char*)_data;
    switch(local_stream_type)
    {
        case FS_STREAM:
            return fread(data, 1, size, StreamFD);

        case INET_STREAM:
        {
            int readed = stream_read(data, size);
            if(readed < 0) {
                    printf("Socket problem\n");
                    return readed;
            }

            block += readed;
            int offset, len;
            char *chunk_at = 0;
            char *chunk = 0;
            int chunk_seek = 0;


            if(icy_metaint > 0 && block >= icy_metaint)
                goto metadata;

            while(readed < (int)size)
            {
                if(icy_metaint > 0 && block >= icy_metaint) {
                    metadata:

                    offset = readed - (block - icy_metaint);
                    len = ((unsigned char*)data)[offset] * 16;
                    chunk_at = (( char*)data)+offset+1;
                    chunk = 0;

                    printf("len: %d\n", len);
                    if(len > 0) {
                        printf("data: %s\n", chunk_at);
                        chunk = malloc(len+1);
                        if(!chunk) {
                            ShowMSG(1, (int)"Out of memory");
                            return -1;
                        }
                    }


                    // если мы не дочитали мета-данные
                    if(len > readed - (offset +1)) {

                        // расчитываем сколько нужно дочитать
                        int need_read = len - (readed - (offset +1));

                        // если у нас уже есть прочитанный кусок мета-данных
                        chunk_seek = readed - (offset+1);
                        if(chunk_seek > 0) {
                            // то копируем его в мета-буффер
                            memcpy(chunk, data+offset+1, chunk_seek);
                        }
                        else
                            chunk_seek = 0;

                        int rd = 0;
                        while(need_read > rd) {
                            int r = 0;
                            //if(rd+chunk_seek >= (int)len)
                            //    r = stream_read(chunk+chunk_seek+rd, need_read-rd);
                            //else
                                r = stream_read(chunk+chunk_seek+rd, need_read-rd);

                            if(r < 0) {
                                free(chunk);
                                return -1;
                            }
                            rd += r;
                        }
                        chunk[chunk_seek+rd] = 0;

                        if(need_read < rd)
                            ShowMSG(1, (int)"Хапанули лишнего...");

                        printf(" - chunk size: %d\n", chunk_seek+rd);
                        chunk_at = chunk;
                        if(!memcmp(chunk_at, "StreamTitle='", 13)) {
                                chunk_at += 13;
                                char *end = strstr(chunk_at, "';");
                                memmove(chunk, chunk_at, end-chunk_at);
                                chunk[end-chunk_at] = 0;
                                updateStreamTitle(chunk);
                        }

                        readed = offset;
                        block = 0;
                    } else {
                        printf("Read complete, copy data...\n");

                        if(len > 0) {
                            if(!memcmp(chunk_at, "StreamTitle='", 13)) {
                                chunk_at += 13;
                                char *end = strstr(chunk_at, "';");
                                memcpy(chunk, chunk_at, end-chunk_at);
                                chunk[end-chunk_at] = 0;
                                updateStreamTitle(chunk);
                            }
                        }

                        // мы дочитали данные, скопируем новые данные на место мета-данных
                        int need_copy = readed - ((offset +1) + len);
                        memmove(data+offset, data+offset+1+len, need_copy);
                        readed = offset + need_copy;
                        block = need_copy;
                    }

                    free(chunk);
                }

                if(readed >= (int)size)
                    break;

                int r = stream_read(data+readed, size-readed);
                if(r < 0) {
                    printf("Socket problem\n");
                    return readed;
                }


                block += r;
                readed += r;
            }

            return readed;
        }
    }

    return -1;
}


int seek_stream(uint32_t to, int type)
{
    switch(local_stream_type)
    {
        case FS_STREAM:
            return fseek(StreamFD, to, type);

        case INET_STREAM:
            return -1;
    }

    return -1;
}


void close_stream()
{
    switch(local_stream_type)
    {
        case FS_STREAM:
            fclose(StreamFD);
            return;

        case INET_STREAM:
            stream_close();
            return;
    }

    StreamFD = 0;
    local_stream_type = -1;
    icy_metaint = 0, block = 0;
}


int size_stream()
{
    switch(local_stream_type)
    {
        case FS_STREAM:{
            off_t cur = lseek(fileno(StreamFD), 0, SEEK_CUR);
            int size =  lseek(fileno(StreamFD), 0, SEEK_END);
            lseek(fileno(StreamFD), cur, SEEK_SET);
            return size;
        }

        case INET_STREAM:
            return INT_MAX;
    }

    return -1;
}

