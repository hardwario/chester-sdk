if (msg.payload.applicationName !== 'ember-application-chester-clime') {
    return null;
}

if (typeof msg.payload.data === 'string') {
    msg.payload.data = decode(Buffer.from(msg.payload.data, 'base64'));
}

return msg;

function decode(buffer) {
    var data = {};

    if (buffer.length >= 8) {
        data.voltage_rest = buffer.readUInt16LE(0);
        data.voltage_load = buffer.readUInt16LE(2);
        data.current_load = buffer.readUInt8(4);
        data.orientation = buffer.readUInt8(5);
        data.therm_temperature = buffer.readInt16LE(6);

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
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    if (buffer.length >= 11) {
        data.hygro_temperature = buffer.readInt16LE(8);
        data.hygro_humidity = buffer.readUInt8(10);

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

    return data;
}
