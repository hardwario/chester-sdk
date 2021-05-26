#include <hio_sys.h>
#include <zephyr.h>

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
hio_sys_sleep(int milliseconds)
{
    k_msleep((int32_t)milliseconds);
}
