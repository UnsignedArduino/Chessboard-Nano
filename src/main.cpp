#include <Arduino.h>

const uint8_t EXPANDERS_NUM = 8;
const uint8_t EXPANDERS_A_PIN = 2;
const uint8_t EXPANDERS_B_PIN = 3;
const uint8_t EXPANDERS_C_PIN = 4;
const uint8_t EXPANDERS_INH_PIN = 5;
const uint8_t EXPANDER_COMS_PINS[EXPANDERS_NUM] = {A7, A6, A5, A4,
                                                   A3, A2, A1, A0};
// Need to read expander values in this order (due to wiring)
const uint8_t EXPANDER_COL_TO_BIS[EXPANDERS_NUM] = {2, 1, 0, 3, 5, 7, 6, 4};

const uint8_t CHESSBOARD_ROWS = 8;
const uint8_t CHESSBOARD_COLS = 8;
uint16_t linHallVals[CHESSBOARD_ROWS][CHESSBOARD_COLS];

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

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
  memset(linHallVals, 0, sizeof(linHallVals));
}

void loop() {
  // For now only read first row
  for (uint8_t i : EXPANDER_COL_TO_BIS) {
    digitalWrite(EXPANDERS_A_PIN, i & 0b001);
    digitalWrite(EXPANDERS_B_PIN, i & 0b010);
    digitalWrite(EXPANDERS_C_PIN, i & 0b100);
    delay(10);
    Serial.print(map(analogRead(EXPANDER_COMS_PINS[0]), 0, 1023, 0, 5000));
    Serial.print(" ");
  }
  Serial.println();
  Serial.println();

  delay(1000);
}
