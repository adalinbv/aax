
#include <stdio.h>
#include <unistd.h>

#include "base/timer.h"


typedef struct __timer_t {

    _aaxTimer *timer;
    float elapsed;

} _timer_t;

int main()
{
    _timer_t *handle = calloc(1, sizeof(_timer_t));
    handle->timer = _aaxTimerCreate();

    _aaxTimerStart(handle->timer);
    for(int i=0; i<3; ++i)
    {
        printf("sleep..\n");
        sleep(1);

        handle->elapsed = _aaxTimerElapsed(handle->timer);
        printf("elapsed: %f\n", handle->elapsed);
    }
    _aaxTimerStop(handle->timer);

    _aaxTimerDestroy(handle->timer);
    free(handle);
}
