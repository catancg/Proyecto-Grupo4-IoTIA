# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-based IoT well monitoring system (Grupo 4, Posgrado IoT) designed to run in the **Wokwi simulator** with real hardware compatibility. Monitors pressure, temperature, vibration, and flow, publishing data via MQTT.

## Build & Flash Commands

```bash
# Build
pio run

# Upload to hardware
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor

# Build + upload + monitor in one step
pio run --target upload && pio device monitor

# Clean build artifacts
pio run --target clean
```

There is no test suite. Wokwi simulation (wokwi.toml + diagram.json) is the primary testing environment.

## Hardware & Pin Map

| Component | Protocol | Pin(s) |
|---|---|---|
| BMP180 (pressure/temp) | I2C | SDA=21, SCL=22 |
| MPU6050 (accel/gyro) | I2C | SDA=21, SCL=22 |
| DS18B20 (temperature) | 1-Wire | GPIO 4 |
| Flow sensor (potentiometer) | ADC | GPIO 34 |

## Architecture

All logic lives in a single file: [src/main.cpp](src/main.cpp).

**Data flow:**
1. `readSensors()` — polls BMP085, DS18B20, MPU6050, and ADC every `READ_INTERVAL_MS` (1000 ms)
2. `computeVibration()` — derives vibration magnitude from MPU6050 acceleration, using an exponential moving average baseline (α=0.02) to cancel gravity on real hardware
3. `loop()` — publishes a JSON payload to MQTT every `PUB_INTERVAL_MS` (1000 ms)

**MQTT payload format:**
```json
{"well_id":"WELL-01","pressure":1013.25,"temperature":25.0,"vibration":0.05,"flow":42.3}
```

- Broker: `broker.hivemq.com:1883` (public, no auth)
- Topic: `cursoiot/well1/sensor`
- Client ID: `ESP32Client_Wokwi_Unico`
- WiFi SSID: `Wokwi-GUEST` (open, Wokwi-only)

## Key Design Notes

- **Unused libraries**: `Adafruit NeoPixel` and `DHT sensor library` are in `platformio.ini` but unused in code — the DHT22 was removed in commit `950da7f`.
- **Vibration baseline**: The EMA in `computeVibration()` is intentional — it allows the same code to run in Wokwi (no gravity offset) and on real hardware (gravity present on Z-axis).
- **Flow sensor**: GPIO 34 is ADC-only (input only, no internal pull-up) — keep it as an input pin only.
- **Timing**: Both read and publish intervals use `millis()` comparisons (`lastReadMs`, `lastPubMs`), not RTOS tasks or interrupts.
