
#include <swilib.h>
#include "resctl.h"
#include "corelist.h"
#include "process.h"
#include "ioresctl.h"
#include "io.h"



static int IOResCtlID = -1;

static void *onCreate(int _pid);
static void onClose(ResCtlData *data);


void createIOResCtl()
{
    IOResCtlID = createResCtl();
    setupResCtl(IOResCtlID, onCreate, onClose);
}


void closeIOResCtl()
{
    destroyResCtl(IOResCtlID);
}



static void *onCreate(int _pid)
{
    UNUSED(_pid);
    CoreList *list = malloc(sizeof *list);
    corelist_init(list);
    return list;
}


static void onClose(ResCtlData *data)
{
    if(!data)
        return;

    CoreList *list = data->data;
    struct CoreListInode *inode;
    corelist_clean_foreach_begin(inode, list->first) {
        if((int)inode->self > -1) {
            printf("\033[1m\033[31mpid: %d - close leak fd: %X\033[0m\n", getpid(), inode->self);
            close((int)inode->self);
        }
        inode->self = (void*)-1;
    }
    corelist_clean_foreach_end(list)

    corelist_release(list);
    free(list);
}



struct CoreListInode *IOStreamCreate(int _pid, int fd)
{
    if(fd < 0)
        return 0;

    ResCtlData *res = dataOfResCtl(_pid, IOResCtlID);
    if(!res || !res->data)
        return 0;

    CoreList *list = res->data;

    struct CoreListInode *inode = corelist_push_back(list, (void*)fd);
    return inode;
}



int IOStreamClose(int _pid, struct CoreListInode *inode, int fd)
{
    if(fd < 0 || !inode)
        return -1;

    ResCtlData *res = dataOfResCtl(_pid, IOResCtlID);
    if(!res || !res->data)
        return -1;

    CoreList *list = res->data;
    corelist_del_inode(list, inode);
    return 0;
}





