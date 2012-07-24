
#include <swilib.h>
#include <nu_swilib.h>
#include "process.h"
#include "waitcondition.h"


typedef struct
{
    NU_SEMAPHORE sema;
    int waiters;
    int pid, dt_id;
    char used;
}WaitCondition;

#define MAX_WAITCOND 512
static WaitCondition __wcond_data[MAX_WAITCOND];

void initWaitCond()
{
    for(int i=0; i<MAX_WAITCOND; ++i)
        __wcond_data[i].used = 0;
}



static int getBestWcID()
{
    for(int i=0; i<MAX_WAITCOND; ++i)
        if(__wcond_data[i].used == 0)
            return i;

    return -1;
}


static WaitCondition *getWcData(int wid)
{
    if(wid < 0 || wid >= MAX_WAITCOND)
        return 0;

    return &__wcond_data[wid];
}



int createWaitCond(const char *name)
{
    int id = getBestWcID();
    WaitCondition *wc = getWcData(id);
    if(!wc)
        return -1;

    wc->used = 1;
    wc->waiters = 0;
    wc->pid = pid();
    wc->dt_id = -1;

    int status = 0;
    if((status = NU_Create_Semaphore(&wc->sema, (CHAR*)name, 0, NU_PRIORITY)) != NU_SUCCESS) {
        wc->used = 0;
        return -1;
    } else {
        wc->dt_id = addProcessDtors(wc->pid, (void*)destroyWaitCond, (void *)id, 0);
    }

    return id;
}


int destroyWaitCond(int wid)
{
    WaitCondition *wc = getWcData(wid);
    if(!wc || !wc->used)
        return -1;

    eraseProcessDtor(wc->pid, wc->dt_id);

    int status = 0;
    status = NU_Delete_Semaphore(&wc->sema);
    wc->used = 0;
    return status;
}


int waitCondition(int wid)
{
    WaitCondition *wc = getWcData(wid);
    if(!wc || !wc->used)
        return -1;

    // если меньше 0, значит вейк вызвали перед ваитом - ждать незачем
    if(wc->waiters < 0) {
        wc->waiters++;
        return 0;
    }


    wc->waiters++;
    int status = NU_Obtain_Semaphore(&wc->sema, NU_SUSPEND);
    if(status != NU_SUCCESS)
        wc->waiters--;

    return status;
}


int wakeOneWaitCond(int wid)
{
    WaitCondition *wc = getWcData(wid);
    if(!wc || !wc->used)
        return -1;

    if(wc->waiters < 1) {
        wc->waiters--;
        return 0;
    }

    int status = NU_Release_Semaphore(&wc->sema);
    if(status == NU_SUCCESS)
        wc->waiters--;
    return status;
}


int wakeAllWaitConds(int wid)
{
    WaitCondition *wc = getWcData(wid);
    if(!wc || !wc->used)
        return -1;

    int status = 0;
    int cnt = wc->waiters;

    while(--cnt > -1) {

        int s = NU_Release_Semaphore(&wc->sema);
        status += s;
        if(s == NU_SUCCESS)
            wc->waiters--;
    }

    return status;
}


int resetWaitCondWaiters(int wid)
{
    WaitCondition *wc = getWcData(wid);
    if(!wc || !wc->used)
        return -1;

    wc->waiters = 0;
    return 0;
}



