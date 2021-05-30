#ifndef HIO_LOG_H
#define HIO_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HIO_LOG_LEVEL_OFF = 0,
    HIO_LOG_LEVEL_ERROR = 1,
    HIO_LOG_LEVEL_WARN = 2,
    HIO_LOG_LEVEL_INFO = 3,
    HIO_LOG_LEVEL_DEBUG = 4,
    HIO_LOG_LEVEL_DUMP = 5
} hio_log_level_t;

typedef enum {
    HIO_LOG_TIMESTAMP_OFF = 0,
    HIO_LOG_TIMESTAMP_ABS = 1,
    HIO_LOG_TIMESTAMP_REL = 2
} hio_log_timestamp_t;

typedef void (*log_init_t)(void);
typedef void (*log_print_t)(const char *s, size_t len);
typedef uint64_t (*log_time_t)(void);
typedef const char *(*log_eol_t)(void);
typedef void (*log_lock_t)(void);
typedef void (*log_unlock_t)(void);
typedef void (*log_reboot_t)(void);

typedef struct {
    log_init_t init;
    log_print_t print;
    log_time_t time;
    log_eol_t eol;
    log_lock_t lock;
    log_unlock_t unlock;
    log_reboot_t reboot;
} hio_log_driver_t;

bool
hio_log_is_init(void);

void
hio_log_init(const hio_log_driver_t *driver,
             hio_log_level_t level, hio_log_timestamp_t ts);

void
hio_log_debug_(const char *fmt, ...);

void
hio_log_info_(const char *fmt, ...);

void
hio_log_warn_(const char *fmt, ...);

void
hio_log_error_(const char *fmt, ...);

void
hio_log_fatal_(const char *fmt, ...);

void
hio_log_dump_(const void *buf, size_t len, const char *fmt, ...);

#define hio_log_debug(fmt, ...)                                  \
    if (HIO_LOG_ENABLED != 0) {                                  \
        hio_log_debug_(HIO_LOG_PREFIX ": " fmt, ## __VA_ARGS__); \
    }

#define hio_log_info(fmt, ...)                                  \
    if (HIO_LOG_ENABLED != 0) {                                 \
        hio_log_info_(HIO_LOG_PREFIX ": " fmt, ## __VA_ARGS__); \
    }

#define hio_log_warn(fmt, ...)                                  \
    if (HIO_LOG_ENABLED != 0) {                                 \
        hio_log_warn_(HIO_LOG_PREFIX ": " fmt, ## __VA_ARGS__); \
    }

#define hio_log_error(fmt, ...)                                  \
    if (HIO_LOG_ENABLED != 0) {                                  \
        hio_log_error_(HIO_LOG_PREFIX ": " fmt, ## __VA_ARGS__); \
    }

#define hio_log_fatal(fmt, ...)                                  \
    if (HIO_LOG_ENABLED != 0) {                                  \
        hio_log_fatal_(HIO_LOG_PREFIX ": " fmt, ## __VA_ARGS__); \
    }

#endif
