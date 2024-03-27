#include <chester/ctr_info.h>

int ctr_info_get_vendor_name(char **vendor_name)
{
	static char name[] = "HARDWARIO";
	*vendor_name = name;
	return 0;
}

int ctr_info_get_product_name(char **product_name)
{
	static char name[] = "CHESTER-M";
	*product_name = name;
	return 0;
}

int ctr_info_get_hw_variant(char **hw_variant)
{
	static char variant[] = "CGLS";
	*hw_variant = variant;
	return 0;
}
int ctr_info_get_hw_revision(char **hw_revision)
{
	static char revision[] = "R3.2";
	*hw_revision = revision;
	return 0;
}

int ctr_info_get_fw_bundle(char **fw_bundle)
{
	static char bundle[] = "com.hardwario.mockup-app";
	*fw_bundle = bundle;
	return 0;
}

int ctr_info_get_fw_name(char **fw_name)
{
	static char name[] = "mockup-app";
	*fw_name = name;
	return 0;
}

int ctr_info_get_fw_version(char **fw_version)
{
	static char version[] = "v0.0.1";
	*fw_version = version;
	return 0;
}

int ctr_info_get_serial_number_uint32(uint32_t *serial_number)
{
	*serial_number = 2159017985;
	return 0;
}

int ctr_info_get_claim_token(char **claim_token)
{
	static char token[] = "98a8856ba6534bd5212176b22f3acbb3";
	*claim_token = token;
	return 0;
}

int ctr_info_get_ble_passkey(char **ble_passkey)
{
	static char passkey[] = "654123";
	*ble_passkey = passkey;
	return 0;
}
