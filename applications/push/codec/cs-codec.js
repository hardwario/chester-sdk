var cursor = 0;
var buffer;

// ChirpStack v3
function Decode(port, bytes, variables)
{
	var data = {};
	buffer = bytes;

	var header = u8();

	if ((header & 0x01) !== 0) {
		data.voltage_rest = u16();
		data.voltage_load = u16();
		data.current_load = u8();

		if (data.voltage_rest === 0xffff) {
			data.voltage_rest = null;
		} else {
			data.voltage_rest = data.voltage_rest / 1000;
		}

		if (data.voltage_load === 0xffff) {
			data.voltage_load = null;
		} else {
			data.voltage_load = data.voltage_load / 1000;
		}

		if (data.current_load === 0xff) {
			data.current_load = null;
		}
	}

	if ((header & 0x02) !== 0) {
		data.orientation = u8();

		if (data.orientation === 0xff) {
			data.orientation = null;
		}
	}

	if ((header & 0x04) !== 0) {
		data.therm_temperature = s16();

		if (data.therm_temperature === 0x7fff) {
			data.therm_temperature = null;
		} else {
			data.therm_temperature = data.therm_temperature / 100;
		}
	}

	if ((header & 0x08) !== 0) {
		data.backup = {};

		data.backup.line_voltage = u16();
		data.backup.battery_voltage = u16();
		data.backup.backup_state = u8() !== 0 ? "connected" : "disconnected";

		if (data.backup.line_voltage === 0x7fff) {
			data.backup.line_voltage = null;
		} else {
			data.backup.line_voltage = data.backup.line_voltage / 1000;
		}

		if (data.backup.battery_voltage === 0x7fff) {
			data.backup.battery_voltage = null;
		} else {
			data.backup.battery_voltage = data.backup.battery_voltage / 1000;
		}

		data.button_x = {};
		data.button_x.press_count = u16();
		data.button_x.hold_count = u16();

		data.button_1 = {};
		data.button_1.press_count = u16();
		data.button_1.hold_count = u16();

		data.button_2 = {};
		data.button_2.press_count = u16();
		data.button_2.hold_count = u16();

		data.button_3 = {};
		data.button_3.press_count = u16();
		data.button_3.hold_count = u16();

		data.button_4 = {};
		data.button_4.press_count = u16();
		data.button_4.hold_count = u16();

		var events = u16();
		data.button_x.press_event = (events & 0x0001) !== 0;
		data.button_x.hold_event = (events & 0x0002) !== 0;

		data.button_1.press_event = (events & 0x0004) !== 0;
		data.button_1.hold_event = (events & 0x0008) !== 0;

		data.button_2.press_event = (events & 0x0010) !== 0;
		data.button_2.hold_event = (events & 0x0020) !== 0;

		data.button_3.press_event = (events & 0x0040) !== 0;
		data.button_3.hold_event = (events & 0x0080) !== 0;

		data.button_4.press_event = (events & 0x0100) !== 0;
		data.button_4.hold_event = (events & 0x0200) !== 0;
	}

	return data;
}

// ChirpStack v4 compatibility wrapper
function decodeUplink(input)
{
	return {data : Decode(input.fPort, input.bytes, input.variables)};
}

function s8()
{
	var value = buffer.slice(cursor);
	value = value[0];
	if ((value & (1 << 7)) > 0) {
		value = (~value & 0xff) + 1;
		value = -value;
	}
	cursor = cursor + 1;
	return value;
}

function u8()
{
	var value = buffer.slice(cursor);
	value = value[0];
	cursor = cursor + 1;
	return value;
}

function s16()
{
	var value = buffer.slice(cursor);
	value = value[0] | value[1] << 8;
	if ((value & (1 << 15)) > 0) {
		value = (~value & 0xffff) + 1;
		value = -value;
	}
	cursor = cursor + 2;
	return value;
}

function u16()
{
	var value = buffer.slice(cursor);
	value = value[0] | value[1] << 8;
	cursor = cursor + 2;
	return value;
}

function u32()
{
	var value = buffer.slice(cursor);
	value = value[0] | value[1] << 8 | value[2] << 16 | value[3] << 24;
	cursor = cursor + 4;
	return value;
}

function s32()
{
	var value = buffer.slice(cursor);
	value = value[0] | value[1] << 8 | value[2] << 16 | value[3] << 24;
	if ((value & (1 << 31)) > 0) {
		value = (~value & 0xffffffff) + 1;
		value = -value;
	}
	cursor = cursor + 4;
	return value;
}

if (false) {
	var buf = Buffer.from("D9sSXRIvASEH+EoaDQEAAAAAHAARABgACgAOAAQABgAEAAAA", 'base64')
	console.log(Decode(1, buf, 0));
}
