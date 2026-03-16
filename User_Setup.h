// =============================================================================
//  User_Setup.h  —  TFT_eSPI configuration for ESP32-2432S028R (CYD)
//
//  PLACE THIS FILE in your TFT_eSPI library folder:
//    Arduino/libraries/TFT_eSPI/User_Setup.h
//  (overwrite the existing one, or use User_Setup_Select.h to point here)
// =============================================================================

// ── Driver ───────────────────────────────────────────────────────────────────
// Most CYD boards ship with an ILI9341.  If your display shows wrong colours
// try uncommenting ST7789 or ILI9341 alternatives below.
#define ILI9341_DRIVER          // ← most common CYD
// #define ST7789_DRIVER         // ← some newer CYD variants
// #define ILI9488_DRIVER        // ← rare 3.5" CYD variant

// ── Display SPI pins (CYD standard) ─────────────────────────────────────────
#define TFT_MOSI  23
#define TFT_MISO  19   // not always connected, but define anyway
#define TFT_SCLK  18
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // tied to EN/RST rail on CYD – no GPIO needed
#define TFT_BL    21   // backlight — control with digitalWrite in sketch

// ── SPI frequency ─────────────────────────────────────────────────────────
#define SPI_FREQUENCY      40000000   // 40 MHz works on most CYD boards
#define SPI_READ_FREQUENCY  20000000

// ── Touch SPI is on a SEPARATE SPI bus (HSPI) ────────────────────────────────
// These are used by XPT2046_Touchscreen, NOT by TFT_eSPI.
// Defined here for reference; set them in config.h / the sketch.
//   XPT_CLK  = 25
//   XPT_MOSI = 32
//   XPT_MISO = 39  (input-only GPIO)
//   TOUCH_CS = 33
//   TOUCH_IRQ= 36  (input-only GPIO)

// ── Display dimensions ───────────────────────────────────────────────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ── Colour order ─────────────────────────────────────────────────────────────
// Uncomment if colours appear inverted (blue sky looks red, etc.)
// #define TFT_RGB_ORDER TFT_BGR

// ── Font loading (built-in bitmapped fonts are enough for pager UI) ──────────
#define LOAD_GLCD    // Font 1 — 6x8 pixels  ← used by the pager
#define LOAD_FONT2   // Font 2 — 7 segment style
#define LOAD_FONT4   // Font 4 — large
// #define LOAD_FONT6
// #define LOAD_FONT7
// #define LOAD_FONT8
#define LOAD_GFXFF   // FreeFonts (optional, not used by default pager)
#define SMOOTH_FONT  // Enable if you want anti-aliased VLW fonts later
