
#include <swilib.h>
#include <stdlib.h>
#include <stdarg.h>




void _init()
{
    void da_handler_vector();
    //da_handler_vector();

    void NU_ExceptionsInit();
    NU_ExceptionsInit();

    void bridgeInit();
    bridgeInit();

    void initWaitCond();
    initWaitCond();

    void fdsInit();
    fdsInit();

    void resCtlInit();
    resCtlInit();

    void memCtlInit();
    memCtlInit();

    void createIOResCtl();
    createIOResCtl();

    void socketsInit();
    socketsInit();

    void queueInit();
    queueInit();

    void helperProcInit();
    helperProcInit();

    void processInit();
    processInit();

    void coreThreadsInit();
    coreThreadsInit();

    void asyncPrintInit();
    asyncPrintInit();

    void timersInit();
    timersInit();

    void csmInit();
    csmInit();

    void guiInit();
    guiInit();
}



void _fini()
{
    void asyncPrintFini();
    asyncPrintFini();

    void processFini();
    processFini();

    void coreThreadsFini();
    coreThreadsFini();

    void csmFini();
    csmFini();

    void socketsFini();
    socketsFini();

    void closeIOResCtl();
    closeIOResCtl();

    void memCtlFini();
    memCtlFini();

    void timersFini();
    timersFini();

    void bridgeFini();
    bridgeFini();

    void helperProcFini();
    helperProcFini();

    void NU_ExceptionsDeInit();
    NU_ExceptionsDeInit();
}

