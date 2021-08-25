#include <hio_net_lrw.h>
#include <hio_lrw_talk.h>

// Zephyr includes
#include <logging/log.h>
#include <settings/settings.h>
#include <zephyr.h>

// Standard includes

LOG_MODULE_REGISTER(hio_net_lrw, LOG_LEVEL_DBG);

int hio_net_lrw_init(void)
{
   int ret;

    LOG_INF("Start");

    ret = hio_lrw_talk_init();

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_init` failed: %d", ret);
        return ret;
    }

    ret = hio_lrw_talk_enable();

    if (ret < 0) {
        LOG_ERR("Call `hio_lrw_talk_enable` failed: %d", ret);
        return ret;
    }

    for (;;) {
        LOG_DBG("Alive");

        ret = hio_lrw_talk_at();

        if (ret < 0) {
            LOG_ERR("Call `hio_lrw_talk_at` failed: %d", ret);
            return ret;
        }

        k_sleep(K_SECONDS(3));
    }

    return 0;
}
