
#include <swilib.h>
#include "gbs_tweak.h"
#include "thread.h"
#include "task.h"
#include "mutex.h"
#include "waitcondition.h"
#include "process.h"


typedef struct
{
    CoreTask t;
    int ppid;
    char *name;
    int (*handle)(void *);
    void *data;
    int retcode;
    int start_wait_cond,
        exit_wait_cond;
    struct CoreListInode *ppid_list_inode;

}CoreThread;


static CoreThread threads[128];
static CoreMutex tb_mutex;


void coreThreadsInit()
{
    createMutex(&tb_mutex);
    for(int i=0; i<128; ++i)
        threads[i].t.id = -1;
}


void coreThreadsFini()
{
    destroyMutex(&tb_mutex);
}


static short get_best_thread_id()
{
    for(int i=0; i<128; ++i)
        if(threads[i].t.id < 0)
            return i;
    return -1;
}

static CoreThread *getThreadData(short id)
{
    if(id < 0 || id > 128-1)
        return 0;

    return &threads[id];
}



CoreThread *newCoreThreadData()
{
    lockMutex(&tb_mutex);

    short _tid;
    CoreThread *thread = getThreadData((_tid = get_best_thread_id()));
    if(thread) {
        thread->t.id = _tid;
        unlockMutex(&tb_mutex);

        thread->t.events = 0;
        thread->t.event_stop = 0;
        thread->t.mem = 0;
        thread->t.stack = 0;
        thread->t.stack_size = 0;
        thread->t.task = 0;
        thread->t.type = 0;

        thread->exit_wait_cond = -1;
        thread->start_wait_cond = -1;
    } else
        unlockMutex(&tb_mutex);

    return thread;
}


int freeCoreThreadData(int _tid)
{
    CoreThread *thread = getThreadData(_tid);
    if(!thread || thread->t.id < 0)
        return -1;

    lockMutex(&tb_mutex);
    thread->t.id = -1;
    thread->t.task = 0;
    unlockMutex(&tb_mutex);
    return 0;
}



int tidByTask(NU_TASK *task)
{
    if(!task)
        return -1;

    short _tid = ((MYTASK*)task)->tc_argc;
    CoreThread *thread = getThreadData(_tid);
    if( thread && thread->t.id == _tid && thread->t.task == task)
        return _tid;

    for(int i =0; i<128; ++i) {
        if(threads[i].t.id > -1 && threads[i].t.task == task)
            //printf("tidByTask: can`t take fast tid\n");
            return i;
    }
    return -1;
}


int tid()
{
    NU_TASK *task = NU_Current_Task_Pointer();
    return tidByTask(task);
}


int ptid(int tid)
{
    CoreThread *thread = getThreadData(tid);
    if(!thread)
        return -1;

    return thread->ppid;
}


static void thread_handle(int argc, void *argv)
{
    CoreThread *thread;
    if(argv)
        thread = (CoreThread *)argv;
    thread = getThreadData(tid());

    CoreProcess *parent = coreProcessData(thread->ppid);
    thread->t.id = tid();

    if(parent->t.id < 0)
        thread->t.events = 0;
    else
        thread->t.events = parent->t.events;


    int swc = thread->start_wait_cond;
    thread->start_wait_cond = -1;

    wakeAllWaitConds(swc);
    destroyWaitCond(swc);

    thread->handle(thread->data);


    int ewc = thread->exit_wait_cond;
    thread->exit_wait_cond = -1;

    wakeAllWaitConds(ewc);
    destroyWaitCond(ewc);
}


int createThread(int prio, int (*handle)(void *), void *data, int run)
{
    short id;
    CoreThread *thread = newCoreThreadData();
    if(!thread)
        return -1;

    id = thread->t.id;

    NU_TASK *task = malloc(sizeof(*task));
    void *stack = malloc(DEFAULT_STACK_SIZE);
    int stack_size = DEFAULT_STACK_SIZE;

    memset(task, 0, sizeof *task);
    thread->t.task = task;
    thread->t.stack = stack;
    thread->t.stack_size = stack_size;
    thread->ppid = pid();
    thread->t.type = 2;
    thread->handle = handle;
    thread->data = data;
    thread->retcode = 0;
    thread->ppid_list_inode = addProcessThread(thread->ppid, id);

    thread->name = malloc(15);
    sprintf(thread->name, "thread_%d", id);

    if( NU_Create_Task(thread->t.task, thread->name,
                       thread_handle, id, thread,
                       stack, stack_size, prio, 0,
                       NU_PREEMPT, NU_NO_START) != NU_SUCCESS )

    {
        free(task);
        free(stack);

        freeCoreThreadData(thread->t.id);
        return -2;
    }

    thread->start_wait_cond = createWaitCond("thread_wait");
    thread->exit_wait_cond = createWaitCond("thread_wait");

    if(run)
        NU_Resume_Task(task);

    return id;
}


int destroyThread(int tid)
{
    CoreThread *thread = getThreadData(tid);
    if(!thread || thread->t.id < 0)
        return -1;

    NU_Suspend_Task(thread->t.task);
    NU_Terminate_Task(thread->t.task);
    NU_Delete_Task(thread->t.task);

    free(thread->t.stack);
    free(thread->t.task);
    free(thread->name);
    delProcessThread(thread->ppid, thread->ppid_list_inode);

    freeCoreThreadData(thread->t.id);
    return 0;
}


int suspendThread(int tid)
{
    CoreThread *thread = getThreadData(tid);
    if(!thread || thread->t.id < 0)
        return -1;

    return NU_Suspend_Task(thread->t.task);
}


int resumeThread(int tid)
{
    CoreThread *thread = getThreadData(tid);
    if(!thread || thread->t.id < 0)
        return -1;

    return NU_Resume_Task(thread->t.task);
}


int waitForThreadStarted(int _tid)
{
    CoreThread *thread = getThreadData(_tid);
    if(!thread || thread->t.id < 0)
        return -1;

    waitCondition(thread->start_wait_cond);
    return 0;
}



int waitForThreadFinished(int _tid, int *retcode)
{
    CoreThread *thread = getThreadData(_tid);
    if(!thread || thread->t.id < 0)
        return -1;

    waitCondition(thread->exit_wait_cond);

    if(*retcode)
        *retcode = thread->retcode;

    return 0;
}

