
#include "mutex.h"




int createMutex(CoreMutex *mutex)
{
    mutex->locks = 0;
    return NU_Create_Semaphore((NU_SEMAPHORE *)mutex, "s", 0, NU_PRIORITY);
}


int destroyMutex(CoreMutex *mutex)
{
    return NU_Delete_Semaphore((NU_SEMAPHORE *)mutex);
}


int lockMutex(CoreMutex *mutex)
{
    int status;

    if(mutex->locks > 0) {
        mutex->locks++;
        status = NU_Obtain_Semaphore((NU_SEMAPHORE *)mutex, NU_SUSPEND);
        if(status != NU_SUCCESS)
            mutex->locks--;
        return status;
    } else {

        mutex->locks++;
        return 0;
    }
}


int tryLockMutex(CoreMutex *mutex)
{
    int status;

    if(mutex->locks < 1)
    {
        mutex->locks++;
        status = 0;
    } else {
        status = -1;
    }

    return status;
}


int unlockMutex(CoreMutex *mutex)
{
    int status = 0;

    if( mutex->locks > 1 )
        status = NU_Release_Semaphore((NU_SEMAPHORE *)mutex);
    else
        status = NU_SUCCESS;

    if(status == NU_SUCCESS)
        mutex->locks--;
    return status;
}









