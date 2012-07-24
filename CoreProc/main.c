
#include <swilib.h>
#include <stdlib.h>
#include <stdarg.h>




void _init()
{
    void NU_ExceptionsInit();
    NU_ExceptionsInit();

    void bridgeInit();
    bridgeInit();

    void initWaitCond();
    initWaitCond();

    void resCtlInit();
    resCtlInit();

    void memCtlInit();
    memCtlInit();

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

    void memCtlFini();
    memCtlFini();

    void timersFini();
    timersFini();

    void bridgeFini();
    bridgeFini();

    void NU_ExceptionsDeInit();
    NU_ExceptionsDeInit();
}

