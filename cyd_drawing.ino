/*
  ============================================================
  CYD DRAWING APP  –  Touch-Malprogramm für ESP32 "Cheap Yellow Display"
  ============================================================
  - 8 Farben, Radierer, Leinwand löschen
  - Speichern als BMP (SPIFFS)
  - Pinselgröße +/-
  - Glatte Striche mit Kreis-Interpolation
  ============================================================
*/

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <SPIFFS.h>

// ==================== PIN DEFINITIONS ====================
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

// ==================== DISPLAY ====================
#define SCREEN_W   240
#define SCREEN_H   320
#define TOOLBAR_Y  270
#define ROW_H       24
#define CANVAS_Y    30

// ==================== HARDWARE ====================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ==================== FARBPALETTE ====================
static const uint16_t palette[8] = {
  TFT_BLACK,   TFT_RED,     TFT_GREEN, TFT_BLUE,
  TFT_YELLOW,  TFT_CYAN,    TFT_MAGENTA, TFT_WHITE
};

// ==================== STATE ====================
static uint16_t color    = TFT_BLACK;
static uint8_t  penSize  = 3;
static bool     eraserOn = false;
static int16_t  lastX    = -1;
static int16_t  lastY    = -1;
static bool     isDown   = false;

// ==================== TOUCH ====================
bool getTouch(int& x, int& y) {
  if (!touchscreen.tirqTouched()) return false;
  if (!touchscreen.touched()) return false;
  TS_Point p = touchscreen.getPoint();
  x = constrain(map(p.x, 200, 3800, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
  y = constrain(map(p.y, 200, 3800, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
  return true;
}

// ==================== TOOLBAR ====================
static void drawToolbar() {
  int cw = SCREEN_W / 8;
  for (int i = 0; i < 8; i++) {
    int x = i * cw;
    tft.fillRect(x, TOOLBAR_Y, cw - 1, ROW_H, palette[i]);
    if (palette[i] == color) {
      tft.drawRect(x + 1, TOOLBAR_Y + 1, cw - 3, ROW_H - 2, eraserOn ? TFT_RED : TFT_WHITE);
      tft.drawRect(x + 2, TOOLBAR_Y + 2, cw - 5, ROW_H - 4, eraserOn ? TFT_RED : TFT_WHITE);
    }
  }

  int bw = SCREEN_W / 5;
  int by = TOOLBAR_Y + ROW_H;
  int bh = SCREEN_H - by;

  const char* labels[5] = {"RAD", "NEU", "SPEICH", "SZ+", "SZ-"};
  uint16_t bg[5];
  bg[0] = eraserOn ? TFT_RED : 0x0841;
  bg[1] = 0x0841;
  bg[2] = 0x0841;
  bg[3] = 0x0841;
  bg[4] = 0x0841;

  for (int i = 0; i < 5; i++) {
    int bx = i * bw;
    tft.fillRect(bx, by, bw - 1, bh, bg[i]);
    tft.setTextColor(TFT_WHITE, bg[i]);
    tft.setTextSize(1);
    tft.setCursor(bx + 3, by + (bh - 8) / 2);
    tft.print(labels[i]);

    if (i == 3) {
      tft.fillCircle(bx + bw - 10, by + bh / 2, penSize,
                     eraserOn ? TFT_WHITE : color);
    }
  }

  tft.drawFastHLine(0, TOOLBAR_Y,     SCREEN_W, TFT_WHITE);
  tft.drawFastHLine(0, TOOLBAR_Y + ROW_H, SCREEN_W, TFT_DARKGREY);
}

// ==================== SPEICHERN ====================
static void saveDrawing() {
  if (!SPIFFS.begin(true)) return;

  String path;
  for (int i = 0; i < 1000; i++) {
    path = "/zeichnung_" + String(i) + ".bmp";
    if (!SPIFFS.exists(path)) break;
  }

  size_t   nPixels = SCREEN_W * SCREEN_H;
  uint16_t* fb = new uint16_t[nPixels];
  tft.readRect(0, 0, SCREEN_W, SCREEN_H, fb);

  fs::File f = SPIFFS.open(path, FILE_WRITE);
  if (!f) { delete[] fb; return; }

  uint32_t rowSize   = ((SCREEN_W * 3) + 3) & ~3U;
  uint32_t pixOffset = 54;
  uint32_t fileSize  = pixOffset + rowSize * SCREEN_H;

  uint8_t hdr[54] = {0};
  hdr[0] = 'B'; hdr[1] = 'M';
  *(uint32_t*)(hdr + 2)  = fileSize;
  *(uint32_t*)(hdr + 10) = pixOffset;
  *(uint32_t*)(hdr + 14) = 40;
  *(uint32_t*)(hdr + 18) = SCREEN_W;
  *(uint32_t*)(hdr + 22) = SCREEN_H;
  *(uint16_t*)(hdr + 26) = 1;
  *(uint16_t*)(hdr + 28) = 24;
  f.write(hdr, 54);

  uint8_t* row = new uint8_t[rowSize]();
  for (int y = SCREEN_H - 1; y >= 0; y--) {
    for (int x = 0; x < SCREEN_W; x++) {
      uint16_t px = fb[y * SCREEN_W + x];
      row[x * 3 + 0] = (px << 3) & 0xF8;
      row[x * 3 + 1] = (px >> 3) & 0xFC;
      row[x * 3 + 2] = (px >> 8) & 0xF8;
    }
    f.write(row, rowSize);
  }

  delete[] row;
  delete[] fb;
  f.close();

  tft.fillRect(0, 0, SCREEN_W, 18, TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("Gespeichert " + path);
  delay(1000);
  tft.fillRect(0, 0, SCREEN_W, 18, TFT_WHITE);
}

// ==================== CLEAR ====================
static void clearCanvas() {
  tft.fillRect(0, CANVAS_Y, SCREEN_W, TOOLBAR_Y - CANVAS_Y, TFT_WHITE);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  drawToolbar();
}

// ==================== LOOP ====================
void loop() {
  int x, y;
  if (!getTouch(x, y)) {
    if (isDown) { isDown = false; lastX = lastY = -1; }
    return;
  }

  if (y >= TOOLBAR_Y) {
    int cw = SCREEN_W / 8;

    if (y < TOOLBAR_Y + ROW_H) {
      int idx = x / cw;
      if (idx >= 0 && idx < 8) {
        color    = palette[idx];
        eraserOn = false;
        drawToolbar();
      }
    } else {
      int bw = SCREEN_W / 5;
      int ti = x / bw;
      switch (ti) {
        case 0: eraserOn = !eraserOn; drawToolbar(); break;
        case 1: clearCanvas();                  break;
        case 2: saveDrawing();                   break;
        case 3: if (penSize < 20) penSize += 2; drawToolbar(); break;
        case 4: if (penSize > 1)  penSize -= 2; drawToolbar(); break;
      }
    }

    isDown = false;
    lastX = lastY = -1;
    return;
  }

  if (y < CANVAS_Y) return;

  uint16_t drawColor = eraserOn ? TFT_WHITE : color;

  if (!isDown || lastX < 0) {
    tft.fillCircle(x, y, penSize, drawColor);
  } else {
    int dx = x - lastX;
    int dy = y - lastY;
    int steps = max(abs(dx), abs(dy));
    for (int i = 0; i <= steps; i++) {
      int cx = lastX + (dx * i) / steps;
      int cy = lastY + (dy * i) / steps;
      tft.fillCircle(cx, cy, penSize, drawColor);
    }
  }

  lastX = x;
  lastY = y;
  isDown = true;

  delay(10);
}
