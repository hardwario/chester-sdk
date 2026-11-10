var cursor = 0;
var buffer;

// Chirpstack v4
function decodeUplink(input) {

	// Keep for repeated unit tests
	cursor = 0;

	buffer = input.bytes || input;
	var data = {};

	var header = u8();

	if (header & BIT(0)) {
		decode_batt(data);
	}

	if (header & BIT(1)) {
		decode_accel(data);
	}

	if ((header & BIT(2))) {
		decode_therm(data);
	}

	if (header & BIT(3)) {
		decode_hygro(data);
	}

	if (header & BIT(4)) {
		decode_w1(data);
	}

	if (header & BIT(5)) {
		decode_ble_tags(data);
	}

	if (header & BIT(6)) {
		decode_x0_inputs(data, 'inputs_a', 4);
	}

	// A frame with only this bit set is the second uplink of an 8-channel report
	// that did not fit one frame: it carries channels 5-8 and nothing else.
	if (header & BIT(7)) {
		decode_x0_inputs(data, 'inputs_b', 4);
	}

	// The sensors group rides on the X0-A header bit; the header byte is full so
	// there is no bit of its own available.
	if (header & BIT(6)) {
		decode_sensors(data);
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

/* A value the device could not measure is left OUT of the output rather than
 * reported as null. A key that is absent reads as "no reading this uplink",
 * whereas a wall of `voltage_rest: null` style entries is noise that has to be
 * filtered downstream. The one deliberate exception is `flooded`, which stays
 * three-state (yes / no / null) because null there means "state unknown" and
 * consumers are expected to distinguish that from dry. */
function set_if_valid(data, key, value) {
	if (value !== null) {
		data[key] = value;
	}
}

function decode_batt(data) {
	var voltage_rest = u16();
	var voltage_load = u16();
	var current_load = u8();

	set_if_valid(data, 'voltage_rest', voltage_rest === 0xffff ? null : voltage_rest / 1000);
	set_if_valid(data, 'voltage_load', voltage_load === 0xffff ? null : voltage_load / 1000);
	set_if_valid(data, 'current_load', current_load === 0xff ? null : current_load);
}

function decode_accel(data) {
	var orientation = u8();

	set_if_valid(data, 'orientation', orientation === 0xff ? null : orientation);
}

function decode_therm(data) {
	var temperature = s16();

	set_if_valid(data, 'therm_temperature', temperature === 0x7fff ? null : temperature / 100);
}

function decode_hygro(data) {
	var temperature = s16();
	var humidity = u8();

	set_if_valid(data, 'hygro_temperature', temperature === 0x7fff ? null : temperature / 100);
	set_if_valid(data, 'hygro_humidity', humidity === 0xff ? null : humidity / 2);
}

function decode_w1(data) {
	data.w1_thermometers = [];

	var count = u8();

	for (var i = 0; i < count; i++) {
		var t = s16();

		if (t === 0x7fff) {
			t = null;
		} else {
			t = t / 100;
		}

		data.w1_thermometers.push(t);
	}
}

function decode_ble_tags(data) {
	data.ble_tags = [];

	var count = u8();

	for (var i = 0; i < count; i++) {
		var tag = {};

		var t = s16();
		var h = u8();

		if (t === 0x7fff) {
			t = null;
		} else {
			t = t / 100;
		}

		if (h === 0xff) {
			h = null;
		} else {
			h = h / 2;
		}

		tag.temperature = t;
		tag.humidity = h;

		data.ble_tags.push(tag);
	}
}

function decode_x0_inputs(data, key, count) {
	data[key] = [];
	for (var i = 0; i < count; i++) {
		var type = u8();
		var input;
		switch (type) {
			case 1: // trigger
				var val = u8();
				input = {
					type: 'trigger',
					state: val & 0x01 ? true : false,
					trigger_active: (val & 0x02) ? true : false,
					trigger_inactive: (val & 0x04) ? true : false
				};
				break;
			case 2: // counter
				input = {
					type: 'counter',
					count: u32(),
					delta: u16()
				};
				break;
			case 3: // voltage
				var value = u16();
				input = { type: 'voltage' };
				/* A sensor's measure channel reports the type with no value at
				 * all: its reading belongs to data.sensors, so 0xffff means
				 * "nothing to report here" and the key is left out entirely
				 * rather than shown as null. */
				set_if_valid(input, 'voltage', value === 0xffff ? null : value / 100);
				break;
			case 4: // current
				var value = u16();
				input = { type: 'current' };
				set_if_valid(input, 'current', value === 0xffff ? null : value / 100);
				break;
			case 5: // power - driven by the sensor layer, no payload
				// Readings for this channel live in data.sensors, not here.
				input = { type: 'power' };
				break;
			default: // disabled or unknown - skip
				input = { type: 'disabled' };
				break;
		}
		data[key].push(input);
	}
}

// Pluggable SENSORS group, appended after the X0 blocks. Layout:
//   [u8 count] then per sensor [u8 type][u8 status][u8 data_len][data]
//   status: bit0 = valid, bits 4-5 = slot (0-based), so `index` matches the CBOR
//   `index` and the `sensor-N` config key even when a lower slot is empty
// Each sensor's data is length-prefixed so an unknown type can be skipped whole.
// Guarded on there being at least one byte left, which makes this safe for stock
// firmware that emits nothing here.
function decode_sensors(data) {
	if ((buffer.length - cursor) < 1) {
		return;
	}

	var count = u8();
	data.sensors = [];

	for (var i = 0; i < count; i++) {
		var type = u8();
		var status = u8();
		var valid = !!(status & 0x01);
		var len = u8();
		var end = cursor + len;
		var sensor = { index: ((status >> 4) & 0x03) + 1, type: type, valid: valid, data: {} };

		if (end > buffer.length) {
			break; // truncated frame: never read past the end
		}

		if (type === 1 && len >= 3) { // LD-81 flood detector
			var mv = u16(); // 0xffff = invalid
			var flooded = u8();
			// volts, matching the CBOR codec (cbor-decoder.yaml $div 1000 /
			// $fpp 2); omitted entirely when there is no reading
			set_if_valid(sensor.data, 'sensor_voltage',
				     (valid && mv !== 0xffff) ? mv / 1000 : null);
			/* Three-state on purpose: "yes" flooded / "no" dry / null unknown.
			 * Unlike the others this one is NOT omitted — null here means the
			 * state could not be determined, which a consumer must be able to
			 * tell apart from dry. */
			sensor.data.flooded = valid ? (flooded ? 'yes' : 'no') : null;
		}

		cursor = end; // skip anything unread, including unknown types
		data.sensors.push(sensor);
	}
}

function BIT(index) {
	return (1 << index);
}

function s8() {
	var value = buffer.slice(cursor)[0];
	cursor += 1;
	return value > 0x7F ? value - 0x100 : value;
}

function u8() {
	var value = buffer.slice(cursor)[0];
	cursor += 1;
	return value;
}

function s16() {
	var value = buffer.slice(cursor);
	value = value[0] | (value[1] << 8);
	cursor += 2;
	return value > 0x7FFF ? value - 0x10000 : value;
}

function u16() {
	var value = buffer.slice(cursor);
	value = value[0] | (value[1] << 8);
	cursor += 2;
	return value;
}

function u32() {
	var value = buffer.slice(cursor);
	value = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);
	cursor += 4;
	return value >>> 0; // convert to unsigned 32-bit
}

function s32() {
	var value = buffer.slice(cursor);
	value =
		(buffer[0]) |
		(buffer[1] << 8) |
		(buffer[2] << 16) |
		(buffer[3] << 24);
	cursor += 4;
	return value;
}

// Used for testing
function setBuffer(buf) {
	buffer = buf;
	cursor = 0;
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
		setBuffer
	};

}
