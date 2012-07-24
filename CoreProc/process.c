#include <swilib.h>
#include <nu_swilib.h>
#include <limits.h>
#include "coreevent.h"
#include "process.h"
#include "thread.h"
#include "mutex.h"
#include "waitcondition.h"
#include "gbs_tweak.h"


typedef struct
{
    void (*h)(void *, void *);
    void *d[2];
}TorsList;

static CoreProcess core_process[256];
static CoreMutex pb_mutex;


void processInit()
{
    createMutex(&pb_mutex);
    for(int i =0; i<256; ++i) {
        core_process[i].t.id = -1;
        core_process[i].t.task = 0;
    }

    extern int print_handle(int, char**);
    extern int __printh_pid;
    __printh_pid = createProcess("print", 120, print_handle, 0, 0, 1);
}


void processFini()
{
    extern void abort_printing();
    extern int __printh_pid;
    int p = __printh_pid;

    abort_printing();
    waitForProcessFinished(p, 0);
    destroyMutex(&pb_mutex);
}



CoreProcess *coreProcessData(short _pid) {
    if(_pid > 255 || _pid < 0)
        return 0;

    return &core_process[_pid];
}


static short emptyPid()
{
    for(short i =0; i<256; ++i)
    {
        if(core_process[i].t.id < 0)
            return i;
    }

    return -1;
}


CoreProcess *newCoreProcessData()
{
    lockMutex(&pb_mutex);

    short _pid;
    CoreProcess *proc = coreProcessData((_pid = emptyPid()));
    if(proc) {
        proc->t.id = _pid;
        unlockMutex(&pb_mutex);

        proc->t.events = 0;
        proc->t.event_stop = 0;
        proc->t.mem = 0;
        proc->t.stack = 0;
        proc->t.stack_size = 0;
        proc->t.task = 0;
        proc->t.type = 0;

        proc->kill_mode =  proc->terminated = 0;
        proc->kostil.kik = 0;
        proc->kostil.d = 0;
        proc->ppid = -1;
        proc->start_wait_cond = -1;
        proc->exit_wait_cond = -1;
    } else
        unlockMutex(&pb_mutex);

    return proc;
}


int freeCoreProcessData(int _pid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    lockMutex(&pb_mutex);
    proc->t.id = -1;
    proc->t.task = 0;
    unlockMutex(&pb_mutex);
    return 0;
}


int sendEvent(int pid, void *event, size_t size)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || !proc->t.events)
        return -1;

    return NU_Send_To_Queue(proc->t.events, event, size, NU_NO_SUSPEND);
}


int parentPid(int _pid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc)
        return 0;

    return proc->ppid;
}


int ppid()
{
    CoreProcess *proc = coreProcessData(pid());
    if(!proc)
        return 0;

    return proc->ppid;
}



int pidByTask(NU_TASK *task)
{
    if(!task)
        return -1;

    /* небольшой костылик для увеличения скорости определения pid,
     * в поле argc мы пишем не количество параметров, а id процесса.
     * Таким образом мы можем просто вытянуть из структуры argc
     * и проверить его на валидность, в случае фейла, ищем старым способом
    */
    short _pid = ((MYTASK*)task)->tc_argc;
    CoreProcess *proc = coreProcessData(_pid);
    if( proc && proc->t.id > -1 && proc->t.id == _pid && proc->t.task == task)
        return _pid;

    for(int i =0; i<256; ++i) {
        if(core_process[i].t.id > -1 && core_process[i].t.task == task)
            return i;
    }

    return -1;
}


int pid()
{
    NU_TASK *task = NU_Current_Task_Pointer();
    if(!task)
        return -1;

    int tid = tidByTask(task);
    if(tid > -1) {
        return ptid(tid);
    }

    return pidByTask(task);
}



typedef struct
{
    CoreEvent head;
    void *data;
}CoreEventProcess;


static void kill_process(int _pid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return;

    NU_Suspend_Task(proc->t.task);
    NU_Terminate_Task(proc->t.task);
    NU_Delete_Task(proc->t.task);

    free(proc->t.stack);
    free(proc->t.task);

    if(proc->kostil.kik)
        proc->kostil.kik(proc->kostil.d);

    printf("process %d destroyed\n", proc->t.id);
    freeCoreProcessData(_pid);
}


static void kill_impl(int _pid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0 || proc->terminated == 2)
        return;

    proc->terminated = 2;

    int wc = proc->exit_wait_cond;
    proc->exit_wait_cond = -1;
    wakeAllWaitConds(wc);
    destroyWaitCond(wc);


    NU_Delete_Queue(proc->t.events);
    free(proc->t.mem);
    proc->t.events = 0;
    proc->t.mem = 0;

    while(--proc->argc > -1)
        free(proc->argv[proc->argc]);
    free(proc->argv);

    struct CoreListInode *inode = proc->threads.list.first;
    while(inode)
    {
        int tid = (int)inode->self;
        destroyThread(tid);

        if(inode != proc->threads.list.first)
            inode = proc->threads.list.first;
        else
            inode = inode->next;
    }


    TorsList *val;
    CoreArray *array = &proc->dtors;
    int i = 0;
    corearray_foreach(TorsList *, val, array, i) {
        corearray_store_cell(array, i, NULL);

        if(val) {
            val->h(val->d[0], val->d[1]);
            free(val);
        }
    }

    corearray_release(&proc->ctors);
    corearray_release(&proc->dtors);
    corearray_release(&proc->idUsersData);
    detachResCtl(_pid, &proc->resData, proc->resAttached);

    if(proc->name)
        free(proc->name);

    SUBPROC(kill_process, _pid);

    while(1)
        NU_Sleep(30);
}


static void handle(int argc, void *argv)
{
    CoreProcess *proc;

    if(argv)
        proc = (CoreProcess *)argv;
    else
        proc = coreProcessData(pid());

    short mpid = proc->t.id;
    if(proc->kill_mode)
    {
        kill_impl(mpid);
        return;
    }

    char *mem = malloc( 128 * (28*4) );
    NU_QUEUE queue;
    /*int err = */NU_Create_Queue(&queue, "ololo", mem, 128 * 28, NU_VARIABLE_SIZE, 128, NU_FIFO);

    proc->t.event_stop = 0;
    proc->t.events = &queue;
    proc->t.mem = mem;

    corelist_init(&proc->threads.list);
    corearray_init(&proc->idUsersData, NULL);
    proc->resAttached = attachResCtl(mpid, &proc->resData);

    TorsList *val;
    CoreArray *array = &proc->ctors;
    int i = 0;
    corearray_foreach(TorsList *, val, array, i) {
        val->h(val->d[0], val->d[1]);
        free(val);
    }

    corearray_release(&proc->ctors);

    int swc = proc->start_wait_cond;
    proc->start_wait_cond = -1;

    wakeAllWaitConds(swc);
    destroyWaitCond(swc);

    proc->retcode = proc->main(proc->argc, proc->argv);

    kill_impl(mpid);
}


static void process_bump(CoreEventProcess *event) {
    coreProcessData(pid())->t.event_stop = 1;
}


void quit()
{
    CoreEventProcess event;
    event.head.id = SIGKILL;
    event.head.type = CORE_EVENT_PROCESS;
    event.head.dispatcher = (void(*)(void*))process_bump;
    sendEvent(pid(), &event, sizeof event);
}


void kill(int _pid, int code)
{
    if(_pid == pid()) { // self kill
        kill_impl(_pid);
    } else {

        CoreProcess *proc = coreProcessData(_pid);
        if(proc->t.id < 0 || proc->terminated)
            return;

        proc->kill_mode = 1;
        proc->terminated = 1;

        NU_Terminate_Task(proc->t.task);
        NU_Reset_Task(proc->t.task, _pid, proc);
        NU_Resume_Task(proc->t.task);
    }
}


void processEvents()
{
    CoreProcess *proc = coreProcessData(pid());
    NU_QUEUE *queue = proc->t.events;
    unsigned long actual_size;
    int event[128];
    int err = 0;

    while((err = NU_Receive_From_Queue(queue, (void*)event, 128, &actual_size, NU_SUSPEND)) == NU_SUCCESS)
    {
        if(actual_size > sizeof(event)) {
            // Warning!!!
        }

        CoreEvent *e = (CoreEvent*)event;
        e->dispatcher(e);

        if(proc->t.event_stop)
            break;
    }
}



int createProcess(const char *name, int prio, int (*_main)(int, char**), int argc, char **argv, int run)
{

    CoreProcess *proc = newCoreProcessData();
    if(!proc)
        return -1;

    short _pid = proc->t.id;
    NU_TASK *task = malloc(sizeof *task);
    void *stack = malloc(DEFAULT_STACK_SIZE);
    int stack_size = DEFAULT_STACK_SIZE;

    memset(task, 0, sizeof *task);
    proc->t.task = task;
    proc->t.stack = stack;
    proc->t.stack_size = stack_size;
    proc->argc = argc;
    proc->argv = argv;
    proc->t.type = 1;
    proc->ppid = pid();
    proc->name = strdup(name);
    proc->main = _main;
    proc->retcode = 0;


    if( NU_Create_Task(task, (char *)name, handle, _pid, proc, stack, stack_size, prio, 0, NU_PREEMPT, NU_NO_START) != NU_SUCCESS )
    {
        free(task);
        free(stack);
        if(proc->name)
            free(proc->name);

        while(--proc->argc > -1)
            free(proc->argv[proc->argc]);
        free(argv);

        freeCoreProcessData(_pid);
        return -1;
    }

    corearray_init(&proc->ctors, NULL);
    corearray_init(&proc->dtors, NULL);

    proc->start_wait_cond = createWaitCond("proc_wait");
    proc->exit_wait_cond = createWaitCond("proc_wait");

    if(run)
        NU_Resume_Task(task);

    return _pid;
}


int resetProcess(int pid, int argc, char **argv)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    proc->argc = 0;
    proc->argv = argv;

    NU_Suspend_Task(proc->t.task);
    NU_Terminate_Task(proc->t.task);
    NU_Reset_Task(proc->t.task, 0, 0);

    return 0;
}


int suspendProcess(int pid)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    return NU_Suspend_Task(proc->t.task);
}


int fullSuspendProcess(int pid)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    struct CoreListInode *inode = 0;
    corelist_foreach(inode, proc->threads.list.first) {
        int tid = (int)inode->self;
        suspendThread(tid);
    }

    return NU_Suspend_Task(proc->t.task);
}


int resumeProcess(int pid)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    return NU_Resume_Task(proc->t.task);
}


int fullresumeProcess(int pid)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    struct CoreListInode *inode = 0;
    corelist_foreach(inode, proc->threads.list.first) {
        int tid = (int)inode->self;
        resumeThread(tid);
    }

    return NU_Resume_Task(proc->t.task);
}


static int addProcessTors(CoreArray *arr, void (*h)(void *, void *), void *data1, void *data2)
{
    int too;
    TorsList *t = malloc(sizeof *t);

    t->h = h;
    t->d[0] = data1;
    t->d[1] = data2;

    if(( too = corearray_push_back(arr, t)) < 0)
    {
        free(t);
        return -2;
    }

    return too;
}


int eraseProcessCtor(int pid, int at)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    if(at < 0 || at >= (int)proc->ctors.size)
        return -1;

    TorsList *l = corearray_cell(&proc->ctors, at);
    corearray_store_cell(&proc->ctors, at, NULL);

    if(l)
        free(l);

    return 0;
}


int eraseProcessDtor(int pid, int at)
{
    CoreProcess *proc = coreProcessData(pid);
    if(!proc || proc->t.id < 0)
        return -1;

    if(at < 0 || at >= (int)proc->dtors.size)
        return -1;

    TorsList *l = corearray_cell(&proc->dtors, at);
    corearray_store_cell(&proc->dtors, at, NULL);

    if(l)
        free(l);

    return 0;
}

int addProcessCtors(int _pid, void (*h)(void *, void *), void *data1, void *data2)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    return addProcessTors(&proc->ctors, h, data1, data2);
}


int addProcessDtors(int _pid, void (*h)(void *, void *), void *data1, void *data2)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    return addProcessTors(&proc->dtors, h, data1, data2);
}


int setProcessDieAction(int _pid, void (*h)(void *), void *d)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    proc->kostil.d = d;
    proc->kostil.kik = h;

    return 0;
}


struct CoreListInode *addProcessThread(int _pid, int tid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return 0;

    return corelist_push_back(&proc->threads.list, (void *)tid);
}


int delProcessThread(int _pid, struct CoreListInode *inode)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    corelist_del_inode(&proc->threads.list, inode);
    return 0;
}



const char *pidName(int _pid)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return 0;

    return proc->name;
}


int setProcessUserdata(int _pid, void *d)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    proc->userdata = d;
    return 0;
}


int waitForProcessFinished(int _pid, int *retcode)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    waitCondition(proc->exit_wait_cond);

    if(*retcode)
        *retcode = proc->retcode;

    return 0;
}


int attachProcessIdUserData(int _pid, void *userData)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return -1;

    if(!userData)
        return -2;

    return corearray_push_back(&proc->idUsersData, userData);
}



ResCtlData *dataOfResCtl(int _pid, int id)
{
    CoreProcess *proc = coreProcessData(_pid);
    if(!proc || proc->t.id < 0)
        return 0;

    if(id < 0 || id >= proc->resAttached) {
        return 0;
    }

    return &proc->resData[id];
}














