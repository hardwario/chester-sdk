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

  if (input.bytes.length >= 11) {
    data.hygro_temperature = input.bytes[8] | input.bytes[9] << 8;
    data.hygro_humidity = input.bytes[10];

    if (data.hygro_temperature === 0x7fff) {
      data.hygro_temperature = null;
    } else if (data.hygro_temperature & 0x8000) {
      data.hygro_temperature = -((~data.hygro_temperature & 0xffff) + 1) / 100;
    } else {
      data.hygro_temperature = data.hygro_temperature / 100;
    }

    if (data.hygro_humidity === 0xff) {
      data.hygro_humidity = null;
    } else {
      data.hygro_humidity = data.hygro_humidity / 2;
    }
  }

  return {
    data: data,
    warnings: [],
    errors: []
  };
}
