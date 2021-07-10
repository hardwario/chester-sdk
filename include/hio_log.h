#ifndef HIO_LOG_H
#define HIO_LOG_H

#include <hio_sys.h>

// Zephyr includes
#include <logging/log_ctrl.h>
#include <logging/log.h>

// Standard includes
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HIO_LOG_LEVEL_OFF LOG_LEVEL_NONE
#define HIO_LOG_LEVEL_ERR LOG_LEVEL_ERR
#define HIO_LOG_LEVEL_WRN LOG_LEVEL_WRN
#define HIO_LOG_LEVEL_INF LOG_LEVEL_INF
#define HIO_LOG_LEVEL_DBG LOG_LEVEL_DBG

#define HIO_LOG_REGISTER LOG_MODULE_REGISTER

#define hio_log_dbg LOG_DBG
#define hio_log_inf LOG_INF
#define hio_log_wrn LOG_WRN
#define hio_log_err LOG_ERR
#define hio_log_fat LOG_ERR

/*
#define hio_log_fat { \
    LOG_ERR(__VA_ARGS__);  \
    LOG_PANIC();           \
    hio_sys_reboot();      \
}
*/

#endif
