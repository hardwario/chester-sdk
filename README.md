# CHESTER SDK

This repository contains source codes for the CHESTER SDK. It is a firmware framework for developing embedded IoT applications on the CHESTER hardware platform. The framework builds on top of nRF Connect SDK (from Nordic Semiconductor), which uses Zephyr OS (backed by Linux Foundation). By design, the users of CHESTER SDK can build their applications or another SDK layer on top of it as long as CHESTER will be the underlying hardware platform.

The CHESTER SDK targets only nRF52840 SoC (Nordic Semiconductor). It has no relation to the onboard LTE module, nor LoRaWAN module.

HARDWARIO provides full support and training materials for CHESTER SDK. The repository is available solely to HARDWARIO partners. It is forbidden to redistribute source codes of this repository without prior written agreement.

## Requirements

These are the basic prerequisites to work with the CHESTER SDK:

* Desktop environment with Windows, macOS, or Ubuntu
* CHESTER hardware platform (preferably `CHESTER-M-BCDGLS`)
* USB debugger SEGGER J-Link
* Power source for CHESTER (e.g. Power Profiler Kit II)

Also, you will need to install required software by this guide:
https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html

Optionally, you may install Visual Studio Code as the editor. It is not a required component and you can use any editor of your preference, but HARDWARIO can provide limited support for this environment.

## Initialization

The first step is to initiaze West workspace:

If you clone from **GitLab** (former repository), use this command:
```
west init -m git@gitlab.hardwario.com:chester/sdk.git --manifest-rev main chester-sdk
```

If you clone from **GitHub** (newer repository which will be used later from July 2023)
```
west init -m git@github.com:hardwario/chester-sdk.git --manifest-rev main chester-sdk
```

Go inside the West workspace folder:

```
cd chester-sdk
```

Synchronize West workspace (will take some time):

```
west update
```

Pre-configure the target board for West workspace:

```
west config build.board chester_nrf52840
```

## Building & flashing

Start with a sample:

```
cd chester/samples/blinky
```

Build the sample:

```
west build
```

Flash the sample to CHESTER:

```
west flash
```

## Debugging application

In order to interact with CHESTER, you can use the helper script `chester/scripts/rtt.sh`. This script will open RTT channel 0 for shell interaction and RTT channel 1 for device logs.

You can open GDB debug session with this command:

```
west debug
```

Alternatively, this command will attach to a running target:

```
west attach
```

## Next steps

For technical documentation, visit: https://docs.hardwario.com/chester

## Contributing

Contributions to CHESTER SDK repository are welcome, but you have to follow some rules and guidelines.

First of all, always follow the existing code style, naming conventions, etc. Search for similarities, and continue in the same direction. Respecting consistency is an important aspect of your contribution.

### Code format

We use Clang-Format with the rules defined in the file `.clang-format`.

In the Visual Studio Code, open the user's settings and enable the option `editor.formatOnSave`. You can enable this option only for the CHESTER workspace.

### Commit style

- The author has a properly set name, surname and email address.
- The commit message follows the format `domain: change` (e.g. `applications: tester: Add GPIO commands`).
- The commit message is always in imperative format (starts with a verb).
- Commit is signed with GPG and contains the `Signed-off-by: ...` sentence at the end.
