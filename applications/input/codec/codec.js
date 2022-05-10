var cursor = 0;
var buffer;

// Uncomment for ChirpStack:
function Decode(port, bytes, variables) {

    // Uncomment for The Things Network:
    // function Decoder(port, bytes) {
    buffer = bytes;

    var timestamp_seconds = u32();
    var timestamp_milliseconds = u16();
    var flags = u32();
    var states_orientation = u8();
    var states_int_temperature = s16();
    var states_ext_temperature = s16();
    var states_ext_humidity = u8();
    var states_input_1_active = (flags & 0x00000001) !== 0 ? false : true;
    var states_input_2_active = (flags & 0x00000002) !== 0 ? false : true;
    var states_input_3_active = (flags & 0x00000004) !== 0 ? false : true;
    var states_input_4_active = (flags & 0x00000008) !== 0 ? false : true;
    var states_input_5_active = (flags & 0x00000010) !== 0 ? false : true;
    var states_input_6_active = (flags & 0x00000020) !== 0 ? false : true;
    var states_input_7_active = (flags & 0x00000040) !== 0 ? false : true;
    var states_input_8_active = (flags & 0x00000080) !== 0 ? false : true;
    var states_line_present = (flags & 0x40000000) !== 0 ? true : false;
    var states_line_voltage = u16();
    var states_bckp_voltage = u16();
    var states_batt_voltage_rest = u16();
    var states_batt_voltage_load = u16();
    var states_batt_current_load = u16();
    var events_device_boot = u16();
    var events_device_tilt = u16();
    var events_input_1_deactivation = u8();
    var events_input_1_activation = u8();
    var events_input_2_deactivation = u8();
    var events_input_2_activation = u8();
    var events_input_3_deactivation = u8();
    var events_input_3_activation = u8();
    var events_input_4_deactivation = u8();
    var events_input_4_activation = u8();
    var events_input_5_deactivation = u8();
    var events_input_5_activation = u8();
    var events_input_6_deactivation = u8();
    var events_input_6_activation = u8();
    var events_input_7_deactivation = u8();
    var events_input_7_activation = u8();
    var events_input_8_deactivation = u8();
    var events_input_8_activation = u8();

    return {
        timestamp: timestamp_seconds + timestamp_milliseconds / 1000,
        states: {
            orientation: states_orientation === 0 ? null : states_orientation,
            int_temperature: states_int_temperature === 0x7fff ? null : states_int_temperature / 100,
            ext_temperature: states_ext_temperature === 0x7fff ? null : states_ext_temperature / 100,
            ext_humidity: states_ext_humidity === 0xff ? null : states_ext_humidity / 2,
            input_1_active: (flags & 0x00000100) !== 0 ? null : states_input_1_active,
            input_2_active: (flags & 0x00000200) !== 0 ? null : states_input_2_active,
            input_3_active: (flags & 0x00000400) !== 0 ? null : states_input_3_active,
            input_4_active: (flags & 0x00000800) !== 0 ? null : states_input_4_active,
            input_5_active: (flags & 0x00001000) !== 0 ? null : states_input_5_active,
            input_6_active: (flags & 0x00002000) !== 0 ? null : states_input_6_active,
            input_7_active: (flags & 0x00004000) !== 0 ? null : states_input_7_active,
            input_8_active: (flags & 0x00008000) !== 0 ? null : states_input_8_active,
            line_present: /* (flags & 0x80000000) !== 0 ? null : */ states_line_present,
            line_voltage: states_line_voltage === 0xffff ? null : states_line_voltage / 1000,
            bckp_voltage: states_bckp_voltage === 0xffff ? null : states_bckp_voltage / 1000,
            batt_voltage_rest: states_batt_voltage_rest === 0xffff ? null : states_batt_voltage_rest / 1000,
            batt_voltage_load: states_batt_voltage_load === 0xffff ? null : states_batt_voltage_load / 1000,
            batt_current_load: states_batt_current_load === 0xffff ? null : states_batt_current_load / 1000,
        },
        events: {
            device_boot: events_device_boot,
            device_tilt: events_device_tilt,
            input_1_activation: events_input_1_activation,
            input_1_deactivation: events_input_1_deactivation,
            input_2_activation: events_input_2_activation,
            input_2_deactivation: events_input_2_deactivation,
            input_3_activation: events_input_3_activation,
            input_3_deactivation: events_input_3_deactivation,
            input_4_activation: events_input_4_activation,
            input_4_deactivation: events_input_4_deactivation,
            input_5_activation: events_input_5_activation,
            input_5_deactivation: events_input_5_deactivation,
            input_6_activation: events_input_6_activation,
            input_6_deactivation: events_input_6_deactivation,
            input_7_activation: events_input_7_activation,
            input_7_deactivation: events_input_7_deactivation,
            input_8_activation: events_input_8_activation,
            input_8_deactivation: events_input_8_deactivation,
        }
    };
}

function s8() {
    var value = buffer.slice(cursor);
    value = value[0];
    if ((value & (1 << 7)) > 0) {
        value = (~value & 0xff) + 1;
        value = -value;
    }
    cursor = cursor + 1;
    return value;
}

function u8() {
    var value = buffer.slice(cursor);
    value = value[0];
    cursor = cursor + 1;
    return value;
}

function s16() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8;
    if ((value & (1 << 15)) > 0) {
        value = (~value & 0xffff) + 1;
        value = -value;
    }
    cursor = cursor + 2;
    return value;
}

function u16() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8;
    cursor = cursor + 2;
    return value;
}

function u32() {
    var value = buffer.slice(cursor);
    value = value[0] | value[1] << 8 | value[2] << 16 | value[3] << 24;
    cursor = cursor + 4;
    return value;
}

function s32() {
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
    var hex = "0f000040023307d904782f28be0bc5124d12b0b30100000001000100000000000000000000000000"
    var buf = Buffer.from(hex, 'hex')
    console.log(JSON.stringify(Decode(1, buf, 0)));
}
