# Dual-ESP32 Modular Audio Streamer

A custom desktop audio streamer featuring a **dual-ESP32 architecture** designed for high reliability, dedicated audio processing, and low-latency hardware control.

---

## 🛠 System Architecture

The project splits functionality across two ESP32 microcontrollers to ensure stability and clean resource allocation:

1. **Main Controller & Bluetooth Receiver (Custom Firmware in `firmware/`):**
   * Implements high-quality Bluetooth A2DP audio sink based on the [esp32-a2dp-sink] with extra codecs (https://github.com/cfint/esp32-a2dp-sink/tree/v5.1) architecture.
   * Handles user controls, audio routing, and display output.
   * Uses the ESP32 hardware **Pulse Counter (PCNT)** peripheral for responsive, zero-lag rotary encoder handling without polling overhead.

2. **Dedicated Web-Radio Module:**
   * Powered by a secondary ESP32 running the proven [Ka-Radio32](https://github.com/karawin/Ka-Radio32) firmware as a dedicated network audio source.
   * Offloads Wi-Fi streaming, network stack management, and web-stream decoding to prevent buffer underruns and core exhaustion on the main system.
   * Communicates with main controller using UART commands.

---

## ⚡ Key Hardware & Firmware Features

* **Digital Audio Routing:** High-resolution I2S output directly to external DAC/amplifier stages.
* **Modular Hardware (KiCad):**
  * Custom mainboard with power management and signal routing.
  * Dedicated front-panel control and display breakout boards.
  * Audio level meter module.
* **Hardware-Accelerated Input:** Native hardware counter (PCNT) integration for smooth volume/menu navigation.
* **Custom Character Handling:** Custom C/C++ routines for Cyrillic metadata decoding and clean display rendering.

---

## 📁 Repository Structure

```text
├── firmware/         # Custom ESP-IDF / C++ firmware for the primary ESP32
├── hardware/         # KiCad schematics, PCB layouts, and component footprints
└── docs/             # Hardware documentation, block diagrams, and photos
---
*Note: The hardware is currently in final assembly and enclosure integration. Full schematic exports (PDF) and device photos will be added.*