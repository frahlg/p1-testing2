# ESP32 Aidon P1 Smart Meter Reader

## Overview

This project reads data from the P1 (HAN) port of an Aidon smart meter using an ESP32-C3 microcontroller. It specifically targets the Swedish Aidon meters using the DLMS/COSEM EFS data profile over the RJ12 HAN interface.

The code listens for incoming DLMS frames, attempts to decode them (without decryption currently), and prints a summary of key values (Timestamp, Voltage, Current, Power, Energy) to the USB serial console.

## Hardware Requirements

*   **ESP32-C3 based board:** The code is developed for an ESP32-C3 (e.g., ESP32-C3-DevKitM-1 or a custom board like the P1 Dongle Pro mentioned during development).
*   **P1 Port Interface:** A circuit to safely connect the ESP32's UART RX pin to the meter's P1 data output pin (Pin 5 on RJ12). This usually involves appropriate level shifting and signal conditioning. The DTR pin (Pin 2 on RJ12) must also be driven HIGH by the ESP32 to request data from the meter.
*   **Aidon Smart Meter:** An Aidon meter with an active RJ12 HAN port configured for the EFS data profile (DLMS/COSEM).
*   **RJ12 Cable:** To connect the interface board to the meter.
*   **USB Cable:** For programming the ESP32 and viewing serial output.

## Software Requirements

*   **PlatformIO:** Recommended IDE for building and uploading the firmware.
*   **Espressif 32 Platform:** Ensure the correct PlatformIO platform is installed (`platform = espressif32`).
*   **Arduino Framework:** The code uses the Arduino framework for ESP32 (`framework = arduino`).

## Pin Configuration (`src/main.cpp`)

*   `LED`: Pin 7 (Used for status indication - quick blink on startup)
*   `DTR_IO`: Pin 6 (Connected to P1 Port Pin 2 - Data Request, driven HIGH)
*   `RXP1`: Pin 10 (Connected to P1 Port Pin 5 - Data Output, requires inversion)

## Building and Uploading

1.  Open the project in PlatformIO.
2.  Connect your ESP32-C3 board via USB.
3.  Build the project (PlatformIO: Build).
4.  Upload the firmware (PlatformIO: Upload).

## Usage

1.  Connect the ESP32 interface board to the Aidon meter's P1 (HAN) RJ12 port.
2.  Power the ESP32 (usually via USB).
3.  Open the Serial Monitor in PlatformIO (or another terminal program).
4.  Set the baud rate to **115200**.
5.  The ESP32 will initialize, enable the DTR line, and start listening for data.
6.  When the meter sends a DLMS frame (typically every 10 seconds), the code will decode it and print a summary like:

    ```
    --- Frame Summary ---
    Timestamp: 2024-04-02 17:00:00
    Voltage (L1/L2/L3): 230.1 V / 229.9 V / 230.0 V
    Current (L1/L2/L3): 1.23 A / 1.24 A / 1.25 A
    Active Power (+/-): 0.850 kW / 0.000 kW
    Active Energy (+/-): 12345.678 kWh / 0.010 kWh
    ---------------------
    ```

## Important Notes

*   **Signal Inversion:** The P1 data signal from Aidon meters typically requires inversion. This code enables UART signal inversion (`P1Serial.begin(..., true)`).
*   **Baud Rate:** The HAN port operates at 115200 baud.
*   **Data Profile:** This code assumes the meter is configured for the EFS (DLMS/COSEM binary) profile, not the EFS2 (ASCII) profile.
*   **Decryption:** This code does *not* currently implement decryption. If your meter requires a key for the HAN port data, this decoder will not work correctly (though it might still parse some unencrypted parts of the frame).

## Potential Future Improvements

*   Add support for DLMS decryption if required.
*   Implement CRC checking for frame validation.
*   Parse more OBIS codes (e.g., reactive power/energy).
*   Add error handling for invalid frame structures.
*   Output data in a structured format like JSON.
*   Send data over network (MQTT, HTTP, etc.). 