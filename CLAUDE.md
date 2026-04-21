# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for the **MissMetal / LipSensor v1.0** LoRaWAN sensor node by Lynkz Instruments, targeting the **STM32WL5Mxx** (single Cortex-M4 core). The device wakes from shutdown, reads sensors, sends a LoRaWAN uplink, then re-enters shutdown — a run-to-completion pattern with no RTOS.

## Build System

The primary build system is **CMake + Ninja** with the `arm-none-eabi-gcc` toolchain. A legacy STM32CubeMX-generated `Makefile` also exists but is not the preferred build path.

Development is done inside the provided **dev container** (`.devcontainer/`), which includes arm-gnu-toolchain 14.2.rel1, cmake, ninja, and gdb-multiarch.

### Configure and build (inside dev container)

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Available presets: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`.

The build output lands in `build/<PresetName>/`. The ELF is `build/Debug/MM_MB_1_v1_X-FW.elf`.

### Flash via GDB + Black Magic Probe

```bash
gdb-multiarch --quiet -f build/Debug/MM_MB_1_v1_X-FW.elf -x flash.gdb
```

BMP is expected at `/dev/ttyACM0`. The VSCode **cortex-debug** launch config (`Debug STM32WL M4`) handles this through the IDE.

## Architecture

### Execution Flow

`main()` runs once per wakeup cycle:
1. HAL init, peripherals init, `MX_LoRaWAN_Init()`
2. EEPROM init (CAT24C32 over I2C) → read device mode and config
3. BMA400 accelerometer init → read temperature
4. Send a LoRaWAN uplink based on mode (`SendHeartBeatPayload`)
5. Call `EnterShutdownWithBMA()` or `EnterShutdownNoBMA()` → device enters STM32 SHUTDOWN mode

The `while(1)` at the bottom of `main()` is never reached under normal operation.

### Device Modes (`AppMode_t` in `main.h`)

- `APP_MODE_OPERATION`: Periodic heartbeat only; BMA400 held in low-power mode during sleep (RTC wakeup only).
- `APP_MODE_BEACON`: Periodic heartbeat; BMA400 threshold interrupt also enabled as a wakeup source.
- `APP_MODE_STORAGE`: Not yet implemented (falls back to OPERATION).

Mode is persisted in EEPROM at `EEPROM_DEVICE_MODE_ADDR` (0x09).

### EEPROM Layout (CAT24C32, I2C)

| Address | Size | Content |
|---------|------|---------|
| 0x00 | 1 B | BMA400 wake flag |
| 0x01 | 4 B | FCntUp (frame counter) |
| 0x05 | 4 B | BMA400 sensor time |
| 0x09 | 1 B | Device mode |
| 0x10 | 3 B | AppConfig (wake_thresh + sleep_time_minutes) |

### LoRaWAN

- Region: **US915**, ABP activation by default (`LORAWAN_DEFAULT_ACTIVATION_TYPE` in `lora_app.h`)
- NVM context stored in flash at `0x0803F000`
- `SetSelectedChannels()` is called during `LoRaWAN_Init()` (custom channel mask — see `lora_app.c`)

**Uplink ports:**
| Port | Payload |
|------|---------|
| 66 | Error code |
| 67 | Reset reason |
| 68 | Beacon data (max_accel + temperature) |
| 69 | Heartbeat (mode + config + temperature) |

**Downlink commands (via `app_communication.c`):**
| Port/CMD | Action |
|----------|--------|
| 0xA1 | Set device config (`AppConfig_s`) |
| 0xA2 | Set device mode (`AppMode_t`) |

### Custom Lynkz Drivers (`Drivers/Lynkz_Library/drivers/`)

- `BMA400/` — BMA400 accelerometer (I2C); threshold-interrupt-based wakeup
- `cat24c32_api/` — CAT24C32 EEPROM (I2C)
- `stm32_uart/` — UART wrapper
- `stm32wl_utils/` — STM32WL utility helpers

### Key Configuration Files

| File | Purpose |
|------|---------|
| `LoRaWAN/App/lora_app.h` | LoRaWAN region, data rate, TX power, ports, activation type |
| `LoRaWAN/App/se-identity.h` | Device EUI, Join EUI, session keys (ABP) |
| `LoRaWAN/Target/lorawan_conf.h` | Enabled regions, context management, Class B toggle |
| `Core/Inc/main.h` | EEPROM addresses, timing constants, threshold limits |
| `Core/Inc/sys_conf.h` | Logging verbosity, low-power mode, debugger pin enable |

## Known Issues / Gotchas

- **`FIRST_BOOT` define** (`main.h`): Uncomment `#define FIRST_BOOT` only on the very first flash to reset FCntUp to 0. Must be commented out for all subsequent flashes.
- **US915 channel count**: Changing `US915_MAX_NB_CHANNELS` to 8 in `RegionUS915.h` allows the join to succeed but causes the device to hang in `__WFI`. Keep the default channel mask via `SetSelectedChannels()`.
- **`print()` vs `printf()`**: `print()` in `utils.c` is a custom va_list wrapper that only handles `%d` and `%f`. Use `printf()` for all other format specifiers.
- **`LM_Delay()`**: Use this instead of `HAL_Delay()` anywhere the LoRaWAN MAC process must keep running (e.g. while waiting for TX/RX windows).
