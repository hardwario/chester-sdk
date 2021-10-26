# CHESTER SDK ToDo List

- Enable BLE passkey.
- Implement Product Information Block (to crypto memory?).
- Change BLE advertising name to CHESTER + HSN.
- Re-implement LTE stack (base new implementation on LoRaWAN stack).
- Implement GNSS driver.
- Implement 1-Wire driver.
- Test flash filesystem on top of NOR flash.
- Transfer created file from NOR flash via BLE.
- Allow users to set number of repetitions (for operations) in LoRaWAN.
- Check and fix four possible sending options.
- Check CHESTER-Z + push button + interrupts (stops beeps).

## Topics for discussion

- Shall we better prefer `if (ret)` instead of `if (ret < 0)`?
