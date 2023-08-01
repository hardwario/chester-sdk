if (msg.payload.applicationName !== 'application-chester-push') {
    return null;
}

if (typeof msg.payload.data === 'string') {
    msg.payload.data = decode(Buffer.from(msg.payload.data, 'base64'));
}

return msg;

function decode(buffer) {
    let data = {};

    let offset = 0;

    let header = buffer.readUInt8(0);
    offset += 1;

    if ((header & 0x01) !== 0) {
        data.voltage_rest = buffer.readUInt16LE(offset);
        offset += 2;

        data.voltage_load = buffer.readUInt16LE(offset);
        offset += 2;

        data.current_load = buffer.readUInt8(offset);
        offset += 1;

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
        data.orientation = buffer.readUInt8(offset);
        offset += 1;

        if (data.orientation === 0xff) {
            data.orientation = null;
        }
    }

    if ((header & 0x04) !== 0) {
        data.therm_temperature = buffer.readInt16LE(offset);
        offset += 2;

        if (data.therm_temperature === 0x7fff) {
            data.therm_temperature = null;
        } else {
            data.therm_temperature = data.therm_temperature / 100;
        }
    }

    if ((header & 0x08) !== 0) {
        data.backup = {}

        data.backup.line_voltage = buffer.readUInt16LE(offset);
        offset += 2;

        data.backup.battery_voltage = buffer.readUInt16LE(offset);
        offset += 2;

        data.backup.backup_state = buffer.readUInt8(offset) !== 0 ? "connected" : "disconnected";
        offset += 1;

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

        data.button_x.press_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_x.hold_count = buffer.readUInt16LE(offset);
        offset += 2;

        data.button_1 = {};

        data.button_1.press_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_1.hold_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_2 = {};

        data.button_2.press_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_2.hold_count = buffer.readUInt16LE(offset);
        offset += 2;

        data.button_3 = {};

        data.button_3.press_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_3.hold_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_4 = {};

        data.button_4.press_count = buffer.readInt16LE(offset);
        offset += 2;

        data.button_4.hold_count = buffer.readInt16LE(offset);
        offset += 2;

        let events = buffer.readInt16LE(offset);
        offset += 2;

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
