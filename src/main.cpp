#include <Arduino.h>
#include <EEPROM.h>
#include <SerialCommands.h>

const uint8_t CHESSBOARD_ROWS = 8;
const uint8_t CHESSBOARD_COLS = 8;
uint16_t linearHallValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint64_t pieces = 0;
char piecesDebug[CHESSBOARD_ROWS][CHESSBOARD_COLS]; // For debugging
uint16_t linearHallPresentValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallPresentMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];

bool autoLoadCalibration = true;
uint8_t detectionMethod = 0;
const uint8_t DETECTION_METHOD_CHECK_BOTH = 0;
const uint8_t DETECTION_METHOD_CHECK_NOT_EMPTY = 1;
const uint8_t DETECTION_METHOD_CHECK_PRESENT = 2;
const uint8_t DETECTION_METHOD_CHECK_EITHER = 3;

const uint16_t arraySizeInEEPROM =
  CHESSBOARD_ROWS * CHESSBOARD_COLS * sizeof(uint16_t);

const uint16_t PRESENT_CALIBRATION_EEPROM_START_ADDR = 0; // 0 - 127
const uint16_t EMPTY_CALIBRATION_EEPROM_START_ADDR =      // 128 - 255
  PRESENT_CALIBRATION_EEPROM_START_ADDR + arraySizeInEEPROM;
const uint16_t PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR = // 256 - 383
  EMPTY_CALIBRATION_EEPROM_START_ADDR + arraySizeInEEPROM;
const uint16_t EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR = // 384 - 511
  PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR + arraySizeInEEPROM;

const uint16_t AUTO_LOAD_CALIBRATION_EEPROM_START_ADDR = // 512
  EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR + arraySizeInEEPROM;
const uint16_t DETECTION_METHOD_EEPROM_START_ADDR = // 513
  AUTO_LOAD_CALIBRATION_EEPROM_START_ADDR + sizeof(detectionMethod);

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
  pieces = 0;
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
  pieces = 0;
  memset(piecesDebug, ' ', sizeof(piecesDebug));
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint16_t currentValue = linearHallValues[row][col];
      const uint16_t presentValue = linearHallPresentValues[row][col];
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
#define SET_THIS_BIT pieces |= (1ULL << (row * CHESSBOARD_COLS + col));
      if (detectionMethod == DETECTION_METHOD_CHECK_BOTH) {
        if (isEmpty && isPresent) {
          SET_THIS_BIT
        }
      } else if (detectionMethod == DETECTION_METHOD_CHECK_NOT_EMPTY) {
        if (!isEmpty) {
          SET_THIS_BIT
        }
      } else if (detectionMethod == DETECTION_METHOD_CHECK_PRESENT) {
        if (isPresent) {
          SET_THIS_BIT
        }
      } else /*if (detectionMethod == DETECTION_METHOD_CHECK_EITHER)*/ {
        if (!isEmpty || isPresent) {
          SET_THIS_BIT
        }
      }
#undef SET_THIS_BIT
      // For debugging values
      //                    Number line
      // <-------[---empty---]-------[---present---]------->
      //     -         .         ?          0          X
      if (currentValue < minEmptyValue) {
        piecesDebug[row][col] = '-';
      } else if (minEmptyValue <= currentValue &&
                 currentValue <= maxEmptyValue) {
        piecesDebug[row][col] = '.';
      } else if (maxEmptyValue < currentValue &&
                 currentValue < minPresentValue) {
        piecesDebug[row][col] = '?';
      } else if (minPresentValue <= currentValue &&
                 currentValue <= maxPresentValue) {
        piecesDebug[row][col] = '0';
      } else if (maxPresentValue < currentValue) {
        piecesDebug[row][col] = 'X';
      }
    }
  }
}

void loadSettings() {
  EEPROM.get(AUTO_LOAD_CALIBRATION_EEPROM_START_ADDR, autoLoadCalibration);
  EEPROM.get(DETECTION_METHOD_EEPROM_START_ADDR, detectionMethod);
}

void saveSettings() {
  EEPROM.put(AUTO_LOAD_CALIBRATION_EEPROM_START_ADDR, autoLoadCalibration);
  EEPROM.put(DETECTION_METHOD_EEPROM_START_ADDR, detectionMethod);
}

char serialCommandsBuffer[64];
SerialCommands serialCommands(&Serial, serialCommandsBuffer,
                              sizeof(serialCommandsBuffer), "\r\n", " ");

void printMemoryArray(Stream* stream,
                      uint16_t array[CHESSBOARD_ROWS][CHESSBOARD_COLS],
                      uint8_t thisRowOnly = 255, uint8_t thisColOnly = 255) {
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      if (thisRowOnly != 255 && row != thisRowOnly) {
        stream->print(F("-    "));
        continue;
      }
      if (thisColOnly != 255 && col != thisColOnly) {
        stream->print(F("-    "));
        continue;
      }
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

uint16_t saveArrayToEEPROM(uint16_t array[CHESSBOARD_ROWS][CHESSBOARD_COLS],
                           uint16_t startAddr) {
  uint16_t bytesUpdated = 0;
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    const uint16_t rowAddr =
      startAddr + row * CHESSBOARD_COLS * sizeof(uint16_t);
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint16_t value = array[row][col];
      const uint8_t highValue = value >> 8;
      const uint8_t lowValue = value & 0xFF;
      EEPROM.update(rowAddr + col * sizeof(uint16_t), highValue);
      EEPROM.update(rowAddr + col * sizeof(uint16_t) + 1, lowValue);
      bytesUpdated += 2;
    }
  }
  return bytesUpdated;
}

uint16_t loadArrayFromEEPROM(uint16_t array[CHESSBOARD_ROWS][CHESSBOARD_COLS],
                             uint16_t startAddr) {
  uint16_t bytesRead = 0;
  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    const uint16_t rowAddr =
      startAddr + row * CHESSBOARD_COLS * sizeof(uint16_t);
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      const uint8_t highValue = EEPROM.read(rowAddr + col * sizeof(uint16_t));
      const uint8_t lowValue =
        EEPROM.read(rowAddr + col * sizeof(uint16_t) + 1);
      const uint16_t value = (highValue << 8) | lowValue;
      array[row][col] = value;
      bytesRead += 2;
    }
  }
  return bytesRead;
}

// print [pieces|piecesDebug|raw|presentCalibration|presentCalibrationEEPROM|
//     emptyCalibration|emptyCalibrationEEPROM|presentCalibrationMargin|
//     presentCalibrationMarginEEPROM|emptyCalibrationMargin|
//     emptyCalibrationMarginEEPROM|all]
//   Prints the values of the linear hall sensors or the calibration values.
//
//   pieces|raw|presentCalibration|presentCalibrationEEPROM|emptyCalibration|
//       emptyCalibrationEEPROM|presentCalibrationMargin|
//       presentCalibrationMarginEEPROM|emptyCalibrationMargin|
//       emptyCalibrationMarginEEPROM|all: The type of value to print.
//     `pieces` prints the current state of the chessboard.
//     `piecesDebug` prints the current state of the chessboard with debug
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
//     `all` prints all of the above.
void cmdPrint(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
  char* type = sender->Next();

  const static char PIECES_STRING[] PROGMEM = "pieces";
  const static char PIECES_DEBUG_STRING[] PROGMEM = "piecesDebug";
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
  const static char ALL_STRING[] PROGMEM = "all";
  const bool printAll = type != nullptr && strcmp_P(type, ALL_STRING) == 0;
  bool printedSomething = false;
  if (type == nullptr || strcmp_P(type, PIECES_STRING) == 0 || printAll) {
    s->println(F("Printing pieces"));
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        const bool isPresent = pieces & (1ULL << (row * CHESSBOARD_COLS + col));
        s->print(isPresent ? F("0 ") : F(". "));
        //        s->print(pieces[row], BIN);
      }
      s->println();
    }
    printedSomething = true;
  }
  if (strcmp_P(type, PIECES_DEBUG_STRING) == 0 || printAll) {
    s->println(F("Printing pieces with debugging"));
    s->println(F("<-------[---empty---]-------[---present---]------->\n"
                 "    -         .         ?          0          X"));
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        s->write(piecesDebug[row][col]);
      }
      s->println();
    }
    printedSomething = true;
  }
  if (strcmp_P(type, RAW_STRING) == 0 || printAll) {
    s->println(F("Printing raw values"));
    printMemoryArray(s, linearHallValues);
    printedSomething = true;
  }
  if (strcmp_P(type, PRESENT_CALIBRATION_STRING) == 0 || printAll) {
    s->println(F("Printing present calibration values"));
    printMemoryArray(s, linearHallPresentValues);
    printedSomething = true;
  }
  if (strcmp_P(type, PRESENT_CALIBRATION_EEPROM_STRING) == 0 || printAll) {
    s->println(F("Printing present calibration values in EEPROM"));
    printEEPROMArray(s, PRESENT_CALIBRATION_EEPROM_START_ADDR);
    printedSomething = true;
  }
  if (strcmp_P(type, EMPTY_CALIBRATION_STRING) == 0 || printAll) {
    s->println(F("Printing empty calibration values"));
    printMemoryArray(s, linearHallEmptyValues);
    printedSomething = true;
  }
  if (strcmp_P(type, EMPTY_CALIBRATION_EEPROM_STRING) == 0 || printAll) {
    s->println(F("Printing empty calibration values in EEPROM"));
    printEEPROMArray(s, EMPTY_CALIBRATION_EEPROM_START_ADDR);
    printedSomething = true;
  }
  if (strcmp_P(type, PRESENT_CALIBRATION_MARGIN_STRING) == 0 || printAll) {
    s->println(F("Printing present calibration margin values"));
    printMemoryArray(s, linearHallPresentMargins);
    printedSomething = true;
  }
  if (strcmp_P(type, PRESENT_CALIBRATION_MARGIN_EEPROM_STRING) == 0 ||
      printAll) {
    s->println(F("Printing present calibration margin values in EEPROM"));
    printEEPROMArray(s, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    printedSomething = true;
  }
  if (strcmp_P(type, EMPTY_CALIBRATION_MARGIN_STRING) == 0 || printAll) {
    s->println(F("Printing empty calibration margin values"));
    printMemoryArray(s, linearHallEmptyMargins);
    printedSomething = true;
  }
  if (strcmp_P(type, EMPTY_CALIBRATION_MARGIN_EEPROM_STRING) == 0 || printAll) {
    s->println(F("Printing empty calibration margin values in EEPROM"));
    printEEPROMArray(s, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    printedSomething = true;
  }
  if (!printedSomething) {
    s->print(F("Invalid print type: "));
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
//     or `global` to set the calibration for all squares. If row is a number
//     and col is 255 than it will set the calibration for that entire row. If
//     row is 255 and col is a number than it will set the calibration for that
//     entire column.
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
    if ((row >= CHESSBOARD_ROWS && row != 255) ||
        (col >= CHESSBOARD_COLS && col != 255) || row < 0 || col < 0) {
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
      if (row == 255 && col == 255) {
        s->println(F("s"));
        printMemoryArray(s, array);
      } else if (row == 255) {
        s->print(F("s for col "));
        s->println(col);
        printMemoryArray(s, array, 255, col);
      } else if (col == 255) {
        s->print(F("s for row "));
        s->println(row);
        printMemoryArray(s, array, row, 255);
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
      if (row == 255 && col == 255) {
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
      } else if (row == 255) {
        s->print(F("s for col "));
        s->print(col);
        s->print(F(" to "));
        if (value == -1) {
          s->println(F("the linear hall's current reading"));
        } else {
          s->println(value);
        }
        for (uint8_t r = 0; r < CHESSBOARD_ROWS; r++) {
          if (value == -1) {
            array[r][col] = linearHallValues[r][col];
          } else {
            array[r][col] = value;
          }
        }
      } else if (col == 255) {
        s->print(F("s for row "));
        s->print(row);
        s->print(F(" to "));
        if (value == -1) {
          s->println(F("the linear hall's current reading"));
        } else {
          s->println(value);
        }
        for (uint8_t c = 0; c < CHESSBOARD_COLS; c++) {
          if (value == -1) {
            array[row][c] = linearHallValues[row][c];
          } else {
            array[row][c] = value;
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

// calibrationSaveToEEPROM [present|empty|presentMargin|emptyMargin|all]
//   Saves the calibration values to EEPROM.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to save.
//     See `calibrate` for the types or "all" to save all types.
void cmdCalibrationSaveToEEPROM(SerialCommands* sender) {
  Stream* s = sender->GetSerial();

  char* type = sender->Next();
  if (type == nullptr) {
    s->println(F("Missing calibration type"));
    return;
  }
  uint16_t bytesUpdated = 0;
  const static char PRESENT_STRING[] PROGMEM = "present";
  const static char EMPTY_STRING[] PROGMEM = "empty";
  const static char PRESENT_MARGIN_STRING[] PROGMEM = "presentMargin";
  const static char EMPTY_MARGIN_STRING[] PROGMEM = "emptyMargin";
  const static char ALL_STRING[] PROGMEM = "all";
  if (strcmp_P(type, PRESENT_STRING) == 0) {
    s->println(F("Saving present calibration values to EEPROM"));
    bytesUpdated = saveArrayToEEPROM(linearHallPresentValues,
                                     PRESENT_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_STRING) == 0) {
    s->println(F("Saving empty calibration values to EEPROM"));
    bytesUpdated = saveArrayToEEPROM(linearHallEmptyValues,
                                     EMPTY_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, PRESENT_MARGIN_STRING) == 0) {
    s->println(F("Saving present calibration margin values to EEPROM"));
    bytesUpdated = saveArrayToEEPROM(
      linearHallPresentMargins, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_MARGIN_STRING) == 0) {
    s->println(F("Saving empty calibration margin values to EEPROM"));
    bytesUpdated = saveArrayToEEPROM(
      linearHallEmptyMargins, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else if (strcmp_P(type, ALL_STRING) == 0) {
    s->println(F("Saving all arrays to EEPROM"));
    s->println(F("(1/4) Saving present calibration values to EEPROM"));
    bytesUpdated += saveArrayToEEPROM(linearHallPresentValues,
                                      PRESENT_CALIBRATION_EEPROM_START_ADDR);
    s->println(F("(2/4) Saving empty calibration values to EEPROM"));
    bytesUpdated += saveArrayToEEPROM(linearHallEmptyValues,
                                      EMPTY_CALIBRATION_EEPROM_START_ADDR);
    s->println(F("(3/4) Saving present calibration margin values to EEPROM"));
    bytesUpdated += saveArrayToEEPROM(
      linearHallPresentMargins, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    s->println(F("(4/4) Saving empty calibration margin values to EEPROM"));
    bytesUpdated += saveArrayToEEPROM(
      linearHallEmptyMargins, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else {
    s->print(F("Invalid calibration type: "));
    s->println(type);
    return;
  }
  s->print(F("Bytes updated: "));
  s->println(bytesUpdated);
}
SerialCommand cmdObjCalibrationSaveToEEPROM("calibrationSaveToEEPROM",
                                            cmdCalibrationSaveToEEPROM);

// calibrationLoadFromEEPROM [present|empty|presentMargin|emptyMargin|all]
//   Loads the calibration values from EEPROM.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to load.
//     See `calibrate` for the types or "all" to load all types.
void cmdCalibrationLoadFromEEPROM(SerialCommands* sender) {
  Stream* s = sender->GetSerial();

  char* type = sender->Next();
  if (type == nullptr) {
    s->println(F("Missing calibration type"));
    return;
  }
  uint16_t bytesRead = 0;
  const static char PRESENT_STRING[] PROGMEM = "present";
  const static char EMPTY_STRING[] PROGMEM = "empty";
  const static char PRESENT_MARGIN_STRING[] PROGMEM = "presentMargin";
  const static char EMPTY_MARGIN_STRING[] PROGMEM = "emptyMargin";
  const static char ALL_STRING[] PROGMEM = "all";
  if (strcmp_P(type, PRESENT_STRING) == 0) {
    s->println(F("Loading present calibration values from EEPROM"));
    bytesRead = loadArrayFromEEPROM(linearHallPresentValues,
                                    PRESENT_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_STRING) == 0) {
    s->println(F("Loading empty calibration values from EEPROM"));
    bytesRead = loadArrayFromEEPROM(linearHallEmptyValues,
                                    EMPTY_CALIBRATION_EEPROM_START_ADDR);
  } else if (strcmp_P(type, PRESENT_MARGIN_STRING) == 0) {
    s->println(F("Loading present calibration margin values from EEPROM"));
    bytesRead = loadArrayFromEEPROM(
      linearHallPresentMargins, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else if (strcmp_P(type, EMPTY_MARGIN_STRING) == 0) {
    s->println(F("Loading empty calibration margin values from EEPROM"));
    bytesRead = loadArrayFromEEPROM(linearHallEmptyMargins,
                                    EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else if (strcmp_P(type, ALL_STRING) == 0) {
    s->println(F("Loading all arrays from EEPROM"));
    s->println(F("(1/4) Loading present calibration values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(linearHallPresentValues,
                                     PRESENT_CALIBRATION_EEPROM_START_ADDR);
    s->println(F("(2/4) Loading empty calibration values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(linearHallEmptyValues,
                                     EMPTY_CALIBRATION_EEPROM_START_ADDR);
    s->println(
      F("(3/4) Loading present calibration margin values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(
      linearHallPresentMargins, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    s->println(F("(4/4) Loading empty calibration margin values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(
      linearHallEmptyMargins, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
  } else {
    s->print(F("Invalid calibration type: "));
    s->println(type);
    return;
  }
  s->print(F("Bytes read: "));
  s->println(bytesRead);
}
SerialCommand cmdObjCalibrationLoadFromEEPROM("calibrationLoadFromEEPROM",
                                              cmdCalibrationLoadFromEEPROM);

// settings [set|get] [key] [value?]
//   Gets or sets a setting. These changes are automatically loaded and written
//   to EEPROM and take effect immediately.
//
//   set|get: Set get the setting
//   key: The key of the setting to get or set. Can be:
//     "AUTO_LOAD_CALIBRATION"
//       0: Don't automatically load calibration from EEPROM on startup.
//       1: Automatically load calibration from EEPROM on startup.
//     "DETECTION_METHOD"
//       0: Check for both the square to be not empty and present.
//       1: Check for the square to be not empty.
//       2: Check for the square to be present.
//       3: Check for either the square to be not empty or present.
//   value: The value to set the setting to. Ignored if getting setting.
void cmdSettings(SerialCommands* sender) {
  Stream* s = sender->GetSerial();

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

  char* key = sender->Next();
  if (key == nullptr) {
    s->println(F("Missing key"));
    return;
  }

  const static char AUTO_LOAD_CALIBRATION_STRING[] PROGMEM =
    "AUTO_LOAD_CALIBRATION";
  const static char DETECTION_METHOD_STRING[] PROGMEM = "DETECTION_METHOD";

  if (act == ACT_GET) {
    if (strcmp_P(key, AUTO_LOAD_CALIBRATION_STRING) == 0) {
      s->println(F("Printing AUTO_LOAD_CALIBRATION setting value"));
      s->println(autoLoadCalibration);
    } else if (strcmp_P(key, DETECTION_METHOD_STRING) == 0) {
      s->println(F("Printing DETECTION_METHOD setting value"));
      s->println(detectionMethod);
    } else {
      s->print(F("Invalid key: "));
      s->println(key);
    }
    return;
  }
  uint8_t keyAsInt = 0;
  if (act == ACT_SET) {
    if (strcmp_P(key, AUTO_LOAD_CALIBRATION_STRING) == 0) {
      keyAsInt = 0;
    } else if (strcmp_P(key, DETECTION_METHOD_STRING) == 0) {
      keyAsInt = 1;
    } else {
      s->print(F("Invalid key: "));
      s->println(key);
      return;
    }
  }

  char* valueStr = sender->Next();
  if (valueStr == nullptr) {
    s->println(F("Missing value"));
    return;
  }
  int32_t value = atoi(valueStr);
  if (keyAsInt == 0) {
    if (value != 0 && value != 1) {
      s->println(F("Invalid value for AUTO_LOAD_CALIBRATION"));
      return;
    }
    Serial.print(F("Setting AUTO_LOAD_CALIBRATION to "));
    Serial.println(value);
    autoLoadCalibration = value;
  } else if (keyAsInt == 1) {
    if (value < 0 || value > 3) {
      s->println(F("Invalid value for DETECTION_METHOD"));
      return;
    }
    Serial.print(F("Setting DETECTION_METHOD to "));
    Serial.println(value);
    detectionMethod = value;
  }
  saveSettings();
}
SerialCommand cmdObjSettings("settings", cmdSettings);

void cmdUnrecognized(SerialCommands* sender, const char* cmd) {
  sender->GetSerial()->print(F("Unrecognized command: "));
  sender->GetSerial()->println(cmd);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println(F("Chessboard controller starting"));

  delay(500);

  Serial.println(F("Loading settings from EEPROM"));
  loadSettings();

  Serial.println(F("Initializing linear hall sensors"));
  linearHallsBegin();

  if (autoLoadCalibration) {
    Serial.println(F("Loading calibration from EEPROM"));
    uint16_t bytesRead = 0;
    Serial.println(F("Loading all arrays from EEPROM"));
    Serial.println(F("(1/4) Loading present calibration values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(linearHallPresentValues,
                                     PRESENT_CALIBRATION_EEPROM_START_ADDR);
    Serial.println(F("(2/4) Loading empty calibration values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(linearHallEmptyValues,
                                     EMPTY_CALIBRATION_EEPROM_START_ADDR);
    Serial.println(
      F("(3/4) Loading present calibration margin values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(
      linearHallPresentMargins, PRESENT_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    Serial.println(
      F("(4/4) Loading empty calibration margin values from EEPROM"));
    bytesRead += loadArrayFromEEPROM(
      linearHallEmptyMargins, EMPTY_CALIBRATION_MARGIN_EEPROM_START_ADDR);
    Serial.print(F("Bytes read: "));
    Serial.println(bytesRead);
  } else {
    Serial.println(F("Loading calibration from EEPROM on startup is disabled"));
  }

  Serial.println(F("Initializing command parser"));
  serialCommands.AddCommand(&cmdObjPrint);
  serialCommands.AddCommand(&cmdObjCalibrate);
  serialCommands.AddCommand(&cmdObjCalibrationSaveToEEPROM);
  serialCommands.AddCommand(&cmdObjCalibrationLoadFromEEPROM);
  serialCommands.AddCommand(&cmdObjSettings);
  serialCommands.SetDefaultHandler(&cmdUnrecognized);

  Serial.println(F("Ready"));
}

void loop() {
  linearHallsRead();
  linearHallsUpdatePieces();

  serialCommands.ReadSerial();
}
