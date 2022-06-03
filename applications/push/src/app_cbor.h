#ifndef APP_CBOR_H_
#define APP_CBOR_H_

/* Zephyr includes */
#include <tinycbor/cbor.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_cbor_encode(CborEncoder *enc);

#ifdef __cplusplus
}
#endif

#endif /* APP_CBOR_H_ */
