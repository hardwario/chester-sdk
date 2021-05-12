#include <hio_sys.h>
#include <zephyr.h>

void
hio_sys_sleep(int milliseconds)
{
    k_msleep((int32_t)milliseconds);
}
