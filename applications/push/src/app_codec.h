#ifndef CODEC_H_
#define CODEC_H_

/* This file has been generated using the script gen-codec.py */

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_CLOUD_DECODER_HASH ((uint64_t)0x7db07ef6fc417121)
#define CODEC_CLOUD_ENCODER_HASH ((uint64_t)0x0000000000000000)

enum codec_key_e {
	CODEC_KEY_E_MESSAGE = 0,
	CODEC_KEY_E_MESSAGE__VERSION = 1,
	CODEC_KEY_E_MESSAGE__SEQUENCE = 2,
	CODEC_KEY_E_MESSAGE__TIMESTAMP = 3,
	CODEC_KEY_E_ATTRIBUTE = 4,
	CODEC_KEY_E_ATTRIBUTE__VENDOR_NAME = 5,
	CODEC_KEY_E_ATTRIBUTE__PRODUCT_NAME = 6,
	CODEC_KEY_E_ATTRIBUTE__HW_VARIANT = 7,
	CODEC_KEY_E_ATTRIBUTE__HW_REVISION = 8,
	CODEC_KEY_E_ATTRIBUTE__FW_VERSION = 9,
	CODEC_KEY_E_ATTRIBUTE__SERIAL_NUMBER = 10,
	CODEC_KEY_E_SYSTEM = 11,
	CODEC_KEY_E_SYSTEM__UPTIME = 12,
	CODEC_KEY_E_SYSTEM__VOLTAGE_LOAD = 13,
	CODEC_KEY_E_SYSTEM__VOLTAGE_REST = 14,
	CODEC_KEY_E_SYSTEM__CURRENT_LOAD = 15,
	CODEC_KEY_E_NETWORK = 16,
	CODEC_KEY_E_NETWORK__PARAMETER = 17,
	CODEC_KEY_E_NETWORK__PARAMETER__EEST = 18,
	CODEC_KEY_E_NETWORK__PARAMETER__ECL = 19,
	CODEC_KEY_E_NETWORK__PARAMETER__RSRP = 20,
	CODEC_KEY_E_NETWORK__PARAMETER__RSRQ = 21,
	CODEC_KEY_E_NETWORK__PARAMETER__SNR = 22,
	CODEC_KEY_E_NETWORK__PARAMETER__PLMN = 23,
	CODEC_KEY_E_NETWORK__PARAMETER__CID = 24,
	CODEC_KEY_E_NETWORK__PARAMETER__BAND = 25,
	CODEC_KEY_E_NETWORK__PARAMETER__EARFCN = 26,
	CODEC_KEY_E_THERMOMETER = 27,
	CODEC_KEY_E_THERMOMETER__TEMPERATURE = 28,
	CODEC_KEY_E_ACCELEROMETER = 29,
	CODEC_KEY_E_ACCELEROMETER__ACCEL_X = 30,
	CODEC_KEY_E_ACCELEROMETER__ACCEL_Y = 31,
	CODEC_KEY_E_ACCELEROMETER__ACCEL_Z = 32,
	CODEC_KEY_E_ACCELEROMETER__ORIENTATION = 33,
	CODEC_KEY_E_BACKUP = 34,
	CODEC_KEY_E_BACKUP__LINE_VOLTAGE = 35,
	CODEC_KEY_E_BACKUP__BATT_VOLTAGE = 36,
	CODEC_KEY_E_BACKUP__STATE = 37,
	CODEC_KEY_E_BACKUP__EVENTS = 38,
	CODEC_KEY_E_BUTTONS = 39,
	CODEC_KEY_E_BUTTONS__BUTTON = 40,
	CODEC_KEY_E_BUTTONS__COUNT_CLICK = 41,
	CODEC_KEY_E_BUTTONS__COUNT_HOLD = 42,
	CODEC_KEY_E_BUTTONS__EVENTS = 43,
	CODEC_KEY_E_TAMPER = 44,
	CODEC_KEY_E_TAMPER__STATE = 45,
	CODEC_KEY_E_TAMPER__EVENTS = 46,
	CODEC_KEY_E_BLE_TAGS = 47,
	CODEC_KEY_E_BLE_TAGS__ADDR = 48,
	CODEC_KEY_E_BLE_TAGS__RSSI = 49,
	CODEC_KEY_E_BLE_TAGS__VOLTAGE = 50,
	CODEC_KEY_E_BLE_TAGS__TEMPERATURE = 51,
	CODEC_KEY_E_BLE_TAGS__TEMPERATURE__MEASUREMENTS = 52,
	CODEC_KEY_E_BLE_TAGS__HUMIDITY = 53,
	CODEC_KEY_E_BLE_TAGS__HUMIDITY__MEASUREMENTS = 54,
};

#define CODEC_CLOUD_OPTIONS_STATIC(_name)                                                          \
	static const uint8_t _name##_cloud_decoder[] = CLOUD_DECODER_BUFFER;                       \
	static struct ctr_cloud_options _name = {                                                  \
		.decoder_hash = CODEC_CLOUD_DECODER_HASH,                                          \
		.encoder_hash = CODEC_CLOUD_ENCODER_HASH,                                          \
		.decoder_buf = _name##_cloud_decoder,                                              \
		.decoder_len = 1220,                                                               \
		.encoder_buf = NULL,                                                               \
		.encoder_len = 0,                                                                  \
	}

#define CLOUD_DECODER_BUFFER                                                                        \
	{                                                                                           \
		0xa4, 0x67, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x02, 0x64, 0x74, 0x79, 0x70, \
		0x65, 0x67, 0x64, 0x65, 0x63, 0x6f, 0x64, 0x65, 0x72, 0x64, 0x6e, 0x61, 0x6d, 0x65, \
		0x78, 0x1e, 0x63, 0x6f, 0x6d, 0x2e, 0x68, 0x61, 0x72, 0x64, 0x77, 0x61, 0x72, 0x69, \
		0x6f, 0x2e, 0x63, 0x68, 0x65, 0x73, 0x74, 0x65, 0x72, 0x2e, 0x61, 0x70, 0x70, 0x2e, \
		0x70, 0x75, 0x73, 0x68, 0x66, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x8a, 0xa1, 0x67, \
		0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x83, 0xa1, 0x67, 0x76, 0x65, 0x72, 0x73, \
		0x69, 0x6f, 0x6e, 0xf6, 0xa1, 0x68, 0x73, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, \
		0xf6, 0xa1, 0x69, 0x74, 0x69, 0x6d, 0x65, 0x73, 0x74, 0x61, 0x6d, 0x70, 0xf6, 0xa1, \
		0x69, 0x61, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x86, 0xa1, 0x6b, 0x76, \
		0x65, 0x6e, 0x64, 0x6f, 0x72, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0xf6, 0xa1, 0x6c, 0x70, \
		0x72, 0x6f, 0x64, 0x75, 0x63, 0x74, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0xf6, 0xa1, 0x6a, \
		0x68, 0x77, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0xf6, 0xa1, 0x6b, 0x68, \
		0x77, 0x5f, 0x72, 0x65, 0x76, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0xf6, 0xa1, 0x6a, 0x66, \
		0x77, 0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0xf6, 0xa1, 0x6d, 0x73, 0x65, \
		0x72, 0x69, 0x61, 0x6c, 0x5f, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0xf6, 0xa1, 0x66, \
		0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x84, 0xa1, 0x66, 0x75, 0x70, 0x74, 0x69, 0x6d, \
		0x65, 0xf6, 0xa1, 0x6c, 0x76, 0x6f, 0x6c, 0x74, 0x61, 0x67, 0x65, 0x5f, 0x6c, 0x6f, \
		0x61, 0x64, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x19, 0x03, 0xe8, 0xa1, 0x64, \
		0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x6c, 0x76, 0x6f, 0x6c, 0x74, 0x61, 0x67, 0x65, \
		0x5f, 0x72, 0x65, 0x73, 0x74, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x19, 0x03, \
		0xe8, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x6c, 0x63, 0x75, 0x72, 0x72, \
		0x65, 0x6e, 0x74, 0x5f, 0x6c, 0x6f, 0x61, 0x64, 0xf6, 0xa1, 0x67, 0x6e, 0x65, 0x74, \
		0x77, 0x6f, 0x72, 0x6b, 0x81, 0xa1, 0x69, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, \
		0x65, 0x72, 0x89, 0xa1, 0x64, 0x65, 0x65, 0x73, 0x74, 0xf6, 0xa1, 0x63, 0x65, 0x63, \
		0x6c, 0xf6, 0xa1, 0x64, 0x72, 0x73, 0x72, 0x70, 0xf6, 0xa1, 0x64, 0x72, 0x73, 0x72, \
		0x71, 0xf6, 0xa1, 0x63, 0x73, 0x6e, 0x72, 0xf6, 0xa1, 0x64, 0x70, 0x6c, 0x6d, 0x6e, \
		0xf6, 0xa1, 0x63, 0x63, 0x69, 0x64, 0xf6, 0xa1, 0x64, 0x62, 0x61, 0x6e, 0x64, 0xf6, \
		0xa1, 0x66, 0x65, 0x61, 0x72, 0x66, 0x63, 0x6e, 0xf6, 0xa1, 0x6b, 0x74, 0x68, 0x65, \
		0x72, 0x6d, 0x6f, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x81, 0xa1, 0x6b, 0x74, 0x65, 0x6d, \
		0x70, 0x65, 0x72, 0x61, 0x74, 0x75, 0x72, 0x65, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, \
		0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x6d, 0x61, 0x63, \
		0x63, 0x65, 0x6c, 0x65, 0x72, 0x6f, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x84, 0xa1, 0x67, \
		0x61, 0x63, 0x63, 0x65, 0x6c, 0x5f, 0x78, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, \
		0x19, 0x03, 0xe8, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x67, 0x61, 0x63, \
		0x63, 0x65, 0x6c, 0x5f, 0x79, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x19, 0x03, \
		0xe8, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x67, 0x61, 0x63, 0x63, 0x65, \
		0x6c, 0x5f, 0x7a, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x19, 0x03, 0xe8, 0xa1, \
		0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x6b, 0x6f, 0x72, 0x69, 0x65, 0x6e, 0x74, \
		0x61, 0x74, 0x69, 0x6f, 0x6e, 0xf6, 0xa1, 0x66, 0x62, 0x61, 0x63, 0x6b, 0x75, 0x70, \
		0x84, 0xa1, 0x6c, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x76, 0x6f, 0x6c, 0x74, 0x61, 0x67, \
		0x65, 0xf6, 0xa1, 0x6c, 0x62, 0x61, 0x74, 0x74, 0x5f, 0x76, 0x6f, 0x6c, 0x74, 0x61, \
		0x67, 0x65, 0xf6, 0xa1, 0x65, 0x73, 0x74, 0x61, 0x74, 0x65, 0x81, 0xa1, 0x65, 0x24, \
		0x65, 0x6e, 0x75, 0x6d, 0x82, 0x6c, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, \
		0x63, 0x74, 0x65, 0x64, 0x69, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x65, 0x64, \
		0xa1, 0x66, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x82, 0xa1, 0x64, 0x24, 0x6b, 0x65, \
		0x79, 0x66, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0xa1, 0x64, 0x24, 0x74, 0x73, 0x6f, \
		0x81, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x81, 0xa1, 0x65, 0x24, 0x65, 0x6e, 0x75, \
		0x6d, 0x82, 0x6c, 0x64, 0x69, 0x73, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x65, \
		0x64, 0x69, 0x63, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x65, 0x64, 0xa1, 0x67, 0x62, \
		0x75, 0x74, 0x74, 0x6f, 0x6e, 0x73, 0x84, 0xa1, 0x66, 0x62, 0x75, 0x74, 0x74, 0x6f, \
		0x6e, 0x81, 0xa1, 0x65, 0x24, 0x65, 0x6e, 0x75, 0x6d, 0x85, 0x68, 0x62, 0x75, 0x74, \
		0x74, 0x6f, 0x6e, 0x5f, 0x78, 0x68, 0x62, 0x75, 0x74, 0x74, 0x6f, 0x6e, 0x5f, 0x31, \
		0x68, 0x62, 0x75, 0x74, 0x74, 0x6f, 0x6e, 0x5f, 0x32, 0x68, 0x62, 0x75, 0x74, 0x74, \
		0x6f, 0x6e, 0x5f, 0x33, 0x68, 0x62, 0x75, 0x74, 0x74, 0x6f, 0x6e, 0x5f, 0x34, 0xa1, \
		0x6b, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x63, 0x6c, 0x69, 0x63, 0x6b, 0xf6, 0xa1, \
		0x6a, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x5f, 0x68, 0x6f, 0x6c, 0x64, 0xf6, 0xa1, 0x66, \
		0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x81, 0xa1, 0x64, 0x24, 0x74, 0x73, 0x6f, 0x81, \
		0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x81, 0xa1, 0x65, 0x24, 0x65, 0x6e, 0x75, 0x6d, \
		0x82, 0x67, 0x63, 0x6c, 0x69, 0x63, 0x6b, 0x65, 0x64, 0x64, 0x68, 0x65, 0x6c, 0x64, \
		0xa1, 0x66, 0x74, 0x61, 0x6d, 0x70, 0x65, 0x72, 0x82, 0xa1, 0x65, 0x73, 0x74, 0x61, \
		0x74, 0x65, 0x81, 0xa1, 0x65, 0x24, 0x65, 0x6e, 0x75, 0x6d, 0x82, 0x68, 0x69, 0x6e, \
		0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0x66, 0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0xa1, \
		0x66, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x82, 0xa1, 0x64, 0x24, 0x6b, 0x65, 0x79, \
		0x66, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0xa1, 0x64, 0x24, 0x74, 0x73, 0x6f, 0x81, \
		0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x81, 0xa1, 0x65, 0x24, 0x65, 0x6e, 0x75, 0x6d, \
		0x82, 0x6b, 0x64, 0x65, 0x61, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x65, 0x64, 0x69, \
		0x61, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x65, 0x64, 0xa1, 0x68, 0x62, 0x6c, 0x65, \
		0x5f, 0x74, 0x61, 0x67, 0x73, 0x85, 0xa1, 0x64, 0x61, 0x64, 0x64, 0x72, 0xf6, 0xa1, \
		0x64, 0x72, 0x73, 0x73, 0x69, 0xf6, 0xa1, 0x67, 0x76, 0x6f, 0x6c, 0x74, 0x61, 0x67, \
		0x65, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, \
		0x70, 0x70, 0x02, 0xa1, 0x6b, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x75, \
		0x72, 0x65, 0x81, 0xa1, 0x6c, 0x6d, 0x65, 0x61, 0x73, 0x75, 0x72, 0x65, 0x6d, 0x65, \
		0x6e, 0x74, 0x73, 0x81, 0xa1, 0x64, 0x24, 0x74, 0x73, 0x70, 0x84, 0xa1, 0x63, 0x6d, \
		0x69, 0x6e, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, \
		0x66, 0x70, 0x70, 0x02, 0xa1, 0x63, 0x6d, 0x61, 0x78, 0x82, 0xa1, 0x64, 0x24, 0x64, \
		0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x63, 0x61, \
		0x76, 0x67, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, \
		0x66, 0x70, 0x70, 0x02, 0xa1, 0x63, 0x6d, 0x64, 0x6e, 0x82, 0xa1, 0x64, 0x24, 0x64, \
		0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x68, 0x68, \
		0x75, 0x6d, 0x69, 0x64, 0x69, 0x74, 0x79, 0x81, 0xa1, 0x6c, 0x6d, 0x65, 0x61, 0x73, \
		0x75, 0x72, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x73, 0x81, 0xa1, 0x64, 0x24, 0x74, 0x73, \
		0x70, 0x84, 0xa1, 0x63, 0x6d, 0x69, 0x6e, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, \
		0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x63, 0x6d, 0x61, 0x78, \
		0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, \
		0x70, 0x02, 0xa1, 0x63, 0x61, 0x76, 0x67, 0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, \
		0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, 0x70, 0x02, 0xa1, 0x63, 0x6d, 0x64, 0x6e, \
		0x82, 0xa1, 0x64, 0x24, 0x64, 0x69, 0x76, 0x18, 0x64, 0xa1, 0x64, 0x24, 0x66, 0x70, \
		0x70, 0x02,                                                                         \
	}

#ifdef __cplusplus
}
#endif

#endif /* CODEC_H_ */