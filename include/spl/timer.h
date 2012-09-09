

#ifndef __TIMER_H__
#define __TIMER_H__

#include "coreevent.h"

#ifdef __cplusplus
extern "C" {
#endif



int timerStart(unsigned long time, void (*callback)(int, void *), void *userdata);
int timerPause(int timer);
int timerResume(int timer);
int timerStop(int timer);


#ifdef __cplusplus
}
#endif

#endif
