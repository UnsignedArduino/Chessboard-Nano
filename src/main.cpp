#include <Arduino.h>
#include <EEPROM.h>
#include <SerialCommands.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
const uint8_t CHESSBOARD_ROWS = 8;
const uint8_t CHESSBOARD_COLS = 8;
uint16_t linearHallValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint8_t pieces[CHESSBOARD_ROWS]; // Bitmask of pieces present on each row
uint16_t linearHallPresentValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallPresentMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];

const uint16_t arraySizeInEEPROM =
  CHESSBOARD_ROWS * CHESSBOARD_COLS * sizeof(uint16_t);
const uint16_t PRESENT_CALIBRATION_EEPROM_START_ADDR = 0;
const uint16_t EMPTY_CALIBRATION_EEPROM_START_ADDR =
  PRESENT_CALIBRATION_EEPROM_START_ADDR + arraySizeInEEPROM;
const uint16_t PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR =
  EMPTY_CALIBRATION_EEPROM_START_ADDR + arraySizeInEEPROM;
const uint16_t EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR =
  PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR + arraySizeInEEPROM;

const uint8_t EXPANDERS_NUM = CHESSBOARD_ROWS;
const uint8_t EXPANDERS_A_PIN = 2;
const uint8_t EXPANDERS_B_PIN = 3;
const uint8_t EXPANDERS_C_PIN = 4;
const uint8_t EXPANDERS_INH_PIN = 5;
const uint8_t EXPANDER_COMS_PINS[EXPANDERS_NUM] = {A7, A6, A5, A4,
                                                   A3, A2, A1, A0};
// Need to read expander in this order (due to wiring)
const uint8_t EXPANDER_COLS_TO_BITS[CHESSBOARD_COLS] = {2, 1, 0, 3, 5, 7, 6, 4};

void linearHallsBegin() {
  pinMode(EXPANDERS_A_PIN, OUTPUT);
  pinMode(EXPANDERS_B_PIN, OUTPUT);
  pinMode(EXPANDERS_C_PIN, OUTPUT);
  pinMode(EXPANDERS_INH_PIN, OUTPUT);
  digitalWrite(EXPANDERS_A_PIN, LOW);
  digitalWrite(EXPANDERS_B_PIN, LOW);
  digitalWrite(EXPANDERS_C_PIN, LOW);
  digitalWrite(EXPANDERS_INH_PIN, LOW); // Low to enable
  for (uint8_t i : EXPANDER_COMS_PINS) {
    pinMode(i, INPUT);
  }
  memset(linearHallValues, 0, sizeof(linearHallValues));
  memset(pieces, 0, sizeof(pieces));
  memset(linearHallPresentValues, 0, sizeof(linearHallPresentValues));
  memset(linearHallEmptyValues, 0, sizeof(linearHallEmptyValues));
  memset(linearHallPresentMargins, 0, sizeof(linearHallPresentMargins));
  memset(linearHallEmptyMargins, 0, sizeof(linearHallEmptyMargins));
}

void linearHallsRead() {
  for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
    digitalWrite(EXPANDERS_A_PIN, EXPANDER_COLS_TO_BITS[col] & 0b001);
    digitalWrite(EXPANDERS_B_PIN, EXPANDER_COLS_TO_BITS[col] & 0b010);
    digitalWrite(EXPANDERS_C_PIN, EXPANDER_COLS_TO_BITS[col] & 0b100);
    delay(10);
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      linearHallValues[row][col] = analogRead(EXPANDER_COMS_PINS[row]);
    }
  }
}

void linearHallsUpdatePieces() {
  memset(pieces, 0, sizeof(pieces));
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint16_t currentValue = linearHallPresentValues[row][col];
      const uint16_t presentValue = linearHallValues[row][col];
      const uint16_t emptyValue = linearHallEmptyValues[row][col];
      const uint16_t presentMargin = linearHallPresentMargins[row][col];
      const uint16_t emptyMargin = linearHallEmptyMargins[row][col];
      const uint16_t minPresentValue = presentValue - presentMargin;
      const uint16_t maxPresentValue = presentValue + presentMargin;
      const uint16_t minEmptyValue = emptyValue - emptyMargin;
      const uint16_t maxEmptyValue = emptyValue + emptyMargin;
      const bool isEmpty =
        minEmptyValue <= currentValue && currentValue <= maxEmptyValue;
      const bool isPresent =
        minPresentValue <= currentValue && currentValue <= maxPresentValue;
      if (isPresent && !isEmpty) {
        pieces[row] |= 1 << col;
      }
    }
  }
}

char serialCommandsBuffer[64];
SerialCommands serialCommands(&Serial, serialCommandsBuffer,
                              sizeof(serialCommandsBuffer), "\r\n", " ");

void printMemoryArray(Stream* stream,
                      uint16_t array[CHESSBOARD_ROWS][CHESSBOARD_COLS]) {
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint16_t value = array[row][col];
      stream->print(value);
      if (value < 10) {
        stream->print(F("    "));
      } else if (value < 100) {
        stream->print(F("   "));
      } else if (value < 1000) {
        stream->print(F("  "));
      } else {
        stream->print(F(" "));
      }
    }
    stream->println();
  }
}

void printEEPROMArray(Stream* stream, uint16_t startAddr) {
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    const uint16_t rowAddr =
      startAddr + row * CHESSBOARD_COLS * sizeof(uint16_t);
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint8_t highValue = EEPROM.read(rowAddr + col * sizeof(uint16_t));
      const uint8_t lowValue =
        EEPROM.read(rowAddr + col * sizeof(uint16_t) + 1);
      const uint16_t value = (highValue << 8) | lowValue;
      stream->print(value);
      if (value < 10) {
        stream->print(F("    "));
      } else if (value < 100) {
        stream->print(F("   "));
      } else if (value < 1000) {
        stream->print(F("  "));
      } else {
        stream->print(F(" "));
      }
    }
    stream->println();
  }
}

// print [pieces|raw|presentCalibration|presentCalibrationEEPROM|
//     emptyCalibration|emptyCalibrationEEPROM|presentCalibrationMargin|
//     presentCalibrationMarginEEPROM|emptyCalibrationMargin|
//     emptyCalibrationMarginEEPROM]
//   Prints the values of the linear hall sensors or the calibration values.
//
//   pieces|raw|presentCalibration|presentCalibrationEEPROM|emptyCalibration|
//       emptyCalibrationEEPROM|presentCalibrationMargin|
//       presentCalibrationMarginEEPROM|emptyCalibrationMargin|
//       emptyCalibrationMarginEEPROM: The type of value to print.
//     `pieces` prints the current state of the chessboard.
//     `raw` prints the raw values of the linear hall sensors.
//     `presentCalibration` prints the calibration values for squares with a
//       piece present.
//     `presentCalibrationEEPROM` prints the calibration values for squares with
//       a piece present stored in EEPROM.
//     `emptyCalibration` prints the calibration values for squares with no
//       piece present.
//     `emptyCalibrationEEPROM` prints the calibration values for squares with
//       no piece present stored in EEPROM.
//     `presentCalibrationMargin` prints the calibration margin values for
//       squares with a piece present.
//     `presentCalibrationMarginEEPROM` prints the calibration margin values
//       for squares with a piece present stored in EEPROM.
//     `emptyCalibrationMargin` prints the calibration margin values for
//       squares with no piece present.
//     `emptyCalibrationMarginEEPROM` prints the calibration margin values for
//       squares with no piece present stored in EEPROM.
void cmdPrint(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
  char* type = sender->Next();

  const static char PIECES_STRING[] PROGMEM = "pieces";
  const static char RAW_STRING[] PROGMEM = "raw";
  const static char PRESENT_CALIBRATION_STRING[] PROGMEM = "presentCalibration";
  const static char PRESENT_CALIBRATION_EEPROM_STRING[] PROGMEM =
    "presentCalibrationEEPROM";
  const static char EMPTY_CALIBRATION_STRING[] PROGMEM = "emptyCalibration";
  const static char EMPTY_CALIBRATION_EEPROM_STRING[] PROGMEM =
    "emptyCalibrationEEPROM";
  const static char PRESENT_CALIBRATION_MARGIN_STRING[] PROGMEM =
    "presentCalibrationMargin";
  const static char PRESENT_CALIBRATION_MARGIN_EEPROM_STRING[] PROGMEM =
    "presentCalibrationMarginEEPROM";
  const static char EMPTY_CALIBRATION_MARGIN_STRING[] PROGMEM =
    "emptyCalibrationMargin";
  const static char EMPTY_CALIBRATION_MARGIN_EEPROM_STRING[] PROGMEM =
    "emptyCalibrationMarginEEPROM";
  if (type == nullptr || strcmp_P(type, PIECES_STRING) == 0) {
    s->println(F("Printing pieces"));
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        s->print((pieces[row] & (1 << col)) ? "O " : ". ");
      }
      s->println();
    }
  } else if (strcmp_P(type, RAW_STRING) == 0) {
    s->println(F("Printing raw values"));
    printMemoryArray(s, linearHallValues);
  } else if (strcmp_P(type, PRESENT_CALIBRATION_STRING) == 0) {
    s->println(F("Printing present calibration values"));
    printMemoryArray(s, linearHallPresentValues);
  } else if (strcmp_P(type, PRESENT_CALIBRATION_EEPROM_STRING) == 0) {
    s->println(F("Printing present calibration values in EEPROM"));
    printEEPROMArray(s, PRESENT_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_CALIBRATION_STRING) == 0) {
    s->println(F("Printing empty calibration values"));
    printMemoryArray(s, linearHallEmptyValues);
  } else if (strcmp_P(type, EMPTY_CALIBRATION_EEPROM_STRING) == 0) {
    s->println(F("Printing empty calibration values in EEPROM"));
    printEEPROMArray(s, EMPTY_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, PRESENT_CALIBRATION_MARGIN_STRING) == 0) {
    s->println(F("Printing present calibration margin values"));
    printMemoryArray(s, linearHallPresentMargins);
  } else if (strcmp_P(type, PRESENT_CALIBRATION_MARGIN_EEPROM_STRING) == 0) {
    s->println(F("Printing present calibration margin values in EEPROM"));
    printEEPROMArray(s, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_CALIBRATION_MARGIN_STRING) == 0) {
    s->println(F("Printing empty calibration margin values"));
    printMemoryArray(s, linearHallEmptyMargins);
  } else if (strcmp_P(type, EMPTY_CALIBRATION_MARGIN_EEPROM_STRING) == 0) {
    s->println(F("Printing empty calibration margin values in EEPROM"));
    printEEPROMArray(s, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else {
    s->print(F("Invalid type: "));
    s->println(type);
  }
}
SerialCommand cmdObjPrint("print", cmdPrint);

// calibrate [present|empty|presentMargin|emptyMargin] [set|get]
//     [row,col|global] [value?]
//   Gets or sets the calibration value or margins for a specific square.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to set.
//     `present` sets the calibration value for a square with a piece present.
//     `empty` sets the calibration value for a square with no piece present.
//     `presentMargin` and `emptyMargin` set the margin values for present and
//       empty squares respectively.
//   set|get: Set get the calibration value
//   row,col|global: The row and column of the square to set the calibration for
//     or `global` to set the calibration for all squares.
//   value: The value to set the calibration to. (0 - 1023 or blank to set to
//     the current reading) Ignored if getting calibration.
void cmdCalibrate(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
  char* type = sender->Next();

  if (type == nullptr) {
    s->println(F("Missing calibration type"));
    return;
  }

  uint16_t(*array)[CHESSBOARD_COLS];
  const static char PRESENT_STRING[] PROGMEM = "present";
  const static char EMPTY_STRING[] PROGMEM = "empty";
  const static char PRESENT_MARGIN_STRING[] PROGMEM = "presentMargin";
  const static char EMPTY_MARGIN_STRING[] PROGMEM = "emptyMargin";
  if (strcmp_P(type, PRESENT_STRING) == 0) {
    array = linearHallPresentValues;
  } else if (strcmp_P(type, EMPTY_STRING) == 0) {
    array = linearHallEmptyValues;
  } else if (strcmp_P(type, PRESENT_MARGIN_STRING) == 0) {
    array = linearHallPresentMargins;
  } else if (strcmp_P(type, EMPTY_MARGIN_STRING) == 0) {
    array = linearHallEmptyMargins;
  } else {
    s->print(F("Invalid calibration type: "));
    s->println(type);
    return;
  }

  char* action = sender->Next();
  if (action == nullptr) {
    s->println(F("Missing action"));
    return;
  }

  const uint8_t ACT_GET = 0;
  const uint8_t ACT_SET = 1;
  uint8_t act = 0;
  const static char GET_STRING[] PROGMEM = "get";
  const static char SET_STRING[] PROGMEM = "set";
  if (strcmp_P(action, GET_STRING) == 0) {
    act = ACT_GET;
  } else if (strcmp_P(action, SET_STRING) == 0) {
    act = ACT_SET;
  } else {
    s->print(F("Invalid action: "));
    s->println(action);
    return;
  }

  char* position = sender->Next();
  if (position == nullptr) {
    s->println(F("Missing position"));
    return;
  }

  uint8_t row = 0;
  uint8_t col = 0;
  const static char GLOBAL_STRING[] PROGMEM = "global";
  if (strcmp_P(position, GLOBAL_STRING) != 0) {
    char* rowStr = strtok(position, ",");
    char* colStr = strtok(nullptr, ",");
    if (rowStr == nullptr || colStr == nullptr) {
      s->println(F("Invalid position"));
      return;
    }
    row = atoi(rowStr);
    col = atoi(colStr);
    if (row >= CHESSBOARD_ROWS || col >= CHESSBOARD_COLS || row < 0 ||
        col < 0) {
      s->println(F("Invalid position"));
      return;
    }
  } else {
    row = 255;
    col = 255;
  }

  char* valueStr = sender->Next();
  int16_t value = 0;
  if (valueStr == nullptr) {
    value = -1;
  } else {
    value = atoi(valueStr);
    value = constrain(value, 0, 1023);
  }

  switch (act) {
    case ACT_GET: {
      s->print(F("Printing "));
      if (strcmp(type, "present") == 0) {
        s->print(F("present calibration value"));
      } else if (strcmp(type, "empty") == 0) {
        s->print(F("empty calibration value"));
      } else if (strcmp(type, "presentMargin") == 0) {
        s->print(F("present calibration margin value"));
      } else if (strcmp(type, "emptyMargin") == 0) {
        s->print(F("empty calibration margin value"));
      }
      if (row == 255) {
        s->println(F("s"));
        printMemoryArray(s, array);
      } else {
        s->print(F(" for row "));
        s->print(row);
        s->print(F(" col "));
        s->println(col);
        s->println(array[row][col]);
      }
      break;
    }
    case ACT_SET: {
      s->print(F("Setting "));
      if (strcmp(type, "present") == 0) {
        s->print(F("present calibration value"));
      } else if (strcmp(type, "empty") == 0) {
        s->print(F("empty calibration value"));
      } else if (strcmp(type, "presentMargin") == 0) {
        s->print(F("present calibration margin value"));
      } else if (strcmp(type, "emptyMargin") == 0) {
        s->print(F("empty calibration margin value"));
      }
      if (row == 255) {
        s->print(F("s to "));
        if (value == -1) {
          s->println(F("the linear hall's current reading"));
        } else {
          s->println(value);
        }
        for (uint8_t r = 0; r < CHESSBOARD_ROWS; r++) {
          for (uint8_t c = 0; c < CHESSBOARD_COLS; c++) {
            if (value == -1) {
              array[r][c] = linearHallValues[r][c];
            } else {
              array[r][c] = value;
            }
          }
        }
      } else {
        s->print(F(" for row "));
        s->print(row);
        s->print(F(" col "));
        s->print(col);
        s->print(F(" to "));
        s->println(value);
        array[row][col] = value;
      }
      break;
    }
    default: {
      s->println(F("Invalid action"));
      break;
    }
  }
}
SerialCommand cmdObjCalibrate("calibrate", cmdCalibrate);

// calibrationSaveToEEPROM [present|empty|presentMargin|emptyMargin]
//   Saves the calibration values to EEPROM.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to save.
//     See `calibrate` for the types.
void cmdCalibrationSaveToEEPROM(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
}
SerialCommand cmdObjCalibrationSaveToEEPROM("calibrationSaveToEEPROM",
                                            cmdCalibrationSaveToEEPROM);

// calibrationLoadFromEEPROM [present|empty|presentMargin|emptyMargin]
//   Loads the calibration values from EEPROM.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to load.
//     See `calibrate` for the types.
void cmdCalibrationLoadFromEEPROM(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
}
SerialCommand cmdObjCalibrationLoadFromEEPROM("calibrationLoadFromEEPROM",
                                              cmdCalibrationLoadFromEEPROM);

void cmdUnrecognized(SerialCommands* sender, const char* cmd) {
  sender->GetSerial()->print(F("Unrecognized command: "));
  sender->GetSerial()->println(cmd);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  linearHallsBegin();

  serialCommands.AddCommand(&cmdObjPrint);
  serialCommands.AddCommand(&cmdObjCalibrate);
  serialCommands.AddCommand(&cmdObjCalibrationSaveToEEPROM);
  serialCommands.AddCommand(&cmdObjCalibrationLoadFromEEPROM);
  serialCommands.SetDefaultHandler(&cmdUnrecognized);
}

void loop() {
  linearHallsRead();
  linearHallsUpdatePieces();

  serialCommands.ReadSerial();
}

#pragma clang diagnostic pop