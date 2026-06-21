# Analizador Nitrox

A pre-dive nitrox (EANx) analyzer built on the Raspberry Pi Pico (RP2040), using a galvanic SGX-VOX oxygen sensor, an ADS1115 16-bit ADC, and a 160x128 ST7735 SPI TFT display.

The device measures the oxygen fraction of a gas sample, displays Maximum Operating Depth (MOD) at both 1.4 and 1.6 ppO2, and supports salt/fresh water mode switching and on-demand recalibration — all from a single push button.

> **Safety note:** This is a pre-dive spot-check tool, not a continuous or life-support sensor. Always cross-check readings against a second, independently calibrated analyzer before diving, and never rely on a single analyzer reading alone.

---

## Features

- Oxygen fraction (%O2) measurement via galvanic SGX-VOX sensor
- 16-bit resolution analog read via ADS1115 over I2C
- MOD calculation at ppO2 1.4 and 1.6, switchable between meters salt water (MSW) and meters fresh water (MFW)
- Single-button interface:
  - **Short press** (&lt;3s): toggle MSW / MFW
  - **Long press** (&ge;3s): recalibrate against current gas (sets reading to 0.209, i.e. air)
- Calibration health checks: flags low sensor output (sensor aging / altitude) and unstable readings (poor connection / failing cell), with a mandatory on-screen acknowledgment before normal operation proceeds
- 160x128 ST7735 SPI TFT display output

---

## Hardware

| Component | Notes |
|---|---|
| Raspberry Pi Pico (RP2040) | Main controller |
| SGX-VOX galvanic O2 sensor | 0.03mV&ndash;55.09mV linear output, 0&ndash;100% O2 @ 1 ATM |
| ADS1115 | 16-bit I2C ADC, used at PGA &plusmn;0.256V for resolution on the sensor's low-millivolt range |
| ST7735 TFT, 160x128, SPI | Display |
| Push button | Mode select / calibrate, internal pull-up, active LOW |

### Wiring

| Signal | Pico GPIO |
|---|---|
| I2C0 SDA (to ADS1115) | GPIO 8 |
| I2C0 SCL (to ADS1115) | GPIO 9 |
| SPI0 SCK (to TFT) | GPIO 18 |
| SPI0 MOSI/SDIN (to TFT) | GPIO 19 |
| TFT DC | GPIO 3 |
| TFT CS | GPIO 2 |
| TFT RST | GPIO 4 |
| Push button | GPIO 15 (internal pull-up, button to GND) |

ADS1115 address pin (`ADDR`) is tied to GND, giving I2C address `0x48`.

---

## Software Dependencies

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (developed against SDK 2.2.0)
- ARM GNU Toolchain (developed against `14_2_Rel1`)
- CMake &ge; 3.13
- Ninja (or another CMake-supported generator)
- [`displaylib_16bit_PICO`](https://github.com/gavinlyonsrepo/displaylib_16bit_PICO) — ST7735 display driver (see installation below)

This project requires **C++20** (set via `CMAKE_CXX_STANDARD 20` in `CMakeLists.txt`) because the display library uses `std::span`.

---

## Installing `displaylib_16bit_PICO`

The display library is consumed as a plain git clone under `lib/`, **not** as a CMake subproject — its own `CMakeLists.txt` defines itself as a standalone executable rather than a library, so it isn't structured for `add_subdirectory()`. Instead, this project's `CMakeLists.txt` references the library's source and header files directly.

1. From the project root, create a `lib/` directory and clone the library into it:

   ```bash
   mkdir -p lib
   cd lib
   git clone https://github.com/gavinlyonsrepo/displaylib_16bit_PICO
   cd ..
   ```

   Your project structure should now look like:

   ```
   analizador_nitrox/
   ├── CMakeLists.txt
   ├── pico_sdk_import.cmake
   ├── analizador_nitrox.cpp
   └── lib/
       └── displaylib_16bit_PICO/
           ├── include/
           └── src/
               └── displaylib_16/
                   ├── st7735.cpp
                   ├── displaylib_16_graphics.cpp
                   ├── displaylib_16_Print.cpp
                   ├── displaylib_16_Font.cpp
                   └── displaylib_16_common.cpp
   ```

2. Do not edit any files inside `lib/displaylib_16bit_PICO/` — this project's `CMakeLists.txt` already points directly at the required source and header paths (see the `pico_displaylib_16` target), so the clone can stay exactly as downloaded.

3. Confirm your physical display's tab type. This project is configured for the **ST7735R Red Tab / 1.8" 128x160** variant via:

   ```cpp
   myTFT.TFTInitPCBType(myTFT.TFT_ST7735R_Red);
   ```

   If your display behaves correctly but shows inverted colors, your panel is likely a different PCB variant — check the library's README for the other supported `TFTInitPCBType` options.

---

## Building

```bash
git clone <this-repo-url>
cd analizador_nitrox
mkdir -p lib && cd lib
git clone https://github.com/gavinlyonsrepo/displaylib_16bit_PICO
cd ..

cmake -B build
cd build
ninja
```

This produces `analizador_nitrox.uf2` in the `build/` directory.

If CMake can't find the Pico SDK automatically, pass it explicitly:

```bash
cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
```

---

## Flashing

1. Hold the **BOOTSEL** button on the Pico while plugging it into USB.
2. The Pico will mount as a USB mass storage device (`RPI-RP2`).
3. Copy `build/analizador_nitrox.uf2` onto the mounted drive.
4. The Pico will automatically reboot and run the firmware.

---

## Usage

1. **Power on.** The device runs an initial calibration against the currently connected gas. If you are powering on in air, this assumes air (20.9% O2) as the reference.
2. **If a calibration warning or error appears**, read it and press the button to acknowledge before the device proceeds.
   - **Warning (low mV):** sensor output is below the expected healthy range for fresh air at sea level — can be normal at altitude or indicate an aging sensor. Verify against a reference analyzer if unsure.
   - **Error (low mV / high variance):** sensor output is too low to trust, or readings are unstable (check connections, or the sensor may need replacement). Normal operation is blocked until a successful recalibration.
3. **Analyze your gas.** Expose the sensor to the gas sample and allow the reading to stabilize.
4. **Short-press** the button to toggle MOD units between MSW and MFW.
5. **Long-press** (&ge;3 seconds) the button to recalibrate — use this with a known reference gas (typically air) immediately before each analysis session.

---

## Calibration Notes

- This sensor is self-calibrating with respect to altitude: because both the calibration reference gas and the measured gas are sampled at the same ambient pressure, the ppO2 ratio cancels out atmospheric pressure effects. No altitude or barometric correction is required.
- Always recalibrate against a known gas (typically air) immediately before each analysis session, not just once after power-on, especially if the device has been off or idle for an extended period.

---

## Known Limitations

- Galvanic O2 sensors have a typical accuracy tolerance on the order of 1&ndash;3%; small deviations from expected values (e.g. a few tenths of a percent) are within normal sensor behavior, not necessarily a fault.
- Long-term sensor drift (rated &lt;5%/year by the sensor manufacturer) is expected and is the reason recalibration before each use is required, rather than relying on a single calibration over the sensor's service life.
- This firmware does not track cumulative oxygen exposure or sensor age — given the sensor's rated lifespan and this device's brief, intermittent use pattern, calendar-based drift and periodic recalibration checks are expected to be the limiting factor well before cumulative oxygen dose becomes relevant.

---

## License

GNU GPL 3.0