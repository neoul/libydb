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
    YTIMER_NO_ERR,
    YTIMER_EXPIRED = YTIMER_NO_ERR,
    YTIMER_COMPLETED,
    YTIMER_ABORTED,
} ytimer_status;


typedef struct _yimer
{
    ytree *timers;    // enabled timers
    ytree *timer_ids; // poll for timer_id
    ylist *dtimers;   // deleted timers
    int timerfd;
    unsigned int cur_id; // The running timer_func id
    unsigned int next_id;
} ytimer;

typedef ytimer_status (*ytimer_func0)(ytimer *timer, unsigned int timer_id, ytimer_status status);
typedef ytimer_status (*ytimer_func1)(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1);
typedef ytimer_status (*ytimer_func2)(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1, void *user2);
typedef ytimer_status (*ytimer_func3)(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1, void *user2, void *user3);
typedef ytimer_status (*ytimer_func4)(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1, void *user2, void *user3, void *user4);
typedef ytimer_status (*ytimer_func5)(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1, void *user2, void *user3, void *user4, void *user5);
typedef union {
    void *v0;
    ytimer_func0 f0;
    ytimer_func1 f1;
    ytimer_func2 f2;
    ytimer_func3 f3;
    ytimer_func4 f4;
    ytimer_func5 f5;
} ytimer_func;

ytimer *ytimer_create(void);
void ytimer_destroy(ytimer *timer);
unsigned int ytimer_set_msec(ytimer *timer, unsigned int msec, bool is_periodic, ytimer_func timer_fn, int user_num, ...);
unsigned int ytimer_set(ytimer *timer, unsigned int seconds, bool is_periodic, ytimer_func timer_fn, int user_num, ...);
int ytimer_restart_msec(ytimer *timer, unsigned int timer_id, unsigned int msec);
int ytimer_restart(ytimer *timer, unsigned int timer_id, unsigned int seconds);
int ytimer_delete(ytimer *timer, unsigned int timer_id);
int ytimer_serve(ytimer *timer);
int ytimer_fd(ytimer *timer);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YTIMER__