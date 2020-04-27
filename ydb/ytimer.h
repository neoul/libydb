#ifndef __YTIMER__
#define __YTIMER__
#include <sys/timerfd.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <ylist.h>
#include <ytree.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YTIMER_ABORT,
    YTIMER_EXPIRED,
} ytimer_status;

typedef int (*ytimer_func)(unsigned int timer_id, ytimer_status status, void *cookie);

typedef struct _yimer
{
    ytree *timers;    // enabled timers
    ytree *timer_ids; // poll for timer_id
    ylist *dtimers;   // deleted timers
    int timerfd;
    unsigned int next_id;
} ytimer;


ytimer *ytimer_create(void);
void ytimer_destroy(ytimer *timer);
unsigned int ytimer_set_msec(ytimer *timer, unsigned int msec, bool periodic, ytimer_func timer_fn, void *cookie);
int ytimer_set(ytimer *timer, unsigned int seconds, bool periodic, ytimer_func timer_fn, void *cookie);
int ytimer_restart_msec(ytimer *timer, unsigned int timer_id, unsigned int msec);
int ytimer_restart(ytimer *timer, unsigned int timer_id, unsigned int seconds);
int ytimer_delete(ytimer *timer, unsigned int timer_id);
int ytimer_handle(ytimer *timer);
int ytimer_fd(ytimer *timer);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YTIMER__