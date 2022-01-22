var cursor = 0;
var buffer;

// Uncomment for ChirpStack:
function Decode(port, bytes, variables) {

// Uncomment for The Things Network:
// function Decoder(port, bytes) {

buffer = bytes;

    var flags = u32();

    var states_orientation = u8();
    var states_int_temperature = s16();
    var states_ext_temperature = s16();
    var states_ext_humidity = u8();
    var states_line_present = (flags & 0x20000000) !== 0 ? true : false;
    var states_line_voltage = u16();
    var states_bckp_voltage = u16();
    var states_batt_voltage_rest = u16();
    var states_batt_voltage_load = u16();
    var states_batt_current_load = u16();

    var events_device_tilt = u16();
    var events_button_x_click = u16();
    var events_button_x_hold = u16();
    var events_button_1_click = u16();
    var events_button_1_hold = u16();
    var events_button_2_click = u16();
    var events_button_2_hold = u16();
    var events_button_3_click = u16();
    var events_button_3_hold = u16();
    var events_button_4_click = u16();
    var events_button_4_hold = u16();

    var sources_device_boot = (flags & 0x80000000) !== 0 ? true : false;
    var sources_button_x_click = (flags & 0x00000001) !== 0 ? true : false;
    var sources_button_x_hold = (flags & 0x00000002) !== 0 ? true : false;
    var sources_button_1_click = (flags & 0x00000004) !== 0 ? true : false;
    var sources_button_1_hold = (flags & 0x00000008) !== 0 ? true : false;
    var sources_button_2_click = (flags & 0x00000010) !== 0 ? true : false;
    var sources_button_2_hold = (flags & 0x00000020) !== 0 ? true : false;
    var sources_button_3_click = (flags & 0x00000040) !== 0 ? true : false;
    var sources_button_3_hold = (flags & 0x00000080) !== 0 ? true : false;
    var sources_button_4_click = (flags & 0x00000100) !== 0 ? true : false;
    var sources_button_4_hold = (flags & 0x00000200) !== 0 ? true : false;

    return {
        states: {
            orientation: states_orientation === 0 ? null : states_orientation,
            int_temperature: states_int_temperature === 0x7fff ? null : states_int_temperature / 100,
            ext_temperature: states_ext_temperature === 0x7fff ? null : states_ext_temperature / 100,
            ext_humidity: states_ext_humidity === 0xff ? null : states_ext_humidity / 2,
            line_present: (flags & 0x40000000) !== 0 ? null : states_line_present,
            line_voltage: states_line_voltage === 0xffff ? null : states_line_voltage / 1000,
            bckp_voltage: states_bckp_voltage === 0xffff ? null : states_bckp_voltage / 1000,
            batt_voltage_rest: states_batt_voltage_rest === 0xffff ? null : states_batt_voltage_rest / 1000,
            batt_voltage_load: states_batt_voltage_load === 0xffff ? null : states_batt_voltage_load / 1000,
            batt_current_load: states_batt_current_load === 0xffff ? null : states_batt_current_load / 1000,
        },
        events: {
            device_tilt: events_device_tilt,
            button_x_click: events_button_x_click,
            button_x_hold: events_button_x_hold,
            button_1_click: events_button_1_click,
            button_1_hold: events_button_1_hold,
            button_2_click: events_button_2_click,
            button_2_hold: events_button_2_hold,
            button_3_click: events_button_3_click,
            button_3_hold: events_button_3_hold,
            button_4_click: events_button_4_click,
            button_4_hold: events_button_4_hold,
        },
        sources: {
            device_boot: sources_device_boot,
            button_x_click: sources_button_x_click,
            button_x_hold: sources_button_x_hold,
            button_1_click: sources_button_1_click,
            button_1_hold: sources_button_1_hold,
            button_2_click: sources_button_2_click,
            button_2_hold: sources_button_2_hold,
            button_3_click: sources_button_3_click,
            button_3_hold: sources_button_3_hold,
            button_4_click: sources_button_4_click,
            button_4_hold: sources_button_4_hold,
        },
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
    var hex = "00000020026609ff7fff97260200e712691298b700000000000000000000000000000000000000000000"
    var buf = Buffer.from(hex, 'hex')
    console.log(JSON.stringify(Decode(1, buf, 0)));
}
