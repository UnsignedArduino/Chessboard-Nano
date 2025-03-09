#include <Arduino.h>
#include <SerialCommands.h>

const uint8_t CHESSBOARD_ROWS = 8;
const uint8_t CHESSBOARD_COLS = 8;
uint16_t linearHallValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint8_t pieces[CHESSBOARD_ROWS]; // Bitmask of pieces present on each row
uint16_t linearHallPresentValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallPresentMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];
uint16_t linearHallEmptyMargins[CHESSBOARD_ROWS][CHESSBOARD_COLS];

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

// print [normalized|raw|presentCalibration|emptyCalibration|
//     presentCalibrationMargin|emptyCalibrationMargin]
//   Prints the values of the linear hall sensors or the calibration values.
//
//   pieces|raw|presentCalibration|emptyCalibration|presentMargin|emptyMargin:
//     The type of values to print. `pieces` is the default and prints whether
//     pieces are present. `raw` prints the raw values directly from the
//     sensors. `presentCalibration` and `emptyCalibration` print the
//     calibration values for present and empty squares respectively.
//     `presentCalibrationMargin` and `emptyCalibrationMargin` print the margin
//     values for present and empty squares respectively.
void cmdPrint(SerialCommands* sender) {
  auto stream = sender->GetSerial();
  char* type = sender->Next();
  if (type == nullptr || strcmp(type, "pieces") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print((pieces[row] & (1 << col)) ? "O " : ". ");
      }
      stream->println();
    }
  } else if (strcmp(type, "raw") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print(linearHallValues[row][col]);
        if (linearHallValues[row][col] < 1000) {
          stream->print("  ");
        } else {
          stream->print(" ");
        }
      }
      stream->println();
    }
  } else if (strcmp(type, "presentCalibration") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print(linearHallPresentValues[row][col]);
        if (linearHallPresentValues[row][col] < 1000) {
          stream->print("  ");
        } else {
          stream->print(" ");
        }
      }
      stream->println();
    }
  } else if (strcmp(type, "emptyCalibration") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print(linearHallEmptyValues[row][col]);
        if (linearHallEmptyValues[row][col] < 1000) {
          stream->print("  ");
        } else {
          stream->print(" ");
        }
      }
      stream->println();
    }
  } else if (strcmp(type, "presentCalibrationMargin") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print(linearHallPresentMargins[row][col]);
        if (linearHallPresentMargins[row][col] < 1000) {
          stream->print("  ");
        } else {
          stream->print(" ");
        }
      }
      stream->println();
    }
  } else if (strcmp(type, "emptyCalibrationMargin") == 0) {
    for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
      for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
        stream->print(linearHallEmptyMargins[row][col]);
        if (linearHallEmptyMargins[row][col] < 1000) {
          stream->print("  ");
        } else {
          stream->print(" ");
        }
      }
      stream->println();
    }
  } else {
    stream->print("Invalid type: ");
    stream->println(type);
  }
}
SerialCommand cmdObjPrint("print", cmdPrint);

// calibrate [present|empty|presentMargin|emptyMargin] [set|unset|get]
// [row,col|global] [value?]
//   Gets or sets the calibration value or margins for a specific square.
//
//   present|empty|presentMargin|emptyMargin: The type of calibration to set.
//     `present` sets the calibration value for a square with a piece present.
//     `empty` sets the calibration value for a square with no piece present.
//     `presentMargin` and `emptyMargin` set the margin values for present and
//       empty squares respectively.
//   set|unset|get: Set, unset, or get the calibration value
//   row,col|global: The row and column of the square to set the calibration
//     for or `global` to set the calibration for all squares.
//   value: The value to set the calibration to. (0 - 1023 or "current" to set
//     to the current reading or any negative value to unset)
//     Ignored if getting or unsetting calibration.
void cmdCalibrate(SerialCommands* sender) {}
SerialCommand cmdObjCalibrate("calibrate", cmdCalibrate);

void cmdUnrecognized(SerialCommands* sender, const char* cmd) {
  sender->GetSerial()->print("Unrecognized command: ");
  sender->GetSerial()->println(cmd);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  linearHallsBegin();

  serialCommands.AddCommand(&cmdObjPrint);
  serialCommands.AddCommand(&cmdObjCalibrate);
  serialCommands.SetDefaultHandler(&cmdUnrecognized);
}

void loop() {
  linearHallsRead();
  linearHallsUpdatePieces();

  serialCommands.ReadSerial();
}
