
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
    cnts++;
    printf("pid: '%d' - '%s()' calls per 1 second: %d\n", pid(), __FUNCTION__, cnts);

    if(cnts > 10) {
        timerStop(id);
        quit();
    }
}


int main()
{
    char d[128];
    sprintf(d, "%d", pid());
    ShowMSG(1, (int)d);
    initUsart();
    printf(" [+] main: pid: %d\n", pid());

    timerStart(216, timer);

    processEvents();
    return 0;
}





