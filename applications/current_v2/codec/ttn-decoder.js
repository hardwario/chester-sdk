function decodeUplink(input) {
  var data = {};

  if (input.bytes.length >= 8) {
    data.voltage_rest = input.bytes[0] | input.bytes[1] << 8;
    data.voltage_load = input.bytes[2] | input.bytes[3] << 8;
    data.current_load = input.bytes[4];
    data.orientation = input.bytes[5];
    data.therm_temperature = input.bytes[6] | input.bytes[7] << 8;

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

    if (data.orientation === 0xff) {
      data.orientation = null;
    }

    if (data.therm_temperature === 0x7fff) {
      data.therm_temperature = null;
    } else if (data.therm_temperature & 0x8000) {
      data.therm_temperature = -((~data.therm_temperature & 0xffff) + 1) / 100;
    } else {
      data.therm_temperature = data.therm_temperature / 100;
    }
  }

  return {
    data: data,
    warnings: [],
    errors: []
  };
}
