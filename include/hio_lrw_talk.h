#ifndef HIO_LRW_TALK_H
#define HIO_LRW_TALK_H

// Standard includes
#include <stddef.h>
#include <stdint.h>

int hio_lrw_talk_init(void);
int hio_lrw_talk_enable(void);
int hio_lrw_talk_disable(void);
int hio_lrw_talk_at(void);
int hio_lrw_talk_at_dformat(uint8_t df);
int hio_lrw_talk_at_band(uint8_t band);
int hio_lrw_talk_at_class(uint8_t class);
int hio_lrw_talk_at_mode(uint8_t mode);
int hio_lrw_talk_at_nwk(uint8_t network);
int hio_lrw_talk_at_adr(uint8_t adr);
int hio_lrw_talk_at_dutycycle(uint8_t dc);
int hio_lrw_talk_at_joindc(uint8_t jdc);
int hio_lrw_talk_at_join(void);
int hio_lrw_talk_at_devaddr(const uint8_t *devaddr, size_t devaddr_size);
int hio_lrw_talk_at_deveui(const uint8_t *deveui, size_t deveui_size);
int hio_lrw_talk_at_appeui(const uint8_t *appeui, size_t appeui_size);
int hio_lrw_talk_at_appkey(const uint8_t *appkey, size_t appkey_size);
int hio_lrw_talk_at_nwkskey(const uint8_t *nwkskey, size_t nwkskey_size);
int hio_lrw_talk_at_appskey(const uint8_t *appskey, size_t appskey_size);

#endif
