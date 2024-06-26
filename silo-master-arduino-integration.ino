#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>

#define MAX485_RE_NEG_PIN   2
#define MAX485_DE_PIN       3
#define SERIAL_RX_PIN       8
#define SERIAL_TX_PIN       9

#define SCREW_CONVEYOR_206_PIN  4 
#define SCREW_CONVEYOR_207_PIN  5 
#define ROTARY_VALVE_208_PIN    6 
#define SCREW_CONVEYOR_202_PIN  10
#define SCREW_CONVEYOR_204_PIN  11

#define BITS_PER_BYTE       8
#define BYTES_PER_INT       2
#define TOTAL_BITS          (BITS_PER_BYTE * BYTES_PER_INT)
#define SLAVE_ID            2
#define BAUD_RATE           38400
#define REGISTER_START      0x40001
#define REGISTER_COUNT      3
#define READ_DELAY_MS       500
#define DECIMAL_PLACE       10.0f
#define JSON_BUFFER_SIZE    50

#define EEPROM_BOOL_START_ADDRESS   0
#define EEPROM_BYTE_START_ADDRESS   1
#define EEPROM_INT_START_ADDRESS    2

#define EEPROM_BOOL_SIZE            sizeof(bool)
#define EEPROM_BYTE_SIZE            sizeof(uint8_t)
#define EEPROM_INT_SIZE             sizeof(uint16_t)

#define EEPROM_IS_IMMEDIATE_FEED_SILO_202_STATE_ADDRESS  EEPROM_BOOL_START_ADDRESS
#define EEPROM_IS_IMMEDIATE_FEED_SILO_204_STATE_ADDRESS  (EEPROM_IS_IMMEDIATE_FEED_SILO_202_STATE_ADDRESS + EEPROM_BOOL_SIZE)
#define EEPROM_S206_SELECTION_STATE_ADDRESS              (EEPROM_IS_IMMEDIATE_FEED_SILO_204_STATE_ADDRESS + EEPROM_BOOL_SIZE)
#define EEPROM_S207_SELECTION_STATE_ADDRESS              (EEPROM_S206_SELECTION_STATE_ADDRESS + EEPROM_BYTE_SIZE)
#define EEPROM_S208_SELECTION_STATE_ADDRESS              (EEPROM_S207_SELECTION_STATE_ADDRESS + EEPROM_BYTE_SIZE)
#define EEPROM_SILO202_THRESHOLD_STATE_ADDRESS           (EEPROM_S208_SELECTION_STATE_ADDRESS + EEPROM_BYTE_SIZE)
#define EEPROM_SILO204_THRESHOLD_STATE_ADDRESS           (EEPROM_SILO202_THRESHOLD_STATE_ADDRESS + EEPROM_INT_SIZE)


#define PRIMARY_KEY                             F("c")
#define TOGGLE_FEED_MODE_COMMAND                F("tfm")
#define SET_SILO_THRESHOLD_COMMAND              F("sst")
#define SET_PRIMARY_SILO_STATE_COMMAND          F("spss")
#define GET_STATE_COMMAND                       F("gs")
#define S_202_SELECTION_KEY                     F("ss202")
#define S_204_SELECTION_KEY                     F("ss204")
#define EXTERNAL_SILO_KEY                       F("ex")
#define THRESHOLD_KEY                           F("t")
#define SILO_KEY                                F("s")

#define NO_SELECTION        0
#define SELECTION_S202      1
#define SELECTION_S204      2

#define SILO_202            202
#define SILO_204            204
#define SILO_206            206
#define SILO_207            207
#define SILO_208            208

#define RELAY_ON LOW
#define RELAY_OFF HIGH

int16_t weight;
uint16_t silo202Threshold;
uint16_t silo204Threshold;
bool weightError;
bool isImmediateFeedSilo202;
bool isImmediateFeedSilo204;
uint8_t s206SelectionState;
uint8_t s207SelectionState;
uint8_t s208SelectionState;

int16_t startingWeight202 = 0;
int16_t startingWeight204 = 0;
bool isFeedingSilo202 = false;
bool isFeedingSilo204 = false;

SoftwareSerial modbusSerial(SERIAL_RX_PIN, SERIAL_TX_PIN);
ModbusMaster node;

void loadStateFromEEPROM();
void saveStateToEEPROM();
void processSerialInput();
void sendStateToGUI();
void sendErrorToGUI(const __FlashStringHelper* errorMessage);
void readWeight();
void processToggleFeedMode();
void toggleFeedMode(int externalSiloNumber);
void processSetSiloThreshold();
void processSetPrimarySiloSelection();
void preTransmission();
void postTransmission();
void processSiloFeedLogic();
void controlConveyor(uint8_t selectionState, bool inputActive, uint8_t selection, uint16_t threshold, bool &isFeeding, int16_t &startingWeight, uint8_t outputPin, bool isImmediateFeed);

StaticJsonDocument<JSON_BUFFER_SIZE> jsonDoc;

void setup() {
    pinMode(MAX485_RE_NEG_PIN, OUTPUT);
    pinMode(MAX485_DE_PIN, OUTPUT);

    pinMode(SCREW_CONVEYOR_202_PIN, OUTPUT);
    pinMode(SCREW_CONVEYOR_204_PIN, OUTPUT);
    pinMode(SCREW_CONVEYOR_206_PIN, INPUT);
    pinMode(SCREW_CONVEYOR_207_PIN, INPUT);
    pinMode(ROTARY_VALVE_208_PIN, INPUT);

    digitalWrite(MAX485_RE_NEG_PIN, LOW);
    digitalWrite(MAX485_DE_PIN, LOW);
    digitalWrite(SCREW_CONVEYOR_202_PIN, RELAY_OFF);
    digitalWrite(SCREW_CONVEYOR_204_PIN, RELAY_OFF);

    Serial.begin(BAUD_RATE);
    modbusSerial.begin(BAUD_RATE);
    node.begin(SLAVE_ID, modbusSerial);

    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    loadStateFromEEPROM();
}

void loop() {
    readWeight();
    processSerialInput();
    processSiloFeedLogic();
    delay(READ_DELAY_MS);
}

void controlConveyor(uint8_t selectionState, bool inputActive, uint8_t selection, uint16_t threshold, bool &isFeeding, int16_t &startingWeight, uint8_t outputPin, bool isImmediateFeed) {
    if (selectionState == selection) {
        if (inputActive) {
            if (!isFeeding) {
                startingWeight = weight;
                isFeeding = true;
            }

            int16_t weightDifference = weight - startingWeight;

            if (((isImmediateFeed && weightDifference < threshold) || (!isImmediateFeed && weightDifference > threshold)) && threshold != 0) {
                digitalWrite(outputPin, RELAY_ON);
            } else {
                digitalWrite(outputPin, RELAY_OFF);
            }
        } else {
            digitalWrite(outputPin, RELAY_OFF);
            isFeeding = false;
        }
    }
}


void processSiloFeedLogic() {
    bool s206Active = digitalRead(SCREW_CONVEYOR_206_PIN) == HIGH;
    bool s207Active = digitalRead(SCREW_CONVEYOR_207_PIN) == HIGH;
    bool s208Active = digitalRead(ROTARY_VALVE_208_PIN) == HIGH;

    if (s206SelectionState != SELECTION_S202 && s207SelectionState != SELECTION_S202 && s208SelectionState != SELECTION_S202) {
      isFeedingSilo202 = false;
      digitalWrite(SCREW_CONVEYOR_202_PIN, RELAY_OFF);
    } else {
      controlConveyor(s206SelectionState, s206Active, SELECTION_S202, silo202Threshold, isFeedingSilo202, startingWeight202, SCREW_CONVEYOR_202_PIN, isImmediateFeedSilo202);
      controlConveyor(s207SelectionState, s207Active, SELECTION_S202, silo202Threshold, isFeedingSilo202, startingWeight202, SCREW_CONVEYOR_202_PIN, isImmediateFeedSilo202);
      controlConveyor(s208SelectionState, s208Active, SELECTION_S202, silo202Threshold, isFeedingSilo202, startingWeight202, SCREW_CONVEYOR_202_PIN, isImmediateFeedSilo202);
    }

    if (s206SelectionState != SELECTION_S204 && s207SelectionState != SELECTION_S204 && s208SelectionState != SELECTION_S204) {
      isFeedingSilo204 = false;
      digitalWrite(SCREW_CONVEYOR_204_PIN, RELAY_OFF);
    } else {
      controlConveyor(s206SelectionState, s206Active, SELECTION_S204, silo204Threshold, isFeedingSilo204, startingWeight204, SCREW_CONVEYOR_204_PIN, isImmediateFeedSilo204);
      controlConveyor(s207SelectionState, s207Active, SELECTION_S204, silo204Threshold, isFeedingSilo204, startingWeight204, SCREW_CONVEYOR_204_PIN, isImmediateFeedSilo204);
      controlConveyor(s208SelectionState, s208Active, SELECTION_S204, silo204Threshold, isFeedingSilo204, startingWeight204, SCREW_CONVEYOR_204_PIN, isImmediateFeedSilo204);
    }
}

void saveStateToEEPROM() {
    EEPROM.write(EEPROM_IS_IMMEDIATE_FEED_SILO_202_STATE_ADDRESS, isImmediateFeedSilo202);
    EEPROM.write(EEPROM_IS_IMMEDIATE_FEED_SILO_204_STATE_ADDRESS, isImmediateFeedSilo204);

    EEPROM.write(EEPROM_S206_SELECTION_STATE_ADDRESS, s206SelectionState);
    EEPROM.write(EEPROM_S207_SELECTION_STATE_ADDRESS, s207SelectionState);
    EEPROM.write(EEPROM_S208_SELECTION_STATE_ADDRESS, s208SelectionState);

    EEPROM.put(EEPROM_SILO202_THRESHOLD_STATE_ADDRESS, silo202Threshold);
    EEPROM.put(EEPROM_SILO204_THRESHOLD_STATE_ADDRESS, silo204Threshold);
}

void loadStateFromEEPROM() {
    isImmediateFeedSilo202 = EEPROM.read(EEPROM_IS_IMMEDIATE_FEED_SILO_202_STATE_ADDRESS);
    isImmediateFeedSilo204 = EEPROM.read(EEPROM_IS_IMMEDIATE_FEED_SILO_204_STATE_ADDRESS);

    s206SelectionState = EEPROM.read(EEPROM_S206_SELECTION_STATE_ADDRESS);
    s207SelectionState = EEPROM.read(EEPROM_S207_SELECTION_STATE_ADDRESS);
    s208SelectionState = EEPROM.read(EEPROM_S208_SELECTION_STATE_ADDRESS);

    EEPROM.get(EEPROM_SILO202_THRESHOLD_STATE_ADDRESS, silo202Threshold);
    EEPROM.get(EEPROM_SILO204_THRESHOLD_STATE_ADDRESS, silo204Threshold);
}

void readWeight() {
    uint8_t result;
    uint16_t weight_high, weight_low, status_word;
    float weight_float;

    result = node.readInputRegisters(REGISTER_START, REGISTER_COUNT);
    if (result == node.ku8MBSuccess) {
        weight_high = node.getResponseBuffer(0);
        weight_low = node.getResponseBuffer(1);
        status_word = node.getResponseBuffer(2);
        int16_t signed_weight;

        signed_weight = ((weight_low << TOTAL_BITS) | weight_high);

        if (status_word & 0x0080) {
            signed_weight = -signed_weight;
        }

        weight = signed_weight;
        weightError = false;
    } else {
        weightError = true;
    }
}

void processSerialInput() {
    if (Serial.available() == 0) return;

    DeserializationError error = deserializeJson(jsonDoc, Serial.readStringUntil('\n'));

    if (error) {
        sendErrorToGUI(error.f_str());
        return;
    }

    if (!jsonDoc.containsKey(PRIMARY_KEY)) {
        sendErrorToGUI(F("Invalid JSON data"));
        return;
    }

    const char* command = jsonDoc[PRIMARY_KEY].as<const char*>();

    if (strcmp_P(command, TOGGLE_FEED_MODE_COMMAND) == 0) {
        processToggleFeedMode();
    } else if (strcmp_P(command, SET_SILO_THRESHOLD_COMMAND) == 0) {
        processSetSiloThreshold();
    } else if (strcmp_P(command, SET_PRIMARY_SILO_STATE_COMMAND) == 0) {
        processSetPrimarySiloSelection();
    } else if (strcmp_P(command, GET_STATE_COMMAND) == 0) {
        sendStateToGUI();
    } else {
        sendErrorToGUI(F("Unknown command"));
    }
}

void processSetPrimarySiloSelection() {
    if (!jsonDoc.containsKey(S_202_SELECTION_KEY) || !jsonDoc.containsKey(S_204_SELECTION_KEY)) {
        sendErrorToGUI(F("Missing 'selectionS202' or 'selectionS204' key"));
        return;
    }

    uint8_t selectionS202 = jsonDoc[S_202_SELECTION_KEY];
    uint8_t selectionS204 = jsonDoc[S_204_SELECTION_KEY];

    if (selectionS202 == selectionS204 && selectionS202 != NO_SELECTION) {
        sendErrorToGUI(F("Selections must be unique for each external Silo"));
        return;
    }

    s206SelectionState = NO_SELECTION;
    s207SelectionState = NO_SELECTION;
    s208SelectionState = NO_SELECTION;

    switch (selectionS202) {
        case SILO_206:
            s206SelectionState = SELECTION_S202;
            break;
        case SILO_207:
            s207SelectionState = SELECTION_S202;
            break;
        case SILO_208:
            s208SelectionState = SELECTION_S202;
            break;
        default:
            break;
    }

    switch (selectionS204) {
        case SILO_206:
            if (s206SelectionState != NO_SELECTION) {
                sendErrorToGUI(F("Selection state for SILO_206 already set"));
                return;
            }
            s206SelectionState = SELECTION_S204;
            break;
        case SILO_207:
            if (s207SelectionState != NO_SELECTION) {
                sendErrorToGUI(F("Selection state for SILO_207 already set"));
                return;
            }
            s207SelectionState = SELECTION_S204;
            break;
        case SILO_208:
            if (s208SelectionState != NO_SELECTION) {
                sendErrorToGUI(F("Selection state for SILO_208 already set"));
                return;
            }
            s208SelectionState = SELECTION_S204;
            break;
        default:
            break;
    }

    saveStateToEEPROM();
    sendStateToGUI();
}

void processToggleFeedMode() {
    if (!jsonDoc.containsKey(EXTERNAL_SILO_KEY)) {
        sendErrorToGUI(F("Missing 'externalSilo' key"));
        return;
    }

    if (!jsonDoc[EXTERNAL_SILO_KEY].is<int>()) {
        sendErrorToGUI(F("Invalid type for 'externalSilo'"));
        return;
    }

    int externalSiloNumber = jsonDoc[EXTERNAL_SILO_KEY];
    if (externalSiloNumber != SILO_202 && externalSiloNumber != SILO_204) {
        sendErrorToGUI(F("Invalid value for 'externalSilo'"));
        return;
    }

    toggleFeedMode(externalSiloNumber);
}

void toggleFeedMode(int externalSiloNumber) {
    switch (externalSiloNumber) {
        case SILO_202:
            isImmediateFeedSilo202 = !isImmediateFeedSilo202;
            break;
        case SILO_204:
            isImmediateFeedSilo204 = !isImmediateFeedSilo204;
            break;
        default:
            break;
    }

    saveStateToEEPROM();
    sendStateToGUI();
}

void processSetSiloThreshold() {
    if (!jsonDoc.containsKey(THRESHOLD_KEY)) {
        sendErrorToGUI(F("Missing 'threshold' key"));
        return;
    }

    if (!jsonDoc.containsKey(SILO_KEY)) {
        sendErrorToGUI(F("Missing 'silo' key"));
        return;
    }

    if (!jsonDoc[THRESHOLD_KEY].is<float>()) {
        sendErrorToGUI(F("Invalid type for 'threshold'"));
        return;
    }

    uint16_t threshold = jsonDoc[THRESHOLD_KEY];
    if (jsonDoc[SILO_KEY].is<int>()) {
        uint8_t siloNumber = jsonDoc[SILO_KEY];
        if (siloNumber == SILO_202) {
            silo202Threshold = threshold;
        } else if (siloNumber == SILO_204) {
            silo204Threshold = threshold;
        } else {
            sendErrorToGUI(F("Invalid value for 'silo'"));
            return;
        }
    } else {
        sendErrorToGUI(F("Invalid type for 'silo'"));
        return;
    }

    saveStateToEEPROM();
    sendStateToGUI();
}

void sendStateToGUI() {
    jsonDoc.clear();
    jsonDoc[F("w")] = weight;
    jsonDoc[F("we")] = weightError;
    jsonDoc[F("im202")] = isImmediateFeedSilo202;
    jsonDoc[F("im204")] = isImmediateFeedSilo204;
    jsonDoc[F("t202")] = silo202Threshold;
    jsonDoc[F("t204")] = silo204Threshold;
    jsonDoc[F("ss206")] = s206SelectionState;
    jsonDoc[F("ss207")] = s207SelectionState;
    jsonDoc[F("ss208")] = s208SelectionState;
    jsonDoc[F("sc206")] = (digitalRead(SCREW_CONVEYOR_206_PIN) == HIGH);
    jsonDoc[F("sc207")] = (digitalRead(SCREW_CONVEYOR_207_PIN) == HIGH);
    jsonDoc[F("rv208")] = (digitalRead(ROTARY_VALVE_208_PIN) == HIGH);
    jsonDoc[F("sc202")] = (digitalRead(SCREW_CONVEYOR_202_PIN) == RELAY_ON);
    jsonDoc[F("sc204")] = (digitalRead(SCREW_CONVEYOR_204_PIN) == RELAY_ON);
    String output;
    serializeJson(jsonDoc, output);
    Serial.println(output);
}

void sendErrorToGUI(const __FlashStringHelper* errorMessage) {
    jsonDoc.clear();
    jsonDoc[F("error")] = errorMessage;
    String output;
    serializeJson(jsonDoc, output);
    Serial.println(output);
}

void preTransmission() {
    digitalWrite(MAX485_RE_NEG_PIN, HIGH);
    digitalWrite(MAX485_DE_PIN, HIGH);
}

void postTransmission() {
    digitalWrite(MAX485_RE_NEG_PIN, LOW);
    digitalWrite(MAX485_DE_PIN, LOW);
}
