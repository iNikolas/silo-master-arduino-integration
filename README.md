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

### Supported Commands

#### 1. `toggleFeedMode`

- **Description:** This command toggles the feed mode for a specified external silo. It allows you to switch between immediate and delayed feed modes for precise control over material flow.
- **Usage:**
  ```json
  {
    "c": "tfm",
    "ex": <siloNumber>
  }
  ```
- **Parameters:**
  - `siloNumber`: The number of the external silo for which to toggle the feed mode (`202` or `204`).
- **Example:**
  ```json
  {
    "c": "tfm",
    "ex": 202
  }
  ```

#### 2. `setSiloThreshold`

- **Description:** This command sets the threshold for a specified silo. It allows you to define the minimum weight at which the system should trigger a feed action.
- **Usage:**
  ```json
  {
    "c": "sst",
    "s": <siloNumber>,
    "t": <thresholdValue>
  }
  ```
- **Parameters:**
  - `siloNumber`: The number of the silo for which to set the threshold (`202` or `204`).
  - `thresholdValue`: The threshold value in the weight unit used by the system.
- **Example:**
  ```json
  {
    "c": "sst",
    "s": 204,
    "t": 500
  }
  ```

#### 3. `setPrimarySiloState`

- **Description:** This command sets the primary silo selection state. It allows you to configure the primary silo selection for feeding operations.
- **Usage:**
  ```json
  {
    "c": "spss",
    "ss202": <selectionState202>,
    "ss204": <selectionState204>
  }
  ```
- **Parameters:**
  - `selectionState202`: The selection state for silo 202 (206, 207, 208 or 0 if "no selection").
  - `selectionState204`: The selection state for silo 204 (206, 207, 208 or 0 if "no selection").
- **Example:**
  ```json
  {
    "c": "spss",
    "ss202": 206,
    "ss204": 0
  }
  ```

#### 4. `getState`

- **Description:** This command retrieves the current state of the system. It provides information about weight readings, feed modes, threshold settings, and silo selection states.
- **Usage:**
  ```json
  {
    "c": "gs"
  }
  ```
- **Example:**
  ```json
  {
    "c": "gs"
  }
  ```
