# ESP32 GPRS IoT Node: Alarm & Telemetry System

A IoT node based on the **ESP32** and **SIM800L** GSM modem. This device monitors environmental data (Temperature/Humidity), manages 4 digital alarm inputs with real-time interrupts, and communicates via **MQTT** over a **GPRS** network. 

Optimized for battery-powered applications using **Deep Sleep** and **Hardware Power Management**.

## 🚀 Key Features
- **Cellular Connectivity:** Managed SIM800L modem via PPP stack (`esp-modem`).
- **Extreme Power Saving:** Deep Sleep mode with dual-trigger wakeup:
    - **Timer:** Periodic telemetry updates (e.g., every 15 mins).
    - **EXT1 (External):** Instant wakeup from any of the 4 alarm sensors.
- **Home Assistant Ready:** Native **MQTT Discovery** support. Sensors appear automatically in your dashboard.
- **Reliability & Resilience:** - Software **Watchdog** for automated recovery on connection failure.
    - Hardware **PWRKEY** control to hard-reset or power-cycle the modem.
- **Data Optimization:** Delta-based reporting (sends data only if values change significantly) to save GPRS data and battery.

## 🛠 Hardware Wiring (Pinout)


| Component | ESP32 Pin | Notes |
| :--- | :--- | :--- |
| **DHT11 Data** | GPIO 4 | Temp/Hum Sensor |
| **Modem TX** | GPIO 17 | UART2 TX |
| **Modem RX** | GPIO 16 | UART2 RX |
| **Modem PWRKEY** | GPIO 13 | Hardware Power Control |
| **Alarm 1** | GPIO 32 | Internal Pull-up supported |
| **Alarm 2** | GPIO 33 | Internal Pull-up supported |
| **Alarm 3** | GPIO 34 | **Requires external 10k Pull-up** |
| **Alarm 4** | GPIO 35 | **Requires external 10k Pull-up** |
| **Status LED** | GPIO 25 | Remote command / Visual feedback |

## 📦 Installation & Setup
1. Ensure you have **ESP-IDF v5.x** installed.

## 🏠 Home Assistant Integration.
The device automatically publishes discovery configurations. Once connected to your MQTT broker, the following entities will be created:

- sensor.esp32_xxxx_temp (Temperature)
- sensor.esp32_xxxx_hum (Humidity)
- sensor.esp32_xxxx_rssi (GSM Signal Strength)
- binary_sensor.esp32_xxxx_alarm (Motion/Security Sensor)

## ⚠️ Important Notes
**Power Supply:** The SIM800L requires peaks of up to 2A during transmission. Use a high-quality 3.7V LiPo battery or a robust 5V power source with a large capacitor (e.g., 1000µF).

**GPIO 34/35:** These pins are input-only and lack internal pull-up resistors. Physical 10k resistors connected to 3.3V are mandatory to prevent false alarms.


## 📝 License
This project is licensed under the MIT License - see the **LICENSE** file for details.
