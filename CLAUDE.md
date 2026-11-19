# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for the **MissMetal / LipSensor v1.0** LoRaWAN sensor node by Lynkz Instruments, targeting the **STM32WL5Mxx** (dual-core STM32WL55 die; only the Cortex-M4 runs, the M0+ is never booted). The application code is ported from the OreTag firmware (`TA_MB_1_v1_X-FW`, branch `feature/ground-engaging-tools`), with MissMetal's OPERATION heartbeat recreated on top. No RTOS: every mode stays awake on the ST sequencer, sleeping in STOP2 between events, with an IWDG safety net.

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

`main()` runs once per boot:
1. HAL init, peripherals init, `MX_LoRaWAN_Init()`, `IwdgMonitor_Start()`
2. EEPROM init (CAT24C32 over I2C) → `app_settings_init()` (one-time conversion of old MissMetal EEPROM) → read device mode and config
3. BMA400 accelerometer init → read temperature
4. Configure BMA400 GEN1/GEN2 interrupts → `SyncModeConfiguration()` → `RunScheduledAction()` → `ScheduleAction()` (wall-clock aligned) → `while(1) MX_LoRaWAN_Process()`, sleeping in STOP2 between events.

`RunScheduledAction()` sends the status payload every cycle in OPERATION (MissMetal's heartbeat) and STORAGE. The IWDG is refreshed by `IwdgCheckTimer` only while cycles keep completing (`IwdgMarkProgress()`).

`EnterShutdown()` (in `utils.c`) is only used by `Error_Handler()`. Because the chip is dual-core, it also sets CPU2's low-power mode (`PWR->C2CR1`) to SHUTDOWN — without that the system never reaches SHUTDOWN and the IWDG resets it. It passes `sleep_time_minutes * 60` to the 16-bit RTC wakeup timer unchanged from TA, so values above 1092 minutes overflow.

### Device Modes (`AppMode_t` in `app_settings.h`)

Numbering follows TA, not the old MissMetal enum: `STORAGE=0, OPERATION=1, RF_TEST=2, BEACON=3, CALIBRATION=4`.

- `APP_MODE_OPERATION`: Sequencer; MissMetal heartbeat (status payload) every `sleep_time_minutes`, BMA400 interrupts disabled.
- `APP_MODE_BEACON`: Sequencer; periodic beacon ping plus immediate pings on BMA400 activity / motion-stopped.
- `APP_MODE_STORAGE`: Sequencer; status payload every scheduled cycle.
- `APP_MODE_RF_TEST`: CW tone / config-uplink loop (see `RunRfTestLoop`).
- `APP_MODE_CALIBRATION`: Sequencer; periodic per-axis vibration uplink.

Mode is persisted in EEPROM at `EEPROM_DEVICE_MODE_ADDR` (0x05). Mode and config are accessed via `app_get_device_mode()` / `app_set_device_mode()` in `app_settings.c` — do not call `app_eeprom` directly for these fields. `app_get_device_mode()` reads the EEPROM over I2C on every call.

### EEPROM Layout (CAT24C32, I2C)

| Address | Size | Content |
|---------|------|---------|
| 0x00 | 1 B | Provisioned flag (0xA6 once on this layout; old MissMetal firmware wrote 0xA5) |
| 0x01 | 4 B | Unused (was MissMetal's FCntUp) |
| 0x05 | 1 B | Device mode (`AppMode_t`) |
| 0x06 | 9 B | AppConfig (wake_thresh, no_motion_duration_s, sleep_time_minutes, idle_sleep_time_minutes, schedule_offset_minutes) |
| 0x0F | 11 B | RF test config |
| 0x1A | 4 B | Last DeviceTimeAns sync (Unix seconds) |

### LoRaWAN

- Region: **US915**, ABP activation by default (`LORAWAN_DEFAULT_ACTIVATION_TYPE` in `lora_app.h`), DR0 (11-byte payload limit) with ADR off
- ABP keys and DevEUI are derived from the MCU unique ID (`LynkzKeyGen_Apply()` in `lynkz_key_gen.c`), not taken from `se-identity.h`
- **FCntUp is not persisted:** the LoRaWAN context is never stored (`CFG_SEQ_Task_LoRaStoreContextEvent` is never triggered), so the frame counter restarts at 0 after every reset, including every OPERATION wake. The network server must have frame-counter checks relaxed for these devices.
- `SetSelectedChannels()` (in `Region.c`) enables US915 sub-band 1 (channels 0–7) plus channel 64
- `SendTxData()` prepends `LYNKZ_DEVICE_TYPE` (`lora_app.h`, 0x04 = MissMetal) to every uplink and staggers each send by a DevEUI-derived slot

**Uplink ports** (all payloads start with the device-type byte):
| Port | Payload |
|------|---------|
| 66 | Error code |
| 68 | Beacon (X/Y/Z vibration RMS, 8 mg units) |
| 69 | Status / heartbeat (mode + 9-byte AppConfig) |
| 70 | RF test config |
| 73 | Calibration (X/Y/Z vibration RMS, 8 mg units) |

**Downlink commands (via `app_communication.c`):**
| CMD | Action |
|-----|--------|
| 0xA1 | Set device config (`AppConfig_s`) |
| 0xA2 | Set device mode (`AppMode_t`) |
| 0xA5 | Set RF test config |
| 0xA6 | Restart |

### Custom Lynkz Drivers (git submodules in `Drivers/`)

- `driver-bma400/` — BMA400 accelerometer (I2C); INT1 on PA0 (EXTI0) per the MissMetal schematic (TA's board uses PB14). GEN1 (no-motion) and GEN2 (activity) are both mapped to INT1; INT2 (PC13) is wired but unused and left in analog mode
- `driver-cat24c32/` — CAT24C32 EEPROM (I2C)
- `lib-stm32-uart/` — UART wrapper
- `lib-stm32wl-utils/` — STM32WL utility helpers

### Key Configuration Files

| File | Purpose |
|------|---------|
| `LoRaWAN/App/lora_app.h` | LoRaWAN region, data rate, TX power, ports, activation type |
| `LoRaWAN/App/lynkz_key_gen.c` | DevEUI / ABP session-key derivation from the MCU unique ID |
| `LoRaWAN/Target/lorawan_conf.h` | Enabled regions, context management, Class B toggle |
| `Core/Inc/gpio.h` | LED, BMA400 interrupt and test-board-detect pin definitions (LED_G on PB1 is active-low on MissMetal; polarity set in `gpio.c`) |
| `Core/Inc/app_settings.h` | `AppMode_t`, `AppConfig_s`, threshold/sleep-time limits |
| `Core/Src/app_eeprom.c` | EEPROM address map (all `#define EEPROM_*_ADDR` constants live here) |
| `Core/Inc/sys_conf.h` | Logging verbosity, low-power mode, debugger pin enable |
