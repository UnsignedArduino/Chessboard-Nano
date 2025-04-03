[Chessboard-Hardware](https://github.com/UnsignedArduino/Chessboard-Hardware) |
[Chessboard-Design](https://github.com/UnsignedArduino/Chessboard-Design) |
[Chessboard-Nano](https://github.com/UnsignedArduino/Chessboard-Nano) |
[Chessboard-Pico2W](https://github.com/UnsignedArduino/Chessboard-Pico2W)

# Chessboard-Nano

Arduino Nano firmware for a magnetic-piece-tracking digital chessboard! WIP

This repository contains the PlatformIO project for the firmware that goes on the Arduino Nano. (that goes on the PCB)
For the PCB project, see [Chessboard-Hardware](https://github.com/UnsignedArduino/Chessboard-Hardware).
For the CAD files, see [Chessboard-Design](https://github.com/UnsignedArduino/Chessboard-Design).
For the Raspberry Pi Pico 2W firmware, see [Chessboard-Pico2W](https://github.com/UnsignedArduino/Chessboard-Pico2W).

## Commands

All these commands are available over serial with the default baud rate of 9600.

### `print [type?]`

Prints the values of the linear hall sensors or the calibration values.

* `[type?]` is the type to print (optional) and should be one of the following:
    * `pieces` prints the current state of the chessboard. (default)
    * `piecesDebug` prints the current state of the chessboard with debug
    * `raw` prints the raw values of the linear hall sensors.
    * `presentCalibration` prints the calibration values for squares with a piece present.
    * `presentCalibrationEEPROM` prints the calibration values for squares with a piece present stored in EEPROM.
    * `emptyCalibration` prints the calibration values for squares with no piece present.
    * `emptyCalibrationEEPROM` prints the calibration values for squares with no piece present stored in EEPROM.
    * `presentCalibrationMargin` prints the calibration margin values for squares with a piece present.
    * `presentCalibrationMarginEEPROM` prints the calibration margin values for squares with a piece present stored in
      EEPROM.
    * `emptyCalibrationMargin` prints the calibration margin values for squares with no piece present.
    * `emptyCalibrationMarginEEPROM` prints the calibration margin values for squares with no piece present stored in
      EEPROM.
    * `all` prints all of the above.

### `calibrate [type] [action] [position] [value?]`

Gets or sets the calibration value or margins for squares.

* `[type]` is the type of calibration to get or set and should be one of the following:
    * `present` for squares with a piece present.
    * `empty` for squares with no piece present.
    * `presentMargin` for squares with a piece present. (to account for noise)
    * `emptyMargin` for squares with no piece present. (to account for noise)
* `[action]` is the action to perform and should be one of the following:
    * `get` to get the calibration value.
    * `set` to set the calibration value.
* `[position]` is the position of the square and should be one of the following formats:
    * `row,col` for the row and column of the square. (0-indexed) Use 255 to change the entire row/column.
    * `global` to get or set all at the same time.
* `[value?]` is the value to set the calibration value to. (optional) This is only used if `[action]` is `set`. If not
  provided, the value will be set to the current value of the square.

### `calibrationSaveToEEPROM [type]`

Saves the calibration values to EEPROM.

* `[type]` is the type of calibration to save and should be one of the following:
    * `present` for squares with a piece present.
    * `empty` for squares with no piece present.
    * `presentMargin` for squares with a piece present. (to account for noise)
    * `emptyMargin` for squares with no piece present. (to account for noise)
    * `all` to save all of the above.

### `calibrationLoadFromEEPROM [type]`

Loads the calibration values from EEPROM.

* `[type]` is the type of calibration to load and should be one of the following:
    * `present` for squares with a piece present.
    * `empty` for squares with no piece present.
    * `presentMargin` for squares with a piece present. (to account for noise)
    * `emptyMargin` for squares with no piece present. (to account for noise)
    * `all` to load all of the above.

### `settings [action] [key] [value?]`

Gets or sets the settings values. These changes are automatically loaded and written to EEPROM and take effect
immediately.

* `[action]` is the action to perform and should be one of the following:
    * `get` to get the settings value.
    * `set` to set the settings value.
* `[key]` is the key of the setting to get or set and should be one of the following:
    * `AUTO_LOAD_CALIBRATION`
        * 0: Don't automatically load calibration from EEPROM on startup.
        * 1: Automatically load calibration from EEPROM on startup.
    * `DETECTION_METHOD`
        * 0: Check for both the square to be not empty and present.
        * 1: Check for the square to be not empty.
        * 2: Check for the square to be present.
        * 3: Check for either the square to be not empty or present.
    * `PRINT_ON_BOARD_CHANGE`
        * 0: Don't automatically print the board state when it changes.
        * 1: Automatically print the board state when it changes.
* `[value?]` is the value to set the settings value to. (optional) This is only required if `[action]` is `set`.