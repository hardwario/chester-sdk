#include <hio_sys.h>
// TODO
// #include <hio_log.h>

static void
task_wrapper(void *p1, void *p2, void *p3)
{
    ((hio_sys_task_entry_t)p1)(p2);
}

hio_sys_task_id_t
hio_sys_task_init(hio_sys_task_t *task,
                  hio_sys_task_stack_t *stack, size_t stack_size,
                  hio_sys_task_entry_t entry, void *param)
{
    k_tid_t ret;

    ret = k_thread_create((struct k_thread *)task,
                          (k_thread_stack_t *)stack, stack_size,
                          task_wrapper, entry, param, NULL,
                          K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);

    return (hio_sys_task_id_t)ret;
}

void
hio_sys_task_sleep(hio_sys_timeout_t timeout)
{
    k_sleep(timeout);
}

void
hio_sys_sem_init(hio_sys_sem_t *sem, unsigned int value)
{
    if (k_sem_init((struct k_sem *)sem, value, UINT_MAX) < 0) {
        // TODO
        // hio_log_critical("Call `k_sem_init` failed");
    }
}

int
hio_sys_sem_take(hio_sys_sem_t *sem, hio_sys_timeout_t timeout)
{
    if (k_sem_take((struct k_sem *)sem, (k_timeout_t)timeout) < 0) {
        return -1;
    }

    return 0;
}

void
hio_sys_sem_give(hio_sys_sem_t *sem)
{
    k_sem_give((struct k_sem *)sem);
}

void
hio_sys_mut_init(hio_sys_mut_t *mut)
{
    if (k_mutex_init((struct k_mutex *)mut) < 0) {
        // TODO
        // hio_log_critical("Call `k_mutex_init` failed");
    }
}

void
hio_sys_mut_acquire(hio_sys_mut_t *mut)
{
    if (k_mutex_lock((struct k_mutex *)mut, K_FOREVER) < 0) {
        // TODO
        // hio_log_critical("Call `k_mutex_lock` failed");
    }
}

void
hio_sys_mut_release(hio_sys_mut_t *mut)
{
    if (k_mutex_unlock((struct k_mutex *)mut) < 0) {
        // TODO
        // hio_log_critical("Call `k_mutex_unlock` failed");
    }
}
