#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <yaml.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>

// epoll & timerfd
#include <sys/epoll.h>
#include <sys/timerfd.h>

// true/false
#include <stdbool.h>

#include <netinet/in.h>
#include <arpa/inet.h>

// #define PTHREAD_LOCK
#ifdef PTHREAD_LOCK
#include <pthread.h>
#endif

#define WRITEV_SEND 1
#ifdef WRITEV_SEND
#include <sys/uio.h>
#endif

#include "ylog.h"
#include "ystr.h"
#include "ytree.h"
#include "ylist.h"
#include "yarray.h"
#include "ytrie.h"
#include "ytimer.h"

#include "ydb.h"
#include "utf8.h"
#include "ynode.h"
#include "base64.h"

int tx_fail_en;
int tx_fail_count;

extern ylog_func ylog_logger;

#define YDB_ASSERT(state, caused_res)                                        \
    do                                                                       \
    {                                                                        \
        if (state)                                                           \
        {                                                                    \
            ylog_logger(YLOG_ERROR, __func__, __LINE__, "ASSERT '%s': %s\n", \
                        #state, ydb_res_str(caused_res));                    \
            assert(!(state));                                                \
        }                                                                    \
    } while (0)

#define YDB_FAIL(state, caused_res)                                              \
    do                                                                           \
    {                                                                            \
        if (state)                                                               \
        {                                                                        \
            res = caused_res;                                                    \
            if (ylog_level >= (YLOG_ERROR))                                      \
            {                                                                    \
                ylog_logger(YLOG_ERROR, __func__, __LINE__,                      \
                            "ydb[%s] '%s': %s %s%s%s\n",                         \
                            datablock ? datablock->name : "...",                 \
                            #state, ydb_res_str(res),                            \
                            (res == YDB_E_SYSTEM_FAILED) ? ":" : "",             \
                            (res == YDB_E_SYSTEM_FAILED) ? strerror(errno) : "", \
                            (res == YDB_E_SYSTEM_FAILED) ? ":" : "");            \
            }                                                                    \
            goto failed;                                                         \
        }                                                                        \
    } while (0)

#define SET_FLAG(flag, v) ((flag) = ((flag) | (v)))
#define UNSET_FLAG(flag, v) ((flag) = ((flag) & (~v)))
#define IS_SET(flag, v) ((flag) & (v))

#define CLEAR_BUF(buf, buflen) \
    do                         \
    {                          \
        if (buf)               \
            free(buf);         \
        buf = NULL;            \
        buflen = 0;            \
    } while (0)

#define YDB_ERR_STRING(RES, RES_STR) \
    case (RES):                      \
        return RES_STR;

char *ydb_res_str(ydb_res res)
{
    switch (res)
    {
        YDB_ERR_STRING(YDB_OK, "ok")
        YDB_ERR_STRING(YDB_W_TIMEOUT, "warning - request timeout")
        YDB_ERR_STRING(YDB_W_MORE_RECV, "warning - need to receive more")
        YDB_ERR_STRING(YDB_W_DISCONN, "warning - disconnected")
        YDB_ERR_STRING(YDB_ERROR, "error")
        YDB_ERR_STRING(YDB_E_TIMER, "timer ctrl error")
        YDB_ERR_STRING(YDB_E_EVENT, "request event error")
        YDB_ERR_STRING(YDB_E_CTRL, "datablock ctrl error")
        YDB_ERR_STRING(YDB_E_SYSTEM_FAILED, "syscall error")
        YDB_ERR_STRING(YDB_E_STREAM_FAILED, "stream failed")
        YDB_ERR_STRING(YDB_E_PERSISTENCY_ERR, "persistency error")
        YDB_ERR_STRING(YDB_E_INVALID_ARGS, "invalid arguments")
        YDB_ERR_STRING(YDB_E_TYPE_ERR, "node type error")
        YDB_ERR_STRING(YDB_E_INVALID_PARENT, "invalid parent")
        YDB_ERR_STRING(YDB_E_NO_ENTRY, "no entry exists")
        YDB_ERR_STRING(YDB_E_MEM_ALLOC, "memory failed")
        YDB_ERR_STRING(YDB_E_FULL_BUF, "buffer full")
        YDB_ERR_STRING(YDB_E_INVALID_YAML_TOKEN, "invalid yaml")
        YDB_ERR_STRING(YDB_E_YAML_INIT_FAILED, "yaml library failed")
        YDB_ERR_STRING(YDB_E_YAML_PARSING_FAILED, "yaml parsing failed")
        YDB_ERR_STRING(YDB_E_MERGE_FAILED, "merge failed")
        YDB_ERR_STRING(YDB_E_DELETE_FAILED, "delete failed")
        YDB_ERR_STRING(YDB_E_INVALID_MSG, "invalid message format")
        YDB_ERR_STRING(YDB_E_ENTRY_EXISTS, "entry exists")
        YDB_ERR_STRING(YDB_E_NO_CONN, "no connection exists")
        YDB_ERR_STRING(YDB_E_CONN_FAILED, "communication failed")
        YDB_ERR_STRING(YDB_E_CONN_CLOSED, "communication closed")
        YDB_ERR_STRING(YDB_E_FUNC, "no callback function")
        YDB_ERR_STRING(YDB_E_HOOK_ADD, "hook add failed")
        YDB_ERR_STRING(YDB_E_UNKNOWN_TARGET, "unknown target node")
        YDB_ERR_STRING(YDB_E_DENIED_DELETE, "delete not allowed")
    default:
        return "unknown";
    }
};

typedef struct _yconn yconn;

#define YCONN_ROLE_PUBLISHER 0x0001
#define YCONN_WRITABLE 0x0002
#define YCONN_UNSUBSCRIBE 0x0004
#define YCONN_SYNC 0x0010
#define YCONN_UNREADABLE 0x0020
#define YCONN_MAJOR_CONN 0x0040
#define YCONN_FLAGS_MASK 0x00ff

#define YCONN_TYPE_UNIX 0x0100
#define YCONN_TYPE_INET 0x0200
#define YCONN_TYPE_FIFO 0x0400
#define YCONN_TYPE_FILE 0x0800
#define YCONN_TYPE_MASK 0xff00

#define STATUS_SERVER 0x010000
#define STATUS_CLIENT 0x020000
#define STATUS_COND_CLIENT 0x040000 // connected client
#define STATUS_DISCONNECT 0x080000
#define STATUS_WAITEVENT 0x100000
#define STATUS_MASK 0xff0000

#define SET_DISCONNECTED(conn) ((conn)->flags = (((conn)->flags & (~STATUS_MASK)) | STATUS_DISCONNECT))
#define CLEAR_DISCONNECTED(conn) ((conn)->flags = ((conn)->flags & (~STATUS_DISCONNECT)))
#define IS_DISCONNECTED(conn) ((conn)->flags & (STATUS_DISCONNECT))

#define SET_WAITEVENT(conn) ((conn)->flags = ((conn)->flags | STATUS_WAITEVENT))
#define IS_WAITEVENT(conn) IS_SET((conn)->flags, STATUS_WAITEVENT)

#define IS_SERVER(conn) IS_SET((conn)->flags, STATUS_SERVER)
#define IS_COND_CLIENT(conn) IS_SET((conn)->flags, STATUS_COND_CLIENT)
#define IS_MAJOR(conn) IS_SET((conn)->flags, YCONN_MAJOR_CONN)

typedef enum
{
    YOP_NONE,
    YOP_INIT,
    YOP_MERGE,
    YOP_DELETE,
    YOP_SYNC,
    YOP_MAX,
} yconn_op;

char *yconn_op_str[] = {
    "none",
    "init",
    "merge",
    "delete",
    "sync"};

typedef enum
{
    YMSG_NONE,
    YMSG_REQUEST,
    YMSG_RESPONSE,
    YMSG_RESP_FAILED,
    YMSG_RESP_CONTINUED,
    YMSG_PUBLISH,
    YMSG_WHISPER,
    YMSG_MAX,
} ymsg_type;

char *ymsg_str[] = {
    "none",
    "request",
    "resp(ok)",
    "resp(failed)",
    "resp(continued)",
    "pubish",
    "whisper",
};

#define YMSG_START_DELIMITER "\n---\n"
#define YMSG_END_DELIMITER "\n...\n"
#define YMSG_START_DELIMITER_LEN (sizeof(YMSG_START_DELIMITER) - 1)
#define YMSG_END_DELIMITER_LEN (sizeof(YMSG_END_DELIMITER) - 1)
#define YMSG_HEAD_DELIMITER "#_-_-_-_\n"
#define YMSG_HEAD_DELIMITER_LEN (sizeof(YMSG_HEAD_DELIMITER) - 1)
#define YMSG_WHISPER_DELIMITER "+whisper-target:"
#define YMSG_WHISPER_DELIMITER_LEN (sizeof(YMSG_WHISPER_DELIMITER) - 1)

typedef struct _eventid
{
    int fd;
    unsigned int seq;
} eventid;

typedef struct _waitevent
{
    ydb *datablock;
    eventid id;
    eventid pid; // parent event id
    int cevents; // child events number
    unsigned int timerid;
} waitevent;

typedef ydb_res (*yconn_func_send)(
    yconn *conn, yconn_op op, ymsg_type type, char *data, size_t datalen);
typedef ydb_res (*yconn_func_recv)(
    yconn *conn, yconn_op *op, ymsg_type *type,
    unsigned int *flags, char **data, size_t *datalen, int *next);
typedef int (*yconn_func_accept)(yconn *conn, yconn *client); // return fd;

typedef ydb_res (*yconn_func_init)(yconn *conn);
typedef void (*yconn_func_deinit)(yconn *conn);

struct _yconn
{
    ydb *datablock;
    const char *address;
    unsigned int flags;
    int fd;
    int timerfd;
    ylist_iter *iter;
    yconn_func_init func_init;
    yconn_func_recv func_recv;
    yconn_func_send func_send;
    yconn_func_accept func_accept;
    yconn_func_deinit func_deinit;
    void *head;
    unsigned int sendseq;
    unsigned int recvseq;
    int error_num; // errno for error reporting under the system call
    int send_timeout;
    int recv_timeout;
    const char *name; // The name of the peer
};

static bool ydb_conn_log;
void ydb_connection_log(int enable)
{
    if (enable)
        ydb_conn_log = true;
    else
        ydb_conn_log = false;
}
static char *yconn_flag_print(yconn *conn);
static void yconn_print(yconn *conn, const char *func, int line, char *state, bool simple);
#define YCONN_INFO(conn, state) \
    yconn_print(conn, __func__, __LINE__, state, false)
#define YCONN_SIMPLE_INFO(conn) \
    yconn_print(conn, __func__, __LINE__, NULL, true)

static unsigned int _yconn_flags(const char *address, char *flagstr);
static yconn *_yconn_new(const char *address, unsigned int flags, ydb *datablock);
static void _yconn_free(yconn *conn);
static void _yconn_free_with_deinit(yconn *conn);

void yconn_close(yconn *conn);
void yconn_deferred_close(yconn *conn);
ydb_res yconn_open(char *addr, char *flags, ydb *datablock);
ydb_res yconn_reopen_or_close(yconn *conn, ydb *datablock);
ydb_res yconn_accept(yconn *conn);
yconn *yconn_get(char *address, ydb *datablock);
ylist *yconn_getall(char *address, ydb *datablock);

static ydb_res yconn_detach_from_conn(yconn *conn);
static ydb_res yconn_attach_to_conn(yconn *conn);
static ydb_res yconn_detach_from_disconn(yconn *conn);
static ydb_res yconn_attach_to_disconn(yconn *conn);

eventid yconn_request(yconn *req_conn, yconn_op op, int timeout, char *buf, size_t buflen, eventid peid);
eventid yconn_init(yconn *req_conn);
eventid yconn_sync(yconn *req_conn, ydb *datablock, bool forced, char *buf, size_t buflen);
ydb_res yconn_response(yconn *req_conn, yconn_op op, unsigned int respseq, bool done, bool ok, char *buf, size_t buflen);
ydb_res yconn_publish(yconn *recv_conn, yconn *req_conn, ydb *datablock, yconn_op op, char *buf, size_t buflen);
ydb_res yconn_whisper(int origin, ydb *datablock, yconn_op op, char *buf, size_t buflen);
ydb_res yconn_merge(yconn *recv_conn, yconn *req_conn, bool not_publish, char *buf, size_t buflen);
ydb_res yconn_delete(yconn *recv_conn, yconn *req_conn, bool not_publish, char *buf, size_t buflen);

eventid yconn_recv(yconn *recv_conn, yconn_op *op, ymsg_type *type, int *next);
ydb_res yconn_serve_blocking(ydb *datablock, eventid eid, int timeout);

#define YCONN_FAILED(conn, res)                                                     \
    do                                                                              \
    {                                                                               \
        char *errstr = strerror(errno);                                             \
        (conn)->error_num = errno;                                                  \
        ylog(YLOG_ERROR, "%s (%d, %s): %s %s%s%s\n",                                \
             (conn)->address, (conn)->fd, yconn_flag_print(conn), ydb_res_str(res), \
             (res == YDB_E_SYSTEM_FAILED) ? "(" : "",                               \
             (res == YDB_E_SYSTEM_FAILED) ? errstr : "",                            \
             (res == YDB_E_SYSTEM_FAILED) ? ")" : "");                              \
    } while (0)

struct _ydb
{
    const char *name; // The name of ydb
    ynode *top;       // top level data node
    ytrie *updater;   // ydb updater (ydb read hook)
    ytree *conn;      // connected remote list
    ylist *disconn;   // disconnected remote list
    ytree *event;     // Event for response wait
    ytimer *timer;    // Timer for event
    ydb_onchange_hook onchange;
    void *onchange_user;
    int epollfd;      // EPOLL for YDB IPC
    int synccount;    // The number of connections (needs sync)
    int timeout;      // timeout for ydb_sync, ydb_path_sync
#ifdef PTHREAD_LOCK
    pthread_mutex_t lock;
    pthread_t lock_id;
    int lock_count;
#endif
};

static inline void lock(struct _ydb *datablock)
{
#ifdef PTHREAD_LOCK
    if (datablock)
    {
        if (datablock->lock_id != pthread_self())
            pthread_mutex_lock(&datablock->lock);
        datablock->lock_id = pthread_self();
        datablock->lock_count++;
    }
#endif
}

static inline void unlock(struct _ydb *datablock)
{
#ifdef PTHREAD_LOCK
    if (datablock)
    {
        if (datablock->lock_id == pthread_self())
        {
            datablock->lock_count--;
            if (datablock->lock_count <= 0)
            {
                datablock->lock_id = 0;
                datablock->lock_count = 0;
                pthread_mutex_unlock(&datablock->lock);
            }
        }
    }
#endif
}

void ydb_lock(struct _ydb *datablock)
{
    lock(datablock);
}

void ydb_unlock(struct _ydb *datablock)
{
    unlock(datablock);
}

static ytrie *ydb_pool;
static ytrie *ymsg_pool;
static ytrie *yop_pool;

int yconn_cmp(int *fd1, int *fd2)
{
    if (*fd1 < *fd2)
        return -1;
    else if (*fd1 > *fd2)
        return 1;
    else
        return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-conversion" // disable casting warning.
static ymsg_type ydb_get_ymsg(char *typestr)
{
    return (ymsg_type)ytrie_search(ymsg_pool, typestr, strlen(typestr));
}

static yconn_op ydb_get_yop(char *opstr)
{
    return (ymsg_type)ytrie_search(yop_pool, opstr, strlen(opstr));
}

static ydb_res ypool_create(void)
{
    if (!ydb_pool)
    {
        ydb_pool = ytrie_create();
        if (!ydb_pool)
        {
            return YDB_E_MEM_ALLOC;
        }
        if (!ymsg_pool)
        {
            int j;
            ymsg_pool = ytrie_create();
            if (!ymsg_pool)
            {
                return YDB_E_MEM_ALLOC;
            }
            for (j = YMSG_NONE; j < YMSG_MAX; j++)
                ytrie_insert(ymsg_pool, ymsg_str[j], strlen(ymsg_str[j]), j);
        }
        if (!yop_pool)
        {
            int j;
            yop_pool = ytrie_create();
            if (!yop_pool)
            {
                return YDB_E_MEM_ALLOC;
            }
            for (j = YOP_NONE; j < YOP_MAX; j++)
                ytrie_insert(yop_pool, yconn_op_str[j], strlen(yconn_op_str[j]), j);
        }
    }
    return YDB_OK;
}
#pragma GCC diagnostic pop

static void ypool_destroy(void)
{
    if (ydb_pool && ytrie_size(ydb_pool) <= 0)
    {
        if (ymsg_pool)
        {
            ytrie_destroy(ymsg_pool);
            ymsg_pool = NULL;
        }
        if (yop_pool)
        {
            ytrie_destroy(yop_pool);
            yop_pool = NULL;
        }
        ytrie_destroy(ydb_pool);
        ydb_pool = NULL;
    }
}

static ydb_res ydb_epoll_timer(ydb *datablock)
{
    int fd;
    struct epoll_event event;
    event.data.ptr = NULL;
    event.events = EPOLLIN;
    fd = ytimer_fd(datablock->timer);
    if (fd <= 0)
        return YDB_E_TIMER;
    if (epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, fd, &event))
        return YDB_E_SYSTEM_FAILED;
    return YDB_OK;
}

static ydb_res ydb_epoll_create(ydb *datablock)
{
    ydb_res res;
    if (datablock->epollfd < 0)
    {
        datablock->epollfd = epoll_create(YDB_CONN_MAX);
        if (datablock->epollfd < 0)
            return YDB_E_SYSTEM_FAILED;
        // attach timerfd
        res = ydb_epoll_timer(datablock);
        if (res)
            return res;
        ylog_info("ydb[%s] open epollfd(%d)\n", datablock->name, datablock->epollfd);
    }
    return YDB_OK;
}

static void ydb_epoll_destroy(ydb *datablock)
{
    if (datablock->epollfd > 0)
    {
        if (ylist_empty(datablock->disconn) &&
            ytree_size(datablock->conn) <= 0)
        {
            ylog_info("ydb[%s] close epollfd(%d)\n", datablock->name, datablock->epollfd);
            close(datablock->epollfd);
            datablock->epollfd = -1;
        }
    }
}

static ydb_res ydb_epoll_attach(ydb *datablock, yconn *conn, int fd)
{
    struct epoll_event event;
    if (!IS_SET(conn->flags, YCONN_UNREADABLE))
    {
        event.data.ptr = conn;
        event.events = EPOLLIN;
        if (epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, fd, &event))
        {
            YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
            return YDB_E_SYSTEM_FAILED;
        }
    }
    return YDB_OK;
}

static ydb_res ydb_epoll_detach(ydb *datablock, yconn *conn, int fd)
{
    struct epoll_event event;
    if (!IS_SET(conn->flags, YCONN_UNREADABLE))
    {
        event.data.ptr = conn;
        event.events = EPOLLIN;
        if (epoll_ctl(datablock->epollfd, EPOLL_CTL_DEL, fd, &event))
        {
            YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
            return YDB_E_SYSTEM_FAILED;
        }
    }
    return YDB_OK;
}

static void ydb_time_set_base(struct timespec *base)
{
    clock_gettime(CLOCK_MONOTONIC, base);
}

static int ydb_time_get_elapsed(struct timespec *base)
{
    int timeout;
    struct timespec cur;
    clock_gettime(CLOCK_MONOTONIC, &cur);

    timeout = (cur.tv_sec - base->tv_sec) * 1000;
    timeout = timeout + (cur.tv_nsec - base->tv_nsec) / 10e5;
    return timeout;
}

static void ydb_print(ydb *datablock, const char *func, int line, char *state)
{
    if (!datablock || !YLOG_SEVERITY_INFO || !ylog_logger)
        return;
    ylog_logger(YLOG_INFO, func, line, "%s ydb[%s]:\n", state ? state : "", datablock->name);
    ylog_logger(YLOG_INFO, func, line, " epollfd: %d, timeout: %d, event: %d\n",
                datablock->epollfd, datablock->timeout, ytree_size(datablock->event));
    ylog_logger(YLOG_INFO, func, line, " synccount: %d, w-hook: %d, conn: %d, disconn: %d\n",
                datablock->synccount, ytrie_size(datablock->updater),
                ytree_size(datablock->conn), ylist_size(datablock->disconn));
}
#define YDB_INFO(conn, state) ydb_print((conn), __func__, __LINE__, (state))

static unsigned int waiteventseq; // local event seq
static int waitevent_cmp(eventid *e1, eventid *e2)
{
    int e = e1->fd - e2->fd;
    if (e == 0)
    {
        return e1->seq - e2->seq;
    }
    return e;
}

void waitevent_free(waitevent *e)
{
    if (e)
        free(e);
}

bool invalid_waitevent(eventid eid)
{
    if (eid.fd < 0)
        return true;
    return false;
}

bool valid_waitevent(eventid eid)
{
    if (eid.fd < 0)
        return false;
    return true;
}

bool is_equal_waitevent(eventid eid1, eventid eid2)
{
    if (eid1.fd == eid2.fd)
        if (eid1.seq == eid2.seq)
            return true;
    return false;
}

waitevent *waitevent_search(ydb *datablock, eventid eid)
{
    return ytree_search(datablock->event, &eid);
}

eventid waitevent_reset(ydb *datablock, int fd, unsigned int seq, bool expired);
ytimer_status waitevent_expire(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user)
{
    waitevent *we = user;
    if (status == YTIMER_ABORTED)
        return YTIMER_NO_ERR;
    waitevent_reset(we->datablock, we->id.fd, we->id.seq, true);
    return YTIMER_NO_ERR;
}

eventid waitevent_set_event(ydb *datablock, int fd, unsigned int seq, int timeout, eventid pevent)
{
    waitevent *we, *pwe, *fwe;
    eventid eid = {.fd = -1, .seq = 0};
    we = malloc(sizeof(waitevent));
    if (!we)
        return eid;
    we->id.fd = eid.fd = fd;
    we->id.seq = eid.seq = seq;
    we->datablock = datablock;
    we->cevents = 0;
    we->timerid = 0;
    we->pid.fd = -1;
    we->pid.seq = 0;
    // fwe = old we
    fwe = (waitevent *)ytree_insert(datablock->event, &(we->id), we);
    if (fwe)
    {
        free(we);
        return eid;
    }
    if (valid_waitevent(pevent))
    {
        pwe = ytree_search(datablock->event, &pevent);
        if (pwe)
        {
            we->pid.fd = pevent.fd;
            we->pid.seq = pevent.seq;
            pwe->cevents++;
        }
    }
    we->timerid = ytimer_set_msec(datablock->timer, timeout, false, (ytimer_func)waitevent_expire, 1, we);
    ylog_info("set waitevent e(%d:%d) pe(%d:%d)\n", we->id.fd, we->id.seq, we->pid.fd, we->pid.seq);
    eid = we->id;
    return eid;
}

eventid waitevent_set_rootevent(ydb *datablock, int timeout)
{
    waitevent *we, *fwe;
    eventid eid = {.fd = -1, .seq = 0};
    we = malloc(sizeof(waitevent));
    if (!we)
        return eid;
    we->id.fd = eid.fd = 0;
    we->id.seq = eid.seq = (++waiteventseq) % 100000;
    we->datablock = datablock;
    we->cevents = 0;
    we->timerid = 0;
    we->pid.fd = -1;
    we->pid.seq = 0;
    // fwe = old we
    fwe = (waitevent *)ytree_insert(datablock->event, &(we->id), we);
    if (fwe)
    {
        free(we);
        return eid;
    }
    we->timerid = ytimer_set_msec(datablock->timer, timeout, false, (ytimer_func) waitevent_expire, 1, we);
    ylog_info("set waitevent e(%d:%d)\n", we->id.fd, we->id.seq);
    eid = we->id;
    return eid;
}

eventid waitevent_reset(ydb *datablock, int fd, unsigned int seq, bool expired)
{
    waitevent *we, *pwe;
    eventid emptyid = {.fd = -1, .seq = 0};
    eventid eid = {.fd = fd, .seq = seq};
    we = ytree_delete(datablock->event, &eid);
    if (we)
    {
        eventid peid = {.fd = we->pid.fd, .seq = we->pid.seq};
        if (!expired)
            ytimer_delete(datablock->timer, we->timerid);
        waitevent_free(we);
        ylog_info("%s waitevent e(%d:%d)\n", expired ? "expired" : "complete", eid.fd, eid.seq);
        if (!expired && valid_waitevent(peid))
        {
            pwe = ytree_search(datablock->event, &(peid));
            if (pwe)
            {
                pwe->cevents--;
                if (pwe->cevents <= 0)
                {
                    ytree_delete(datablock->event, &(peid));
                    if (!expired)
                        ytimer_delete(datablock->timer, pwe->timerid);
                    ylog_info("%s waitevent e(%d:%d)\n", expired ? "expired" : "complete", peid.fd, peid.seq);
                    waitevent_free(pwe);
                    return peid;
                }
            }
            else
            {
                return emptyid;
            }
        }
        return eid;
    }
    return emptyid;
}

eventid waitevent_complete(ydb *datablock, eventid eid)
{
    return waitevent_reset(datablock, eid.fd, eid.seq, false);
}

yconn *waitevent_get_conn(ydb *datablock, eventid eid)
{
    if (eid.fd < 0)
        return NULL;
    if (eid.fd == 0)
        return NULL;
    return ytree_search(datablock->conn, &(eid.fd));
}

eventid waitevent_get_relay(ydb *datablock, eventid eid, yconn **conn)
{
    eventid emptyid = {.fd=-1, .seq=0};
    waitevent *we;
    if (eid.fd < 0)
        return emptyid;
    if (eid.fd == 0)
        return emptyid;
    we = waitevent_search(datablock, eid);
    if (we == NULL)
        return emptyid;
    if (we->pid.fd <= 0)
        return emptyid;
    *conn = ytree_search(datablock->conn, &(we->pid.fd));
    return we->pid;
}

// str2yaml --
// Return new string converted to YAML character set.
char *str2yaml(char *cstr)
{
    int is_new = 0;
    if (!cstr)
        return strdup("");
    char *yamlstr = to_yaml(cstr, -1, &is_new, 0);
    if (is_new)
        return yamlstr;
    return strdup(cstr);
}

// str2yaml --
// Return new C string converted from YAML character set.
char *yaml2str(char *ystr, size_t len)
{
    return to_string(ystr, len, NULL);
}

// binary_to_base64 --
// Return base64 string with the length.
// It should be free
char *binary_to_base64(unsigned char *binary, size_t binarylen, size_t *base64len)
{
    if (binary && binarylen > 0)
        return (char *)base64_encode((const unsigned char *)binary, binarylen, base64len);
    return NULL;
}

// binary_to_base64_if --
// Return base64 string with LF (Line Feed).
// It should be free
char *binary_to_base64_lf(unsigned char *binary, size_t binarylen, size_t *base64len)
{
    if (binary && binarylen > 0)
        return (char *)base64_encode_lf((const unsigned char *)binary, binarylen, base64len);
    return NULL;
}

// base64_to_binary --
// Return base64 string with the length.
// It should be free
unsigned char *base64_to_binary(char *base64, size_t base64len, size_t *binarylen)
{
    if (base64 && binarylen)
    {
        if (base64len == 0)
            base64len = strlen(base64);
        return base64_decode((const unsigned char *)base64, base64len, binarylen);
    }
    return NULL;
}

// open local ydb (yaml data block)
ydb *ydb_open(char *name)
{
    ydb_res res = YDB_OK;
    ydb *datablock = NULL;
    int namelen;
    ylog_inout();
    res = (ydb_res)res;
    YDB_FAIL(!name, YDB_E_INVALID_ARGS);
    YDB_FAIL(ypool_create(), YDB_E_CTRL);
    namelen = strlen(name);
    datablock = ytrie_search(ydb_pool, name, namelen);
    if (datablock)
        return datablock;
    ylog_in();
    datablock = malloc(sizeof(ydb));
    YDB_FAIL(!datablock, YDB_E_MEM_ALLOC);
    memset(datablock, 0x0, sizeof(ydb));
    datablock->epollfd = -1;
    datablock->timeout = YDB_DEFAULT_TIMEOUT;

    datablock->name = ystrdup(name);
    YDB_FAIL(!datablock->name, YDB_E_CTRL);
    datablock->conn = ytree_create((ytree_cmp)yconn_cmp, NULL);
    YDB_FAIL(!datablock->conn, YDB_E_CTRL);
    datablock->top = ynode_create_path(name, NULL, NULL);
    YDB_FAIL(!datablock->top, YDB_E_CTRL);
    datablock->disconn = ylist_create();
    YDB_FAIL(!datablock->disconn, YDB_E_CTRL);
    datablock->updater = ytrie_create();
    YDB_FAIL(!datablock->updater, YDB_E_CTRL);
    datablock->event = ytree_create((ytree_cmp)waitevent_cmp, NULL);
    YDB_FAIL(!datablock->event, YDB_E_CTRL);
    datablock->timer = ytimer_create();
    YDB_FAIL(!datablock->timer, YDB_E_CTRL);

#ifdef PTHREAD_LOCK
    int ret = pthread_mutex_init(&datablock->lock, NULL);
    YDB_FAIL(ret, YDB_E_CTRL);
#endif
    ytrie_insert(ydb_pool, datablock->name, namelen, datablock);
    YDB_INFO(datablock, "opened");
    ylog_out();
    return datablock;
failed:
    ydb_close(datablock);
    ylog_out();
    return NULL;
}

// ydb_connect --
// Create or connect to YDB IPC (Inter Process Communication) channel
//  - address: YDB communication channel address.
//   - us://unix-socket-name (unix socket)
//   - uss://unix-socket-name (hidden unix socket; socket file doesn’t appear from filesystem.)
//   - tcp://ipaddr:port (tcp)
//   - fifo://named-fifo-input,named-fifo-output
//  - flags:
//    pub(publisher)/sub(subscriber): YDB role configuration
//    w(writable): connect to the channel to write data in subscriber role.
//    u(unsubscribe): disable the subscription of the data change
//    s(sync-before-read mode): request the update of the YDB instance before ydb_read()
// e.g. ydb_connect(db, "uss://netconf", "pub")
//      ydb_connect(db, "us:///tmp/ydb_channel", "sub")
ydb_res ydb_connect(ydb *datablock, char *addr, char *flags)
{
    ydb_res res = YDB_OK;
    unsigned int conn_flags = 0;
    yconn *conn = NULL;
    char _addr[256];
    ylog_in();
    YDB_FAIL(!datablock || !flags, YDB_E_INVALID_ARGS);
    lock(datablock);
    res = ydb_epoll_create(datablock);
    YDB_FAIL(res || datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);
    if (!addr)
    {
        snprintf(_addr, sizeof(_addr), "uss://%s", datablock->name);
        addr = _addr;
    }
    conn_flags = _yconn_flags(addr, flags);
    YDB_FAIL(!conn_flags, YDB_E_INVALID_ARGS);
    conn = yconn_get(addr, datablock);
    if (conn)
        yconn_close(conn);
    ylog_info("try to connect %s (%s)\n", addr, flags);
    res = yconn_open(addr, flags, datablock);
    YDB_FAIL(res, res);
    unlock(datablock);
    ylog_out();
    return res;
failed:
    yconn_close(conn);
    ydb_epoll_destroy(datablock);
    unlock(datablock);
    ylog_out();
    return res;
}

// ydb_disconnect --
// Destroy or disconnect to the YDB IPC (Inter Process Communication) channel
ydb_res ydb_disconnect(ydb *datablock, char *addr)
{
    ydb_res res = YDB_E_NO_ENTRY;
    ylist *conns;
    char _addr[256];
    ylog_in();
    lock(datablock);
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    if (!addr)
    {
        snprintf(_addr, sizeof(_addr), "uss://%s", datablock->name);
        addr = _addr;
    }
    conns = yconn_getall(addr, datablock);
    while (conns && !ylist_empty(conns))
    {
        yconn *conn = ylist_pop_front(conns);
        if (conn)
        {
            res = YDB_OK;
            yconn_close(conn);
        }
    }
    ylist_destroy(conns);
    ydb_epoll_destroy(datablock);
    if (res == YDB_OK)
        ylog_info("disconnected %s\n", addr);
failed:
    unlock(datablock);
    ylog_out();
    return res;
}

// ydb_is_connected --
// Check the YDB IPC channel connected or not.
int ydb_is_connected(ydb *datablock, char *addr)
{
    int ret = 0;
    yconn *conn = NULL;
    lock(datablock);
    conn = yconn_get(addr, datablock);
    if (conn)
    {
        if (!IS_DISCONNECTED(conn))
            ret = 1;
    }
    unlock(datablock);
    return ret;
}

// ydb_is_server()
// Check the YDB IPC channel is running as server
int ydb_is_server(ydb *datablock, char *addr)
{
    int ret = 0;
    yconn *conn = NULL;
    lock(datablock);
    conn = yconn_get(addr, datablock);
    if (conn)
    {
        if (IS_SERVER(conn))
            ret = 1;
    }
    unlock(datablock);
    return ret;
}

// ydb_is_publisher()
// Check the YDB IPC channel is running as server
int ydb_is_publisher(ydb *datablock, char *addr)
{
    int ret = 0;
    yconn *conn = NULL;
    lock(datablock);
    conn = yconn_get(addr, datablock);
    if (conn)
    {
        if (IS_SET(conn->flags, YCONN_ROLE_PUBLISHER))
            ret = 1;
    }
    unlock(datablock);
    return ret;
}

ydb_res ydb_onchange_hook_add(ydb *datablock, ydb_onchange_hook hook, void *user)
{
    if (datablock)
    {
        lock(datablock);
        datablock->onchange = hook;
        datablock->onchange_user = user;
        unlock(datablock);
    }
    return YDB_OK;
}

void ydb_onchange_hook_delete(ydb *datablock)
{
    if (datablock)
    {
        lock(datablock);
        datablock->onchange = NULL;
        datablock->onchange_user = NULL;
        unlock(datablock);
    }
}

void _ydb_onchange_run(ydb *datablock, bool started)
{
    if (datablock->onchange) {
        datablock->onchange(datablock, started, datablock->onchange_user);
    }
}

ynode_log *ydb_log_open(ydb *datablock, FILE *dumpfp)
{
    _ydb_onchange_run(datablock, true);
    return ynode_log_open(datablock->top, dumpfp);
}

void ydb_log_close(ydb *datablock, ynode_log *log, char **buf, size_t *buflen)
{
    ynode_log_close(log, buf, buflen);
    _ydb_onchange_run(datablock, false);
}

// Clear all data in YAML DataBlock
ydb_res ydb_clear(ydb *datablock)
{
    ydb_res res = YDB_OK;
    ynode_log *log;
    size_t buflen = 0;
    char *buf = NULL;
    ynode *n;
    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    lock(datablock);
    log = ydb_log_open(datablock, NULL);
    n = ynode_down(datablock->top);
    while (n)
    {
        ynode_delete(n, log);
        n = ynode_down(datablock->top);
    }
    ydb_log_close(datablock, log, &buf, &buflen);
    yconn_publish(NULL, NULL, datablock, YOP_DELETE, buf, buflen);
failed:
    CLEAR_BUF(buf, buflen);
    unlock(datablock);
    ylog_out();
    return res;
}

// Close YAML Datablock
static void ydb_read_hook_free(void *rhook);

void ydb_close(ydb *datablock)
{
    ylog_in();
    if (datablock)
    {
        YDB_INFO(datablock, "closed");
        lock(datablock);
        ytrie_delete(ydb_pool, datablock->name, strlen(datablock->name));
        if (datablock->disconn)
            ylist_destroy_custom(datablock->disconn, (user_free)_yconn_free_with_deinit);
        if (datablock->conn)
            ytree_destroy_custom(datablock->conn, (user_free)_yconn_free_with_deinit);
        if (datablock->timer)
            ytimer_destroy(datablock->timer);
        if (datablock->event)
            ytree_destroy_custom(datablock->event, (user_free)waitevent_free);
        if (datablock->updater)
            ytrie_destroy_custom(datablock->updater, (user_free)ydb_read_hook_free);
        if (datablock->top)
            ynode_delete(ynode_top(datablock->top), NULL);
        if (datablock->name)
            yfree(datablock->name);
        if (datablock->epollfd > 0)
            close(datablock->epollfd);
#ifdef PTHREAD_LOCK
        unlock(datablock);
        pthread_mutex_destroy(&datablock->lock);
#endif
        free(datablock);
    }
    ypool_destroy();
    ylog_out();
}

ydb *ydb_get(char *name_and_path, ynode **node)
{
    ydb *datablock;
    int mlen = 0, slen;
    if (!name_and_path)
        return NULL;
    slen = strlen(name_and_path);
    if (!ydb_pool)
        return NULL;
    datablock = ytrie_best_match(ydb_pool, name_and_path, slen, &mlen);
    lock(datablock);
    if (datablock && node)
    {
        if (mlen < slen)
            *node = ynode_search(datablock->top, name_and_path + mlen);
    }
    unlock(datablock);
    return datablock;
}

// return the new string consisting of the YDB name and the path to the iter.
// the return string must be free.
char *ydb_name_and_path(ynode *node, int *pathlen)
{
    return ynode_path(node, YDB_LEVEL_MAX, pathlen);
}

const char *ydb_name(ydb *datablock)
{
    return datablock->name;
}

// return the node in the path of the yaml data block.
ynode *ydb_search(ydb *datablock, const char *format, ...)
{
    ynode *node = NULL;
    char *path = NULL;
    size_t pathlen = 0;
    FILE *fp;
    if (!datablock || !format)
        return NULL;
    fp = open_memstream(&path, &pathlen);
    if (!fp)
        return NULL;
    {
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
    }
    lock(datablock);
    if (path)
    {
        node = ynode_search(datablock->top, path);
        free(path);
    }
    unlock(datablock);
    return node;
}

// return the path of the node. (the path must be free.)
char *ydb_path(ydb *datablock, ynode *node, int *pathlen)
{
    char *p;
    lock(datablock);
    p = ynode_path(node, ynode_level(datablock->top, node), pathlen);
    unlock(datablock);
    return p;
}

// return the path of the node. (the path must be free.)
char *ydb_path_and_value(ydb *datablock, ynode *node, int *pathlen)
{
    char *p;
    lock(datablock);
    p = ynode_path_and_val(node, ynode_level(datablock->top, node), pathlen);
    unlock(datablock);
    return p;
}

char *ydb_path_nodes(ynode *ancestor, ynode *descendant, int *pathlen)
{
    return ynode_path(descendant, ynode_level(ancestor, descendant), pathlen);
}

// return the ylist instance that tokenizes the path.
// ylist and each entry should be free (ylist_destroy_custom(ylist, free)).
ylist *ydb_path_tokenize(char *path, char **val)
{
    return ynode_path_tokenize(path, val);
}

// return the top node of the yaml data block.
ynode *ydb_top(ydb *datablock)
{
    if (datablock)
        return datablock->top;
    return NULL;
}

// return the root node of the yaml data block.
ynode *ydb_root(ydb *datablock)
{
    return ynode_top(datablock->top);
}

// return 1 if ynode is empty.
int ydb_empty(ynode *node)
{
    return ynode_empty(node);
}

// return the number of child nodes.
int ydb_size(ynode *node)
{
    return ynode_size(node);
}

ynode *ydb_find_child(ynode *base, char *key)
{
    return ynode_find_child(base, key);
}

ynode *ydb_find_child_by_prefix(ynode *base, char *prefix)
{
    return ynode_find_nearby(base, prefix, 0);
}

ynode *ydb_find(ynode *base, const char *format, ...)
{
    FILE *fp;
    char *path = NULL;
    size_t pathlen = 0;
    ynode *found = NULL;
    if (!base || !format)
        return NULL;
    fp = open_memstream(&path, &pathlen);
    if (!fp)
        return NULL;

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    if (path)
    {
        if (pathlen > 0)
            found = ynode_search(base, path);
        free(path);
    }
    return found;
}

// return the parent node of the node.
ynode *ydb_up(ynode *node)
{
    return ynode_up(node);
}

// return the first child node of the node.
ynode *ydb_down(ynode *node)
{
    return ynode_down(node);
}

// return the previous sibling node of the node.
ynode *ydb_prev(ynode *node)
{
    return ynode_prev(node);
}

// return the next sibling node of the node.
ynode *ydb_next(ynode *node)
{
    return ynode_next(node);
}

// return the first sibling node of the node.
ynode *ydb_first(ynode *node)
{
    return ynode_first(node);
}

// return the last sibling node of the node.
ynode *ydb_last(ynode *node)
{
    return ynode_last(node);
}

// return node tag
const char *ydb_tag(ynode *node)
{
    return ynode_tag(node);
}

// return node value if that is a leaf.
const char *ydb_value(ynode *node)
{
    return ynode_value(node);
}

// return node key if that has a hash key.
const char *ydb_key(ynode *node)
{
    return ynode_key(node);
}

// return node index if the nodes' parent is a list.
int ydb_index(ynode *node)
{
    return ynode_index(node);
}

int ydb_level(ynode *top, ynode *node)
{
    return ynode_level(top, node);
}

// return YAML string for node.
char *ydb_ynode2yaml(ydb *datablock, ynode *node, int *slen)
{
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    if (!datablock || !node)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    if (fp == NULL) {
        return NULL;
    }
    int level = ynode_level(datablock->top, node);
    if (level == 0 && ynode_type(datablock->top) == YNODE_TYPE_VAL)
        fprintf(fp, "%s", ynode_value(node));
    else
        ynode_printf_to_fp(fp, node, 1 - level, 0);
    if (fp)
        fclose(fp);
    if (slen) {
        *slen = buflen;
    }
    return buf;
}

// ydb_clean --
// Remove all child nodes
ydb_res ydb_clean(ydb *datablock, ynode *n)
{
    ydb_res res = YDB_OK;
    ynode_log *log;
    size_t buflen = 0;
    char *buf = NULL;
    ynode *c;
    ylog_in();
    YDB_FAIL(!datablock || !n, YDB_E_INVALID_ARGS);
    lock(datablock);
    log = ydb_log_open(datablock, NULL);
    c = ynode_down(n);
    while (c)
    {
        ynode_delete(c, log);
        c = ynode_down(n);
    }
    ydb_log_close(datablock, log, &buf, &buflen);
    yconn_publish(NULL, NULL, datablock, YOP_DELETE, buf, buflen);
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ylog_out();
    return res;
}

ydb_res ydb_parse(ydb *datablock, FILE *stream)
{
    ydb_res res = YDB_OK;
    char *buf = NULL;
    size_t buflen = 0;
    ynode *src = NULL;
    ylog_in();
    res = ynode_scanf_from_fp(stream, &src);
    YDB_FAIL(res, res);
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        lock(datablock);
        log = ydb_log_open(datablock, NULL);
        top = ynode_merge(datablock->top, src, log);
        ydb_log_close(datablock, log, &buf, &buflen);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, NULL, datablock, YOP_MERGE, buf, buflen);
        }
        else
        {
            YDB_FAIL(YDB_E_MERGE_FAILED, YDB_E_MERGE_FAILED);
        }
    }
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ylog_out();
    return res;
}

ydb_res ydb_parses(ydb *datablock, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    char *ibuf = NULL;
    size_t ibuflen = 0;
    ylog_in();
    res = ynode_scanf_from_buf(buf, buflen, 0, &src);
    YDB_FAIL(res, res);
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        lock(datablock);
        log = ydb_log_open(datablock, NULL);
        top = ynode_merge(datablock->top, src, log);
        ydb_log_close(datablock, log, &ibuf, &ibuflen);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, NULL, datablock, YOP_MERGE, ibuf, ibuflen);
        }
        else
        {
            YDB_FAIL(YDB_E_MERGE_FAILED, YDB_E_MERGE_FAILED);
        }
    }
failed:
    lock(datablock);
    CLEAR_BUF(ibuf, ibuflen);
    ynode_remove(src);
    ylog_out();
    return res;
}

int ydb_dump(ydb *datablock, FILE *stream)
{
    int len;
    if (!datablock)
        return -1;
    lock(datablock);
    if (ynode_type(datablock->top) == YNODE_TYPE_VAL)
        len = fprintf(stream, "%s", ynode_value(datablock->top));
    else
        len = ynode_printf_to_fp(stream, datablock->top, 1, YDB_LEVEL_MAX);
    unlock(datablock);
    return len;
}

int ydb_dump_debug(ydb *datablock, FILE *stream)
{
    if (!datablock)
        return -1;
    lock(datablock);
    ynode_dump_to_fp(stream, datablock->top, 0, YDB_LEVEL_MAX);
    unlock(datablock);
    return 0;
}

int ydb_dumps(ydb *datablock, char **buf, size_t *buflen)
{
    FILE *fp;
    if (!datablock)
        return -1;
    *buf = NULL;
    *buflen = 0;
    fp = open_memstream(buf, buflen);
    if (fp)
    {
        int n;
        lock(datablock);
        n = ynode_printf_to_fp(fp, datablock->top, 1, YDB_LEVEL_MAX);
        unlock(datablock);
        fclose(fp);
        return n;
    }
    else
        return -1;
}

// update ydb using the input string
ydb_res ydb_write(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        ynode *top = NULL;
        ynode_log *log = NULL;
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        YDB_FAIL(res || !src, res);
        CLEAR_BUF(buf, buflen);
        lock(datablock);
        log = ydb_log_open(datablock, NULL);
        // ynode_dump(src, 0, 24);
        top = ynode_merge(datablock->top, src, log);
        ydb_log_close(datablock, log, &buf, &buflen);
        YDB_FAIL(!top, YDB_E_MERGE_FAILED);
        datablock->top = top;
        yconn_publish(NULL, NULL, datablock, YOP_MERGE, buf, buflen);
    }
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ylog_out();
    return res;
}

// update the remote ydb targeted by origin.
ydb_res ydb_whisper_merge(ydb *datablock, char *path, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *target;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ylog_in();
    YDB_FAIL(!datablock || !path, YDB_E_INVALID_ARGS);
    lock(datablock);
    target = ynode_search(datablock->top, path);
    YDB_FAIL(!target, YDB_E_NO_ENTRY);
    YDB_FAIL(ynode_origin(target) == 0, YDB_E_UNKNOWN_TARGET);

    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    fprintf(fp, YMSG_WHISPER_DELIMITER " %s\n", path);

    va_list args;
    va_start(args, format);
    vfprintf(fp, (const char *)format, args);
    va_end(args);
    fclose(fp);

    yconn_whisper(ynode_origin(target), datablock, YOP_MERGE, buf, buflen);
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ylog_out();
    return res;
}

// delete the remote ydb targeted by origin.
ydb_res ydb_whisper_delete(ydb *datablock, char *path, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *target;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ylog_in();
    YDB_FAIL(!datablock || !path, YDB_E_INVALID_ARGS);
    lock(datablock);
    target = ynode_search(datablock->top, path);
    YDB_FAIL(!target, YDB_E_NO_ENTRY);
    YDB_FAIL(ynode_origin(target) == 0, YDB_E_UNKNOWN_TARGET);

    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    fprintf(fp, YMSG_WHISPER_DELIMITER " %s\n", path);

    va_list args;
    va_start(args, format);
    vfprintf(fp, (const char *)format, args);
    va_end(args);
    fclose(fp);

    yconn_whisper(ynode_origin(target), datablock, YOP_DELETE, buf, buflen);
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ylog_out();
    return res;
}

struct ydb_delete_data
{
    ynode_log *log;
    ynode *node;
};

static ydb_res ydb_delete_sub(ynode *cur, void *addition)
{
    struct ydb_delete_data *pddata = (void *)addition;
    ynode *n = pddata->node;
    ynode *target = ynode_lookup(n, cur, 1);
    if (target)
        ynode_delete(target, pddata->log);
    return YDB_OK;
}

// delete ydb using the input string
ydb_res ydb_delete(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        unsigned int flags;
        struct ydb_delete_data ddata;
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        YDB_FAIL(res || !src, res);
        CLEAR_BUF(buf, buflen);
        lock(datablock);
        ddata.log = ydb_log_open(datablock, NULL);
        ddata.node = datablock->top;
        flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY; // YNODE_VAL_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, &ddata, flags);
        ydb_log_close(datablock, ddata.log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (rbuflen > 0)
                yconn_publish(NULL, NULL, datablock, YOP_DELETE, rbuf, rbuflen);
            free(rbuf);
        }
    }
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ylog_out();
    return res;
}

// ydb_add --
// Add data to YDB using YAML input string
// (that shod be terminated with null as a string)
ydb_res ydb_add(ydb *datablock, char *s)
{
    return ydb_parses(datablock, s, strlen(s));
}

// ydb_rm --
// Delete data from YDB using YAML input string
// (that shod be terminated with null as a string)
ydb_res ydb_rm(ydb *datablock, char *s)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    size_t slen = 0;
    if (s)
    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        unsigned int flags;
        struct ydb_delete_data ddata;
        slen = strlen(s);
        res = ynode_scanf_from_buf(s, slen, 0, &src);
        YDB_FAIL(res || !src, res);
        lock(datablock);
        ddata.log = ydb_log_open(datablock, NULL);
        ddata.node = datablock->top;
        flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY; // YNODE_VAL_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, &ddata, flags);
        ydb_log_close(datablock, ddata.log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (rbuflen > 0)
                yconn_publish(NULL, NULL, datablock, YOP_DELETE, rbuf, rbuflen);
            free(rbuf);
        }
    }
failed:
    unlock(datablock);
    ynode_remove(src);
    ylog_out();
    return res;
}

struct readhook
{
    const char *path;
    size_t pathlen;
    union {
        ydb_read_hook hook;
        ydb_read_hook0 hook0;
        ydb_read_hook1 hook1;
        ydb_read_hook2 hook2;
        ydb_read_hook3 hook3;
        ydb_read_hook4 hook4;
    };
    int num;
    void *user[];
};

struct ydb_update_params
{
    yconn *src_conn;
    ydb *datablock;
    struct readhook *rhook;
    bool updated;
};

static ydb_res ydb_update_rhook_exec(struct ydb_update_params *params, struct readhook *rhook)
{
    ydb_res res;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    ynode *src = NULL;
    ynode *top = NULL;
    ynode_log *log = NULL;
    ydb *datablock = params->datablock;

    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        switch (rhook->num)
        {
        case 0:
            res = rhook->hook0(datablock, rhook->path, fp);
            break;
        case 1:
            res = rhook->hook1(datablock, rhook->path, fp, rhook->user[0]);
            break;
        case 2:
            res = rhook->hook2(
                datablock, rhook->path, fp, rhook->user[0], rhook->user[1]);
            break;
        case 3:
            res = rhook->hook3(
                datablock, rhook->path, fp, rhook->user[0], rhook->user[1], rhook->user[2]);
            break;
        case 4:
            res = rhook->hook4(
                datablock, rhook->path, fp, rhook->user[0], rhook->user[1], rhook->user[2], rhook->user[3]);
            break;
        default:
            break;
        }
        fclose(fp);
    }
    res = ynode_scanf_from_buf(buf, buflen, 0, &src);
    CLEAR_BUF(buf, buflen);
    if (res)
    {
        ynode_remove(src);
        return res;
    }
    if (!src)
        return YDB_OK;

    log = ydb_log_open(datablock, NULL);
    top = ynode_merge(datablock->top, src, log);
    ydb_log_close(datablock, log, &buf, &buflen);
    ynode_remove(src);
    if (top)
    {
        datablock->top = top;
        yconn_publish(params->src_conn, NULL, datablock, YOP_MERGE, buf, buflen);
        params->updated = true;
    }
    else
        res = YDB_E_MERGE_FAILED;
    CLEAR_BUF(buf, buflen);
    return res;
}

static ydb_res ydb_update_sub(ynode *cur, void *addition)
{
    ydb_res res = YDB_OK;
    struct ydb_update_params *params = addition;
    ydb *datablock = params->datablock;
    struct readhook *rhook = NULL;

    int pathlen = 0;
    char *path = ydb_path(datablock, cur, &pathlen);
    if (!path)
    {
        path = strdup("/");
        pathlen = 1;
    }
    if (path && pathlen > 0)
    {
        ylog_info("ydb[%s] path=%s\n", datablock->name, path);
        ylist *child_rhooks = ytrie_search_range(datablock->updater, path, pathlen);
        if (ylist_size(child_rhooks) > 0)
        {
            while (!ylist_empty(child_rhooks))
            {
                rhook = ylist_pop_front(child_rhooks);
                if (rhook)
                {
                    ylog_info("ydb[%s] read hook (%s) found\n", datablock->name, rhook->path);
                    res = ydb_update_rhook_exec(params, rhook);
                    if (res)
                        ylog_error("ydb[%s] read hook (%s) failed with %s\n",
                                   datablock->name, rhook->path, ydb_res_str(res));
                }
            }
        }
        else
        {
            int matched_len = 0;
            rhook = ytrie_best_match(datablock->updater, path, pathlen, &matched_len);
            if (rhook != params->rhook)
            {
                if (params->rhook)
                {
                    // run rhook before change rhook
                    ylog_info("ydb[%s] read hook (%s) found\n", datablock->name, params->rhook->path);
                    res = ydb_update_rhook_exec(params, params->rhook);
                    if (res)
                        ylog_error("ydb[%s] read hook (%s) failed with %s\n",
                                   datablock->name, params->rhook->path, ydb_res_str(res));
                }
                params->rhook = rhook;
            }
        }
        if (child_rhooks)
            ylist_destroy(child_rhooks);
    }
    if (path)
        free(path);
    return YDB_OK;
}

bool ydb_update(yconn *src_conn, ydb *datablock, ynode *target)
{
    ydb_res res = YDB_OK;
    struct ydb_update_params params;
    params.src_conn = src_conn;
    params.datablock = datablock;
    params.updated = false;
    params.rhook = NULL;
    ynode_traverse(target, ydb_update_sub, &params, YNODE_LEAF_ONLY);
    if (params.rhook)
    {
        // run the last rhook.
        ylog_info("ydb[%s] read hook (%s) found\n", datablock->name, params.rhook->path);
        res = ydb_update_rhook_exec(&params, params.rhook);
        if (res)
            ylog_error("ydb[%s] read hook (%s) failed with %s\n",
                       datablock->name, params.rhook->path, ydb_res_str(res));
    }
    return params.updated;
}

ydb_res ydb_read_hook_add(ydb *datablock, char *path, ydb_read_hook func, int num, ...)
{
    ydb_res res = YDB_OK;
    int pathlen;
    struct readhook *rhook;
    struct readhook *oldhook;
    ynode *src = NULL;
    char *newpath = NULL;
    ylog_in();
    YDB_FAIL(!datablock || !func || !path || num < 0, YDB_E_INVALID_ARGS);
    YDB_FAIL(num > 4 || num < 0, YDB_E_INVALID_ARGS);
    src = ynode_create_path(path, NULL, NULL);
    YDB_FAIL(!src, YDB_E_CTRL);
    newpath = ynode_path(src, YDB_LEVEL_MAX, NULL);
    if (!newpath) // set root
        newpath = strdup("/");
    YDB_FAIL(!newpath, YDB_E_CTRL);
    pathlen = strlen(newpath);

    rhook = malloc(sizeof(struct readhook) + sizeof(void *) * num);
    YDB_FAIL(!rhook, YDB_E_MEM_ALLOC);
    rhook->hook = func;
    rhook->path = ystrdup(newpath);
    rhook->pathlen = pathlen;
    rhook->num = num;
    {
        int i;
        va_list ap;
        va_start(ap, num);
        ylog_debug("user total = %d\n", num);
        for (i = 0; i < num; i++)
        {
            void *p = va_arg(ap, void *);
            rhook->user[i] = p;
            ylog_debug("U%d=%p\n", i, p);
        }
        va_end(ap);
    }
    lock(datablock);
    oldhook = ytrie_insert(datablock->updater, rhook->path, rhook->pathlen, rhook);
    if (oldhook)
    {
        ylog_debug("old read hook (%p) deleted\n", oldhook);
        if (oldhook->path)
            yfree(oldhook->path);
        free(oldhook);
    }
    ylog_info("ydb[%s] read hook (%p) added to %s (%d)\n",
              datablock->name, rhook->hook, rhook->path, rhook->pathlen);
failed:
    unlock(datablock);
    if (newpath)
        free(newpath);
    ynode_remove(ynode_top(src));
    ylog_out();
    return res;
}

void ydb_read_hook_delete(ydb *datablock, char *path)
{
    int pathlen;
    ynode *src = NULL;
    char *newpath = NULL;
    struct readhook *rhook;
    ylog_in();
    if (!datablock || !path)
    {
        ylog_out();
        return;
    }
    src = ynode_top(ynode_create_path(path, NULL, NULL));
    if (!src)
    {
        ylog_out();
        return;
    }
    newpath = ynode_path(src, YDB_LEVEL_MAX, NULL);
    ynode_remove(src);
    if (!newpath) // set root
        newpath = strdup("/");
    pathlen = strlen(newpath);
    lock(datablock);
    rhook = ytrie_delete(datablock->updater, newpath, pathlen);
    if (rhook)
    {
        ylog_info("ydb[%s] read hook (%p) deleted from %s (%d)\n",
                  datablock->name, rhook->hook, path, pathlen);
        if (rhook->path)
            yfree(rhook->path);
        free(rhook);
    }
    unlock(datablock);
    if (newpath)
        free(newpath);
    ylog_out();
}

static void ydb_read_hook_free(void *hook)
{
    struct readhook *rhook = hook;
    if (rhook)
    {
        ylog_info("ydb[...] read hook (%p) deleted from %s (%d)\n",
                  rhook->hook, rhook->path, rhook->pathlen);
        if (rhook->path)
            yfree(rhook->path);
        free(hook);
    }
}

struct ydb_read_data
{
    ydb *datablock;
    yarray *vararray;
    int vartotal;
    int varnum;
};

static ydb_res ydb_read_sub(ynode *cur, void *addition)
{
    struct ydb_read_data *data = addition;
    const char *value = ynode_value(cur);

    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->datablock->top, cur, 0);
        if (n)
        {
            int index = atoi(value);
            void *p = yarray_data(data->vararray, index);
            ylog_debug("index=%d p=%p\n", index, p);
            if (YLOG_SEVERITY_DEBUG)
            {
                char buf[512];
                ynode_dump_to_buf(buf, sizeof(buf), n, 0, 0);
                ylog_debug("%s", buf);
                ynode_dump_to_buf(buf, sizeof(buf), cur, 0, 0);
                ylog_debug("%s", buf);
            }
#if 0
            sscanf(ynode_value(n), &(value[4]), p);
#else
            int len = strlen(value);
            if (value[len - 1] == 's')
                strcpy(p, ynode_value(n));
            else
                sscanf(ynode_value(n), &(value[4]), p);
#endif
            data->varnum++;
        }
        else
        {
            if (YLOG_SEVERITY_INFO)
            {
                char *path = ynode_path(cur, YDB_LEVEL_MAX, NULL);
                ylog_info("ydb[%s] no data for (%s)\n", data->datablock->name, path);
                free(path);
            }
        }
    }
    return YDB_OK;
}

ydb_res ynode_scan(FILE *fp, char *buf, int buflen, int origin, ynode **n, int *queryform);

// read the date from ydb as the scanf()
int ydb_read(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    struct ydb_read_data data = {
        .vararray = NULL,
    };
    ynode *src = NULL;
    unsigned int flags;
    int ap_num = 0;
    int formatlen;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    formatlen = strlen(format);
    res = ynode_scan(NULL, (char *)format, formatlen, 0, &src, &ap_num);
    YDB_FAIL(res, res);

    if (ap_num <= 0 || !src)
    {
        ynode_remove(src);
        ylog_out();
        return 0;
    }

    data.vararray = yarray_create(16);
    data.vartotal = ap_num;
    data.varnum = 0;
    data.datablock = datablock;

    {
        va_list ap;
        va_start(ap, format);
        ylog_debug("var total = %d\n", ap_num);
        do
        {
            void *p = va_arg(ap, void *);
            yarray_push_back(data.vararray, p);
            ylog_debug("p=%p\n", p);
            ap_num--;
        } while (ap_num > 0);
        va_end(ap);
    }

    flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
    lock(datablock);
    if (datablock->synccount > 0)
    {
        eventid eid = yconn_sync(NULL, datablock, false, (char *)format, formatlen);
        if (valid_waitevent(eid))
        {
            res = yconn_serve_blocking(datablock, eid, datablock->timeout);
            YDB_FAIL(YDB_FAILED(res), res);
        }
    }
    if (ytrie_size(datablock->updater) > 0)
        ydb_update(NULL, datablock, src);
    res = ynode_traverse(src, ydb_read_sub, &data, flags);
    YDB_FAIL(res, res);
    ylog_debug("var read = %d\n", data.varnum);
failed:
    unlock(datablock);
    yarray_destroy(data.vararray);
    ynode_remove(src);
    ylog_out();
    if (res)
        return -1;
    return data.varnum;
}

struct ydb_fprintf_data
{
    ydb *datablock;
    ynode_log *log;
    int num_of_nodes;
    int origin;
};

extern int ynode_get_with_origin(ynode *src, int origin, ynode_log *log);
static ydb_res ydb_fprintf_sub(ynode *cur, void *addition)
{
    struct ydb_fprintf_data *data = addition;
    ynode *node = ynode_lookup(data->datablock->top, cur, 0);
    if (node)
    {
        data->num_of_nodes += ynode_get_with_origin(node, data->origin, data->log);
    }
    else
    {
        if (YLOG_SEVERITY_INFO)
        {
            char *path = ynode_path(cur, YDB_LEVEL_MAX, NULL);
            ylog_info("ydb[%s] no data for (%s)\n", data->datablock->name, path);
            free(path);
        }
    }
    return YDB_OK;
}

// print the target data to the stream
int ydb_fprintf(FILE *stream, ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    int ret;

    ylog_in();
    YDB_FAIL(!datablock || !stream, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
    lock(datablock);
    if (datablock->synccount > 0)
    {
        eventid eid = yconn_sync(NULL, datablock, false, buf, buflen);
        if (valid_waitevent(eid))
        {
            res = yconn_serve_blocking(datablock, eid, datablock->timeout);
            YDB_FAIL(YDB_FAILED(res), res);
        }
    }

    {
        ynode_log *log = NULL;
        struct ydb_fprintf_data data;
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        YDB_FAIL(res || !src, res);
        CLEAR_BUF(buf, buflen);
        if (ytrie_size(datablock->updater) > 0)
            ydb_update(NULL, datablock, src);
        log = ydb_log_open(datablock, NULL);
        data.log = log;
        data.datablock = datablock;
        data.num_of_nodes = 0;
        data.origin = -1;
        ynode_traverse(src, ydb_fprintf_sub, &data, YNODE_LEAF_ONLY);
        ydb_log_close(datablock, log, &buf, &buflen);
        if (buf)
            fwrite(buf, buflen, 1, stream);
        // fprintf(stream, "%s", buf);
    }
failed:
    unlock(datablock);
    ret = buflen;
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ylog_out();
    return ret;
}

// update the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    FILE *fp;
    char *pathbuf = NULL;
    size_t pathbuflen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&pathbuf, &pathbuflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    lock(datablock);
    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        ynode_log *log = NULL;
        log = ydb_log_open(datablock, NULL);
        src = ynode_create_path(pathbuf, datablock->top, log);
        ydb_log_close(datablock, log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (src)
                yconn_publish(NULL, NULL, datablock, YOP_MERGE, rbuf, rbuflen);
            free(rbuf);
        }
    }
    YDB_FAIL(!src, YDB_E_MERGE_FAILED);
failed:
    unlock(datablock);
    CLEAR_BUF(pathbuf, pathbuflen);
    ylog_out();
    return res;
}

// delete the ydb using input path
// ydb_path_delete(datablock, "/path/to/update\n")
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *target = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    lock(datablock);
    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        ynode_log *log = NULL;
        log = ydb_log_open(datablock, NULL);
        target = ynode_search(datablock->top, buf);
        if (target)
        {
            int index = ynode_index(target);
            if (index <= 0)
                ynode_delete(target, log);
            else
                res = YDB_E_DENIED_DELETE;
        }
        ydb_log_close(datablock, log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (rbuflen > 0)
                yconn_publish(NULL, NULL, datablock, YOP_DELETE, rbuf, rbuflen);
            free(rbuf);
        }
    }
    YDB_FAIL(!target, YDB_E_DELETE_FAILED);
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ylog_out();
    return res;
}

// read the value from ydb using input path
// char *value = ydb_path_read(datablock, "/path/to/update")
const char *ydb_path_read(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    ynode *target = NULL;
    FILE *fp;
    char *path = NULL;
    size_t pathlen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&path, &pathlen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    {
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);

        src = ynode_top(ynode_create_path(path, NULL, NULL));
        YDB_FAIL(!src, YDB_E_CTRL);
        lock(datablock);
        if (datablock->synccount > 0)
        {
            char buf[512];
            int buflen;
            buf[0] = 0;
            buflen = ynode_printf_to_buf(buf, sizeof(buf), src, 1, YDB_LEVEL_MAX);
            eventid eid = yconn_sync(NULL, datablock, false, buf, buflen);
            if (valid_waitevent(eid))
            {
                res = yconn_serve_blocking(datablock, eid, datablock->timeout);
                YDB_FAIL(YDB_FAILED(res), res);
            }
        }
        if (ytrie_size(datablock->updater) > 0)
            ydb_update(NULL, datablock, src);
        target = ynode_search(datablock->top, path);
    }
failed:
    unlock(datablock);
    CLEAR_BUF(path, pathlen);
    ynode_remove(src);
    ylog_out();
    if (target && ynode_type(target) == YNODE_TYPE_VAL)
        return ynode_value(target);
    return NULL;
}

int ydb_path_fprintf(FILE *stream, ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    ynode *target = NULL;
    FILE *fp;
    char *path = NULL;
    size_t pathlen = 0;
    int ret = 0;

    ylog_in();
    YDB_FAIL(!datablock || !stream, YDB_E_INVALID_ARGS);
    fp = open_memstream(&path, &pathlen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    src = ynode_top(ynode_create_path(path, NULL, NULL));
    YDB_FAIL(!src, YDB_E_CTRL);
    lock(datablock);
    if (datablock->synccount > 0)
    {
        char buf[512];
        int buflen;
        buf[0] = 0;
        buflen = ynode_printf_to_buf(buf, sizeof(buf), src, 1, YDB_LEVEL_MAX);
        if (buflen >= 0)
            buf[buflen] = 0;
        eventid eid = yconn_sync(NULL, datablock, false, buf, buflen);
        if (valid_waitevent(eid))
        {
            res = yconn_serve_blocking(datablock, eid, datablock->timeout);
            YDB_FAIL(YDB_FAILED(res), res);
        }
    }
    if (ytrie_size(datablock->updater) > 0)
        ydb_update(NULL, datablock, src);

    target = ynode_search(datablock->top, path);
    if (target)
    {
        int level = ynode_level(datablock->top, target);
        if (level == 0 && ynode_type(datablock->top) == YNODE_TYPE_VAL)
            ret = fprintf(stream, "%s", ynode_value(target));
        else
            ret = ynode_printf_to_fp(stream, target, 1 - level, YDB_LEVEL_MAX);
    }
failed:
    unlock(datablock);
    CLEAR_BUF(path, pathlen);
    ynode_remove(src);
    ylog_out();
    return ret;
}

struct yconn_socket_head
{
    struct
    {
        int fd;
    } send;
    struct
    {
        yconn_op op;
        ymsg_type type;
        FILE *fp;
        char *buf;
        size_t buflen;
        size_t bufused;
        int next;
    } recv;
};

void yconn_socket_deinit(yconn *conn)
{
    struct yconn_socket_head *head;
    if (!conn)
        return;
    head = conn->head;
    if (head)
    {
        if (head->send.fd > 0)
            close(head->send.fd);
        if (head->recv.fp)
            fclose(head->recv.fp);
        if (head->recv.buf)
            free(head->recv.buf);
        free(head);
    }
    conn->head = NULL;
    if (conn->fd > 0)
        close(conn->fd);
    conn->fd = -1;
    SET_DISCONNECTED(conn);
}

ydb_res yconn_socket_init(yconn *conn)
{
    int fd = -1;
    int addrlen;
    union {
        struct sockaddr_un un;
        struct sockaddr_in in;
    } addr;

    const char *address = conn->address;
    unsigned int flags = conn->flags;
    if (!IS_SET(flags, STATUS_DISCONNECT))
        return YDB_OK;
    UNSET_FLAG(flags, STATUS_MASK);
    if (IS_SET(flags, YCONN_TYPE_INET))
        fd = socket(AF_INET, SOCK_STREAM, 0);
    else // if (IS_SET(flags, YCONN_TYPE_UNIX))
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
        SET_DISCONNECTED(conn);
        return YDB_E_SYSTEM_FAILED;
    }
    memset(&addr, 0, sizeof(addr));

    if (IS_SET(flags, YCONN_TYPE_INET))
    {
        int ret = 1;
        int opt = 1;
        char *cport;
        in_addr_t caddr;
        char cname[128];
        int socket_opt = 0;
        strcpy(cname, &(address[strlen("tcp://")]));
        cport = strtok(cname, ":");
        cport = strtok(NULL, ":");
        ret = inet_pton(AF_INET, cname, &caddr); // INADDR_ANY;
        if (ret != 1)
        {
            YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
            SET_DISCONNECTED(conn);
            return YDB_E_SYSTEM_FAILED;
        }

        addr.in.sin_family = AF_INET;
        addr.in.sin_addr.s_addr = caddr;
        if (!cport)
            addr.in.sin_port = YDB_DEFAULT_PORT;
        else
            addr.in.sin_port = htons(atoi(cport));
        addrlen = sizeof(struct sockaddr_in);
        socket_opt = SO_REUSEADDR;
        // Major publisher selection in TCP if uncommented.
        // #ifdef SO_REUSEPORT
        //         socket_opt = socket_opt | SO_REUSEPORT;
        // #endif
        if (setsockopt(fd, SOL_SOCKET, socket_opt, &opt, sizeof(opt)))
        {
            YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
            SET_DISCONNECTED(conn);
            return YDB_E_SYSTEM_FAILED;
        }
        ylog_debug("addr: %s, port: %s\n", cname[0] ? cname : "null", cport);
    }
    else if (strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        const char *sname = &(address[strlen("uss://")]);
        addr.un.sun_family = AF_UNIX;
        snprintf(addr.un.sun_path, sizeof(addr.un.sun_path), "#%s", sname);
        addr.un.sun_path[0] = 0;
        addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sname) + 1;
    }
    else
    {
        const char *sname = &(address[strlen("us://")]);
        addr.un.sun_family = AF_UNIX;
        // Removed because it causes invalid access when multiple publishers exist.
        // if (access(sname, F_OK) == 0)
        //     unlink(sname);
        snprintf(addr.un.sun_path, sizeof(addr.un.sun_path), "%s", sname);
        addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sname) + 1;
    }

    if (IS_SET(flags, YCONN_ROLE_PUBLISHER))
    {
        if (bind(fd, (struct sockaddr *)&addr, addrlen) < 0)
        {
            if (connect(fd, (struct sockaddr *)&addr, addrlen) == -1)
                goto failed;
            SET_FLAG(flags, STATUS_CLIENT);
        }
        else
        {
            if (listen(fd, YDB_CONN_MAX) < 0)
                goto failed;
            SET_FLAG(flags, STATUS_SERVER);
        }
    }
    else
    {
        if (connect(fd, (struct sockaddr *)&addr, addrlen) == -1)
            goto failed;
        SET_FLAG(flags, STATUS_CLIENT);
    }
    if (!conn->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            close(fd);
            YCONN_FAILED(conn, YDB_E_MEM_ALLOC);
            return YDB_E_MEM_ALLOC;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        conn->head = head;
    }
    conn->fd = fd;
    conn->flags = flags;
    return YDB_OK;
failed:
    YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
    SET_DISCONNECTED(conn);
    if (fd > 0)
        close(fd);
    return YDB_E_CONN_FAILED;
}

int yconn_socket_accept(yconn *conn, yconn *client)
{
    int cfd = -1;
    union {
        struct sockaddr_un un;
        struct sockaddr_in in;
    } addr;
    socklen_t clen;
    if (IS_SET(conn->flags, YCONN_TYPE_INET))
        clen = sizeof(addr.in);
    else
        clen = sizeof(addr.un);
    cfd = accept(conn->fd, (struct sockaddr *)&addr, &clen);
    if (cfd < 0)
    {
        YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
        SET_DISCONNECTED(conn);
        return -1;
    }
    if (!client->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            close(cfd);
            YCONN_FAILED(conn, YDB_E_MEM_ALLOC);
            return -1;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        client->head = head;
    }
    client->fd = cfd;
    if (IS_SET(conn->flags, YCONN_TYPE_INET))
    {
        char buf[128];
        char caddr[128] = {0};
        const char *client_addr = inet_ntop(AF_INET, &addr.in.sin_addr, buf, clen);
        snprintf(caddr, sizeof(caddr), "tcp://%s:%d", client_addr ? client_addr : "unknown", ntohs(addr.in.sin_port));
        ylog_debug("accept conn: %s\n", caddr);
        if (client->address)
            yfree(client->address);
        client->address = ystrdup(caddr);
    }
    UNSET_FLAG(client->flags, STATUS_MASK);
    SET_FLAG(client->flags, STATUS_COND_CLIENT);
    SET_FLAG(client->flags, YCONN_TYPE_UNIX);
    return cfd;
}

void yconn_default_recv_head(
    yconn *conn, yconn_op *op, ymsg_type *type,
    unsigned int *flags, char **data, size_t *datalen)
{
    int n = 0;
    struct yconn_socket_head *head;
    char *recvdata;
    char name[128];
    char opstr[32];
    char typestr[32];
    head = conn->head;
    recvdata = strstr(*data, YMSG_START_DELIMITER);
    if (!recvdata)
        goto failed;
    recvdata += YMSG_START_DELIMITER_LEN;
    n = sscanf(recvdata,
               "#name: %s\n"
               "#seq: %u\n"
               "#type: %s\n"
               "#op: %s\n"
               "#timeout: %d\n",
               name,
               &conn->recvseq,
               typestr,
               opstr,
               &conn->recv_timeout);
    if (n < 3)
        goto failed;
    // Operation type
    *op = head->recv.op = ydb_get_yop(opstr);
    // message type (request/response/publish)
    *type = head->recv.type = ydb_get_ymsg(typestr);

    if (head->recv.op == YOP_INIT)
    {
        recvdata = strstr(recvdata, "#flags:");
        if (!recvdata)
            goto failed;
        opstr[0] = 0;
        sscanf(recvdata, "#flags: %s", opstr);
        if (opstr[0])
        {
            if (opstr[0] == 'p')
                SET_FLAG(*flags, YCONN_ROLE_PUBLISHER);
            else
                UNSET_FLAG(*flags, YCONN_ROLE_PUBLISHER);
            if (opstr[1] == 'w')
                SET_FLAG(*flags, YCONN_WRITABLE);
            else
                UNSET_FLAG(*flags, YCONN_WRITABLE);
            if (opstr[2] == 'u')
                SET_FLAG(*flags, YCONN_UNSUBSCRIBE);
            else
                UNSET_FLAG(*flags, YCONN_UNSUBSCRIBE);
        }
        if (conn->name)
            yfree(conn->name);
        conn->name = ystrdup(name);
    }
    ylog_info("ydb[%s] head {peer name: %s, seq: %u, type: %s, op: %s, to: %d}\n",
              conn->datablock->name,
              (name[0]) ? name : "...", conn->recvseq, ymsg_str[*type], yconn_op_str[*op],
              conn->recv_timeout);
    if (*flags)
    {
        ylog_info("ydb[%s] head {flags: %s%s%s}\n",
                  conn->datablock->name,
                  IS_SET(*flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                  IS_SET(*flags, YCONN_WRITABLE) ? "w" : "_",
                  IS_SET(*flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
    }
    ylog_info("ydb[%s] datalen {%ld} data {\n%.*s}\n",
              conn->datablock->name,
              *datalen, *datalen, *data);
    return;
failed:
    *op = head->recv.op = YOP_NONE;
    return;
}

#define RECV_BUF_SIZE 2048
ydb_res yconn_default_recv(
    yconn *conn, yconn_op *op, ymsg_type *type,
    unsigned int *flags, char **data, size_t *datalen,
    int *next)
{
    ydb_res res = YDB_OK;
    struct yconn_socket_head *head;
    char recvbuf[RECV_BUF_SIZE + 4];
    char *end, *start;
    ssize_t len, used;
    if (conn == NULL || conn->head == NULL)
        return YDB_E_CONN_FAILED;
    head = conn->head;
    *data = NULL;
    *datalen = 0;
    *next = 0;
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
        return YDB_E_CONN_FAILED;
    if (!head->recv.fp)
    {
        if (head->recv.buf)
            free(head->recv.buf);
        head->recv.buf = NULL;
        head->recv.buflen = 0;
        head->recv.bufused = 0;
        head->recv.fp = open_memstream(&head->recv.buf, &head->recv.buflen);
        if (!head->recv.fp)
            goto conn_failed;
        fflush(head->recv.fp);
    }
    if (head->recv.next)
    {
        end = strstr(head->recv.buf + head->recv.bufused, YMSG_END_DELIMITER);
        if (end)
        {
            used = (end + YMSG_END_DELIMITER_LEN) - head->recv.buf;
            len = used - head->recv.bufused;
            if (used < head->recv.buflen)
                *next = head->recv.next = 1;
            else
            {
                *next = head->recv.next = 0;
                fclose(head->recv.fp);
                head->recv.fp = NULL;
            }
            *data = head->recv.buf + head->recv.bufused;
            *datalen = len;
            head->recv.bufused = used;
            ylog_debug("message-len %ld, bufused %ld, recv.buflen %ld %s\n",
                       *datalen, head->recv.bufused, head->recv.buflen, (*next) ? "next on" : "");
            yconn_default_recv_head(conn, op, type, flags, data, datalen);
            return YDB_OK;
        }
        else
        {
            // The recv process continues to get more messages.
            char *remain;
            size_t remainlen = 0;
            *next = head->recv.next = 0;
            fclose(head->recv.fp);
            head->recv.fp = NULL;
            start = head->recv.buf;
            remain = head->recv.buf + head->recv.bufused;
            remainlen = head->recv.buflen - head->recv.bufused;
            head->recv.buf = NULL;
            head->recv.buflen = 0;
            head->recv.bufused = 0;
            head->recv.fp = open_memstream(&head->recv.buf, &head->recv.buflen);
            if (!head->recv.fp)
                goto conn_failed;
            if (remainlen > 0)
                if (fwrite(remain, remainlen, 1, head->recv.fp) != 1)
                    goto conn_failed;
            fflush(head->recv.fp);
            if (start)
                free(start);
        }
    }

    if (IS_SET(conn->flags, (YCONN_TYPE_INET | YCONN_TYPE_UNIX)))
        len = recv(conn->fd, recvbuf, RECV_BUF_SIZE, MSG_DONTWAIT);
    else
        len = read(conn->fd, recvbuf, RECV_BUF_SIZE);
    if (len <= 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            goto wait_next_recv;
        if (len == 0)
        {
            res = YDB_E_CONN_CLOSED;
            goto conn_closed;
        }
        if (len < 0)
            goto conn_failed;
    }
    recvbuf[len] = 0;
    if (fwrite(recvbuf, len, 1, head->recv.fp) != 1)
        goto conn_failed;
    fflush(head->recv.fp);

    if ((head->recv.buflen - head->recv.bufused) >= (len + YMSG_END_DELIMITER_LEN))
        start = head->recv.buf + head->recv.buflen - (len + YMSG_END_DELIMITER_LEN);
    else
        start = head->recv.buf + head->recv.bufused;
    end = strstr(start, YMSG_END_DELIMITER);
    if (!end)
        goto wait_next_recv;
    used = (end + YMSG_END_DELIMITER_LEN) - head->recv.buf;
    len = used - head->recv.bufused;
    if (used < head->recv.buflen)
        *next = head->recv.next = 1;
    else
    {
        *next = head->recv.next = 0;
        fclose(head->recv.fp);
        head->recv.fp = NULL;
    }
    *data = head->recv.buf + head->recv.bufused;
    *datalen = len;
    head->recv.bufused = used;
    ylog_debug("message-len %ld, bufused %ld, recv.buflen %ld %s\n",
               *datalen, head->recv.bufused, head->recv.buflen, (*next) ? "next on" : "");
    yconn_default_recv_head(conn, op, type, flags, data, datalen);
    return YDB_OK;

wait_next_recv:
    *next = head->recv.next = 0;
    return YDB_OK;
conn_failed:
    YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
    res = YDB_E_CONN_FAILED;
conn_closed:
    SET_DISCONNECTED(conn);
    if (head->recv.fp)
        fclose(head->recv.fp);
    if (head->recv.buf)
        free(head->recv.buf);
    head->recv.fp = NULL;
    head->recv.buf = NULL;
    head->recv.buflen = 0;
    head->recv.bufused = 0;
    *next = head->recv.next = 0;
    return res;
}

ydb_res yconn_default_send(yconn *conn, yconn_op op, ymsg_type type, char *data, size_t datalen)
{
    int n, fd;
    char msghead[256 + 128];
    struct yconn_socket_head *head;
    ylog_in();
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
    {
        ylog_out();
        return YDB_E_CONN_FAILED;
    }
    head = (struct yconn_socket_head *)conn->head;
    n = sprintf(msghead,
                YMSG_START_DELIMITER
                "#name: %s\n"
                "#seq: %u\n"
                "#type: %s\n"
                "#op: %s\n",
                conn->datablock->name,
                conn->sendseq,
                ymsg_str[type],
                yconn_op_str[op]);

    ylog_info("ydb[%s] head {seq: %u, type: %s, op: %s}\n",
              conn->datablock->name,
              conn->sendseq,
              ymsg_str[type],
              yconn_op_str[op]);
    switch (op)
    {
    case YOP_INIT:
        if (type == YMSG_REQUEST)
        {
            n += sprintf(msghead + n, "#timeout: %d\n", conn->send_timeout);
        }
        n += sprintf(msghead + n,
                     "#flags: %s%s%s\n",
                     IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                     IS_SET(conn->flags, YCONN_WRITABLE) ? "w" : "_",
                     IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
        ylog_info("ydb[%s] head {flags: %s%s%s}\n",
                  conn->datablock->name,
                  IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                  IS_SET(conn->flags, YCONN_WRITABLE) ? "w" : "_",
                  IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
        break;
    case YOP_SYNC:
        if (type == YMSG_REQUEST)
        {
            n += sprintf(msghead + n, "#timeout: %d\n", conn->send_timeout);
        }
        break;
    default:
        break;
    }
    n += sprintf(msghead + n, "%s", YMSG_HEAD_DELIMITER);
    fd = conn->fd;
    if (head->send.fd > 0)
        fd = head->send.fd;
#ifndef WRITEV_SEND
    n = write(fd, msghead, n);
    if (n < 0)
        goto conn_failed;
    if (datalen > 0)
    {
        tx_fail_count--;
        if (tx_fail_en && tx_fail_count <= 0)
        {
            ylog_error("TX FAILURE CASE TRIGGERED\n");
            tx_fail_en = 0;
            close(fd);
        }
        n = write(fd, data, datalen);
        if (n < 0)
            goto conn_failed;
    }
    ylog_info("ydb[%s] data {\n%s%.*s%s}\n",
              conn->datablock->name,
              msghead, datalen, data ? data : "", "\n...\n");
    n = write(fd, YMSG_END_DELIMITER, YMSG_END_DELIMITER_LEN);
#else
    int cnt = 0;
    struct iovec iov[3];
    iov[cnt].iov_base = msghead;
    iov[cnt].iov_len = n;
    cnt++;
    if (datalen > 0 && data)
    {
        iov[cnt].iov_base = data;
        iov[cnt].iov_len = datalen;
        cnt++;
    }
    iov[cnt].iov_base = YMSG_END_DELIMITER;
    iov[cnt].iov_len = YMSG_END_DELIMITER_LEN;
    cnt++;
    ylog_info("ydb[%s] data {\n%s%.*s%s}\n",
              conn->datablock->name,
              msghead, datalen, data ? data : "", "\n...\n");
    if (tx_fail_en)
    {
        if (tx_fail_count > 0)
        {
            tx_fail_count--;
            ylog_debug("tx-fail %d\n", tx_fail_count);
        }
        if (tx_fail_count == 0)
        {
            ylog_error("tx-fail for test\n");
            tx_fail_en = 0;
            close(fd);
        }
        else if (tx_fail_count < 0)
        {
            ylog_error("no-tx for test\n");
            ylog_out();
            return YDB_OK;
        }
    }
    n = writev(fd, iov, cnt);
#endif
    if (n < 0)
        goto conn_failed;
    ylog_out();
    return YDB_OK;
conn_failed:
    YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
    SET_DISCONNECTED(conn);
    ylog_out();
    return YDB_E_CONN_FAILED;
}

ydb_res yconn_file_init(yconn *conn)
{
    const char *fname;
    const char *address = conn->address;
    unsigned int flags = conn->flags;
    struct yconn_socket_head *head;
    if (!IS_SET(flags, STATUS_DISCONNECT))
        return YDB_OK;
    UNSET_FLAG(flags, STATUS_MASK);
    head = conn->head;
    if (!head)
    {
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            YCONN_FAILED(conn, YDB_E_MEM_ALLOC);
            return YDB_E_MEM_ALLOC;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        conn->head = head;
    }

    if (IS_SET(flags, YCONN_TYPE_FIFO))
    {
        char buf[256];
        fname = &(address[strlen("fifo://")]);
        strcpy(buf, fname);
        char *fi = strtok(buf, ", :");
        char *fo = strtok(NULL, ", :");
        if (!fi || !fo)
        {
            YCONN_FAILED(conn, YDB_E_INVALID_ARGS);
            SET_DISCONNECTED(conn);
            return YDB_E_INVALID_ARGS;
        }

        if (access(fi, F_OK) != 0)
        {
            if (mkfifo(fi, 0666))
            {
                YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
                SET_DISCONNECTED(conn);
                return YDB_E_SYSTEM_FAILED;
            }
        }

        if (access(fo, F_OK) != 0)
        {
            if (mkfifo(fo, 0666))
            {
                YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
                SET_DISCONNECTED(conn);
                return YDB_E_SYSTEM_FAILED;
            }
        }
        ylog_debug("fi=%s, fo=%s\n", fi, fo);
        // open(fifo_path, O_RDONLY | O_NONBLOCK);
        conn->fd = open(fi, O_RDONLY | O_NONBLOCK);
        head->send.fd = open(fo, O_RDWR);
        // head->send.fd = open(fo, O_WRONLY); // It would be pending if WRONLY mode
        if (conn->fd < 0 || head->send.fd < 0)
        {
            if (conn->fd > 0)
                close(conn->fd);
            if (head->send.fd > 0)
                close(head->send.fd);
            goto disconnected;
        }
    }
    else
    {
        FILE *fp;
        fname = &(address[strlen("file://")]);
        if (strcmp(fname, "stdout") == 0)
        {
            if (feof(stdout))
            {
                YCONN_FAILED(conn, YDB_E_STREAM_FAILED);
                return YDB_E_STREAM_FAILED;
            }
            fp = stdout;
        }
        else
            fp = fopen(fname, "w");
        if (!fp)
            goto disconnected;
        conn->fd = fileno(fp);
        if (conn->fd < 0)
        {
            if (fp)
                fclose(fp);
            goto disconnected;
        }
    }
    conn->flags = flags;
    return YDB_OK;
disconnected:
    YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
    free(head);
    conn->head = NULL;
    SET_DISCONNECTED(conn);
    return YDB_E_CONN_FAILED;
}

void yconn_file_deinit(yconn *conn)
{
    struct yconn_socket_head *head;
    if (!conn)
        return;
    head = conn->head;
    if (head)
    {
        if (IS_SET(conn->flags, YCONN_TYPE_FIFO))
        {
            if (head->send.fd > 0)
                close(head->send.fd);
        }
        if (head->recv.fp)
            fclose(head->recv.fp);
        if (head->recv.buf)
            free(head->recv.buf);
        free(head);
    }
    conn->head = NULL;
    if (strcmp(conn->address, "file://stdout") != 0 && conn->fd > 0)
        close(conn->fd);
    conn->fd = -1;
    SET_DISCONNECTED(conn);
}

static char *yconn_flag_print(yconn *conn)
{
    static char flagstr[64];
    snprintf(flagstr, sizeof(flagstr), "%s:%s:%s:%s:%s:%s:%s:%s",
             IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "pub" : "sub",
             IS_SET(conn->flags, YCONN_WRITABLE) ? "writable" : "-",
             IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsub" : "-",
             IS_SET(conn->flags, YCONN_UNREADABLE) ? "no-read" : "-",
             IS_SET(conn->flags, YCONN_MAJOR_CONN) ? "major" : "minor",
             IS_SET(conn->flags, STATUS_SERVER) ? "server" : "-",
             IS_SET(conn->flags, STATUS_CLIENT) ? "client" : "-",
             IS_SET(conn->flags, STATUS_COND_CLIENT) ? "connected" : "-");
    return flagstr;
}

static void yconn_print(yconn *conn, const char *func, int line, char *state, bool simple)
{
    int n;
    char flagstr[128];
    if (!conn)
        return;
    if (!simple && ydb_conn_log)
    {
        FILE *fp;
        char connlog[256];
        snprintf(connlog, sizeof(connlog), "/tmp/ydb.conn.%d.log", getpid());
        fp = fopen(connlog, "a");
        if (fp)
        {
            fprintf(fp, "%s:%s:ydb[%s]:epoll(%d):%s(%s,%d):%s(%s)::%s:%s:%s:%s:%s:%s:%s:%s\n",
                    ylog_datetime(),
                    ylog_pname(), conn->datablock->name, conn->datablock->epollfd,
                    conn->address, conn->name ? conn->name : "...", conn->fd, state ? state : "???",
                    (conn->error_num > 0) ? strerror(conn->error_num) : "no-err",
                    IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "pub" : "sub",
                    IS_SET(conn->flags, YCONN_WRITABLE) ? "writable" : "-",
                    IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsub" : "-",
                    IS_SET(conn->flags, YCONN_UNREADABLE) ? "no-read" : "-",
                    IS_SET(conn->flags, YCONN_MAJOR_CONN) ? "major" : "minor",
                    IS_SET(conn->flags, STATUS_SERVER) ? "server" : "-",
                    IS_SET(conn->flags, STATUS_CLIENT) ? "client" : "-",
                    IS_SET(conn->flags, STATUS_COND_CLIENT) ? "connected" : "-");
            fclose(fp);
        }
    }
    if (!ylog_logger)
        return;
    if (!YLOG_SEVERITY_INFO)
        return;
    if (!simple)
    {
        if (state)
            ylog_logger(YLOG_INFO, func, line, "ydb[%s] %s conn:\n",
                        conn->datablock ? conn->datablock->name : "...", state);
        ylog_logger(YLOG_INFO, func, line, " address: %s (peer name: %s, fd: %d)\n",
                    conn->address, conn->name ? conn->name : "...", conn->fd);
        if (conn->error_num > 0)
            ylog_logger(YLOG_INFO, func, line, " sys-err: %s\n", strerror(conn->error_num));
        if (IS_SET(conn->flags, YCONN_ROLE_PUBLISHER))
            n = sprintf(flagstr, "PUB");
        else
            n = sprintf(flagstr, "SUB");
        n += sprintf(flagstr + n, "(%s", IS_SET(conn->flags, YCONN_WRITABLE) ? "write" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsub" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_UNREADABLE) ? "no-read" : "-");
        n += sprintf(flagstr + n, "/%s) ", IS_SET(conn->flags, YCONN_MAJOR_CONN) ? "major" : "");
        ylog_logger(YLOG_INFO, func, line, " flags: %s\n", flagstr);

        n = sprintf(flagstr, "(%s", IS_SET(conn->flags, STATUS_DISCONNECT) ? "dis-conn" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, STATUS_SERVER) ? "server" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, STATUS_CLIENT) ? "client" : "-");
        n += sprintf(flagstr + n, "/%s)", IS_SET(conn->flags, STATUS_COND_CLIENT) ? "connected" : "-");
        ylog_logger(YLOG_INFO, func, line, " status: %s\n", flagstr);
        ylog_logger(YLOG_INFO, func, line,
                    " ydb(epollfd): %s(%d)\n",
                    conn->datablock->name, conn->datablock->epollfd);
        ylog_logger(YLOG_INFO, func, line,
                    " ydb(synccount): %d\n", conn->datablock->synccount);
    }
    else
    {
        ylog_logger(YLOG_INFO, func, line, "ydb[%s] conn: %s (peer name: %s, fd: %d)\n",
                    conn->datablock ? conn->datablock->name : "...",
                    conn->address, conn->name ? conn->name : "...", conn->fd);
    }
}

static unsigned int _yconn_flags(const char *address, char *flagstr)
{
    unsigned int flags = 0;
    char flagbuf[256];
    strcpy(flagbuf, flagstr);
    char *token;
    token = strtok(flagbuf, ":,.- ");
    while (token)
    {
        if (strncmp(token, "subscriber", 3) == 0) // subscriber role
        {
            UNSET_FLAG(flags, YCONN_ROLE_PUBLISHER);
        }
        else if (strncmp(token, "publisher", 3) == 0) // publisher role
        {
            SET_FLAG(flags, YCONN_ROLE_PUBLISHER);
            SET_FLAG(flags, YCONN_WRITABLE);
        }
        else if (strncmp(token, "unsubscribe", 1) == 0) // unsubscribe mode
            SET_FLAG(flags, YCONN_UNSUBSCRIBE);
        else if (strncmp(token, "writable", 1) == 0) // writable mode
            SET_FLAG(flags, YCONN_WRITABLE);
        else if (strncmp(token, "sync-before-read", 4) == 0) // sync-before-read mode
            SET_FLAG(flags, YCONN_SYNC);
        token = strtok(NULL, ":,.- ");
    }

    if (strncmp(address, "us://", strlen("us://")) == 0 ||
        strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_UNIX);
    }
    else if (strncmp(address, "file://", strlen("file://")) == 0)
    {
        flags = 0;
        SET_FLAG(flags, YCONN_TYPE_FILE);
        SET_FLAG(flags, YCONN_WRITABLE);
        SET_FLAG(flags, YCONN_UNREADABLE);
    }
    else if (strncmp(address, "tcp://", strlen("tcp://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_INET);
    }
    else if (strncmp(address, "fifo://", strlen("fifo://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_FIFO);
    }
    // else if (strncmp(address, "ws://", strlen("ws://")) == 0)
    // else if (strncmp(address, "wss://", strlen("wss://")) == 0)
    else
    {
        return 0;
    }
    return flags;
}

static yconn *_yconn_new(const char *address, unsigned int flags, ydb *datablock)
{
    yconn_func_init func_init = NULL;
    yconn_func_deinit func_deinit = NULL;
    yconn_func_recv func_recv = NULL;
    yconn_func_send func_send = NULL;
    yconn_func_accept func_accept = NULL;
    yconn *conn = NULL;

    if (IS_SET(flags, YCONN_TYPE_UNIX | YCONN_TYPE_INET))
    {
        func_init = yconn_socket_init;
        func_send = yconn_default_send;
        func_recv = yconn_default_recv;
        func_accept = yconn_socket_accept;
        func_deinit = yconn_socket_deinit;
    }
    else if (IS_SET(flags, YCONN_TYPE_FILE | YCONN_TYPE_FIFO))
    {
        func_init = yconn_file_init;
        func_send = yconn_default_send;
        func_recv = yconn_default_recv;
        func_accept = NULL;
        func_deinit = yconn_file_deinit;
    }
    else
    {
        return NULL;
    }

    SET_FLAG(flags, STATUS_DISCONNECT);
    conn = malloc(sizeof(struct _yconn));
    if (!conn)
        return NULL;
    memset(conn, 0x0, sizeof(struct _yconn));
    conn->address = ystrdup((char *)address);
    conn->flags = flags;
    conn->fd = -1;
    conn->timerfd = -1;
    conn->datablock = NULL;
    conn->func_init = func_init;
    conn->func_send = func_send;
    conn->func_recv = func_recv;
    conn->func_accept = func_accept;
    conn->func_deinit = func_deinit;
    conn->datablock = datablock;
    conn->send_timeout = datablock->timeout;
    conn->recv_timeout = datablock->timeout;
    conn->name = NULL;
    return conn;
}

static void _yconn_free(yconn *conn)
{
    if (conn)
    {
        if (conn->timerfd > 0)
            close(conn->timerfd);
        if (conn->fd > 0)
            close(conn->fd);
        if (conn->address)
            yfree(conn->address);
        if (conn->name)
            yfree(conn->name);
        free(conn);
    }
}

static void _yconn_free_with_deinit(yconn *conn)
{
    ylog_inout();
    if (conn)
    {
        if (conn->fd > 0)
            YCONN_INFO(conn, "closed");
        conn->func_deinit(conn);
        _yconn_free(conn);
    }
}

void yconn_close(yconn *conn)
{
    ylog_inout();
    if (conn)
    {
        yconn_detach_from_disconn(conn);
        yconn_detach_from_conn(conn);
        if (conn->fd > 0)
            YCONN_INFO(conn, "closed");
        conn->func_deinit(conn);
        _yconn_free(conn);
    }
}

ydb_res yconn_accept(yconn *conn)
{
    ydb_res res;
    int client_fd;
    yconn *client;
    unsigned int conn_flags;
    ylog_inout();
    conn_flags = _yconn_flags(conn->address, "sub:unsubscribe");
    client = _yconn_new(conn->address, conn_flags, conn->datablock);
    if (!client)
        return YDB_OK;
    client_fd = conn->func_accept(conn, client);
    if (client_fd < 0)
    {
        client->func_deinit(client);
        _yconn_free(client);
        if (IS_DISCONNECTED(conn))
        {
            yconn_deferred_close(conn);
            return YDB_E_CONN_FAILED;
        }
        return YDB_OK;
    }
    res = yconn_attach_to_conn(client);
    if (YDB_FAILED(res))
    {
        client->func_deinit(client);
        _yconn_free(client);
        return YDB_OK;
    }
    YCONN_INFO(client, "accepted");
    return YDB_OK;
}

yconn *yconn_get(char *address, ydb *datablock)
{
    ytree_iter *i;
    ylist_iter *j;
    if (!datablock || !address)
        return NULL;
    i = ytree_first(datablock->conn);
    for (; i; i = ytree_next(datablock->conn, i))
    {
        yconn *conn = ytree_data(i);
        if (strcmp(conn->address, address) == 0 && IS_MAJOR(conn))
            return conn;
    }

    j = ylist_first(datablock->disconn);
    for (; j; j = ylist_next(datablock->disconn, j))
    {
        yconn *conn = ylist_data(j);
        if (strcmp(conn->address, address) == 0 && IS_MAJOR(conn))
            return conn;
    }
    return NULL;
}

ylist *yconn_getall(char *address, ydb *datablock)
{
    ylist *yconns;
    ytree_iter *i;
    ylist_iter *j;
    if (!datablock || !address)
        return NULL;
    yconns = ylist_create();
    if (!yconns)
        return NULL;

    i = ytree_first(datablock->conn);
    for (; i; i = ytree_next(datablock->conn, i))
    {
        yconn *conn = ytree_data(i);
        if (strcmp(conn->address, address) == 0)
        {
            if (IS_MAJOR(conn))
                ylist_push_front(yconns, conn);
            else
                ylist_push_back(yconns, conn);
        }
    }

    j = ylist_first(datablock->disconn);
    for (; j; j = ylist_next(datablock->disconn, j))
    {
        yconn *conn = ylist_data(j);
        if (strcmp(conn->address, address) == 0)
        {
            if (IS_MAJOR(conn))
                ylist_push_front(yconns, conn);
            else
                ylist_push_back(yconns, conn);
        }
    }
    return yconns;
}

// detach from conn and then attach to disconn
void yconn_deferred_close(yconn *conn)
{
    YCONN_INFO(conn, "disconnected");
    yconn_detach_from_conn(conn);
    conn->func_deinit(conn);
    yconn_attach_to_disconn(conn);
}

ydb_res yconn_open(char *addr, char *flags, ydb *datablock)
{
    ydb_res res;
    yconn *conn = NULL;
    unsigned int conn_flags = 0;
    ylog_inout();
    conn = yconn_get(addr, datablock);
    if (conn)
        return YDB_E_ENTRY_EXISTS;
    conn_flags = _yconn_flags(addr, flags);
    if (!conn_flags)
        return YDB_E_INVALID_ARGS;
    SET_FLAG(conn_flags, YCONN_MAJOR_CONN);
    conn = _yconn_new(addr, conn_flags, datablock);
    if (!conn)
        return YDB_E_MEM_ALLOC;
    res = conn->func_init(conn);
    if (YDB_FAILED(res))
    {
        conn->func_deinit(conn);
        res = yconn_attach_to_disconn(conn);
        YDB_ASSERT(YDB_FAILED(res), res);
        return res;
    }
    res = yconn_attach_to_conn(conn);
    if (YDB_FAILED(res))
    {
        conn->func_deinit(conn);
        _yconn_free(conn);
        return res;
    }
    YCONN_INFO(conn, "opened");
    if (IS_SET(conn->flags, STATUS_CLIENT))
    {
        eventid eid = yconn_init(conn);
        if (valid_waitevent(eid))
            yconn_serve_blocking(datablock, eid, datablock->timeout);
    }
    return res;
}

ydb_res yconn_reopen_or_close(yconn *conn, ydb *datablock)
{
    int len;
    ydb_res res;
    uint64_t expired;
    ylog_inout();

    if (!IS_SET(conn->flags, YCONN_MAJOR_CONN))
    {
        yconn_close(conn);
        return YDB_OK;
    }

    if (conn->timerfd > 0)
    {
        len = read(conn->timerfd, &expired, sizeof(uint64_t));
        if (len != sizeof(uint64_t))
        {
            if (errno == EAGAIN)
                return YDB_OK;
            YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
            return YDB_E_SYSTEM_FAILED;
        }
        ylog_debug("timerfd %d expired (%d)\n", conn->timerfd, expired);
    }

    conn->func_deinit(conn);
    res = conn->func_init(conn);
    if (YDB_FAILED(res))
    {
        if (IS_SET(conn->flags, YCONN_MAJOR_CONN))
        {
            res = yconn_attach_to_disconn(conn);
            YDB_ASSERT(YDB_FAILED(res), res);
            return res;
        }
        conn->func_deinit(conn);
        _yconn_free(conn);
        return YDB_OK;
    }
    yconn_detach_from_disconn(conn);
    res = yconn_attach_to_conn(conn);
    if (YDB_FAILED(res))
    {
        conn->func_deinit(conn);
        _yconn_free(conn);
        return res;
    }
    YCONN_INFO(conn, "reopened");
    if (IS_SET(conn->flags, STATUS_CLIENT))
    {
        eventid eid = yconn_init(conn);
        if (valid_waitevent(eid))
            yconn_serve_blocking(datablock, eid, datablock->timeout);
    }
    return res;
}

static ydb_res yconn_detach_from_conn(yconn *conn)
{
    ydb_res res;
    yconn *found;
    ydb *datablock;
    if (!conn || !conn->datablock)
    {
        YCONN_FAILED(conn, YDB_E_NO_CONN);
        return YDB_E_NO_CONN;
    }
    datablock = conn->datablock;
    if (conn->fd <= 0)
        return YDB_OK;
    found = ytree_delete(datablock->conn, &conn->fd);
    YDB_ASSERT(found && found != conn, YDB_E_PERSISTENCY_ERR);
    if (!found)
        return YDB_OK;
    if (IS_SET(conn->flags, YCONN_SYNC))
        datablock->synccount--;
    res = ydb_epoll_detach(datablock, conn, conn->fd);
    if (YDB_FAILED(res))
    {
        YCONN_FAILED(conn, res);
        return res;
    }
    return YDB_OK;
}

static ydb_res yconn_attach_to_conn(yconn *conn)
{
    ydb_res res;
    yconn *found;
    ydb *datablock;
    if (!conn || !conn->datablock)
    {
        YCONN_FAILED(conn, YDB_E_NO_CONN);
        return YDB_E_NO_CONN;
    }
    datablock = conn->datablock;
    YDB_ASSERT(conn->fd <= 0, YDB_E_CTRL);
    found = ytree_search(datablock->conn, &conn->fd);
    YDB_ASSERT(found && found != conn, YDB_E_PERSISTENCY_ERR);
    if (found)
        return YDB_OK;
    res = ydb_epoll_attach(datablock, conn, conn->fd);
    if (YDB_FAILED(res))
    {
        YCONN_FAILED(conn, res);
        return res;
    }
    ytree_insert(datablock->conn, &conn->fd, conn);
    if (IS_SET(conn->flags, YCONN_SYNC))
        datablock->synccount++;
    conn->error_num = 0;
    return YDB_OK;
}

static ydb_res yconn_detach_from_disconn(yconn *conn)
{
    ydb_res res = YDB_OK;
    ydb *datablock;
    if (!conn || !conn->datablock)
    {
        YCONN_FAILED(conn, YDB_E_NO_CONN);
        return YDB_E_NO_CONN;
    }
    datablock = conn->datablock;
    if (conn->iter)
        ylist_erase(datablock->disconn, conn->iter, NULL);
    conn->iter = NULL;
    if (conn->timerfd > 0)
    {
        res = ydb_epoll_detach(datablock, conn, conn->timerfd);
        if (YDB_FAILED(res))
        {
            YCONN_FAILED(conn, res);
        }
        close(conn->timerfd);
        conn->timerfd = -1;
    }
    return res;
}

static ydb_res yconn_attach_to_disconn(yconn *conn)
{
    ydb_res res;
    ydb *datablock;
    int timerfd, ret;
    struct itimerspec timespec;
    if (!conn || !conn->datablock)
    {
        YCONN_FAILED(conn, YDB_E_NO_CONN);
        return YDB_E_NO_CONN;
    }
    datablock = conn->datablock;
    if (conn->iter || conn->timerfd > 0)
        return YDB_OK;
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0)
    {
        YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
        return YDB_E_SYSTEM_FAILED;
    }
    if (IS_SET(conn->flags, YCONN_MAJOR_CONN) && datablock->timeout > 0)
    {
        // Waiting time for reconnection.
        timespec.it_value.tv_sec = datablock->timeout / 1000;
        timespec.it_value.tv_nsec = (datablock->timeout % 1000) * 10e5;
        timespec.it_interval.tv_sec = datablock->timeout / 1000;
        timespec.it_interval.tv_nsec = (datablock->timeout % 1000) * 10e5;
    }
    else
    {
        timespec.it_value.tv_sec = 0;
        timespec.it_value.tv_nsec = 1;
        // timespec.it_value.tv_nsec = 1 * 10e5;
        timespec.it_interval.tv_sec = 0;
        timespec.it_interval.tv_nsec = 0;
    }
    // fprintf(stdout, "timespec.it_value.tv_sec = %ld\n", timespec.it_value.tv_sec);
    // fprintf(stdout, "timespec.it_value.tv_nsec = %ld\n", timespec.it_value.tv_nsec);
    // fprintf(stdout, "timespec.it_interval.tv_sec = %ld\n", timespec.it_interval.tv_sec);
    // fprintf(stdout, "timespec.it_interval.tv_nsec = %ld\n", timespec.it_interval.tv_nsec);
    ret = timerfd_settime(timerfd, 0x0, &timespec, NULL);
    if (ret < 0)
    {
        YCONN_FAILED(conn, YDB_E_SYSTEM_FAILED);
        close(timerfd);
        return YDB_E_SYSTEM_FAILED;
    }
    if (!conn->iter)
    {
        conn->iter = ylist_push_back(datablock->disconn, conn);
        if (!conn->iter)
        {
            YCONN_FAILED(conn, YDB_E_CTRL);
            close(timerfd);
            return YDB_E_CTRL;
        }
    }
    res = ydb_epoll_attach(datablock, conn, timerfd);
    if (YDB_FAILED(res))
    {
        YCONN_FAILED(conn, res);
        close(timerfd);
        return res;
    }
    conn->timerfd = timerfd;
    return YDB_OK;
}

ydb_res yconn_response(yconn *req_conn, yconn_op op, unsigned int respseq, bool done, bool ok, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ymsg_type msgtype;
    unsigned int curseq;
    ylog_inout();
    if (!req_conn)
        return YDB_E_INVALID_ARGS;
    YCONN_SIMPLE_INFO(req_conn);
    if (done)
    {
        if (ok)
            msgtype = YMSG_RESPONSE;
        else
            msgtype = YMSG_RESP_FAILED;
    }
    else
        msgtype = YMSG_RESP_CONTINUED;
    YDB_ASSERT(!req_conn->func_send, YDB_E_FUNC);
    curseq = req_conn->sendseq;
    req_conn->sendseq = respseq;
    res = req_conn->func_send(req_conn, op, msgtype, buf, buflen);
    req_conn->sendseq = curseq;
    if (res)
        yconn_deferred_close(req_conn);
    return res;
}

eventid yconn_request(yconn *req_conn, yconn_op op, int timeout, char *buf, size_t buflen, eventid peid)
{
    ydb_res res = YDB_OK;
    eventid eid = {.fd = -1, .seq = 0};
    ylog_inout();
    if (!req_conn)
        return eid;
    YCONN_SIMPLE_INFO(req_conn);
    req_conn->sendseq++;
    if ((timeout - YDB_DELIVERY_LATENCY) > 0)
        req_conn->send_timeout = timeout - YDB_DELIVERY_LATENCY;
    else
        req_conn->send_timeout = timeout;
    YDB_ASSERT(!req_conn->func_send, YDB_E_FUNC);
    res = req_conn->func_send(req_conn, op, YMSG_REQUEST, buf, buflen);
    if (res)
    {
        yconn_deferred_close(req_conn);
        return eid;
    }
    return waitevent_set_event(req_conn->datablock, req_conn->fd, req_conn->sendseq, req_conn->send_timeout, peid);
}

ydb_res yconn_publish(yconn *recv_conn, yconn *req_conn, ydb *datablock, yconn_op op, char *buf, size_t buflen)
{
    yconn *conn;
    ylist *publist = NULL;
    ytree_iter *iter;
    ylog_inout();
    if (op == YOP_SYNC)
        return YDB_E_INVALID_MSG;
    if (op == YOP_MERGE || op == YOP_DELETE)
    {
        if (buf == NULL || buflen <= 0)
        {
            if (datablock)
                ylog_info("ydb[%s] no data to publish.\n", datablock->name);
            else if (recv_conn)
                ylog_info("ydb[%s] no data to publish.\n", recv_conn->datablock->name);
            return YDB_OK;
        }
    }
    publist = ylist_create();
    if (!publist)
        return YDB_E_MEM_ALLOC;
    ylog_in();
    if (datablock)
    {
        iter = ytree_first(datablock->conn);
        for (; iter != NULL; iter = ytree_next(datablock->conn, iter))
        {
            conn = ytree_data(iter);
            if (conn == recv_conn || conn == req_conn)
                continue;
            else if (IS_SET(conn->flags, STATUS_SERVER | STATUS_DISCONNECT))
                continue;
            else if (IS_SET(conn->flags, STATUS_CLIENT))
            {
                if (!IS_SET(conn->flags, YCONN_WRITABLE))
                    continue;
            }
            else if (IS_SET(conn->flags, STATUS_COND_CLIENT))
            {
                if (IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                    continue;
            }
            ylist_push_back(publist, conn);
        }
    }
    else
    {
        ylist_push_back(publist, recv_conn);
    }
    ylog_info("ydb[%s] publish num: %d\n",
              datablock ? datablock->name : "...", ylist_size(publist));
    conn = ylist_pop_front(publist);
    while (conn)
    {
        ydb_res res;
        YCONN_SIMPLE_INFO(conn);
        YDB_ASSERT(!conn->func_send, YDB_E_FUNC);
        conn->sendseq++;
        res = conn->func_send(conn, op, YMSG_PUBLISH, buf, buflen);
        if (res)
            yconn_deferred_close(conn);
        conn = ylist_pop_front(publist);
    }
    ylog_out();
    ylist_destroy(publist);
    return YDB_OK;
}

ydb_res yconn_whisper(int origin, ydb *datablock, yconn_op op, char *buf, size_t buflen)
{
    ydb_res res;
    yconn *tar_conn;
    ylog_inout();
    if (!buf || !datablock)
        return YDB_E_INVALID_ARGS;
    if (op != YOP_MERGE && op != YOP_DELETE)
        return YDB_E_INVALID_MSG;
    if (buf == NULL || buflen <= 0)
    {
        ylog_info("ydb[%s] no data to whisper.\n", datablock->name);
        return YDB_OK;
    }
    tar_conn = ytree_search(datablock->conn, &origin);
    if (!tar_conn)
    {
        ylog_info("ydb[%s] no origin to whisper.\n", datablock->name);
        return YDB_E_NO_CONN;
    }

    ylog_in();
    YCONN_SIMPLE_INFO(tar_conn);
    tar_conn->sendseq++;
    res = tar_conn->func_send(tar_conn, op, YMSG_WHISPER, buf, buflen);
    if (res)
        yconn_deferred_close(tar_conn);
    ylog_out();
    return YDB_OK;
}

static char *yconn_remove_head_tail(char *buf, size_t buflen, size_t *outbuflen)
{
    // removed the head from buf.
    char *rbuf;
    size_t rbuflen;
    if (!buf || buflen <= 0)
    {
        *outbuflen = buflen;
        return buf;
    }
    rbuf = strstr(buf, YMSG_HEAD_DELIMITER);
    if (!rbuf)
    {
        *outbuflen = buflen;
        return buf;
    }
    rbuf = rbuf + YMSG_HEAD_DELIMITER_LEN;
    rbuflen = buflen - (rbuf - buf);

    if (rbuflen > YMSG_END_DELIMITER_LEN)
    {
        if (strncmp(&rbuf[rbuflen - YMSG_END_DELIMITER_LEN],
                    YMSG_END_DELIMITER, YMSG_END_DELIMITER_LEN) == 0)
        {
            rbuflen = rbuflen - YMSG_END_DELIMITER_LEN;
            // rbuf[rbuflen] = 0; // only change rbuflen;
        }
    }
    *outbuflen = rbuflen;
    return rbuf;
}

eventid yconn_sync(yconn *req_conn, ydb *datablock, bool forced, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ytree_iter *conni;
    char *rbuf = buf;
    size_t rbuflen = buflen;
    eventid eid = {.fd = -1, .seq = 0};
    ylist *synclist = NULL;
    yconn *conn;
    int timeout;

    ylog_in();
    YDB_FAIL(datablock == NULL, YDB_E_CTRL);
    // ydb channel is not opened.
    if (datablock->epollfd < 0)
        goto failed;
    synclist = ylist_create();

    conni = ytree_first(datablock->conn);
    for (; conni != NULL; conni = ytree_next(datablock->conn, conni))
    {
        conn = ytree_data(conni);
        if (conn == req_conn)
            continue;
        if (IS_SET(conn->flags, (STATUS_DISCONNECT | STATUS_SERVER | YCONN_UNREADABLE)))
            continue;
        else if (IS_SET(conn->flags, STATUS_CLIENT))
        {
            if (!forced && !IS_SET(conn->flags, YCONN_SYNC))
                continue;
        }
        else if (IS_SET(conn->flags, STATUS_COND_CLIENT))
        {
            if (!IS_SET(conn->flags, YCONN_WRITABLE))
                continue;
        }
        ylist_push_back(synclist, conn);
    }
    if (ylist_empty(synclist))
        goto failed;

    if (req_conn)
    {
        // removed the head from buf. (for relay)
        rbuf = yconn_remove_head_tail(buf, buflen, &rbuflen);
        timeout = req_conn->recv_timeout;
        eid = waitevent_set_event(datablock, req_conn->fd, req_conn->recvseq, timeout, eid);
    }
    else
    {
        // local sync request
        timeout = datablock->timeout;
        eid = waitevent_set_rootevent(datablock, timeout);
    }
    conn = ylist_pop_front(synclist);
    while (conn)
    {
        YCONN_SIMPLE_INFO(conn);
        yconn_request(conn, YOP_SYNC, timeout, rbuf, rbuflen, eid);
        conn = ylist_pop_front(synclist);
    }
failed:
    ylist_destroy(synclist);
    ylog_out();
    return eid;
}

eventid yconn_init(yconn *req_conn)
{
    ydb_res res = YDB_OK;
    ydb *datablock;
    char *buf = NULL;
    size_t buflen = 0;
    eventid eid = {.fd = -1, .seq = 0};
    datablock = req_conn->datablock;

    ylog_in();
    YDB_FAIL(datablock == NULL, YDB_E_CTRL);
    YDB_FAIL(datablock->epollfd < 0, YDB_E_CTRL);
    // dump the datablock to send.
    if (IS_SET(req_conn->flags, YCONN_WRITABLE) && !ydb_empty(datablock->top))
        ydb_dumps(req_conn->datablock, &buf, &buflen);
    // send
    if (IS_SET(req_conn->flags, YCONN_UNSUBSCRIBE))
    {
        yconn_publish(req_conn, NULL, NULL, YOP_INIT, buf, buflen);
    }
    else // subscribe the YDB data change
    {
        // set local event for waiting
        eid = waitevent_set_rootevent(datablock, datablock->timeout);
        yconn_request(req_conn, YOP_INIT, datablock->timeout, buf, buflen, eid);
        YDB_FAIL(invalid_waitevent(eid), YDB_E_EVENT);
        ylog_debug("waitevent e(%d:%d), res = %s\n", eid.fd, eid.seq, ydb_res_str(res));
    }
    CLEAR_BUF(buf, buflen);
failed:
    ylog_out();
    return eid;
}

ydb_res yconn_merge(yconn *recv_conn, yconn *req_conn, bool not_publish, char *buf, size_t buflen)
{
    ydb_res res;
    ynode *src = NULL;
    ylog_in();
    res = ynode_scanf_from_buf(buf, buflen, recv_conn->fd, &src);
    if (res)
    {
        ynode_remove(src);
        ylog_out();
        return res;
    }
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        char *logbuf = NULL;
        size_t logbuflen = 0;
        YCONN_SIMPLE_INFO(recv_conn);
        log = ydb_log_open(recv_conn->datablock, NULL);
        top = ynode_merge(recv_conn->datablock->top, src, log);
        ydb_log_close(recv_conn->datablock, log, &logbuf, &logbuflen);
        ynode_remove(src);
        if (top)
        {
            recv_conn->datablock->top = top;
            if (!not_publish)
                yconn_publish(recv_conn, req_conn, recv_conn->datablock, YOP_MERGE, logbuf, logbuflen);
        }
        else
            res = YDB_E_MERGE_FAILED;
        CLEAR_BUF(logbuf, logbuflen);
    }
    ylog_out();
    return res;
}

// delete ydb using the input string
ydb_res yconn_delete(yconn *recv_conn, yconn *req_conn, bool not_publish, char *buf, size_t buflen)
{
    ydb_res res;
    ynode *src = NULL;
    unsigned int flags;
    struct ydb_delete_data ddata;
    ylog_in();
    res = ynode_scanf_from_buf(buf, buflen, recv_conn->fd, &src);
    if (res)
    {
        ynode_remove(src);
        ylog_out();
        return res;
    }
    if (src)
    {
        char *logbuf = NULL;
        size_t logbuflen = 0;
        ddata.log = ydb_log_open(recv_conn->datablock, NULL);
        ddata.node = recv_conn->datablock->top;
        YCONN_SIMPLE_INFO(recv_conn);
        flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY; // YNODE_VAL_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, &ddata, flags);
        ydb_log_close(recv_conn->datablock, ddata.log, &logbuf, &logbuflen);
        ynode_remove(src);
        if (!res)
        {
            if (!not_publish)
                yconn_publish(recv_conn, req_conn, recv_conn->datablock, YOP_DELETE, logbuf, logbuflen);
        }
        CLEAR_BUF(logbuf, logbuflen);
    }
    ylog_out();
    return res;
}

ydb_res yconn_sync_local(yconn *req_conn, char *inbuf, size_t inbuflen, char **outbuf, size_t *outbuflen)
{
    ydb_res res;
    ydb *datablock;
    ynode *src = NULL;
    ylog_in();
    datablock = req_conn->datablock;
    res = ynode_scanf_from_buf(inbuf, inbuflen, req_conn->fd, &src);
    if (res)
    {
        ynode_remove(src);
        ylog_out();
        return res;
    }
    if (!src)
        src = ynode_top(ynode_create_path("/", NULL, NULL));
    if (src)
    {
        char *buf = NULL;
        size_t buflen = 0;
        ynode_log *log = NULL;
        struct ydb_fprintf_data data;
        // ynode_dump(src, 0, 24);
        if (ytrie_size(datablock->updater) > 0)
            ydb_update(req_conn, datablock, src);
        log = ydb_log_open(datablock, NULL);
        data.log = log;
        data.datablock = datablock;
        data.num_of_nodes = 0;
        data.origin = 0; // getting mine
        ynode_traverse(src, ydb_fprintf_sub, &data, YNODE_LEAF_ONLY);
        ydb_log_close(datablock, log, &buf, &buflen);
        if (data.num_of_nodes <= 0 || !buf || buflen <= 0)
        {
            CLEAR_BUF(buf, buflen);
        }
        *outbuf = buf;
        *outbuflen = buflen;
        ynode_remove(src);
    }
    ylog_out();
    return res;
}

static int yconn_whisper_process(yconn *recv_conn, char *inbuf, size_t inbuflen, char **outbuf, size_t *outbuflen)
{
    ynode *target;
    char *start;
    char path[512];
    ydb *datablock;
    *outbuf = inbuf;
    *outbuflen = inbuflen;
    datablock = recv_conn->datablock;
    if (!datablock)
        return -1;

    path[0] = 0;
    start = strstr(inbuf, YMSG_WHISPER_DELIMITER);
    if (start)
    {
        int target_origin, recv_origin;
        sscanf(start, YMSG_WHISPER_DELIMITER " %s\n", path);
        target = ynode_search(datablock->top, path);
        if (!target)
            return -1;
        target_origin = ynode_origin(target);
        recv_origin = recv_conn->fd;
        ylog_info("%s (recv_origin %d, target_origin %d)\n", path, recv_origin, target_origin);
        if (target_origin == 0)
        {
            start = strchr(start + YMSG_WHISPER_DELIMITER_LEN, '\n');
            if (!start)
                return -1;
            *outbuflen = inbuflen - (start - inbuf);
            *outbuf = start;
        }
        else
        {
            *outbuf = yconn_remove_head_tail(inbuf, inbuflen, outbuflen);
        }
        return target_origin;
    }
    return -1;
}

eventid yconn_recv(yconn *recv_conn, yconn_op *op, ymsg_type *type, int *next)
{
    ydb_res res;
    char *buf = NULL;
    size_t buflen = 0;
    unsigned int flags = 0x0;
    unsigned int recvseq = 0;
    eventid eid = {.fd = -1, .seq = 0};
    eventid reqid = {.fd = -1, .seq = 0};
    yconn *req_conn = NULL;
    *next = 0;
    ylog_in();
    if (IS_DISCONNECTED(recv_conn))
        goto _done;

    YCONN_SIMPLE_INFO(recv_conn);
    YDB_ASSERT(!recv_conn->func_recv, YDB_E_FUNC);
    res = recv_conn->func_recv(recv_conn, op, type, &flags, &buf, &buflen, next);
    if (res)
    {
        yconn_deferred_close(recv_conn);
        goto _done;
    }
    eid.fd = recv_conn->fd;
    eid.seq = recv_conn->recvseq;
    recvseq = recv_conn->recvseq;
    ylog_debug("received e(%d:%d)\n", eid.fd, eid.seq);
    switch (*type)
    {
    case YMSG_PUBLISH:
        switch (*op)
        {
        case YOP_MERGE:
            yconn_merge(recv_conn, NULL, false, buf, buflen);
            break;
        case YOP_DELETE:
            yconn_delete(recv_conn, NULL, false, buf, buflen);
            break;
        case YOP_INIT:
            if (IS_SET(recv_conn->flags, STATUS_COND_CLIENT))
            {
                // updated flags
                recv_conn->flags = flags | (recv_conn->flags & (YCONN_TYPE_MASK | STATUS_MASK));
                YCONN_INFO(recv_conn, "updated");
                yconn_merge(recv_conn, NULL, false, buf, buflen);
            }
        default:
            break;
        }
        break;
    case YMSG_REQUEST:
        switch (*op)
        {
        case YOP_MERGE:
            res = yconn_merge(recv_conn, NULL, false, buf, buflen);
            yconn_response(recv_conn, YOP_MERGE, recvseq, true, YDB_FAILED(res) ? false : true, NULL, 0);
            break;
        case YOP_DELETE:
            res = yconn_delete(recv_conn, NULL, false, buf, buflen);
            yconn_response(recv_conn, YOP_DELETE, recvseq, true, YDB_FAILED(res) ? false : true, NULL, 0);
            break;
        case YOP_SYNC:
        {
            bool done = true;
            char *rbuf = NULL;
            size_t rbuflen = 0;
            eid = yconn_sync(recv_conn, recv_conn->datablock, false, buf, buflen);
            if (valid_waitevent(eid))
                done = false;
            res = yconn_sync_local(recv_conn, buf, buflen, &rbuf, &rbuflen);
            yconn_response(recv_conn, YOP_SYNC, recvseq, done, YDB_FAILED(res) ? false : true, rbuf, rbuflen);
            CLEAR_BUF(rbuf, rbuflen);
            break;
        }
        case YOP_INIT:
            if (IS_SET(recv_conn->flags, STATUS_COND_CLIENT))
            {
                // updated flags
                recv_conn->flags = flags | (recv_conn->flags & (YCONN_TYPE_MASK | STATUS_MASK));
                YCONN_INFO(recv_conn, "updated");
                res = yconn_merge(recv_conn, NULL, false, buf, buflen);
                if (IS_SET(recv_conn->flags, YCONN_UNSUBSCRIBE))
                {
                    yconn_response(recv_conn, YOP_INIT, recvseq, true, YDB_FAILED(res) ? false : true, NULL, 0);
                }
                else
                {
                    char *ibuf = NULL;
                    size_t ibuflen = 0;
                    ydb_dumps(recv_conn->datablock, &ibuf, &ibuflen);
                    yconn_response(recv_conn, YOP_INIT, recvseq, true, YDB_FAILED(res) ? false : true, ibuf, ibuflen);
                    CLEAR_BUF(ibuf, ibuflen);
                }
            }
            break;
        default:
            break;
        }
        break;
    case YMSG_RESP_CONTINUED:
        // The response is not complete.
        reqid = waitevent_get_relay(recv_conn->datablock, eid, &req_conn);
        if (*op == YOP_DELETE)
            res = yconn_delete(recv_conn, req_conn, false, buf, buflen);
        else if (*op == YOP_INIT || *op == YOP_MERGE || *op == YOP_SYNC)
            res = yconn_merge(recv_conn, req_conn, false, buf, buflen);
        if (req_conn)
        {
            char *rbuf;
            size_t rbuflen = 0;
            ylog_info("ydb[%s] relay response from %s(%d) to %s(%d)\n",
                      req_conn->datablock->name, 
                      recv_conn->name?recv_conn->name:"...", recv_conn->fd,
                      req_conn->name?req_conn->name:"...", req_conn->fd);
            rbuf = yconn_remove_head_tail(buf, buflen, &rbuflen);
            yconn_response(req_conn, *op, reqid.seq, false, YDB_FAILED(res) ? false : true, rbuf, rbuflen);
        }
        eid.fd = -1;
        eid.seq = 0;
        break;
    case YMSG_RESPONSE:
        reqid = waitevent_get_relay(recv_conn->datablock, eid, &req_conn);
        eid = waitevent_complete(recv_conn->datablock, eid);
        if (*op == YOP_DELETE)
            res = yconn_delete(recv_conn, req_conn, false, buf, buflen);
        else if (*op == YOP_INIT || *op == YOP_MERGE || *op == YOP_SYNC)
            res = yconn_merge(recv_conn, req_conn, false, buf, buflen);
        if (req_conn)
        {
            char *rbuf;
            size_t rbuflen = 0;
            bool done = false;
            if (is_equal_waitevent(reqid, eid))
                done = true;
            ylog_info("ydb[%s] relay response from %s(%d) to %s(%d)\n",
                      req_conn->datablock->name, 
                      recv_conn->name?recv_conn->name:"...", recv_conn->fd,
                      req_conn->name?req_conn->name:"...", req_conn->fd);
            rbuf = yconn_remove_head_tail(buf, buflen, &rbuflen);
            yconn_response(req_conn, *op, reqid.seq, done, YDB_FAILED(res) ? false : true, rbuf, rbuflen);
        }
        break;
    case YMSG_RESP_FAILED:
        // complete event but, does not update datablock.
        reqid = waitevent_get_relay(recv_conn->datablock, eid, &req_conn);
        eid = waitevent_complete(recv_conn->datablock, eid);
        if (req_conn)
        {
            bool done = false;
            ylog_info("ydb[%s] relay response from %s(%d) to %s(%d)\n",
                      req_conn->datablock->name, 
                      recv_conn->name?recv_conn->name:"...", recv_conn->fd,
                      req_conn->name?req_conn->name:"...", req_conn->fd);
            if (is_equal_waitevent(reqid, eid))
                done = true;
            yconn_response(req_conn, *op, reqid.seq, done, false, NULL, 0);
        }
        break;
    case YMSG_WHISPER:
    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        int origin_to_relay;
        origin_to_relay = yconn_whisper_process(recv_conn, buf, buflen, &rbuf, &rbuflen);
        ylog_info("ydb[%s] origin_to_relay %d\n",
                  recv_conn->datablock->name, origin_to_relay);
        if (origin_to_relay < 0)
            break;
        if (origin_to_relay > 0)
        {
            if (recv_conn->fd != origin_to_relay)
                yconn_whisper(origin_to_relay, recv_conn->datablock, *op, rbuf, rbuflen);
        }
        else
        {
            switch (*op)
            {
            case YOP_MERGE:
                yconn_merge(recv_conn, NULL, true, rbuf, rbuflen);
                break;
            case YOP_DELETE:
                yconn_delete(recv_conn, NULL, true, rbuf, rbuflen);
                break;
            default:
                break;
            }
        }
        break;
    }
    default:
        break;
    }
_done:
    ylog_out();
    return eid;
}

ydb_res yconn_serve(ydb *datablock, int timeout)
{
    ydb_res res;
    int i, n;
    struct epoll_event event[YDB_CONN_MAX];
    ylog_inout();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    lock(datablock);
    YDB_FAIL(datablock->epollfd < 0, YDB_E_NO_CONN);
    res = YDB_OK;
    n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
    if (n < 0)
    {
        if (errno == EINTR)
            goto failed; // no error
        YDB_FAIL(n < 0, YDB_E_SYSTEM_FAILED);
    }
    if (n > 0)
        ylog_debug("ydb[%s] %d events received\n", datablock->name, n);
    for (i = 0; i < n; i++)
    {
        yconn *conn = event[i].data.ptr;
        if (event[i].data.ptr == NULL)
        {
            if (ytimer_serve(datablock->timer) < 0)
                res = YDB_E_CTRL;
        }
        else if (IS_DISCONNECTED(conn))
            yconn_reopen_or_close(conn, datablock);
        else if (IS_SERVER(conn))
            yconn_accept(conn);
        else
        {
            int next = 0;
            yconn_op op = YOP_NONE;
            ymsg_type type = YMSG_NONE;
        recv_again:
            yconn_recv(conn, &op, &type, &next);
            if (next)
                goto recv_again;
            // sleep(100);
        }
        if (res)
            break;
    }
failed:
    unlock(datablock);
    return res;
}

ydb_res ydb_serve(ydb *datablock, int timeout)
{
    return yconn_serve(datablock, timeout);
}

ydb_res yconn_serve_blocking(ydb *datablock, eventid eid, int timeout)
{
    int i, n, blockingtime;
    ydb_res res = YDB_NO_ERR;
    struct epoll_event event[YDB_CONN_MAX];
    struct timespec base;
    bool done = false;
    ylog_inout();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    YDB_FAIL(datablock->epollfd < 0, YDB_E_NO_CONN);
    ydb_time_set_base(&base);
    blockingtime = timeout;
    do
    {
        int elapsed = ydb_time_get_elapsed(&base);
        elapsed = blockingtime - elapsed;
        if (elapsed > blockingtime || elapsed < 0)
        {
            res = YDB_W_TIMEOUT;
            done = true;
            break;
        }

        n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, elapsed);
        if (n < 0)
        {
            if (errno == EINTR)
                goto failed; // no error
            YDB_FAIL(n < 0, YDB_E_SYSTEM_FAILED);
        }
        if (n > 0)
            ylog_debug("ydb[%s] %d events received\n", datablock->name, n);
        for (i = 0; i < n; i++)
        {
            yconn *conn = event[i].data.ptr;
            if (event[i].data.ptr == NULL)
            {
                waitevent *we;
                ytimer_serve(datablock->timer);
                we = waitevent_search(datablock, eid);
                if (we == NULL)
                {
                    res = YDB_W_TIMEOUT;
                    break;
                }
            }
            else if (IS_DISCONNECTED(conn))
            {
                res = yconn_reopen_or_close(conn, datablock);
            }
            else if (IS_SERVER(conn))
            {
                res = yconn_accept(conn);
            }
            else
            {
                int next = 0;
                yconn_op op = YOP_NONE;
                ymsg_type type = YMSG_NONE;
                eventid reid;
            recv_again:
                reid = yconn_recv(conn, &op, &type, &next);
                if (is_equal_waitevent(eid, reid))
                {
                    res = YDB_NO_ERR;
                    done = true;
                    break;
                }
                if (next)
                    goto recv_again;
            }
            if (res)
                break;
        }
    } while (!done);
failed:
    return res;
}

int ydb_fd(ydb *datablock)
{
    if (datablock)
        return datablock->epollfd;
    return -1;
}

ydb_res ydb_write_hook_add(ydb *datablock, char *path, int suppressed, ydb_write_hook func, int num, ...)
{
    ydb_res res = YDB_OK;
    ynode *cur;
    void *user[5] = {0};
    unsigned int flags = 0x0;

    ylog_in();
    YDB_FAIL(!datablock || !func || num < 0, YDB_E_INVALID_ARGS);
    YDB_FAIL(num > 4 || num < 0, YDB_E_INVALID_ARGS);

    if (suppressed)
        SET_FLAG(flags, YNODE_SUPPRESS_HOOK);

    if (path)
    {
        lock(datablock);
        cur = ynode_search(datablock->top, path);
        if (!cur)
        {
            char *rbuf = NULL;
            size_t rbuflen = 0;
            ynode_log *log = NULL;
            ynode *src = NULL;
            log = ydb_log_open(datablock, NULL);
            src = ynode_create_path(path, datablock->top, log);
            ydb_log_close(datablock, log, &rbuf, &rbuflen);
            if (rbuf)
            {
                if (src)
                    yconn_publish(NULL, NULL, datablock, YOP_MERGE, rbuf, rbuflen);
                free(rbuf);
            }
            cur = ynode_search(datablock->top, path);
            YDB_FAIL(!cur, YDB_E_NO_ENTRY);
        }
    }
    else
        cur = datablock->top;

    user[0] = datablock;
    num++;
    {
        int i;
        va_list ap;
        va_start(ap, num);
        ylog_debug("user total = %d\n", num);
        ylog_debug("user[0]=%p\n", user[0]);
        for (i = 1; i < num; i++)
        {
            void *p = va_arg(ap, void *);
            user[i] = p;
            ylog_debug("user[%d]=%p\n", i, user[i]);
        }
        va_end(ap);
    }
    res = yhook_register(cur, flags, (yhook_func)func, num, user);
    YDB_FAIL(res, YDB_E_HOOK_ADD);
failed:
    unlock(datablock);
    ylog_out();
    return res;
}

void ydb_write_hook_delete(ydb *datablock, char *path)
{
    ynode *cur;
    if (!datablock)
        return;
    lock(datablock);
    if (path)
    {
        cur = ynode_search(datablock->top, path);
        if (!cur)
            goto failed;
    }
    else
    {
        cur = datablock->top;
    }
    yhook_unregister(cur);
failed:
    unlock(datablock);
}

// set the timeout of the YDB synchonization
ydb_res ydb_timeout(ydb *datablock, int msec)
{
    if (datablock)
    {
        lock(datablock);
        datablock->timeout = msec;
        ylog_debug("set timeout %d\n", datablock->timeout);
        unlock(datablock);
        return YDB_OK;
    }
    return YDB_E_INVALID_ARGS;
}

// synchornize the remote ydb manually.
ydb_res ydb_sync(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ydb_res sync_res = YDB_OK;
    ynode *src = NULL;
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);
    lock(datablock);
    eventid eid = yconn_sync(NULL, datablock, true, buf, buflen);
    if (valid_waitevent(eid))
    {
        sync_res = yconn_serve_blocking(datablock, eid, datablock->timeout);
        YDB_FAIL(YDB_FAILED(sync_res), sync_res);
    }

    res = ynode_scanf_from_buf(buf, buflen, 0, &src);
    YDB_FAIL(res, res);
    if (!src)
        src = ynode_top(ynode_create_path("/", NULL, NULL));
    YDB_FAIL(!src, YDB_E_CTRL);
    if (ytrie_size(datablock->updater) > 0)
        ydb_update(NULL, datablock, src);
failed:
    unlock(datablock);
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ylog_out();
    if (YDB_SUCCESS(res) && sync_res == YDB_W_TIMEOUT)
        return YDB_W_TIMEOUT;
    return res;
}

ydb_res ydb_path_sync(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    FILE *fp;
    char *path = NULL;
    size_t pathlen = 0;

    ylog_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&path, &pathlen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    src = ynode_top(ynode_create_path(path, NULL, NULL));
    YDB_FAIL(!src, YDB_E_CTRL);

    {
        char buf[512];
        int buflen;
        buf[0] = 0;
        buflen = ynode_printf_to_buf(buf, sizeof(buf), src, 1, YDB_LEVEL_MAX);
        if (buflen >= 0)
            buf[buflen] = 0;
        lock(datablock);
        eventid eid = yconn_sync(NULL, datablock, true, buf, buflen);
        if (valid_waitevent(eid))
        {
            res = yconn_serve_blocking(datablock, eid, datablock->timeout);
            YDB_FAIL(YDB_FAILED(res), res);
        }
    }

    if (ytrie_size(datablock->updater) > 0)
        ydb_update(NULL, datablock, src);

failed:
    unlock(datablock);
    CLEAR_BUF(path, pathlen);
    ynode_remove(src);
    ylog_out();
    return res;
}

struct ydb_traverse_data
{
    union {
        ydb_traverse_callback0 cb0;
        ydb_traverse_callback1 cb1;
        ydb_traverse_callback2 cb2;
        ydb_traverse_callback3 cb3;
        ydb_traverse_callback4 cb4;
        ydb_traverse_callback cb;
    };
    int num;
    void *user[4];
    ydb *datablock;
};

ydb_res ydb_traverse_sub(ynode *cur, void *U1)
{
    struct ydb_traverse_data *pd = U1;
    switch (pd->num)
    {
    case 0:
        return pd->cb0(pd->datablock, cur);
    case 1:
        return pd->cb1(pd->datablock, cur, pd->user[0]);
    case 2:
        return pd->cb2(pd->datablock, cur, pd->user[0], pd->user[1]);
    case 3:
        return pd->cb3(pd->datablock, cur, pd->user[0], pd->user[1], pd->user[2]);
    case 4:
        return pd->cb4(pd->datablock, cur, pd->user[0], pd->user[1], pd->user[2], pd->user[3]);
    default:
        break;
    }
    return YDB_E_FUNC;
}

ydb_res ydb_traverse(ydb *datablock, ynode *cur, ydb_traverse_callback func, char *flags, int num, ...)
{
    ydb_res res;
    unsigned int trflags = 0x0;
    struct ydb_traverse_data data;
    if (!datablock || !func || num < 0)
        return YDB_E_INVALID_ARGS;
    if (num > 4 || num < 0)
        return YDB_E_INVALID_ARGS;
    if (flags)
    {
        if (strstr(flags, "leaf-first"))
            SET_FLAG(trflags, YNODE_LEAF_FIRST);
        if (strstr(flags, "leaf-only"))
            SET_FLAG(trflags, YNODE_LEAF_ONLY);
        else if (strstr(flags, "val-only"))
            SET_FLAG(trflags, YNODE_VAL_ONLY);
    }
    memset(&data, 0x0, sizeof(struct ydb_traverse_data));
    data.cb = func;
    data.num = num;
    data.datablock = datablock;

    {
        int i;
        va_list ap;
        va_start(ap, num);
        ylog_debug("user total = %d\n", num);
        for (i = 0; i < num; i++)
        {
            void *p = va_arg(ap, void *);
            data.user[i] = p;
            ylog_debug("user[%d]=%p\n", i, data.user[i]);
        }
        va_end(ap);
    }
    lock(datablock);
    if (!cur)
        cur = datablock->top;
    res = ynode_traverse(cur, ydb_traverse_sub, &data, trflags);
    unlock(datablock);
    return res;
}
