var cursor = 0;
var buffer;

// Chirpstack v4
function decodeUplink(input) {

  cursor = 0;
  buffer = input.bytes || input;

  var data = {};

  var header = u8();

  if (header & 0x80) {
    header = header | (u8() << 8);
  }

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
    // TODO - refactor S1
    decode_iaq(data);
  }

  if (header & BIT(4)) {
    decode_hygro(data);
  }

  if (header & BIT(5)) {
    decode_w1(data);
  }

  if (header & BIT(6)) {
    decode_rtd(data);
  }

  if (header & BIT(8)) {
    decode_ble_tags(data);
  }

  if (header & BIT(9)) {
    decode_soil_sensor(data);
  }

  if (header & BIT(10)) {
    decode_tc(data);
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

// TODO refactor S1
function decode_iaq(data) {
  data.therm_temperature = s16();
  if (data.therm_temperature === 0x7fff) {
    data.therm_temperature = null;
  } else {
    data.therm_temperature = data.therm_temperature / 100;
  }

  data.hygro_humidity = u16();
  if (data.hygro_humidity === 0xffff) {
    data.hygro_humidity = null;
  } else {
    data.hygro_humidity = data.hygro_humidity / 100;
  }

  data.altitude = u16();
  if (data.altitude === 0xffff) {
    data.altitude = null;
  } else {
    data.altitude = data.altitude / 100;
  }

  data.co2 = u16();
  if (data.co2 === 0xffff) {
    data.co2 = null;
  } else {
    data.co2 = data.co2 / 100;
  }

  data.illuminance = u16();
  if (data.illuminance === 0xffff) {
    data.illuminance = null;
  } else {
    data.illuminance = data.illuminance / 100;
  }

  data.pressure = u16();
  if (data.pressure === 0xffff) {
    data.pressure = null;
  } else {
    data.pressure = data.pressure / 100;
  }
}

function decode_hygro(data) {
  data.hygro_temperature = s16();
  data.hygro_humidity = u8();

  if (data.hygro_temperature === 0x7fff) {
    data.hygro_temperature = null;
  } else {
    data.hygro_temperature = data.hygro_temperature / 100;
  }

  if (data.hygro_humidity === 0xff) {
    data.hygro_humidity = null;
  } else {
    data.hygro_humidity = data.hygro_humidity / 2;
  }
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

function decode_rtd(data) {
  data.rtd_thermometers = [];

  var count = u8();

  for (var i = 0; i < count; i++) {
    var t = s16();

    if (t === 0x7fff) {
      t = null;
    } else {
      t = t / 100;
    }

    data.rtd_thermometers.push(t);
  }
}

function decode_tc(data) {
  data.tc_thermometers = [];

  var count = u8();

  for (var i = 0; i < count; i++) {
    var t = s16();

    if (t === 0x7fff) {
      t = null;
    } else {
      t = t / 100;
    }

    data.tc_thermometers.push(t);
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

function decode_soil_sensor(data) {
  data.soil_sensors = [];

  let count = u8();

  for (let i = 0; i < count; i++) {
    let sensor = {};

    let temperature = s16();
    if (temperature === 0x7fff) {
      sensor.temperature = null;
    } else {
      sensor.temperature = temperature / 100;
    }

    let moisture = u16();
    if (moisture === 0xffff) {
      sensor.moisture = null;
    } else {
      sensor.moisture = moisture;
    }

    data.soil_sensors.push(sensor);
  }
}

function decode_x0_a_inputs(data) {
  data.inputs_a = [];
  for (i = 0; i < 4; i++) {
    var type = u8();
    switch (type) {
      case 0: // trigger
        var val = u8();
        var input = {
          type: 'trigger',
          state: val & 0x01 ? true : false,
          trigger_active: (val & 0x02) ? true : false,
          trigger_inactive: (val & 0x04) ? true : false
        };
        break;
      case 1: // counter
        var input = {
          type: 'counter',
          count: u32(),
          delta: u16()
        };
        break;
      case 2: // voltage
        var value = u16();
        if (value === 0xffff) {
          value = null;
        } else {
          value = value / 100;
        }
        var input = {
          type: 'voltage',
          voltage: value
        };
        break;
      case 3: // current
        var value = u16();
        if (value === 0xffff) {
          value = null;
        } else {
          value = value / 100;
        }
        var input = {
          type: 'current',
          current: value
        };
        break;
    }
    data.inputs_a.push(input);
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
