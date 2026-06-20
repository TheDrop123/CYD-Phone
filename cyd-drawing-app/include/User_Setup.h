#define USER_SETUP_INFO "CYD (ESP32-2432S028R)"

// ── Driver ──────────────────────────────────────────────────────
#define ILI9341_DRIVER

// ── Display dimensions (landscape after setRotation(1)) ─────────
#define TFT_WIDTH  320
#define TFT_HEIGHT 240

// ── SPI pins (HSPI) ─────────────────────────────────────────────
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   -1            // tied to EN
#define TFT_BL    21

// ── SPI speed ───────────────────────────────────────────────────
#define SPI_FREQUENCY   40000000
#define SPI_READ_FREQUENCY   20000000

// ── Fonts (keep minimal to save flash) ──────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_GFXFF
