# SiloMaster Arduino Integration

Welcome to SiloMaster Arduino Integration, the companion firmware for SiloMaster, your solution for seamless integration of external Silos into STEP7 systems lacking native support. With SiloMaster Arduino Integration, you can efficiently communicate with external Silos, monitor weight readings, and manage dosing processes, empowering precise control over batch management.

## Features

- **Modbus Communication:** Communicate with external Silos via Modbus protocol for real-time data exchange.
  
- **EEPROM Persistence:** Store Silo states and threshold settings in EEPROM for persistence across power cycles.

- **Flexible Configuration:** Easily configure Silo states, threshold settings, and communication parameters to suit your specific setup.

## Getting Started

To integrate SiloMaster Arduino Integration into your project, follow these steps:

1. Clone this repository to your local machine.
2. Open the Arduino IDE or your preferred Arduino development environment.
3. Load the SiloMaster Arduino Integration sketch (`SiloMaster_Arduino_Integration.ino`) into your development environment.
4. Configure the necessary parameters such as pin assignments, Modbus settings, and EEPROM addresses according to your setup.
5. Upload the sketch to your Arduino board.
6. Connect your Arduino board to your STEP7 system or other control system.

## Usage

Once SiloMaster Arduino Integration is running on your Arduino board, it will:

- Read weight data from external Silos via Modbus communication.
- Store and retrieve Silo states and threshold settings from EEPROM for persistence.
- Respond to commands from the control system to toggle feed modes, set Silo thresholds, and manage primary Silo selections.
