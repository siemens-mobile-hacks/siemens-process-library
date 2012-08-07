
#include <process.h>
#include <timer.h>
#include <coreevent.h>
#include <waitcondition.h>

/* NOTE
 * Таймер циклически дёргает кальбак, не одиночно.
 */

int cnts;

void timer(int id)
{
    SetVibration(40);
    NU_Sleep(30);
    SetVibration(0);
}


int main()
{
    //initUsart();
    //printf(" [+] main: pid: %d\n", getpid());

    kill(2, 0);
    timerStart(216*5, timer);

    processEvents();
    return 0;
}





