# MM_MB_1_v1_X-FW

Firmware for the **MissMetal sensor node** by Lynkz Instruments.

Targets the **STM32WL5Mxx** (single Cortex-M4 core, integrated Sub-GHz radio). The device follows a run-to-completion pattern: wake from shutdown, read sensors, send a LoRaWAN uplink, re-enter shutdown. No RTOS.

---

## Hardware

| Component | Interface | Notes |
|-----------|-----------|-------|
| STM32WL5Mxx | — | Cortex-M4 + integrated Sub-GHz radio |
| BMA400 | I2C (0x14) | Accelerometer; threshold interrupt used as beacon wakeup source |
| CAT24C32 | I2C | 32 Kbit EEPROM; stores mode, config, frame counter |

---

## Getting Started

### Prerequisites

Open the project inside the provided **dev container** (`.devcontainer/`). It includes:
- `arm-gnu-toolchain` 14.2.rel1
- `cmake`, `ninja`
- `gdb-multiarch`

### Build

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Available presets: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`.

Build output: `build/<Preset>/MM_MB_1_v1_X-FW.elf`

### Flash

Uses a **Black Magic Probe** on `/dev/ttyACM0`:

```bash
gdb-multiarch --quiet -f build/Debug/MM_MB_1_v1_X-FW.elf -x flash.gdb
```

The VSCode **cortex-debug** launch config (`Debug STM32WL M4`) handles this from the IDE.

---

## Architecture

### Execution Flow

`main()` runs once per wakeup cycle:

1. HAL init, peripherals init, `MX_LoRaWAN_Init()`
2. EEPROM init → `app_fcntup_init()` → read device mode and config
3. BMA400 accelerometer init → read temperature
4. Send LoRaWAN uplink (`SendHeartBeatPayload`)
5. `EnterShutdownWithBMA()` or `EnterShutdownNoBMA()` → STM32 SHUTDOWN mode

The `while(1)` at the bottom of `main()` is never reached under normal operation.

### Device Modes

| Mode | Behaviour |
|------|-----------|
| `APP_MODE_OPERATION` | Periodic heartbeat only; BMA400 in low-power during sleep (RTC wakeup) |
| `APP_MODE_BEACON` | Periodic heartbeat; BMA400 threshold interrupt also enabled as wakeup source |
| `APP_MODE_STORAGE` | Not yet implemented — falls back to OPERATION |

Mode is persisted in EEPROM and managed through `app_get_device_mode()` / `app_set_device_mode()` (`app_settings.c`).

### EEPROM Layout

| Address | Size | Content |
|---------|------|---------|
| 0x00 | 1 B | Provisioned flag (0xA5 after first boot) |
| 0x01 | 4 B | FCntUp — LoRaWAN uplink frame counter (little-endian) |
| 0x05 | 1 B | Device mode (`AppMode_t`) |
| 0x06 | 3 B | AppConfig (`wake_thresh` + `sleep_time_minutes`) |

First boot is detected automatically: if the provisioned flag is absent (erased EEPROM = 0xFF), `app_fcntup_init()` resets FCntUp to 0 and stamps the flag. No compile-time define needed.

---

## LoRaWAN

- **Region**: US915
- **Activation**: ABP (default; set in `lora_app.h`)
- **NVM**: stored in flash at `0x0803F000`
- **Channel mask**: applied via `SetSelectedChannels()` during init (see `lora_app.c`)

### Uplink Ports

| Port | Payload |
|------|---------|
| 66 | Error code |
| 67 | Reset reason |
| 68 | Beacon — max acceleration + temperature |
| 69 | Heartbeat — mode + config + temperature |

### Downlink Commands

| Port | Command | Action |
|------|---------|--------|
| 1 | 0xA1 + 3 bytes | Set device config (`AppConfig_s`: wake_thresh, sleep_time_minutes) |
| 1 | 0xA2 + 1 byte | Set device mode (`AppMode_t`) |
| 1 | 0xA3 | Clear the uplink fcnt |

Handled in `app_communication.c`.

---

## Key Files

| File | Purpose |
|------|---------|
| `LoRaWAN/App/lora_app.h` | Region, data rate, TX power, port numbers, activation type |
| `LoRaWAN/App/se-identity.h` | Device EUI, Join EUI, ABP session keys |
| `LoRaWAN/Target/lorawan_conf.h` | Enabled regions, NVM context, Class B toggle |
| `Core/Inc/app_settings.h` | `AppMode_t`, `AppConfig_s`, limits, FCntUp API |
| `Core/Src/app_eeprom.c` | EEPROM address map and all read/write helpers |
| `Core/Inc/sys_conf.h` | Logging verbosity, low-power mode, debugger pin |

---

## Known Gotchas

- **US915 channel count**: Setting `US915_MAX_NB_CHANNELS` to 8 in `RegionUS915.h` allows the join but causes the device to hang in `__WFI`. Use the default channel count and apply the custom mask via `SetSelectedChannels()`.
- **`print()` vs `printf()`**: `print()` in `utils.c` is a custom wrapper supporting only `%d` and `%f`. Use `printf()` for all other format specifiers.
- **`LM_Delay()`**: Use instead of `HAL_Delay()` anywhere the LoRaWAN MAC must keep running (e.g. waiting for TX/RX windows). Signature: `LM_Delay(uint32_t delay_ms, uint32_t start_time_ms)`.

---

*Lynkz Instruments — Amos, QC*
