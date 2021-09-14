#include <hio_rtc.h>

// Zephyr includes
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <irq.h>
#include <logging/log.h>
#include <nrfx_rtc.h>
#include <shell/shell.h>
#include <zephyr.h>

// Standard includes
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(hio_rtc, LOG_LEVEL_DBG);

static const nrfx_rtc_t m_rtc = NRFX_RTC_INSTANCE(0);
static struct onoff_client m_lfclk_cli;

static int m_year = 1970;
static int m_month = 1;
static int m_day = 1;
static int m_hours = 0;
static int m_minutes = 0;
static int m_seconds = 0;

static int get_days_in_month(int year, int month)
{
    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;

    } else if (month == 2) {
        if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0)) {
            return 29;

        } else {
            return 28;
        }
    }

    return 31;
}

static void rtc_handler(nrfx_rtc_int_type_t int_type)
{
    if (int_type != NRFX_RTC_INT_TICK) {
        return;
    }

    static int prescaler = 0;

    if (++prescaler % 8 != 0) {
        return;
    }

    prescaler = 0;

    if (++m_seconds >= 60) {
        m_seconds = 0;

        if (++m_minutes >= 60) {
            m_minutes = 0;

            if (++m_hours >= 24) {
                m_hours = 0;

                if (++m_day > get_days_in_month(m_year, m_month)) {
                    m_day = 1;

                    if (++m_month > 12) {
                        m_month = 1;

                        ++m_year;
                    }
                }
            }
        }
    }
}

static int request_lfclk(void)
{
    int ret;

    struct onoff_manager *mgr =
        z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_LF);

    if (mgr == NULL) {
        LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
        return -ENXIO;
    }

    static struct k_poll_signal sig;

    k_poll_signal_init(&sig);
    sys_notify_init_signal(&m_lfclk_cli.notify, &sig);

    ret = onoff_request(mgr, &m_lfclk_cli);

    if (ret < 0) {
        LOG_ERR("Call `onoff_request` failed: %d", ret);
        return ret;
    }

    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 &sig)
    };

    ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

    if (ret < 0) {
        LOG_ERR("Call `k_poll` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_rtc_init(void)
{
    int ret;

    ret = request_lfclk();

    if (ret < 0) {
        LOG_ERR("Call `request_lfclk` failed: %d", ret);
        return ret;
    }

    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler = 4095;

    nrfx_err_t err = nrfx_rtc_init(&m_rtc, &config, rtc_handler);

    if (err != NRFX_SUCCESS) {
        LOG_ERR("Call `nrfx_rtc_init` failed: %d", (int)err);
        return -EIO;
    }

    nrfx_rtc_tick_enable(&m_rtc, true);
    nrfx_rtc_enable(&m_rtc);

    IRQ_CONNECT(RTC0_IRQn, 0, nrfx_rtc_0_irq_handler, NULL, 0);
    irq_enable(RTC0_IRQn);

    return 0;
}

int hio_rtc_get(struct hio_rtc_tm *tm)
{
    irq_disable(RTC0_IRQn);

    tm->year = m_year;
    tm->month = m_month;
    tm->day = m_day;
    tm->hours = m_hours;
    tm->minutes = m_minutes;
    tm->seconds = m_seconds;

    irq_enable(RTC0_IRQn);

    return 0;
}

int hio_rtc_set(const struct hio_rtc_tm *tm)
{
    if (tm->year < 1970 || tm->year > 2099) {
        return -EINVAL;
    }

    if (tm->month < 1 || tm->month > 12) {
        return -EINVAL;
    }

    if (tm->day < 1 || tm->day > get_days_in_month(tm->year, tm->month)) {
        return -EINVAL;
    }

    if (tm->hours < 0 || tm->hours > 23) {
        return -EINVAL;
    }

    if (tm->minutes < 0 || tm->minutes > 59) {
        return -EINVAL;
    }

    if (tm->seconds < 0 || tm->seconds > 59) {
        return -EINVAL;
    }

    irq_disable(RTC0_IRQn);

    m_year = tm->year;
    m_month = tm->month;
    m_day = tm->day;
    m_hours = tm->hours;
    m_minutes = tm->minutes;
    m_seconds = tm->seconds;

    irq_enable(RTC0_IRQn);

    return 0;
}

static int cmd_rtc_get(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int ret;

    struct hio_rtc_tm tm;

    ret = hio_rtc_get(&tm);

    if (ret < 0) {
        LOG_ERR("Call `hio_rtc_get` failed: %d", ret);
        return ret;
    }

    shell_print(shell, "%04d/%02d/%02d %02d:%02d:%02d",
                tm.year, tm.month, tm.day, tm.hours, tm.minutes, tm.seconds);

    return 0;
}

static int cmd_rtc_set(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);

    int ret;

    char *date = argv[1];
    char *time = argv[2];

    if (strlen(date) != 10 ||
        !isdigit((unsigned char)date[0]) ||
        !isdigit((unsigned char)date[1]) ||
        !isdigit((unsigned char)date[2]) ||
        !isdigit((unsigned char)date[3]) ||
        date[4] != '/' ||
        !isdigit((unsigned char)date[5]) ||
        !isdigit((unsigned char)date[6]) ||
        date[7] != '/'||
        !isdigit((unsigned char)date[8]) ||
        !isdigit((unsigned char)date[9])
    ) {
        shell_help(shell);
        return -EINVAL;
    }

    if (strlen(time) != 8 ||
        !isdigit((unsigned char)time[0]) ||
        !isdigit((unsigned char)time[1]) ||
        time[2] != ':' ||
        !isdigit((unsigned char)time[3]) ||
        !isdigit((unsigned char)time[4]) ||
        time[5] != ':'||
        !isdigit((unsigned char)time[6]) ||
        !isdigit((unsigned char)time[7])
    ) {
        shell_help(shell);
        return -EINVAL;
    }

    date[4] = '\0';
    date[7] = '\0';
    time[2] = '\0';
    time[5] = '\0';

    struct hio_rtc_tm tm;

    tm.year = atoi(&date[0]);
    tm.month = atoi(&date[5]);
    tm.day = atoi(&date[8]);
    tm.hours = atoi(&time[0]);
    tm.minutes = atoi(&time[3]);
    tm.seconds = atoi(&time[6]);

    ret = hio_rtc_set(&tm);

    if (ret < 0) {
        LOG_ERR("Call `hio_rtc_set` failed: %d", ret);
        return ret;
    }

    return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
    if (argc > 1) {
        shell_error(shell, "%s: Command not found", argv[1]);
        shell_help(shell);
        return -EINVAL;
    }

    shell_help(shell);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_rtc,
    SHELL_CMD_ARG(get, NULL,
                  "Get current date/time"
                  "(format YYYY/MM/DD hh:mm:ss).", cmd_rtc_get, 1, 0),
    SHELL_CMD_ARG(set, NULL,
                  "Set current date/time"
                  "(format YYYY/MM/DD hh:mm:ss).", cmd_rtc_set, 3, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rtc, &sub_rtc,
                   "RTC commands for date/time operations", print_help);
