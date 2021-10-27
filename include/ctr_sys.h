#ifndef CHESTER_INCLUDE_SYS_H_
#define CHESTER_INCLUDE_SYS_H_

/* Zephyr includes */
#include <sys/ring_buffer.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_SYS_FOREVER ((int64_t)0x7fffffffffffffff)
#define CTR_SYS_NO_WAIT ((int64_t)0)

#define CTR_SYS_MSEC(n) ((int64_t)(n))
#define CTR_SYS_SECONDS(n) ((int64_t)(n)*CTR_SYS_MSEC(1000))
#define CTR_SYS_MINUTES(n) ((int64_t)(n)*CTR_SYS_SECONDS(60))
#define CTR_SYS_HOURS(n) ((int64_t)(n)*CTR_SYS_MINUTES(60))
#define CTR_SYS_DAYS(n) ((int64_t)(n)*CTR_SYS_HOURS(24))

#define CTR_SYS_TASK_STACK_DEFINE K_THREAD_STACK_DEFINE
#define CTR_SYS_TASK_STACK_MEMBER K_THREAD_STACK_MEMBER
#define CTR_SYS_TASK_STACK_SIZEOF K_THREAD_STACK_SIZEOF

typedef struct k_heap ctr_sys_heap_t;
typedef k_tid_t ctr_sys_task_id_t;
typedef struct k_thread ctr_sys_task_t;
typedef k_thread_stack_t ctr_sys_task_stack_t;
typedef void (*ctr_sys_task_entry_t)(void *param);
typedef struct k_sem ctr_sys_sem_t;
typedef struct k_mutex ctr_sys_mut_t;
typedef struct k_msgq ctr_sys_msgq_t;
typedef struct ring_buf ctr_sys_rbuf_t;

void ctr_sys_reboot(void);
int64_t ctr_sys_uptime_get(void);
void ctr_sys_heap_init(ctr_sys_heap_t *heap, void *mem, size_t mem_size);
void *ctr_sys_heap_alloc(ctr_sys_heap_t *heap, size_t bytes, int64_t timeout);
void ctr_sys_heap_free(ctr_sys_heap_t *heap, void *mem);
ctr_sys_task_id_t ctr_sys_task_init(ctr_sys_task_t *task, const char *name,
                                    ctr_sys_task_stack_t *stack, size_t stack_size,
                                    ctr_sys_task_entry_t entry, void *param);
void ctr_sys_task_sleep(int64_t timeout);
void ctr_sys_sem_init(ctr_sys_sem_t *sem, unsigned int value);
void ctr_sys_sem_give(ctr_sys_sem_t *sem);
int ctr_sys_sem_take(ctr_sys_sem_t *sem, int64_t timeout);
void ctr_sys_mut_init(ctr_sys_mut_t *mut);
void ctr_sys_mut_acquire(ctr_sys_mut_t *mut);
void ctr_sys_mut_release(ctr_sys_mut_t *mut);
void ctr_sys_msgq_init(ctr_sys_msgq_t *msgq, void *mem, size_t item_size, size_t max_items);
int ctr_sys_msgq_put(ctr_sys_msgq_t *msgq, const void *item, int64_t timeout);
int ctr_sys_msgq_get(ctr_sys_msgq_t *msgq, void *item, int64_t timeout);
void ctr_sys_rbuf_init(ctr_sys_rbuf_t *rbuf, void *mem, size_t mem_size);
size_t ctr_sys_rbuf_put(ctr_sys_rbuf_t *rbuf, const uint8_t *data, size_t bytes);
size_t ctr_sys_rbuf_get(ctr_sys_rbuf_t *rbuf, uint8_t *data, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_SYS_H_ */
