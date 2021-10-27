#include <ctr_sys.h>

/* Zephyr includes */
#include <logging/log.h>
#include <power/reboot.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_sys, LOG_LEVEL_DBG);

static k_timeout_t conv_timeout(int64_t timeout)
{
	if (timeout == CTR_SYS_NO_WAIT || timeout < 0) {
		return K_NO_WAIT;
	}

	if (timeout == CTR_SYS_FOREVER) {
		return K_FOREVER;
	}

	return Z_TIMEOUT_MS(timeout);
}

void ctr_sys_reboot(void)
{
/* TODO Flip this */
#if 0
    sys_reboot(SYS_REBOOT_COLD);
#else
	for (;;)
		;
#endif
}

int64_t ctr_sys_uptime_get(void)
{
	return k_uptime_get();
}

void ctr_sys_heap_init(ctr_sys_heap_t *heap, void *mem, size_t mem_size)
{
	k_heap_init((struct k_heap *)heap, mem, mem_size);
}

void *ctr_sys_heap_alloc(ctr_sys_heap_t *heap, size_t bytes, int64_t timeout)
{
	return k_heap_alloc((struct k_heap *)heap, bytes, conv_timeout(timeout));
}

void ctr_sys_heap_free(ctr_sys_heap_t *heap, void *mem)
{
	k_heap_free((struct k_heap *)heap, mem);
}

static void task_wrapper(void *p1, void *p2, void *p3)
{
	((ctr_sys_task_entry_t)p1)(p2);
}

ctr_sys_task_id_t ctr_sys_task_init(ctr_sys_task_t *task, const char *name,
                                    ctr_sys_task_stack_t *stack, size_t stack_size,
                                    ctr_sys_task_entry_t entry, void *param)
{
	k_tid_t ret;

	ret = k_thread_create((struct k_thread *)task, (k_thread_stack_t *)stack, stack_size,
	                      task_wrapper, entry, param, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
	                      K_NO_WAIT);

	if (k_thread_name_set(ret, name) < 0) {
		LOG_ERR("Call `k_thread_name_set` failed");
		k_oops();
	}

	return (ctr_sys_task_id_t)ret;
}

void ctr_sys_task_sleep(int64_t timeout)
{
	k_sleep(conv_timeout(timeout));
}

void ctr_sys_sem_init(ctr_sys_sem_t *sem, unsigned int value)
{
	if (k_sem_init((struct k_sem *)sem, value, UINT_MAX) < 0) {
		LOG_ERR("Call `k_sem_init` failed");
		k_oops();
	}
}

void ctr_sys_sem_give(ctr_sys_sem_t *sem)
{
	k_sem_give((struct k_sem *)sem);
}

int ctr_sys_sem_take(ctr_sys_sem_t *sem, int64_t timeout)
{
	if (k_sem_take((struct k_sem *)sem, conv_timeout(timeout)) < 0) {
		return -1;
	}

	return 0;
}

void ctr_sys_mut_init(ctr_sys_mut_t *mut)
{
	if (k_mutex_init((struct k_mutex *)mut) < 0) {
		LOG_ERR("Call `k_mutex_init` failed");
		k_oops();
	}
}

void ctr_sys_mut_acquire(ctr_sys_mut_t *mut)
{
	if (k_mutex_lock((struct k_mutex *)mut, K_FOREVER) < 0) {
		LOG_ERR("Call `k_mutex_lock` failed");
		k_oops();
	}
}

void ctr_sys_mut_release(ctr_sys_mut_t *mut)
{
	if (k_mutex_unlock((struct k_mutex *)mut) < 0) {
		LOG_ERR("Call `k_mutex_unlock` failed");
		k_oops();
	}
}

void ctr_sys_msgq_init(ctr_sys_msgq_t *msgq, void *mem, size_t item_size, size_t max_items)
{
	k_msgq_init((struct k_msgq *)msgq, (char *)mem, item_size, max_items);
}

int ctr_sys_msgq_put(ctr_sys_msgq_t *msgq, const void *item, int64_t timeout)
{
	if (k_msgq_put((struct k_msgq *)msgq, item, conv_timeout(timeout)) < 0) {
		return -1;
	}

	return 0;
}

int ctr_sys_msgq_get(ctr_sys_msgq_t *msgq, void *item, int64_t timeout)
{
	if (k_msgq_get((struct k_msgq *)msgq, item, conv_timeout(timeout)) < 0) {
		return -1;
	}

	return 0;
}

void ctr_sys_rbuf_init(ctr_sys_rbuf_t *rbuf, void *mem, size_t mem_size)
{
	ring_buf_init((struct ring_buf *)rbuf, mem_size, mem);
}

size_t ctr_sys_rbuf_put(ctr_sys_rbuf_t *rbuf, const uint8_t *data, size_t bytes)
{
	return (size_t)ring_buf_put((struct ring_buf *)rbuf, data, (uint32_t)bytes);
}

size_t ctr_sys_rbuf_get(ctr_sys_rbuf_t *rbuf, uint8_t *data, size_t bytes)
{
	return (size_t)ring_buf_get((struct ring_buf *)rbuf, data, (uint32_t)bytes);
}
