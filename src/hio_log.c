#include <hio_log.h>

// Standard includes
#include <stdio.h>
#include <string.h>

#define LOG_BUF_SIZE 2048
#define LOG_DUMP_WIDTH 8
#define LOG_PREFIX "# "

static struct {
    const hio_log_driver_t *driver;
    hio_log_level_t level;
    hio_log_timestamp_t ts;
    uint64_t last;
    char buf[LOG_BUF_SIZE];
} log;

void
hio_log_init(const hio_log_driver_t *driver,
             hio_log_level_t level, hio_log_timestamp_t ts)
{
    log.driver = driver;
    log.level = level;
    log.ts = ts;

    if (log.driver->init != NULL) {
        log.driver->init();
    }
}

static void
message(hio_log_level_t level, char id, const char *fmt, va_list ap)
{
    if (log.level < level) {
        return;
    }

    if (log.driver->lock != NULL) {
        log.driver->lock();
    }

    uint64_t now = log.driver->time != NULL ? log.driver->time() : 0;

    if (log.ts == HIO_LOG_TIMESTAMP_ABS) {
        uint32_t ts = now / 10;
        const char *fmt = LOG_PREFIX "%lu.%02lu <%c> ";
        snprintf(log.buf, sizeof(log.buf), fmt, ts / 100, ts % 100, id);
    } else if (log.ts == HIO_LOG_TIMESTAMP_REL) {
        uint32_t ts = (now - log.last) / 10;
        const char *fmt = LOG_PREFIX "+%lu.%02lu <%c> ";
        snprintf(log.buf, sizeof(log.buf), fmt, ts / 100, ts % 100, id);
    } else {
        const char *fmt = LOG_PREFIX "<%c> ";
        snprintf(log.buf, sizeof(log.buf), fmt, id);
    }

    size_t end = strlen(log.buf);

    vsnprintf(&log.buf[end], sizeof(log.buf) - end, fmt, ap);

    end = strlen(log.buf);

    const char *eol = log.driver->eol != NULL ? log.driver->eol() : "";

    if (end < sizeof(log.buf) - strlen(eol)) {
        strcpy(&log.buf[end], eol);

        if (log.driver->print != NULL) {
            log.driver->print(log.buf, end + strlen(eol));
        }
    }

    log.last = now;

    if (log.driver->unlock != NULL) {
        log.driver->unlock();
    }
}

void
hio_log_dump_(const void *buf, size_t len, const char *fmt, ...)
{
    if (log.level < HIO_LOG_LEVEL_DUMP) {
        return;
    }

    if (len > 0xffff) {
        len = 0xffff;
    }

    if (log.driver->lock != NULL) {
        log.driver->lock();
    }

    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_DUMP, 'X', fmt, ap);
    va_end(ap);

    if (buf == NULL || len == 0) {
        if (log.driver->unlock != NULL) {
            log.driver->unlock();
        }

        return;
    }

    size_t base = 0;

    for (; base < sizeof(log.buf); base++) {
        if (log.buf[base] == '>') {
            break;
        }
    }

    base += 2;

    const char *eol = log.driver->eol != NULL ? log.driver->eol() : "";

    size_t end = base + 6 + LOG_DUMP_WIDTH * 3 + 2 + LOG_DUMP_WIDTH;

    if (sizeof(log.buf) < end + strlen(eol) + 1) {
        if (log.driver->unlock != NULL) {
            log.driver->unlock();
        }

        return;
    }

    for (size_t i = 0; i < len; i += LOG_DUMP_WIDTH) {
        snprintf(&log.buf[base], 6 + 1, "%04x: ", (uint16_t) i);

        char *hex = &log.buf[base + 6];
        char *asc = &log.buf[base + 6 + LOG_DUMP_WIDTH * 3 + 2];

        size_t line;

        if (len >= i + LOG_DUMP_WIDTH) {
            line = LOG_DUMP_WIDTH;
        } else {
            line = len - i;
        }

        size_t j;

        for (j = 0; j < line; j++) {
            if (j == LOG_DUMP_WIDTH / 2) {
                *hex++ = '|';
                *hex++ = ' ';
            }

            uint8_t value = ((uint8_t *) buf)[i + j];

            snprintf(hex, 2 + 1, "%02x", value);

            hex += 2;
            *hex++ = ' ';

            if (value < 32 || value > 126) {
                *asc++ = '.';
            } else {
                *asc++ = value;
            }
        }

        for (; j < LOG_DUMP_WIDTH; j++) {
            if (j == LOG_DUMP_WIDTH / 2) {
                *hex++ = '|';
                *hex++ = ' ';
            }

            strcpy(hex, "  ");

            hex += 2;
            *hex++ = ' ';

            *asc++ = ' ';
        }

        strcpy(&log.buf[end], eol);

        if (log.driver->print != NULL) {
            log.driver->print(log.buf, end + strlen(eol));
        }
    }

    if (log.driver->unlock != NULL) {
        log.driver->unlock();
    }
}

void
hio_hio_log_debug_(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_DEBUG, 'D', fmt, ap);
    va_end(ap);
}

void
hio_hio_log_info_(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_INFO, 'I', fmt, ap);
    va_end(ap);
}

void
hio_hio_log_warn_(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_WARN, 'W', fmt, ap);
    va_end(ap);
}

void
hio_hio_log_error_(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_ERROR, 'E', fmt, ap);
    va_end(ap);
}

void
hio_hio_log_fatal_(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    message(HIO_LOG_LEVEL_ERROR, 'F', fmt, ap);
    va_end(ap);
}
