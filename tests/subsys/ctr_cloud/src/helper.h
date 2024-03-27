#ifndef TESTS_SUBSYS_CTR_CLOUD_SRC_HELPER_H_
#define TESTS_SUBSYS_CTR_CLOUD_SRC_HELPER_H_

#define PRINT_CTR_BUF(_name)                                                                       \
	printf("buffer: ");                                                                        \
	for (int i = 0; i < _name.len; i++) {                                                      \
		printf("%02x", _name.mem[i]);                                                      \
	}                                                                                          \
	printf("\n");

#endif
