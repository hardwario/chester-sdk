var cursor = 0;
var buffer;

// ChirpStack v4:
function decodeUplink(input) {
    cursor = 0;

    buffer = input.bytes;

    var data = {};

    var header = u8();

	if (header & BIT(0)) {
		decode_batt(data);
	}

	if (header & BIT(1)) {
		decode_accel(data);
	}

	if (header & BIT(2)) {
		decode_therm(data);
	}

	if (header & (BIT(3))) {
		decode_backup(data);
        decode_buttons(data);
	}


	return { data: data };
}

// Common parser functions
// Do not touch unless needed
// Keep in sync with other catalog apps

// The Things Network
function Decoder(bytes, port) {
    var decoded = decodeUplink({ bytes: bytes, fPort: port });
    return decoded.data;
}

// Chirpstack v3
function Decode(fPort, bytes) {
    var decoded = decodeUplink({ fPort: fPort, bytes: bytes });
    return decoded.data;
}

function decode_batt(data) {
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

function decode_accel(data) {
    data.orientation = u8();

    if (data.orientation === 0xff) {
        data.orientation = null;
    }
}

function decode_therm(data) {
    data.therm_temperature = s16();
    if (data.therm_temperature === 0x7fff) {
        data.therm_temperature = null;
    } else {
        data.therm_temperature /= 100;
    }
}

function decode_backup(data) {
    data.backup = {};

    data.backup.line_voltage = u16();
    data.backup.battery_voltage = u16();
    data.backup.backup_state = u8() !== 0 ? "connected" : "disconnected";

    if (data.backup.line_voltage === 0xffff) {
        data.backup.line_voltage = null;
    } else {
        data.backup.line_voltage = data.backup.line_voltage / 1000;
    }

    if (data.backup.battery_voltage === 0xffff) {
        data.backup.battery_voltage = null;
    } else {
        data.backup.battery_voltage = data.backup.battery_voltage / 1000;
    }

}
function decode_buttons(data) {
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
    data.button_x.press_event = (events & BIT(0)) !== 0; // 0000 0000 0000 0001
    data.button_x.hold_event = (events & BIT(1)) !== 0; // 0000 0000 0000 0010

    data.button_1.press_event = (events & BIT(2)) !== 0; // 0000 0000 0000 0100
    data.button_1.hold_event = (events & BIT(3)) !== 0; // 0000 0000 0000 1000

    data.button_2.press_event = (events & BIT(4)) !== 0; // 0000 0000 0001 0000
    data.button_2.hold_event = (events & BIT(5)) !== 0; // 0000 0000 0010 0000

    data.button_3.press_event = (events & BIT(6)) !== 0; // 0000 0000 0100 0000
    data.button_3.hold_event = (events & BIT(7)) !== 0; // 0000 0000 1000 0000

    data.button_4.press_event = (events & BIT(8)) !== 0; // 0000 0001 0000 0000
    data.button_4.hold_event = (events & BIT(9)) !== 0; // 0000 0010 0000 0000
}

function BIT(index) {
    return (1 << index);
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

if (typeof module !== "undefined") {
    module.exports = {
        decodeUplink,
        Decoder,
        Decode,
        s8,
        u8,
        s16,
        u16,
        s32,
        u32,
        //setBuffer
    };
}
