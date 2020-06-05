#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ylog.h>
#include <ytimer.h>

typedef struct _ytimer_cb
{
    bool timer_periodic;
    unsigned int timer_id;
    ytimer_func timer_func;
    struct timespec starttime;
    // duration_ms is msec
    unsigned int duration_ms; /* seconds */
    int user_num;
    void *user[];
} ytimer_cb;

static ytimer_cb *new_timer_cb(int user_num)
{
    ytimer_cb *timer_cb;
    timer_cb = malloc(sizeof(ytimer_cb) + sizeof(void *) * user_num);
    if (timer_cb == NULL)
    {
        return NULL;
    }
    memset(timer_cb, 0x0, sizeof(ytimer_cb) + sizeof(void *) * user_num);
    return timer_cb;
} /* new_timer_cb */

static void free_timer_cb(ytimer_cb *timer_cb)
{
    if (timer_cb)
        free(timer_cb);
} /* free_timer_cb */

static ytimer_status run_timer_cb(ytimer *timer, ytimer_status cur_status, ytimer_cb *timer_cb)
{
    ytimer_status status = YTIMER_NO_ERR;
    timer->cur_id = timer_cb->timer_id;
    switch (timer_cb->user_num)
    {
    case 0:
        status = timer_cb->timer_func.f0(timer, timer_cb->timer_id, cur_status);
        break;
    case 1:
        status = timer_cb->timer_func.f1(timer, timer_cb->timer_id, cur_status, timer_cb->user[0]);
        break;
    case 2:
        status = timer_cb->timer_func.f2(timer, timer_cb->timer_id, cur_status, timer_cb->user[0], timer_cb->user[1]);
        break;
    case 3:
        status = timer_cb->timer_func.f3(timer, timer_cb->timer_id, cur_status, timer_cb->user[0], timer_cb->user[1], timer_cb->user[2]);
        break;
    case 4:
        status = timer_cb->timer_func.f4(timer, timer_cb->timer_id, cur_status, timer_cb->user[0], timer_cb->user[1], timer_cb->user[2], timer_cb->user[3]);
        break;
    case 5:
        status = timer_cb->timer_func.f5(timer, timer_cb->timer_id, cur_status, timer_cb->user[0], timer_cb->user[1], timer_cb->user[2], timer_cb->user[3], timer_cb->user[4]);
        break;
    default:
        ylog_error("ytimer[fd=%d]: timer_func[%d] invalid user_num (%d)\n", timer->timerfd, timer_cb->timer_id, timer_cb->user_num);
        status = YTIMER_ABORTED;
        break;
    }
    timer->cur_id = 0;
    return status;
}

// get timer remained...
static double get_timer_remained(ytimer_cb *timer_cb, struct timespec *cur)
{
    double elapsed_time = 0.0;
    double offset = timer_cb->duration_ms * 0.001;
    struct timespec *start = &timer_cb->starttime;
    elapsed_time = (cur->tv_sec - start->tv_sec) * 1.0;
    elapsed_time = elapsed_time + (cur->tv_nsec - start->tv_nsec) / 1e9;
    return offset - elapsed_time;
}

static int timer_cb_cmp(ytimer_cb *tcb1, ytimer_cb *tcb2)
{
    long t1, t2, n1, n2;
    n1 = tcb1->starttime.tv_nsec + ((tcb1->duration_ms % 1000) * 1000000);
    n2 = tcb2->starttime.tv_nsec + ((tcb2->duration_ms % 1000) * 1000000);
    t1 = tcb1->starttime.tv_sec + (tcb1->duration_ms / 1000);
    t2 = tcb2->starttime.tv_sec + (tcb2->duration_ms / 1000);
    if (n1 >= 1000000000)
    {
        t1++;
        n1 = n1 - 1000000000;
    }
    if (n2 >= 1000000000)
    {
        t2++;
        n2 = n2 - 1000000000;
    }

    if (t1 < t2)
        return -1;
    else if (t1 > t2)
        return 1;
    if (n1 < n2)
        return -1;
    else if (n1 > n2)
        return 1;
    return tcb1->timer_id - tcb2->timer_id;
} /* timer_cb_cmp */

static int timer_id_cmp(ytimer_cb *tcb1, ytimer_cb *tcb2)
{
    return tcb2->timer_id - tcb1->timer_id;
} /* timer_id_cmp */

static int restart_timer(ytimer *timer)
{
    int ret;
    ytree_iter *i;
    ytimer_cb *timer_cb;
    struct itimerspec timespec;
    struct timespec cur;
    double remained;

    if (ytree_size(timer->timers) <= 0)
    {
        timespec.it_value.tv_sec = 0;
        timespec.it_value.tv_nsec = 0;
        timespec.it_interval.tv_sec = 0;
        timespec.it_interval.tv_nsec = 0;
        ret = timerfd_settime(timer->timerfd, 0x0, &timespec, NULL);
        if (ret < 0)
        {
            ylog_error("ytimer[fd=%d]: %s\n", timer->timerfd, strerror(errno));
            close(timer->timerfd);
            timer->timerfd = 0;
            return -1;
        }
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &cur);
    i = ytree_first(timer->timers);
    if (i)
    {
        timer_cb = ytree_data(i);
        remained = get_timer_remained(timer_cb, &cur);
        // ylog_debug("ytimer[fd=%d]: timer_func[%d] remained %f\n",
        //            timer->timerfd, timer_cb->timer_id, remained);
        timespec.it_interval.tv_sec = 0;
        timespec.it_interval.tv_nsec = 0;
        if (remained <= 0.0)
        {
            timespec.it_value.tv_sec = 0;
            timespec.it_value.tv_nsec = 1;
        }
        else
        {
            timespec.it_value.tv_sec = (time_t)remained;
            timespec.it_value.tv_nsec = ((long)(remained * 1e9)) % 1000000000;
        }
        ret = timerfd_settime(timer->timerfd, 0x0, &timespec, NULL);
        if (ret < 0)
        {
            ylog_error("ytimer[fd=%d]: %s\n", timer->timerfd, strerror(errno));
            close(timer->timerfd);
            timer->timerfd = 0;
            return -1;
        }
    }
    return 0;
}

void ytimer_destroy(ytimer *timer)
{
    if (timer)
    {
        ytree_iter *i;
        if (timer->timerfd > 0)
        {
            close(timer->timerfd);
            timer->timerfd = 0;
        }
        for (i = ytree_first(timer->timers); i; i = ytree_next(timer->timers, i))
        {
            ytimer_cb *timer_cb = ytree_data(i);
            run_timer_cb(timer, YTIMER_ABORTED, timer_cb);
        }
        ylist_destroy(timer->dtimers);
        ytree_destroy(timer->timer_ids);
        ytree_destroy_custom(timer->timers, (user_free)free_timer_cb);
        free(timer);
    }
} /* ytimer_destroy */

ytimer *ytimer_create(void)
{
    ytimer *timer = malloc(sizeof(ytimer));
    if (timer == NULL)
        return NULL;
    memset(timer, 0x0, sizeof(ytimer));
    timer->timers = ytree_create((ytree_cmp)timer_cb_cmp, NULL);
    timer->timer_ids = ytree_create((ytree_cmp)timer_id_cmp, NULL);
    timer->dtimers = ylist_create();
    timer->timerfd = timerfd_create(CLOCK_MONOTONIC, 0x0);
    if (timer->timers == NULL || timer->timer_ids == NULL ||
        timer->dtimers == NULL || timer->timerfd < 0)
    {
        ytimer_destroy(timer);
        return NULL;
    }
    return timer;
} /* ytimer_create */

static unsigned int _ytimer_set_msec(ytimer *timer, unsigned int msec, bool is_periodic, ytimer_func timer_fn, int user_num, void *user[])
{
    ytimer_cb *timer_cb;
    if (timer == NULL)
    {
        ylog_error("ytimer: no timer created\n");
        return 0;
    }
    if (timer->timerfd <= 0)
    {
        ylog_error("ytimer: no timerfd\n");
        return 0;
    }
    if (timer_fn.v0 == NULL)
    {
        ylog_error("ytimer[fd=%d]: no timer func\n", timer->timerfd);
        return 0;
    }
    if (user_num > 5 || user_num < 0)
    {
        ylog_error("ytimer[fd=%d]: user-defined arguments supported up to 5\n");
        return 0;
    }

    timer_cb = new_timer_cb(user_num);
    if (timer_cb == NULL)
    {
        ylog_error("ytimer[fd=%d]: timer_func alloc failed\n", timer->timerfd);
        return 0;
    }

    timer_cb->timer_id = ((++timer->next_id)%10000)+1;
    while (timer_cb->timer_id > 0 && ytree_search(timer->timer_ids, timer_cb) != NULL)
    {
        timer_cb->timer_id = ((++timer->next_id)%10000)+1;
    }

    timer_cb->user_num = user_num;
    timer_cb->timer_periodic = is_periodic;
    timer_cb->timer_func = timer_fn;
    timer_cb->duration_ms = msec;
    memcpy(timer_cb->user, user, sizeof(void *) * user_num);

    clock_gettime(CLOCK_MONOTONIC, &timer_cb->starttime);

    ytree_insert(timer->timer_ids, timer_cb, timer_cb);
    ytree_insert(timer->timers, timer_cb, timer_cb);
    ylog_debug("ytimer[fd=%d]: timer_func[%d] started in %u msec timers=%d timer_ids=%d\n", timer->timerfd,
               timer_cb->timer_id, msec, ytree_size(timer->timers), ytree_size(timer->timer_ids));
    if (restart_timer(timer) < 0)
        return 0;
    return timer_cb->timer_id;
} /* ytimer_set_msec */

unsigned int ytimer_set_msec(ytimer *timer, unsigned int msec, bool is_periodic, ytimer_func timer_fn, int user_num, ...)
{
    int i;
    va_list ap;
    void *user[5] = {0};
    va_start(ap, user_num);
    ylog_debug("user total = %d\n", user_num);
    for (i = 0; i < user_num; i++)
    {
        void *p = va_arg(ap, void *);
        user[i] = p;
        ylog_debug("user[%d]=%p\n", i, user[i]);
    }
    va_end(ap);
    return _ytimer_set_msec(timer, msec, is_periodic, timer_fn, user_num, user);
} /* ytimer_set_msec */

unsigned int ytimer_set(ytimer *timer, unsigned int seconds, bool is_periodic, ytimer_func timer_fn, int user_num, ...)
{
    int i;
    va_list ap;
    void *user[5] = {0};
    va_start(ap, user_num);
    ylog_debug("user total = %d\n", user_num);
    for (i = 1; i < user_num; i++)
    {
        void *p = va_arg(ap, void *);
        user[i] = p;
        ylog_debug("user[%d]=%p\n", i, user[i]);
    }
    va_end(ap);
    return _ytimer_set_msec(timer, seconds * 1000, is_periodic, timer_fn, user_num, user);
} /* ytimer_set */

int ytimer_restart_msec(ytimer *timer, unsigned int timer_id, unsigned int msec)
{
    ytimer_cb temp_timer;
    ytimer_cb *timer_cb = NULL;
    if (timer == NULL)
    {
        ylog_error("ytimer: no timer created\n");
        return -1;
    }
    if (timer->timerfd <= 0)
    {
        ylog_error("ytimer: no timerfd\n");
        return -1;
    }

    temp_timer.timer_id = timer_id;
    timer_cb = ytree_search(timer->timer_ids, &temp_timer);
    if (timer_cb == NULL)
    {
        ylog_error("ytimer: no timer func\n");
        return -1;
    }

    ytree_delete(timer->timers, timer_cb);
    ytree_delete(timer->timer_ids, timer_cb);
    timer_cb->duration_ms = msec;
    clock_gettime(CLOCK_MONOTONIC, &timer_cb->starttime);
    ytree_insert(timer->timer_ids, timer_cb, timer_cb);
    ytree_insert(timer->timers, timer_cb, timer_cb);
    ylog_debug("ytimer[fd=%d]: timer_func[%d] restarted in %u nsec\n", timer->timerfd, timer_id, msec);
    return restart_timer(timer);
} /* ytimer_restart_msec */

int ytimer_restart(ytimer *timer, unsigned int timer_id, unsigned int seconds)
{
    return ytimer_restart_msec(timer, timer_id, seconds * 1000);
} /* ytimer_restart */

int ytimer_delete(ytimer *timer, unsigned int timer_id)
{
    ytimer_cb temp_timer;
    ytimer_cb *timer_cb = NULL;
    if (timer == NULL)
    {
        ylog_error("ytimer: no timer created\n");
        return -1;
    }
    if (timer->timerfd <= 0)
    {
        ylog_error("ytimer: no timerfd\n");
        return -1;
    }
    if (timer->cur_id == timer_id)
    {
        ylog_error("ytimer[fd=%d]: timer_func[%d] is running func.\n", timer->timerfd, timer_cb->timer_id);
        return -1;
    }
    temp_timer.timer_id = timer_id;
    timer_cb = ytree_search(timer->timer_ids, &temp_timer);
    if (timer_cb)
    {
        ytree_delete(timer->timer_ids, timer_cb);
        ytree_delete(timer->timers, timer_cb);
        ylog_debug("ytimer[fd=%d]: timer_func[%d] deleted\n", timer->timerfd, timer_cb->timer_id);
        free_timer_cb(timer_cb);
    }
    else
    {
        ylog_error("ytimer: no timer func\n");
        return -1;
    }
    return restart_timer(timer);
} /* ytimer_delete */

int ytimer_serve(ytimer *timer)
{
    ytree_iter *i;
    ytimer_cb *timer_cb;
    uint64_t num_of_expires = 0;
    struct timespec cur;
    unsigned long timer_id;
    if (timer == NULL)
    {
        ylog_error("ytimer: no timer created\n");
        return -1;
    }

    ssize_t len = read(timer->timerfd, &num_of_expires, sizeof(uint64_t));
    if (len < 0)
    {
        ylog_error("ytimer[fd=%d]: %s\n", timer->timerfd, strerror(errno));
        return -1;
    }
    // log_debug2("\nagt_timer: expired (%d)", num_of_expires);

    clock_gettime(CLOCK_MONOTONIC, &cur);
    for (i = ytree_first(timer->timers); i; i = ytree_next(timer->timers, i))
    {
        double remained;
        timer_cb = ytree_data(i);
        remained = get_timer_remained(timer_cb, &cur);
        // ylog_debug("ytimer[fd=%d]: timer_func[%d] remained %f\n",
        //            timer->timerfd, timer_cb->timer_id, remained);
        if (remained <= 0.0)
        {
            timer_id = timer_cb->timer_id;
            ylist_push_back(timer->dtimers, (void *) timer_id);
        }
        else
            break;
    }

    timer_id = (unsigned long) ylist_pop_front(timer->dtimers);
    while (timer_id > 0)
    {
        ytimer_status status;
        ytimer_cb temp_timer;
        temp_timer.timer_id = (unsigned int) timer_id;
        timer_cb = ytree_search(timer->timer_ids, &temp_timer);
        if (timer_cb == NULL)
            continue;
        status = run_timer_cb(timer, YTIMER_EXPIRED, timer_cb);
        if (!timer_cb->timer_periodic)
        {
            ylog_debug("ytimer[fd=%d]: timer_func[%d] expired (non-periodic)\n",
                           timer->timerfd, timer_cb->timer_id);
            ytree_delete(timer->timer_ids, timer_cb);
            ytree_delete_custom(timer->timers, timer_cb, (user_free)free_timer_cb);
        }
        else if (status != YTIMER_NO_ERR)
        {
            if (status == YTIMER_ABORTED)
                ylog_error("ytimer[fd=%d]: timer_func[%d] stopped (aborted)\n",
                            timer->timerfd, timer_cb->timer_id);
            else if (status == YTIMER_COMPLETED)
                ylog_debug("ytimer[fd=%d]: timer_func[%d] stopped (completed)\n",
                           timer->timerfd, timer_cb->timer_id);
            ytree_delete(timer->timer_ids, timer_cb);
            ytree_delete_custom(timer->timers, timer_cb, (user_free)free_timer_cb);
        }
        else
        {
            ylog_debug("ytimer[fd=%d]: timer_func[%d] restarted in %d msec\n",
                       timer->timerfd, timer_cb->timer_id, timer_cb->duration_ms);
            ytree_delete(timer->timer_ids, timer_cb);
            ytree_delete(timer->timers, timer_cb);
            timer_cb->starttime = cur;
            ytree_insert(timer->timers, timer_cb, timer_cb);
            ytree_insert(timer->timer_ids, timer_cb, timer_cb);
        }
        timer_id = (unsigned long) ylist_pop_front(timer->dtimers);
    }

    return restart_timer(timer);
} /* ytimer_serve */

int ytimer_fd(ytimer *timer)
{
    return timer->timerfd;
}