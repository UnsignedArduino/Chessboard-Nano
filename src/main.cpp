#include <Arduino.h>

const uint8_t CHESSBOARD_ROWS = 8;
const uint8_t CHESSBOARD_COLS = 8;
uint16_t linearHallValues[CHESSBOARD_ROWS][CHESSBOARD_COLS];

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

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  linearHallsBegin();
}

void loop() {
  linearHallsRead();

  for (uint8_t row = 0; row < CHESSBOARD_ROWS; row++) {
    for (uint8_t col = 0; col < CHESSBOARD_COLS; col++) {
      Serial.print(linearHallValues[row][col]);
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.println();
  delay(200);
}
