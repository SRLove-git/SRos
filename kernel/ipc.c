#include "ipc.h"
#include "serial.h"
#include "string.h"

/* ================================================================
 * 信号量 (Semaphore) 实现
 * ================================================================ */

static sem_t sem_table[MAX_SEMS];
static int sem_count = 0;

void sem_init(void)
{
    for (int i = 0; i < MAX_SEMS; i++)
        sem_table[i].id = -1;
    serial_printf(SERIAL_COM1, "[ipc] Semaphore subsystem initialized (%d max)\n",
                  MAX_SEMS);
}

/* 查找空闲信号量槽位 */
static int sem_find_slot(void)
{
    for (int i = 0; i < MAX_SEMS; i++) {
        if (sem_table[i].id < 0)
            return i;
    }
    return -1;
}

int sem_create(const char *name, int initial_value)
{
    int slot = sem_find_slot();
    if (slot < 0) {
        serial_printf(SERIAL_COM1, "[ipc] sem_create: no free slots!\n");
        return -1;
    }

    sem_table[slot].id = sem_count++;
    if (name)
        strcpy(sem_table[slot].name, name);
    else
        sem_table[slot].name[0] = '\0';
    sem_table[slot].value = (initial_value < 0) ? 0 : initial_value;
    sem_table[slot].wait_head = NULL;
    sem_table[slot].wait_tail = NULL;

    serial_printf(SERIAL_COM1, "[ipc] sem %d '%s' created (value=%d)\n",
                  sem_table[slot].id, sem_table[slot].name,
                  sem_table[slot].value);

    return sem_table[slot].id;
}

int sem_wait(int sem_id)
{
    if (sem_id < 0 || sem_id >= sem_count)
        return -1;

    /* 查找信号量 */
    sem_t *sem = NULL;
    for (int i = 0; i < MAX_SEMS; i++) {
        if (sem_table[i].id == sem_id) {
            sem = &sem_table[i];
            break;
        }
    }
    if (!sem) return -1;

    /* 关中断保护临界区 */
    __asm__ volatile("cli");

    if (sem->value > 0) {
        /* 有可用资源，直接获取 */
        sem->value--;
        __asm__ volatile("sti");
        return 0;
    }

    /* 无可用资源，阻塞当前任务 */
    if (!current_task) {
        __asm__ volatile("sti");
        return -1;
    }

    serial_printf(SERIAL_COM1, "[ipc] sem %d '%s': task %d BLOCKED on wait\n",
                  sem_id, sem->name, current_task->pid);

    /* 将当前任务加入信号量等待队列 */
    task_block();
    current_task->next = NULL;
    if (sem->wait_tail) {
        sem->wait_tail->next = current_task;
        sem->wait_tail = current_task;
    } else {
        sem->wait_head = current_task;
        sem->wait_tail = current_task;
    }

    __asm__ volatile("sti");

    /* 触发调度，切换到其他任务 */
    __asm__ volatile("int $32");

    /* 被唤醒后返回（此时信号量已被 sem_post 消耗） */
    return 0;
}

int sem_post(int sem_id)
{
    if (sem_id < 0 || sem_id >= sem_count)
        return -1;

    /* 查找信号量 */
    sem_t *sem = NULL;
    for (int i = 0; i < MAX_SEMS; i++) {
        if (sem_table[i].id == sem_id) {
            sem = &sem_table[i];
            break;
        }
    }
    if (!sem) return -1;

    __asm__ volatile("cli");

    if (sem->wait_head) {
        /* 有等待的任务，唤醒第一个 */
        task_t *wake = sem->wait_head;
        sem->wait_head = wake->next;
        if (!sem->wait_head)
            sem->wait_tail = NULL;
        wake->next = NULL;

        serial_printf(SERIAL_COM1, "[ipc] sem %d '%s': waking task %d\n",
                      sem_id, sem->name, wake->pid);

        task_wake(wake);
    } else {
        /* 没有等待者，增加计数值 */
        sem->value++;
    }

    __asm__ volatile("sti");
    return 0;
}

int sem_destroy(int sem_id)
{
    if (sem_id < 0)
        return -1;

    __asm__ volatile("cli");

    for (int i = 0; i < MAX_SEMS; i++) {
        if (sem_table[i].id == sem_id) {
            /* 唤醒所有等待的任务（避免死锁） */
            while (sem_table[i].wait_head) {
                task_t *wake = sem_table[i].wait_head;
                sem_table[i].wait_head = wake->next;
                wake->next = NULL;
                serial_printf(SERIAL_COM1,
                              "[ipc] sem %d destroyed, waking task %d\n",
                              sem_id, wake->pid);
                task_wake(wake);
            }
            sem_table[i].wait_tail = NULL;
            sem_table[i].id = -1;
            sem_table[i].name[0] = '\0';
            sem_table[i].value = 0;

            __asm__ volatile("sti");
            serial_printf(SERIAL_COM1, "[ipc] sem %d destroyed\n", sem_id);
            return 0;
        }
    }

    __asm__ volatile("sti");
    return -1;
}

/* ================================================================
 * 消息队列 (Message Queue) 实现
 * ================================================================ */

static msg_queue_t mq_table[MAX_MSGQUEUES];
static int mq_count = 0;

void msg_queue_init(void)
{
    for (int i = 0; i < MAX_MSGQUEUES; i++)
        mq_table[i].id = -1;
    serial_printf(SERIAL_COM1, "[ipc] Message queue subsystem initialized "
                  "(%d max)\n", MAX_MSGQUEUES);
}

static int mq_find_slot(void)
{
    for (int i = 0; i < MAX_MSGQUEUES; i++) {
        if (mq_table[i].id < 0)
            return i;
    }
    return -1;
}

int msg_queue_create(void)
{
    int slot = mq_find_slot();
    if (slot < 0) {
        serial_printf(SERIAL_COM1, "[ipc] msg_queue_create: no free slots!\n");
        return -1;
    }

    mq_table[slot].id = mq_count++;
    mq_table[slot].head = 0;
    mq_table[slot].tail = 0;
    mq_table[slot].count = 0;
    mq_table[slot].recv_wait_head = NULL;
    mq_table[slot].recv_wait_tail = NULL;
    mq_table[slot].send_wait_head = NULL;
    mq_table[slot].send_wait_tail = NULL;

    serial_printf(SERIAL_COM1, "[ipc] msg queue %d created\n", mq_table[slot].id);
    return mq_table[slot].id;
}

int msg_queue_send(int mq_id, const char *data, u32 len)
{
    if (mq_id < 0 || !data) return -1;

    msg_queue_t *mq = NULL;
    for (int i = 0; i < MAX_MSGQUEUES; i++) {
        if (mq_table[i].id == mq_id) {
            mq = &mq_table[i];
            break;
        }
    }
    if (!mq) return -1;

    if (len > MSG_MAX_DATA)
        len = MSG_MAX_DATA;

    __asm__ volatile("cli");

    /* 队列满时，阻塞发送者 */
    while (mq->count >= MSGQUEUE_SIZE) {
        if (!current_task) {
            __asm__ volatile("sti");
            return -1;
        }
        serial_printf(SERIAL_COM1, "[ipc] mq %d full, task %d BLOCKED on send\n",
                      mq_id, current_task->pid);
        task_block();
        current_task->next = NULL;
        if (mq->send_wait_tail) {
            mq->send_wait_tail->next = current_task;
            mq->send_wait_tail = current_task;
        } else {
            mq->send_wait_head = current_task;
            mq->send_wait_tail = current_task;
        }
        __asm__ volatile("sti");
        __asm__ volatile("int $32");
        __asm__ volatile("cli");
    }

    /* 写入消息到环形缓冲区 */
    message_t *msg = &mq->msgs[mq->tail];
    msg->len = len;
    memcpy(msg->data, data, len);
    mq->tail = (mq->tail + 1) % MSGQUEUE_SIZE;
    mq->count++;

    /* 如果有接收者在等待，唤醒第一个 */
    if (mq->recv_wait_head) {
        task_t *wake = mq->recv_wait_head;
        mq->recv_wait_head = wake->next;
        if (!mq->recv_wait_head)
            mq->recv_wait_tail = NULL;
        wake->next = NULL;
        task_wake(wake);
        serial_printf(SERIAL_COM1, "[ipc] mq %d: waking recv task %d\n",
                      mq_id, wake->pid);
    }

    __asm__ volatile("sti");
    return 0;
}

int msg_queue_recv(int mq_id, char *buf, u32 *len)
{
    if (mq_id < 0 || !buf || !len) return -1;

    msg_queue_t *mq = NULL;
    for (int i = 0; i < MAX_MSGQUEUES; i++) {
        if (mq_table[i].id == mq_id) {
            mq = &mq_table[i];
            break;
        }
    }
    if (!mq) return -1;

    __asm__ volatile("cli");

    /* 队列空时，阻塞接收者 */
    while (mq->count <= 0) {
        if (!current_task) {
            __asm__ volatile("sti");
            return -1;
        }
        serial_printf(SERIAL_COM1, "[ipc] mq %d empty, task %d BLOCKED on recv\n",
                      mq_id, current_task->pid);
        task_block();
        current_task->next = NULL;
        if (mq->recv_wait_tail) {
            mq->recv_wait_tail->next = current_task;
            mq->recv_wait_tail = current_task;
        } else {
            mq->recv_wait_head = current_task;
            mq->recv_wait_tail = current_task;
        }
        __asm__ volatile("sti");
        __asm__ volatile("int $32");
        __asm__ volatile("cli");
    }

    /* 从环形缓冲区读取消息 */
    message_t *msg = &mq->msgs[mq->head];
    *len = msg->len;
    memcpy(buf, msg->data, msg->len);
    mq->head = (mq->head + 1) % MSGQUEUE_SIZE;
    mq->count--;

    /* 如果有发送者在等待（队列之前满了），唤醒第一个 */
    if (mq->send_wait_head) {
        task_t *wake = mq->send_wait_head;
        mq->send_wait_head = wake->next;
        if (!mq->send_wait_head)
            mq->send_wait_tail = NULL;
        wake->next = NULL;
        task_wake(wake);
        serial_printf(SERIAL_COM1, "[ipc] mq %d: waking send task %d\n",
                      mq_id, wake->pid);
    }

    __asm__ volatile("sti");
    return 0;
}

int msg_queue_destroy(int mq_id)
{
    if (mq_id < 0) return -1;

    __asm__ volatile("cli");

    for (int i = 0; i < MAX_MSGQUEUES; i++) {
        if (mq_table[i].id == mq_id) {
            /* 唤醒所有等待的接收者 */
            while (mq_table[i].recv_wait_head) {
                task_t *wake = mq_table[i].recv_wait_head;
                mq_table[i].recv_wait_head = wake->next;
                wake->next = NULL;
                serial_printf(SERIAL_COM1,
                              "[ipc] mq %d destroyed, waking recv task %d\n",
                              mq_id, wake->pid);
                task_wake(wake);
            }
            mq_table[i].recv_wait_tail = NULL;

            /* 唤醒所有等待的发送者 */
            while (mq_table[i].send_wait_head) {
                task_t *wake = mq_table[i].send_wait_head;
                mq_table[i].send_wait_head = wake->next;
                wake->next = NULL;
                serial_printf(SERIAL_COM1,
                              "[ipc] mq %d destroyed, waking send task %d\n",
                              mq_id, wake->pid);
                task_wake(wake);
            }
            mq_table[i].send_wait_tail = NULL;

            mq_table[i].id = -1;
            mq_table[i].head = 0;
            mq_table[i].tail = 0;
            mq_table[i].count = 0;

            __asm__ volatile("sti");
            serial_printf(SERIAL_COM1, "[ipc] msg queue %d destroyed\n", mq_id);
            return 0;
        }
    }

    __asm__ volatile("sti");
    return -1;
}

/* ================================================================
 * IPC 子系统初始化
 * ================================================================ */
void ipc_init(void)
{
    serial_printf(SERIAL_COM1, "[ipc] Initializing IPC subsystem...\n");
    sem_init();
    msg_queue_init();
    serial_printf(SERIAL_COM1, "[ipc] IPC subsystem ready.\n");
}
