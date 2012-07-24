
#include <swilib.h>
#include <usart.h>
#include "mutex.h"




char _debug_data[4*1024];
static CoreMutex mutex = {.locks = 0};
int __printh_pid = -1;
static NU_QUEUE queue;


void initUsart()
{
    /* слоупочный способ проверки наличия шнура,
     * правда оно возвращает true даже если подключить зарядку
     * от этого происходит пикнах.
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
}


void asyncPrintFini()
{
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

    NU_Send_To_Queue(&queue, (void*)str, sz/4+1, NU_SUSPEND);
}


void abort_printing()
{
    __printh_pid = -1;
    NU_Send_To_Queue(&queue, (void*)"abrt", 1, NU_SUSPEND);
}


int print_handle()
{
    void *mem = malloc(10* 1024*4);

    if(NU_Create_Queue(&queue, "print", mem, 10*1024, NU_VARIABLE_SIZE, 1024*2, NU_FIFO) != NU_SUCCESS) {
        return -1;
    }

    unsigned long asize;
    char data[4*1024 * 2];
    while(NU_Receive_From_Queue(&queue, data, 1024*2, &asize, NU_SUSPEND) == NU_SUCCESS)
    {
        if(__printh_pid < 0)
            break;

        uart_poll_tx_string(0, data);
    }

    NU_Delete_Queue(&queue);
    free(mem);

    return 0;
}

