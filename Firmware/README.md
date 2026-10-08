# ESP32-WROOM-DA Firmware

This folder contains the main Arduino firmware for the **ESP32 Low Power Weather Station** project.

The firmware controls the ESP32-WROOM-DA module and manages environmental sensing, battery monitoring, low-power operation, data storage, and the local web dashboard.

---

## Main Firmware File

### `LowPowerWeatherLogger.ino`

This is the main Arduino sketch responsible for operating the complete system.

The firmware performs:

- Temperature and humidity measurement using the **DFRobot Gravity: Analog SHT30 Temperature & Humidity Sensor (DFR0588)**
- Battery voltage, current, and power monitoring using the **INA219 Current and Voltage Sensor Module**
- Deep sleep power management for extended battery life
- Wi-Fi Access Point creation for local dashboard access
- Web server operation
- Battery history logging using LittleFS
- CSV data generation for battery performance analysis

---

# Hardware Used

The firmware is designed for:

- ESP32-WROOM-DA Module
- INA219 Current and Voltage Sensor Module
- DFRobot Gravity: Analog SHT30 Temperature & Humidity Sensor (DFR0588)
- Samsung INR18650-35E 3500mAh Rechargeable Lithium-Ion Battery
- Push Button Wake-Up Control
- LED Status Indicator

---

# Main Features

## Low Power Operation

The ESP32 periodically wakes from deep sleep, collects sensor measurements, saves the data, and returns to sleep mode to reduce battery consumption.

This allows the system to demonstrate multi-day operation using a single 18650 lithium-ion battery.

---

## Battery Monitoring

The INA219 sensor measures:

- Battery voltage
- Current consumption
- Power usage

These values are recorded to evaluate the energy performance of the system and analyze battery behavior over time.

---

## Temperature and Humidity Measurement

The project uses the **DFRobot Gravity: Analog SHT30 Temperature & Humidity Sensor (DFR0588)**.

Unlike a standard digital SHT30 module, the DFR0588 provides calibrated analog voltage outputs. The sensor internally performs signal processing before sending analog voltage signals to the ESP32 ADC.

The ESP32 reads these analog voltages and converts them into temperature and humidity values using the sensor transfer equations.

Temperature conversion:

```text
Temperature (°C) = -66.875 + (72.917 × Voltage)
```

Humidity conversion:

```text
Humidity (%RH) = -12.5 + (41.667 × Voltage)
```

The converted environmental values are stored together with battery measurements for long-term monitoring and analysis.

---

## Data Logging

The firmware stores measurements inside the ESP32 internal flash memory using LittleFS.

Generated file:

```
battery_log.csv
```

Stored information:

- Reading number
- Runtime
- Battery voltage
- Current
- Power
- Temperature
- Humidity

Example data format:

```csv
reading,time,voltage,current_mA,power_mW,temperature,humidity

1,180,4.18,22.5,94,27.3,61

2,360,4.17,22.2,92,27.2,60
```

This data is used for:

- Battery runtime evaluation
- Voltage history graphs
- Multi-day battery demonstration

---

## Local Web Dashboard

The ESP32 creates its own Wi-Fi network and hosts a webpage showing:

- Current temperature
- Humidity
- Battery voltage
- Battery status
- Current consumption
- Power usage
- Runtime
- Battery history graph
- CSV download option

The dashboard allows users to monitor the system without requiring internet access.

---

# Required Libraries

Install these libraries before uploading:

- Adafruit INA219
- ArduinoJson
- ESP32 Arduino Core

The ESP32 built-in libraries used are:

- WiFi.h
- WebServer.h
- Wire.h
- LittleFS.h

No additional library is required for the DFR0588 sensor because it provides analog voltage output that is directly read using the ESP32 ADC.

---

# Upload Instructions

1. Open `LowPowerWeatherLogger.ino` using Arduino IDE.

2. Select the board:

```
ESP32-WROOM-DA Module
```

3. Select the correct COM port.

4. Upload the firmware.

5. Open Serial Monitor:

```
115200 baud
```

6. Verify that the ESP32 starts measuring sensors and saving battery data.

---

# File Output

After operation, the ESP32 creates:

```
battery_log.csv
```

This file contains recorded measurements used for:

- Battery runtime evaluation
- Voltage versus time analysis
- Battery endurance demonstration

---

# Project Goal

The firmware supports the main objective of the project:

> Demonstrating real multi-day operation of a useful weather monitoring device powered by a single 18650 lithium-ion battery.

The collected battery and environmental data provide measurable evidence of system performance and energy efficiency.
