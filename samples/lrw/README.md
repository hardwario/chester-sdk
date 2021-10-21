LRW Example
-----------

# Runtime configuration

## OTAA Mode (Over The  Air Authentication)

1. Configure DevEUI: `lwr config deveui <...>`
2. Configure JoinEUI (formerly known as `AppEUI`): `lwr config joineui <...>`
3. Configure AppKey: `lwr config appkey <...>`
4. Save configuration and reboot: `config save`

# Notes

## Enable/Disable duty cycle

`lrw config dutycycle <true|false>`
