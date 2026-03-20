// =============================================================================
//  User_Setup.h  —  TFT_eSPI config for ESP32-2432S028R (CYD)
//  Copy to: Arduino/libraries/TFT_eSPI/User_Setup.h
//
//  If screen stays white, swap to Variant B pins below.
// =============================================================================
#define ILI9341_DRIVER

// Variant A (most common)
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_SCLK  18

// Variant B — uncomment if Variant A doesn't work
// #define TFT_MOSI  13
// #define TFT_MISO  12
// #define TFT_SCLK  14

#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1
#define TFT_BL    21

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
