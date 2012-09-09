
#ifndef __CXX_THREAD_H__
#define __CXX_THREAD_H__

#include <spl/thread.h>
#include <swilib.h>


class Thread
{
public:
    Thread(int stack_size = 0x4000, int prio = 95);
    ~Thread();

    int start();
    int terminate();


protected:
    virtual void run() {}

private:
    int thread_id;
    int stack_size;
    int prio;
    TaskConf conf;

    static int thread_handle(void *_Thread);
};





#endif
