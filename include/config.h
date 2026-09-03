#pragma once

// I2C pin configuration for ESP32-S3 Zero
// Physical silkscreen labels "3" and "4" = GPIO3 / GPIO4
#define I2C_SDA 3
#define I2C_SCL 4

// Indicator LED (+ side), HIGH = on
#define LED_PIN 6

// Color matching: max Euclidean RGB distance to count as "same" color
// Range 0-441. ~50 = fairly tight, ~80 = more forgiving
#define MATCH_TOLERANCE 55

// Max number of saved colors
#define MAX_SAVED_COLORS 16

// Serial baud rate
#define SERIAL_BAUD 115200

// TCS34725 integration time & gain
// Options: TCS34725_INTEGRATIONTIME_2_4MS ... TCS34725_INTEGRATIONTIME_700MS
#define TCS_INTEGRATION_TIME TCS34725_INTEGRATIONTIME_50MS
// Options: TCS34725_GAIN_1X, 4X, 16X, 60X
#define TCS_GAIN              TCS34725_GAIN_4X
