
#include <process.h>
#include <coreevent.h>
#include <csm.h>



void onCreateCSM(CSM_RAM *ram)
{
    printf("%s: pid: %d\n", __FUNCTION__, pid());
}


void onCloseCSM(CSM_RAM *ram)
{
    printf("%s: pid: %d\n", __FUNCTION__, pid());
    quit();
}


void onMessageCSM(CSM_RAM *ram, GBS_MSG *msg)
{
    printf("%s: pid: %d\n", __FUNCTION__, pid());

    printf("msg: %X\n"
           "pid_from: %X\n"
           "submess: %X\n", msg->msg, msg->pid_from, msg->submess);
}



int main()
{
    initUsart();
    printf(" [+] main: pid: %d\n", getpid());

    csmCreate("test", CoreCSM_IDLE, onCreateCSM, onCloseCSM, onMessageCSM);

    processEvents();
    return 0;
}





