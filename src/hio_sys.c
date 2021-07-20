#include <hio_sys.h>
#include <hio_log.h>

// Zephyr includes
#include <power/reboot.h>

HIO_LOG_REGISTER(hio_sys, HIO_LOG_LEVEL_DBG);

static k_timeout_t
conv_timeout(int64_t timeout)
{
    if (timeout == HIO_SYS_NO_WAIT || timeout < 0) {
        return K_NO_WAIT;
    }

    if (timeout == HIO_SYS_FOREVER) {
        return K_FOREVER;
    }

    return Z_TIMEOUT_MS(timeout);
}

void
hio_sys_reboot(void)
{
    // TODO Flip this
    #if 0
    sys_reboot(SYS_REBOOT_COLD);
    #else
    for (;;);
    #endif
}

int64_t
hio_sys_uptime_get(void)
{
    return k_uptime_get();
}

void
hio_sys_heap_init(hio_sys_heap_t *heap, void *mem, size_t mem_size)
{
    k_heap_init((struct k_heap *)heap, mem, mem_size);
}

void *
hio_sys_heap_alloc(hio_sys_heap_t *heap, size_t bytes, int64_t timeout)
{
    return k_heap_alloc((struct k_heap *)heap, bytes, conv_timeout(timeout));
}

void
hio_sys_heap_free(hio_sys_heap_t *heap, void *mem)
{
    k_heap_free((struct k_heap *)heap, mem);
}

static void
task_wrapper(void *p1, void *p2, void *p3)
{
    ((hio_sys_task_entry_t)p1)(p2);
}

hio_sys_task_id_t
hio_sys_task_init(hio_sys_task_t *task, const char *name,
                  hio_sys_task_stack_t *stack, size_t stack_size,
                  hio_sys_task_entry_t entry, void *param)
{
    k_tid_t ret;

    ret = k_thread_create((struct k_thread *)task,
                          (k_thread_stack_t *)stack, stack_size,
                          task_wrapper, entry, param, NULL,
                          K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);

    if (k_thread_name_set(ret, name) < 0) {
        hio_log_fat("Call `k_thread_name_set` failed");
    }

    return (hio_sys_task_id_t)ret;
}

void
hio_sys_task_sleep(int64_t timeout)
{
    k_sleep(conv_timeout(timeout));
}

void
hio_sys_sem_init(hio_sys_sem_t *sem, unsigned int value)
{
    if (k_sem_init((struct k_sem *)sem, value, UINT_MAX) < 0) {
        hio_log_fat("Call `k_sem_init` failed");
    }
}

void
hio_sys_sem_give(hio_sys_sem_t *sem)
{
    k_sem_give((struct k_sem *)sem);
}

int
hio_sys_sem_take(hio_sys_sem_t *sem, int64_t timeout)
{
    if (k_sem_take((struct k_sem *)sem, conv_timeout(timeout)) < 0) {
        return -1;
    }

    return 0;
}

void
hio_sys_mut_init(hio_sys_mut_t *mut)
{
    if (k_mutex_init((struct k_mutex *)mut) < 0) {
        hio_log_fat("Call `k_mutex_init` failed");
    }
}

void
hio_sys_mut_acquire(hio_sys_mut_t *mut)
{
    if (k_mutex_lock((struct k_mutex *)mut, K_FOREVER) < 0) {
        hio_log_fat("Call `k_mutex_lock` failed");
    }
}

void
hio_sys_mut_release(hio_sys_mut_t *mut)
{
    if (k_mutex_unlock((struct k_mutex *)mut) < 0) {
        hio_log_fat("Call `k_mutex_unlock` failed");
    }
}

void
hio_sys_msgq_init(hio_sys_msgq_t *msgq, void *mem,
                  size_t item_size, size_t max_items)
{
    k_msgq_init((struct k_msgq *)msgq, (char *)mem, item_size, max_items);
}

int
hio_sys_msgq_put(hio_sys_msgq_t *msgq, const void *item, int64_t timeout)
{
    if (k_msgq_put((struct k_msgq *)msgq, item, conv_timeout(timeout)) < 0) {
        return -1;
    }

    return 0;
}

int
hio_sys_msgq_get(hio_sys_msgq_t *msgq, void *item, int64_t timeout)
{
    if (k_msgq_get((struct k_msgq *)msgq, item, conv_timeout(timeout)) < 0) {
        return -1;
    }

    return 0;
}

void
hio_sys_rbuf_init(hio_sys_rbuf_t *rbuf, void *mem, size_t mem_size)
{
    ring_buf_init((struct ring_buf *)rbuf, mem_size, mem);
}

size_t
hio_sys_rbuf_put(hio_sys_rbuf_t *rbuf, const uint8_t *data, size_t bytes)
{
    return (size_t)ring_buf_put((struct ring_buf *)rbuf,
                                data, (uint32_t)bytes);
}

size_t
hio_sys_rbuf_get(hio_sys_rbuf_t *rbuf, uint8_t *data, size_t bytes)
{
    return (size_t)ring_buf_get((struct ring_buf *)rbuf,
                                data, (uint32_t)bytes);
}
