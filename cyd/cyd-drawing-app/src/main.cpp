#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <SPIFFS.h>

// ── Touch pins (CYD: separate SPI from display's HSPI) ──────────
#define TOUCH_CS  33
#define TOUCH_IRQ 36

// ── Touch calibration ────────────────────────────────────────────
// Raw ADC min/max for X & Y (portrait orientation).
// If touch is inverted/mirrored, swap the min/max values.
// If axes are wrong, swap X <-> Y and also swap the min/max.
#define RAW_X_MIN  300
#define RAW_X_MAX  3800
#define RAW_Y_MIN  200
#define RAW_Y_MAX  3800

// landscape mode: swap axes so touch-Y → screen-X, touch-X → screen-Y
#define MAP_X(v) map(v, RAW_Y_MIN, RAW_Y_MAX, 0, SCREEN_W)
#define MAP_Y(v) map(v, RAW_X_MAX, RAW_X_MIN, 0, SCREEN_H)

// ── Screen ──────────────────────────────────────────────────────
#define SCREEN_W  320
#define SCREEN_H  240

// ── Toolbar layout ──────────────────────────────────────────────
#define TOOLBAR_Y  180     // y-offset where toolbar starts
#define ROW_H      30      // height per toolbar row (2 rows = 60 px)

// ── Drawing state ───────────────────────────────────────────────
static uint16_t color     = TFT_BLACK;
static uint8_t  penSize   = 3;
static bool     eraserOn  = false;
static int16_t  lastX     = -1;
static int16_t  lastY     = -1;
static bool     isDown    = false;

// ── Hardware objects ────────────────────────────────────────────
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
static TFT_eSPI            tft;

// ── Colour palette (8 swatches) ─────────────────────────────────
static const uint16_t palette[8] = {
  TFT_BLACK,   TFT_RED,     TFT_GREEN, TFT_BLUE,
  TFT_YELLOW,  TFT_CYAN,    TFT_MAGENTA, TFT_WHITE
};

// ═════════════════════════════════════════════════════════════════
//  UI DRAWING
// ═════════════════════════════════════════════════════════════════

static void drawToolbar() {
  // -- background --------------------------------------------------
  tft.fillRect(0, TOOLBAR_Y, SCREEN_W, SCREEN_H - TOOLBAR_Y, TFT_DARKGREY);

  // -- row 0: colour swatches --------------------------------------
  int cw = SCREEN_W / 8;
  for (int i = 0; i < 8; i++) {
    int x = i * cw;
    tft.fillRect(x, TOOLBAR_Y, cw - 1, ROW_H, palette[i]);
    // highlight active colour
    if (palette[i] == color) {
      tft.drawRect(x + 1, TOOLBAR_Y + 1, cw - 3, ROW_H - 2, TFT_WHITE);
      tft.drawRect(x + 2, TOOLBAR_Y + 2, cw - 5, ROW_H - 4, TFT_WHITE);
    }
  }

  // -- row 1: tool buttons -----------------------------------------
  int bw     = SCREEN_W / 5;
  int by     = TOOLBAR_Y + ROW_H;
  int bh     = SCREEN_H - by;

  const char *labels[5] = {"ERASE", "NEW", "SAVE", "SZ+", "SZ-"};
  uint16_t    bg[5];

  bg[0] = eraserOn ? TFT_RED : TFT_BLUE;
  bg[1] = TFT_BLUE;
  bg[2] = TFT_BLUE;
  bg[3] = TFT_BLUE;
  bg[4] = TFT_BLUE;

  for (int i = 0; i < 5; i++) {
    int bx = i * bw;
    tft.fillRect(bx, by, bw - 1, bh, bg[i]);
    tft.setTextColor(TFT_WHITE, bg[i]);
    tft.setTextSize(1);
    tft.setCursor(bx + 6, by + (bh - 8) / 2);
    tft.print(labels[i]);

    // pen-size indicator drawn over SZ+ button
    if (i == 3) {
      tft.fillCircle(bx + bw - 14, by + bh / 2, penSize,
                     eraserOn ? TFT_WHITE : color);
    }
  }

  // -- separator line ----------------------------------------------
  tft.drawFastHLine(0, TOOLBAR_Y, SCREEN_W, TFT_WHITE);
  tft.drawFastHLine(0, TOOLBAR_Y + ROW_H, SCREEN_W, TFT_LIGHTGREY);
}

// ═════════════════════════════════════════════════════════════════
//  SAVE (24-bit BMP to SPIFFS)
// ═════════════════════════════════════════════════════════════════

static void saveDrawing() {
  // pick the first free filename
  String path;
  for (int i = 0; i < 1000; i++) {
    path = "/drawing_" + String(i) + ".bmp";
    if (!SPIFFS.exists(path)) break;
  }

  // grab frame-buffer
  size_t   nPixels = SCREEN_W * SCREEN_H;
  uint16_t *fb     = new uint16_t[nPixels];
  tft.readRect(0, 0, SCREEN_W, SCREEN_H, fb);

  fs::File f = SPIFFS.open(path, FILE_WRITE);
  if (!f) {
    delete[] fb;
    return;
  }

  uint32_t rowSize   = ((SCREEN_W * 3) + 3) & ~3U;  // 4-byte aligned
  uint32_t pixOffset = 54;
  uint32_t fileSize  = pixOffset + rowSize * SCREEN_H;

  // ── BMP file header (14 B) + DIB header (40 B) ──────────────
  uint8_t hdr[54] = {0};
  hdr[0] = 'B';
  hdr[1] = 'M';
  *(uint32_t *)(hdr + 2)  = fileSize;
  *(uint32_t *)(hdr + 10) = pixOffset;
  *(uint32_t *)(hdr + 14) = 40;          // DIB size
  *(uint32_t *)(hdr + 18) = SCREEN_W;
  *(uint32_t *)(hdr + 22) = SCREEN_H;
  *(uint16_t *)(hdr + 26) = 1;           // colour planes
  *(uint16_t *)(hdr + 28) = 24;          // bits per pixel
  f.write(hdr, 54);

  // ── pixel data (BGR, bottom-up) ─────────────────────────────
  uint8_t *row = new uint8_t[rowSize]();
  for (int y = SCREEN_H - 1; y >= 0; y--) {
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t p = fb[y * SCREEN_W + x];
      row[x * 3 + 0] = (p << 3) & 0xF8;  // B
      row[x * 3 + 1] = (p >> 3) & 0xFC;  // G
      row[x * 3 + 2] = (p >> 8) & 0xF8;  // R
    }
    f.write(row, rowSize);
  }

  delete[] row;
  delete[] fb;
  f.close();

  // ── brief on-screen feedback ─────────────────────────────────
  tft.fillRect(0, 0, SCREEN_W, 18, TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("Saved " + path);
  delay(1200);

  // restore canvas top
  tft.fillRect(0, 0, SCREEN_W, 18, TFT_WHITE);
}

// ═════════════════════════════════════════════════════════════════
//  CANVAS
// ═════════════════════════════════════════════════════════════════

static void clearCanvas() {
  tft.fillScreen(TFT_WHITE);
  drawToolbar();
}

// ═════════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  // backlight ON
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  // display (HSPI: 13/12/14/15/2, RST tied to EN)
  tft.init();
  tft.setRotation(1);                // landscape 320×240
  tft.fillScreen(TFT_WHITE);

  // touch (separate SPI: 25/39/32/33)
  // XPT2046_Touchscreen uses the global SPI object; init it with touch pins
  SPI.begin(25, 39, 32, TOUCH_CS);   // sck, miso, mosi, cs
  ts.begin();
  ts.setRotation(1);

  // storage
  SPIFFS.begin(true);

  drawToolbar();
}

// ═════════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════════

void loop() {
  if (!ts.tirqTouched() && !ts.touched()) {
    if (isDown) { isDown = false; lastX = lastY = -1; }
    return;
  }

  TS_Point p = ts.getPoint();

  // pressure threshold (ignore ghost / light touches)
  if (p.z < 200) {
    if (isDown) { isDown = false; lastX = lastY = -1; }
    return;
  }

  // ── map raw touch → screen coords ─────────────────────────────
  int16_t sx = constrain(MAP_X(p.y), 0, SCREEN_W - 1);
  int16_t sy = constrain(MAP_Y(p.x), 0, SCREEN_H - 1);

  // ── toolbar hit? ──────────────────────────────────────────────
  if (sy >= TOOLBAR_Y) {
    int cw = SCREEN_W / 8;

    if (sy < TOOLBAR_Y + ROW_H) {            // colour row
      int idx = sx / cw;
      if (idx >= 0 && idx < 8) {
        color    = palette[idx];
        eraserOn = false;
        drawToolbar();
      }
    } else {                                  // tool row
      int bw = SCREEN_W / 5;
      int ti = sx / bw;
      switch (ti) {
        case 0: eraserOn = !eraserOn; drawToolbar(); break;
        case 1: clearCanvas();                  break;
        case 2: saveDrawing();                  break;
        case 3: if (penSize < 20) penSize += 2; drawToolbar(); break;
        case 4: if (penSize > 1)  penSize -= 2; drawToolbar(); break;
      }
    }

    isDown = false;
    lastX = lastY = -1;
    return;
  }

  // ── draw on canvas ────────────────────────────────────────────
  uint16_t drawColor = eraserOn ? TFT_WHITE : color;

  if (!isDown || lastX < 0) {
    tft.fillCircle(sx, sy, penSize, drawColor);
  } else {
    // smooth thick stroke: fill circles along the line segment
    int dx = sx - lastX;
    int dy = sy - lastY;
    int steps = max(abs(dx), abs(dy));
    for (int i = 0; i <= steps; i++) {
      int cx = lastX + (dx * i) / steps;
      int cy = lastY + (dy * i) / steps;
      tft.fillCircle(cx, cy, penSize, drawColor);
    }
  }

  lastX = sx;
  lastY = sy;
  isDown = true;

  delay(10);   // debounce / sampling rate
}
