
#include <swilib.h>
#include <usart.h>
#include <spl/process.h>
#include <spl/mutex.h>
#include <spl/queue.h>
#include <spl/memctl.h>


char _debug_data[4*1024];
static CoreMutex mutex = {.locks = 0};
int __printh_pid = -1;
static int queue;


void initUsart()
{
    /* слоупочный способ проверки наличия шнура,
     * правда оно возвращает true даже если подключить зарядку
     * и от этого происходит пикнах.
     *
     * TODO
     * Найти более продвинутый способ
     * определения шнура.
     */
    if(!GetPeripheryState (2, 4))
        return;

    lockMutex(&mutex);
    uart_set_speed(0, USART_115200);
    unlockMutex(&mutex);
}


void asyncPrintInit()
{
    createMutex(&mutex);

    extern int print_handle(int, char**);
    __printh_pid = createProcess("print", 120, print_handle, 0, 0, 1);
}


void asyncPrintFini()
{
    extern void abort_printing();
    int p = __printh_pid;

    abort_printing();
    waitForProcessFinished(p, 0);
    __printh_pid = -1;

    destroyMutex(&mutex);
}


void printLock()
{
   lockMutex(&mutex);
}


void printUnLock()
{
   unlockMutex(&mutex);
}


void print(int sz, const char *str)
{
    if(!GetPeripheryState (2, 4)) {
        return;
    }

    NU_Send_To_Queue(getQueueDataByID(queue), (void*)str, sz/4+1, NU_SUSPEND);
}


void abort_printing()
{
    __printh_pid = -1;
    NU_Send_To_Queue(getQueueDataByID(queue), (void*)"abrt", 1, NU_SUSPEND);
}


int print_handle(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    void *mem = memoryAlloc(getpid(), 10* 1024*4);

    if( (queue = createQueue("print", mem, 10*1024, NU_VARIABLE_SIZE, 1024*2, NU_FIFO)) < 0 ) {
        return -1;
    }

    unsigned long asize;
    char data[4*1024 * 2];
    while(NU_Receive_From_Queue(getQueueDataByID(queue), data, 1024*2, &asize, NU_SUSPEND) == NU_SUCCESS)
    {
        if(__printh_pid < 0)
            break;

        uart_poll_tx_string(0, data);
    }

    destroyQueue(queue);
    queue = -1;
    memoryFree(getpid(), mem);
    return 0;
}

