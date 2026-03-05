# ESP32 Digital Gauge Cluster for 12V Cummins

A custom digital gauge system built using an **ESP32-S3 with a 3.5" touchscreen** to monitor critical engine parameters on a **1994 Dodge Ram with a 12-valve Cummins engine**.

This project replaces traditional analog gauges with a **fully customizable digital display** capable of monitoring:

- Boost pressure
- Exhaust Gas Temperature (EGT)
- Transmission temperature

Sensor data is processed by the ESP32 and displayed on a **multi-page touchscreen dashboard** designed for automotive use.

The system is powered from vehicle ignition power so the gauge **automatically turns on with the key**.

---

# Features

- Real-time engine monitoring
- Touchscreen interface
- Multiple display pages
- Combined dashboard overview
- High resolution sensor readings
- Automotive-safe power design
- Expandable architecture for additional sensors
- Open source and customizable

---

# Hardware

## Main Controller

Waveshare ESP32-S3 3.5" Touch LCD

Features:

- Dual-core Xtensa LX7 processor
- 240MHz clock
- 3.5" capacitive touchscreen
- WiFi and Bluetooth support
- USB-C power input
- SPI display interface

---

# Sensors

## Boost Pressure Sensor

Specifications:

- Range: **0–60 PSI**
- Output: **0.5–4.5V analog**
- Accuracy: ±0.5% FS
- Thread: **1/8" NPT**

Wiring:

| Wire | Function |
|-----|-----------|
| Red | +5V |
| Black | Ground |
| Green | Signal |

Signal is read using the ADS1115 ADC.

---

## Exhaust Gas Temperature (EGT)

Hardware:

- **K-Type thermocouple probe**
- **MAX31855 thermocouple amplifier**

Installed in the **exhaust manifold (pre-turbo recommended)**.

The MAX31855 converts the thermocouple signal into a digital temperature reading using SPI.

---

## Transmission Temperature

Transmission temperature is measured using an **analog temperature sender** connected to the ADS1115 ADC.

---

# Electronics

## ADS1115 ADC

The ADS1115 is a **16-bit analog-to-digital converter** used to read analog sensors.

Used for:

- Boost pressure sensor
- Transmission temperature sensor

Communication: **I2C**

Benefits:

- High resolution
- Programmable gain
- Accurate voltage measurement

---

## MAX31855 Thermocouple Amplifier

Used for reading **K-type thermocouples**.

Communication: **SPI**

Provides:

- Cold junction compensation
- Direct temperature output
- High temperature measurement capability

---

# Power System

The system is powered from **vehicle ignition power**.

Vehicle 12V (ACC/RUN)
↓
Add-a-fuse (2A fuse)
↓
12V → 5V Buck Converter
↓
USB-C cable
↓
ESP32 board

The buck converter provides a stable **5V rail** for:

- ESP32
- Boost sensor
- ADS1115

This ensures the system powers on when the ignition key is turned.

---

# Wiring

## Boost Sensor

| Sensor Wire | Connection |
|--------------|------------|
Red | 5V supply |
Black | Ground |
Green | ADS1115 A0 |

---

## ADS1115 Connections

| ADS1115 Pin | ESP32 Connection |
|-------------|------------------|
VDD | 3.3V or 5V |
GND | Ground |
SDA | GPIO8 |
SCL | GPIO7 |
A0 | Boost sensor signal |
A1 | Transmission temperature |

---

## MAX31855 Connections

| MAX31855 Pin | ESP32 Connection |
|---------------|------------------|
VIN | 3.3V |
GND | Ground |
CLK | GPIO11 |
DO | GPIO10 |
CS | GPIO18 |

Thermocouple wires connect directly to the MAX31855 terminal block.

---

# ESP32 Pin Map

| Function | GPIO |
|----------|------|
I2C SDA | GPIO8 |
I2C SCL | GPIO7 |
SPI CLK | GPIO11 |
SPI MISO | GPIO10 |
SPI CS | GPIO18 |

---

# Software

The firmware is built using the **Arduino framework for ESP32**.

Recommended libraries:

- Adafruit ADS1X15
- Adafruit MAX31855
- LVGL
- TFT_eSPI (or Waveshare display library)

---

# User Interface

The touchscreen interface provides multiple pages.

## Dashboard

Displays all sensors simultaneously:

- Boost pressure
- Exhaust gas temperature
- Transmission temperature

## Boost Page

Dedicated boost gauge display.

## EGT Page

Real-time exhaust gas temperature monitoring.

## Transmission Page

Transmission fluid temperature display.

Users can swipe between pages.

---

# Installation

1. Install boost sensor in intake manifold or boost reference port.
2. Install thermocouple probe in exhaust manifold (pre-turbo).
3. Route thermocouple cable into cab.
4. Mount touchscreen display in dashboard or gauge pod.
5. Connect sensors according to wiring diagram.
6. Install buck converter behind dashboard.
7. Tap ignition power using an add-a-fuse.
8. Upload firmware to ESP32.
9. Start vehicle and verify readings.

---

# Calibration

Boost sensor output mapping:

0.5V = 0 PSI  
4.5V = 60 PSI

Conversion formula:

PSI = (Voltage - 0.5) * (60 / 4)

The MAX31855 provides calibrated EGT readings automatically.

---

# Safety Notes

- Always fuse ignition power.
- Verify buck converter output before connecting electronics.
- Use proper thermocouple extension wire.
- Ensure all grounds share a common reference.
- Avoid routing signal wires near ignition or alternator wiring.

---

# Future Improvements

Planned upgrades include:

- Data logging to SD card
- Warning alarms
- Peak boost / peak EGT recording
- Wireless telemetry
- Mobile dashboard
- CAN bus integration
- Additional sensors

Potential future sensors:

- Oil pressure
- Coolant temperature
- Fuel pressure
- Engine RPM
- Battery voltage

---

# Project Goals

This project aims to create a **fully customizable open-source digital gauge system for diesel engines**.

Compared to traditional aftermarket gauges, this platform allows complete control over:

- Display design
- Sensor calibration
- Data logging
- System expansion

---

# License

MIT License

---

# Author

Built for monitoring performance and engine safety on a **1994 12-valve Cummins diesel truck**.
