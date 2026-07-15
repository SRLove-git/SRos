#ifndef IPC_H
#define IPC_H

#include "types.h"
#include "scheduler.h"

/* ================================================================
 * 信号量 (Semaphore) — 经典计数信号量
 * ================================================================ */

#define MAX_SEMS      64
#define SEM_NAME_MAX  24

/* 信号量结构体 */
typedef struct {
    int id;                        /* -1 表示空闲 */
    char name[SEM_NAME_MAX];
    int value;                     /* 信号量计数值 */
    task_t *wait_head;             /* 等待队列头（链表） */
    task_t *wait_tail;             /* 等待队列尾 */
} sem_t;

/* 信号量 API（供内核内部和系统调用使用） */
void sem_init(void);
int  sem_create(const char *name, int initial_value);
int  sem_wait(int sem_id);
int  sem_post(int sem_id);
int  sem_destroy(int sem_id);

/* ================================================================
 * 消息队列 (Message Queue) — 进程间消息传递
 * ================================================================ */

#define MAX_MSGQUEUES   16
#define MSGQUEUE_SIZE   64       /* 每个队列最大消息数 */
#define MSG_MAX_DATA    64       /* 每条消息最大数据字节数 */

/* 消息结构体 */
typedef struct {
    u32 len;                      /* 消息数据长度 */
    char data[MSG_MAX_DATA];      /* 消息数据 */
} message_t;

/* 消息队列结构体 */
typedef struct {
    int id;                       /* -1 表示空闲 */
    message_t msgs[MSGQUEUE_SIZE];/* 环形缓冲区 */
    int head;                     /* 读索引 */
    int tail;                     /* 写索引 */
    int count;                    /* 当前消息数 */
    task_t *recv_wait_head;       /* 接收等待队列头 */
    task_t *recv_wait_tail;
    task_t *send_wait_head;       /* 发送等待队列头（队列满时） */
    task_t *send_wait_tail;
} msg_queue_t;

/* 消息队列 API */
void msg_queue_init(void);
int  msg_queue_create(void);
int  msg_queue_send(int mq_id, const char *data, u32 len);
int  msg_queue_recv(int mq_id, char *buf, u32 *len);
int  msg_queue_destroy(int mq_id);

/* ================================================================
 * IPC 子系统整体初始化
 * ================================================================ */
void ipc_init(void);

/* ================================================================
 * 用户态系统调用编号
 *   SYS_SEM_INIT    = 17
 *   SYS_SEM_WAIT    = 18
 *   SYS_SEM_POST    = 19
 *   SYS_SEM_DESTROY = 20
 *   SYS_MSGGET      = 21
 *   SYS_MSGSND      = 22
 *   SYS_MSGRCV      = 23
 *   SYS_MSGCTL      = 24
 * ================================================================ */

#endif /* IPC_H */
