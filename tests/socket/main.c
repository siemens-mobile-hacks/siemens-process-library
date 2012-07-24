
#include <swilib.h>
#include <process.h>
#include <timer.h>
#include <csm.h>
#include <coreevent.h>
#include <waitcondition.h>
#include <processbridge.h>
#include "socket.h"

int wid;



int getHostByName(const char *host, int ****DNR_RES, int *DNR_ID)
{
    void *sp[4] = {0, (char*)host, DNR_RES, DNR_ID};

    void get_host(void *sp[3]) {
        sp[0] = (void*)async_gethostbyname(sp[1], sp[2], sp[3]);
        wakeOneWaitCond(wid);
    }

    SUBPROC(get_host, sp);
    waitCondition(wid);
    return (int)sp[0];
}





void onCreate(CSM_RAM *ram)
{
    int ***p_res = NULL;
    SOCK_ADDR sa;
    int err = 1;
    int dnr_id = -1;
    int attempts = 0;

    err = getHostByName("team-sc.ru", &p_res, &dnr_id);

    if (err)
    {
        if ((err==0xC9)||(err==0xD6))
        {
            if (dnr_id)
            {
                attempts ++;
                if(attempts > 4) {
                    printf("Error\n");
                    return;
                }

                printf("DNR not ready\n");
                return;
            }
        }
        else
        {
            printf("Error\n");
            return;
        }
    }

    printf("ip: %X\n", p_res[3][0][0]);
}


void onClose(CSM_RAM *ram)
{
    quit();
}


void onMessage(CSM_RAM *ram, GBS_MSG *msg)
{

}


int main()
{
    initUsart();
    printf(" [+] main: pid: %d\n", pid());
    wid = createWaitCond("sock");

    csmCreate("sock", CoreCSM_IDLE, onCreate, onClose, onMessage);
    processEvents();
    return 0;
}





