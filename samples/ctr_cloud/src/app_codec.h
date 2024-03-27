#ifndef CODEC_H_
#define CODEC_H_

/* This file has been generated using the script gen-codec.py */

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_CLOUD_DECODER_HASH ((uint64_t)0)
#define CODEC_CLOUD_ENCODER_HASH ((uint64_t)0)

#define CODEC_CLOUD_OPTIONS_STATIC(_name) \
	static struct ctr_cloud_options _name = { \
		.decoder_hash = CODEC_CLOUD_DECODER_HASH, \
		.encoder_hash = CODEC_CLOUD_ENCODER_HASH, \
		.decoder_buf = NULL, \
		.decoder_len = 0, \
		.encoder_buf = NULL, \
		.encoder_len = 0, \
}

#ifdef __cplusplus
}
#endif

#endif /* CODEC_H_ */
