
#include <stddef.h>
#include <stdio.h>
#include "process.h"
#include "ioresctl.h"
#include "io.h"



static idStream fds[256];


void fdsInit()
{
    for(int i=0; i<256; ++i)
        fds[i].id = -1;
}


idStream *getStreamData(int fd)
{
    if(fd < 0 || fd >= 256)
        return 0;

    return &fds[fd];
}


static void stub(){}
int open_fd()
{
    for(int i=0; i<256; ++i)
        if(fds[i].id == -1) {
            fds[i].id = i;

            fds[i].read = (ssize_t (*)(int, void*, size_t))stub;
            fds[i].write = (ssize_t (*)(int, const void*, size_t))stub;
            fds[i].close = (int (*)(int))stub;
            fds[i].flush = (int (*)(int))stub;
            fds[i].lseek = (off_t (*)(int, off_t, int))stub;
            fds[i].pid = getpid();
            fds[i].resctl_inode = IOStreamCreate(fds[i].pid, i);
            return i;
        }

    return -1;
}



int close_fd(int fd)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    printf("close_fd(%d)\n", fd);

    void *d = s->resctl_inode;
    s->resctl_inode = 0;
    IOStreamClose(s->pid, d, fd);
    int r = s->close? s->close(fd) : 0;
    s->id = -1;
    return r;
}




ssize_t read(int fd, void *buf, size_t len)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    return s->read(fd, buf, len);
}



ssize_t write(int fd, const void *buf, size_t len)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    return s->write(fd, buf, len);
}



int close(int fd)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    return close_fd(fd);
}


int flush(int fd)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    return s->flush(fd);
}


off_t lseek(int fd, off_t offset, int whence)
{
    idStream *s = getStreamData(fd);
    if(!s)
        return -1;

    return s->lseek(fd, offset, whence);
}






