var cursor = 0;
var buffer;

// ChirpStack v4:
function decodeUplink(input) {
  cursor = 0;

  buffer = input.bytes;

  var data = {};

  var header = u8();

  if (header & BIT(7)) {
    header = header | (u8() << 8);
  }

  /* Flag BATT */
  if ((header & BIT(0)) !== 0) {
    decode_batt(data);
  }

  /* Flag ACCEL */
  if ((header & BIT(1)) !== 0) {
    decode_accel(data);
  }

  /* Flag THERM */
  if ((header & BIT(2)) !== 0) {
    decode_therm(data);
  }

  /* Flag METEO */
  if ((header & BIT(3)) !== 0) {
    decode_meteo(data);
  }

  /* Flag HYGRO */
  if ((header & BIT(4)) !== 0) {
    decode_hygro(data);
  }

  /* Flag W1 */
  if ((header & BIT(5)) !== 0) {
    decode_w1(data);
  }

  /* Flag BLE Tags */
  if ((header & BIT(6)) !== 0) {
    decode_ble_tags(data);
  }

  // Bit 7 is reserved for extended header

  /* Flag Barometer tag */
  if ((header & BIT(8)) !== 0) {
    decode_barometer_tag(data);
  }

    /* Flag Soil sensor */
    if ((header & BIT(9)) !== 0) {
        decode_soil_sensor(data);
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

function decode_meteo(data) {
  data.wind_speed = u16();
  if (data.wind_speed === 0xffff) {
    data.wind_speed = null;
  } else {
    data.wind_speed /= 100;
  }

  data.wind_direction = u16();
  if (data.wind_direction === 0xffff) {
    data.wind_direction = null;
  }

  data.rainfall = u16();
  if (data.rainfall === 0xffff) {
    data.rainfall = null;
  } else {
    data.rainfall /= 100;
  }
}

function decode_barometer_tag(data) {
  data.barometer = u32();
  if (data.barometer === 0xffffffff) {
    data.barometer = null;
  } else {
    data.barometer = data.barometer / 1000;
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

function BIT(index) {
  return 1 << index;
}

function s8() {
  var value = buffer.slice(cursor)[0];
  cursor += 1;
  return value > 0x7f ? value - 0x100 : value;
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
  return value > 0x7fff ? value - 0x10000 : value;
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
  value = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
  cursor += 4;
  return value;
}

if (false) {
  var hex = "7fffffffffff0266090000000000000000000000";
  var buf = Buffer.from(hex, "hex");
  console.log(JSON.stringify(decodeUplink({ bytes: buf })));
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
