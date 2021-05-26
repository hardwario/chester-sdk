#ifndef HIO_SYS_H
#define HIO_SYS_H

#include <zephyr.h>

#define HIO_SYS_FOREVER ((hio_sys_timeout_t)K_FOREVER)
#define HIO_SYS_NO_WAIT ((hio_sys_timeout_t)K_NO_WAIT)

#define HIO_SYS_MSEC K_MSEC
#define HIO_SYS_SECONDS K_SECONDS
#define HIO_SYS_MINUTES K_MINUTES
#define HIO_SYS_HOURS K_HOURS

#define HIO_SYS_TASK_STACK_DEFINE K_THREAD_STACK_DEFINE
#define HIO_SYS_TASK_STACK_SIZEOF K_THREAD_STACK_SIZEOF

typedef k_timeout_t hio_sys_timeout_t;
typedef k_tid_t hio_sys_task_id_t;
typedef struct k_thread hio_sys_task_t;
typedef k_thread_stack_t hio_sys_task_stack_t;
typedef void (*hio_sys_task_entry_t)(void *param);
typedef struct k_sem hio_sys_sem_t;
typedef struct k_mutex hio_sys_mut_t;

hio_sys_task_id_t
hio_sys_task_init(hio_sys_task_t *task,
                  hio_sys_task_stack_t *stack, size_t stack_size,
                  hio_sys_task_entry_t entry, void *param);

void
hio_sys_task_sleep(hio_sys_timeout_t timeout);

void
hio_sys_sem_init(hio_sys_sem_t *sem, unsigned int value);

int
hio_sys_sem_take(hio_sys_sem_t *sem, hio_sys_timeout_t timeout);

void
hio_sys_sem_give(hio_sys_sem_t *sem);

void
hio_sys_mut_init(hio_sys_mut_t *mut);

void
hio_sys_mut_acquire(hio_sys_mut_t *mut);

void
hio_sys_sleep(int milliseconds);

#endif
