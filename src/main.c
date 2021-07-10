#include <application.h>
#include <hio_bsp.h>
#include <hio_bsp_i2c.h>
#include <hio_bus_i2c.h>
#include <hio_log.h>
#include <hio_sys.h>

// Zephyr includes
#include <zephyr.h>

// TODO
#include <stdio.h>

HIO_LOG_REGISTER(main, HIO_LOG_LEVEL_DBG);

void
main(void)
{
    if (hio_bsp_init() < 0) {
        hio_log_fat("Call `hio_bsp_init` failed");
    }

    application_init();

    for (;;) {
        application_loop();
    }
}
