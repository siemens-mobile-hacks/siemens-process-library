
#include <swilib.h>
#include <spl/process.h>
#include <spl/memctl.h>
#include <spl/resctl.h>
#include <spl/corelist.h>


/* TODO
 * Протестировать.
 */


static int memAllocID = -1;
int _memoryFree(int _pid, void *ptr, int nodel);

static void *onCreate(int _pid)
{
    UNUSED(_pid);

    CoreList *list = malloc(sizeof *list);
    corelist_init(list);
    //printf("mem: onCreate %d %X\n", _pid, list);
    return list;
}


static void onClose(ResCtlData *data)
{
    //printf("mem: onClose %X\n", data);
    if(!data)
        return;

    CoreList *list = data->data;
    int pid = getpid();
    struct CoreListInode *inode;
    corelist_clean_foreach_begin(inode, list->first) {
        if(inode->self) {
            printf("\033[1m\033[31mpid: %d - free leak ptr: %X\033[0m\n", pid, inode->self);
            free(inode->self);
        }
        inode->self = 0;
    }
    corelist_clean_foreach_end(list)

    if(list->first)
        printf("memctl: list not fully freed!\n");
    corelist_release(list);
    free(list);
}


void memCtlInit()
{
    memAllocID = createResCtl();
    setupResCtl(memAllocID, onCreate, onClose);
}

void memCtlFini()
{
    destroyResCtl(memAllocID);
}



void *memoryAlloc(int _pid, size_t size)
{
    if(size < 1)
        return 0;

    if(_pid < 0)
        return malloc(size);

    ResCtlData *res = dataOfResCtl(_pid, memAllocID);
    if(!res || !res->data)
        return 0;

    CoreList *list = res->data;

    char *ptr = malloc(size+sizeof(void*));
    struct CoreListInode *inode = corelist_push_back(list, ptr);
    *(uint32_t*)ptr = (uint32_t)inode;

    return ptr + sizeof(void*);
}


void *memoryRealloc(int _pid, void *_ptr, size_t size)
{
    if(size < 1)
        return 0;

    if(_pid < 0)
        return realloc(_ptr, size);

    if(!_ptr)
        return memoryAlloc(_pid, size);

    /*ResCtlData *res = dataOfResCtl(_pid, memAllocID);
    if(!res || !res->data)
        return 0;*/

    char *ptr = ((char*)_ptr) - sizeof(void*);
    struct CoreListInode *inode = (struct CoreListInode *) *(uint32_t*)ptr;

    ptr = realloc(ptr, size+sizeof(void*));

    inode->self = ptr;
    memcpy(ptr, inode, sizeof(void*));

    return ptr + sizeof(void*);
}



int memoryFree(int _pid, void *ptr)
{
    if(!ptr)
        return -1;

    if(_pid < 0) {
        free(ptr);
        return 0;
    }

    ResCtlData *res = dataOfResCtl(_pid, memAllocID);
    if(!res || !res->data)
        return -1;

    CoreList *list = res->data;

    ptr = ((char*)ptr) - sizeof(void*);
    struct CoreListInode *inode = (struct CoreListInode *) *(uint32_t*)ptr;

    free(ptr);
    corelist_del_inode(list, inode);
    return 0;
}











