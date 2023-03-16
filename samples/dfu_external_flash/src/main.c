#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);
}
