#include <chester/ctr_rtc.h>

static int64_t m_ts = 0;

int ctr_rtc_set_ts(int64_t ts)
{
	m_ts = ts;
	return 0;
}

int ctr_rtc_get_ts(int64_t *ts)
{
	*ts = m_ts;
	return 0;
}
