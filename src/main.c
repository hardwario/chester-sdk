#include <application.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_sys.h>

// Zephyr includes
#include <zephyr.h>

// TODO
#include <stdio.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "MAIN"

static hio_sys_mut_t log_mut;

static const char *
log_driver_eol(void)
{
    return "\n";
}

static void
log_driver_init(void)
{
}

static void
log_driver_print(const char *s, size_t len)
{
    printk("%s", s);
}

static uint64_t
log_driver_time(void)
{
    return (uint64_t)hio_sys_uptime_get();
}

static void
log_driver_lock(void)
{
    hio_sys_mut_acquire(&log_mut);
}

static void
log_driver_unlock(void)
{
    hio_sys_mut_release(&log_mut);
}

static void
log_driver_reboot(void)
{
    hio_sys_reboot();
}

void
main(void)
{
    hio_sys_mut_init(&log_mut);

    static const hio_log_driver_t log_driver = {
        .init = log_driver_init,
        .print = log_driver_print,
        .time = log_driver_time,
        .eol = log_driver_eol,
        .lock = log_driver_lock,
        .unlock = log_driver_unlock,
        .reboot = log_driver_reboot
    };

    hio_log_init(&log_driver, HIO_LOG_LEVEL_DUMP, HIO_LOG_TIMESTAMP_ABS);

    if (hio_bsp_init() < 0) {
        hio_log_fatal("Call `hio_bsp_init` failed");
    }

    application_init();

    for (;;) {
        application_loop();
    }
}
