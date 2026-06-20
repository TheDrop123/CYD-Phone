#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <FS.h>
#include <SPIFFS.h>

// ============ PINS ============
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ============ DISPLAY / TOUCH KALIBRIERUNG ============
#define SCREEN_W 240
#define SCREEN_H 320
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// =====================================================================
// ========================== APP FRAMEWORK =============================
// =====================================================================
// Jede App besteht aus vier Funktionen:
//  - enter():       einmalig beim Start der App (Bildschirm aufbauen)
//  - onTouch(x,y):  solange der Bildschirm berührt wird
//  - onRelease():   sobald der Finger losgelassen wird
//  - loop():        jeden Durchlauf, auch ohne Touch (z.B. für Animationen)
//
// NEUE APP HINZUFÜGEN:
//  1. Die vier Funktionen schreiben (siehe Beispiele unten im Sketch)
//  2. Eintrag im "apps[]" Array ergänzen -> erscheint automatisch im Menü
// =====================================================================

struct App {
  const char* name;
  uint16_t color;
  void (*enter)();
  void (*onTouch)(int x, int y);
  void (*onRelease)();
  void (*loop)();
};

// ---- Vorwärtsdeklarationen Paint-App ----
void paint_enter();
void paint_touch(int x, int y);
void paint_release();
void paint_loop();

// ---- Vorwärtsdeklarationen Uhr-App ----
void clock_enter();
void clock_touch(int x, int y);
void clock_release();
void clock_loop();

// ---- Vorwärtsdeklarationen Info-App ----
void info_enter();
void info_touch(int x, int y);
void info_release();
void info_loop();

App apps[] = {
  { "Malen", TFT_CYAN,   paint_enter, paint_touch, paint_release, paint_loop },
  { "Uhr",   TFT_ORANGE, clock_enter, clock_touch, clock_release, clock_loop },
  { "Info",  TFT_GREEN,  info_enter,  info_touch,  info_release,  info_loop  },
};
const int APP_COUNT = sizeof(apps) / sizeof(apps[0]);

int currentApp = -1;     // -1 = Menü aktiv
bool wasTouched = false; // für Tap-Flankenerkennung im Menü/Home-Button

// =====================================================================
// ===================== HOME-BUTTON (oben links) =======================
// =====================================================================
#define HOME_X 0
#define HOME_Y 0
#define HOME_W 36
#define HOME_H 28

void drawHomeButton() {
  tft.fillRoundRect(HOME_X + 2, HOME_Y + 2, HOME_W - 4, HOME_H - 4, 4, TFT_DARKGREY);
  tft.drawRoundRect(HOME_X + 2, HOME_Y + 2, HOME_W - 4, HOME_H - 4, 4, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(HOME_X + 8, HOME_Y + 10);
  tft.print("<<");
}

bool touchInHomeButton(int x, int y) {
  return x >= HOME_X && x <= HOME_X + HOME_W && y >= HOME_Y && y <= HOME_Y + HOME_H;
}

// =====================================================================
// ============================ TOUCH LESEN ==============================
// =====================================================================
bool getTouch(int &x, int &y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = constrain(map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    y = constrain(map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
    return true;
  }
  return false;
}

// =====================================================================
// =============================== MENÜ ==================================
// =====================================================================
#define TILE_MARGIN 10
#define TILE_H 70

void drawMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Hauptmenue");

  int y = 40;
  for (int i = 0; i < APP_COUNT; i++) {
    int tx = TILE_MARGIN;
    int ty = y;
    int tw = SCREEN_W - 2 * TILE_MARGIN;
    int th = TILE_H;

    tft.fillRoundRect(tx, ty, tw, th, 8, apps[i].color);
    tft.drawRoundRect(tx, ty, tw, th, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(tx + 14, ty + th / 2 - 8);
    tft.print(apps[i].name);

    y += TILE_H + TILE_MARGIN;
  }
}

bool tileHit(int index, int x, int y) {
  int ty = 40 + index * (TILE_H + TILE_MARGIN);
  int tx = TILE_MARGIN;
  int tw = SCREEN_W - 2 * TILE_MARGIN;
  return x >= tx && x <= tx + tw && y >= ty && y <= ty + TILE_H;
}

void openApp(int index) {
  currentApp = index;
  tft.fillScreen(TFT_BLACK);
  apps[index].enter();
  drawHomeButton();
}

void goToMenu() {
  currentApp = -1;
  drawMenu();
}

void handleMenuTouch(int x, int y) {
  for (int i = 0; i < APP_COUNT; i++) {
    if (tileHit(i, x, y)) {
      openApp(i);
      return;
    }
  }
}

// =====================================================================
// =============================== SETUP ==================================
// =====================================================================
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  drawMenu();
}

// =====================================================================
// =============================== LOOP ===================================
// =====================================================================
void loop() {
  int x, y;
  bool touched = getTouch(x, y);

  if (currentApp == -1) {
    // Im Menü: nur auf Tap reagieren (Flanke), kein Dauer-Touch nötig
    if (touched && !wasTouched) {
      handleMenuTouch(x, y);
    }
  } else {
    // In einer App: Home-Button hat Vorrang vor der App-Logik
    if (touched && touchInHomeButton(x, y)) {
      if (!wasTouched) {
        goToMenu();
      }
    } else if (touched) {
      apps[currentApp].onTouch(x, y);
    } else if (wasTouched) {
      apps[currentApp].onRelease();
    }
    apps[currentApp].loop();
  }

  wasTouched = touched;
}

// =====================================================================
// =====================  APP: MALEN (VOLL AUSSTATTUNG)  ===============
// =====================================================================
// Portiert vom CYD-Drawing-App standalone project:
// 8 Farben, Radierer, Leinwand löschen, Speichern (SPIFFS), Pinselgröße

// ── Layout (240×320 portrait) ─────────────────────────────────────
#define P_TOOLBAR_Y   270     // toolbar start
#define P_ROW_H       24      // color row height
#define P_CANVAS_Y    30      // below title bar

// ── Farbpalette (8) ───────────────────────────────────────────────
static const uint16_t p_palette[8] = {
  TFT_BLACK,   TFT_RED,     TFT_GREEN, TFT_BLUE,
  TFT_YELLOW,  TFT_CYAN,    TFT_MAGENTA, TFT_WHITE
};

// ── Zustand ────────────────────────────────────────────────────────
static uint16_t p_color    = TFT_BLACK;
static uint8_t  p_penSize  = 3;
static bool     p_eraserOn = false;
static int16_t  p_lastX    = -1;
static int16_t  p_lastY    = -1;
static bool     p_isDown   = false;

// ═════════════════════════════════════════════════════════════════
//  TOOLBAR
// ═════════════════════════════════════════════════════════════════

static void p_drawToolbar() {
  int cw = SCREEN_W / 8;
  for (int i = 0; i < 8; i++) {
    int x = i * cw;
    tft.fillRect(x, P_TOOLBAR_Y, cw - 1, P_ROW_H, p_palette[i]);
    if (p_palette[i] == p_color) {
      tft.drawRect(x + 1, P_TOOLBAR_Y + 1, cw - 3, P_ROW_H - 2, p_eraserOn ? TFT_RED : TFT_WHITE);
      tft.drawRect(x + 2, P_TOOLBAR_Y + 2, cw - 5, P_ROW_H - 4, p_eraserOn ? TFT_RED : TFT_WHITE);
    }
  }

  int bw = SCREEN_W / 5;
  int by = P_TOOLBAR_Y + P_ROW_H;
  int bh = SCREEN_H - by;

  const char *labels[5] = {"RAD", "NEU", "SPEICH", "SZ+", "SZ-"};
  uint16_t bg[5];
  bg[0] = p_eraserOn ? TFT_RED : 0x0841;
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
      tft.fillCircle(bx + bw - 10, by + bh / 2, p_penSize,
                     p_eraserOn ? TFT_WHITE : p_color);
    }
  }

  tft.drawFastHLine(0, P_TOOLBAR_Y,     SCREEN_W, TFT_WHITE);
  tft.drawFastHLine(0, P_TOOLBAR_Y + P_ROW_H, SCREEN_W, TFT_DARKGREY);
}

// ═════════════════════════════════════════════════════════════════
//  SPEICHERN (BMP via SPIFFS)
// ═════════════════════════════════════════════════════════════════

static void p_saveDrawing() {
  if (!SPIFFS.begin(true)) return;

  String path;
  for (int i = 0; i < 1000; i++) {
    path = "/zeichnung_" + String(i) + ".bmp";
    if (!SPIFFS.exists(path)) break;
  }

  size_t   nPixels = SCREEN_W * SCREEN_H;
  uint16_t *fb = new uint16_t[nPixels];
  tft.readRect(0, 0, SCREEN_W, SCREEN_H, fb);

  fs::File f = SPIFFS.open(path, FILE_WRITE);
  if (!f) { delete[] fb; return; }

  uint32_t rowSize   = ((SCREEN_W * 3) + 3) & ~3U;
  uint32_t pixOffset = 54;
  uint32_t fileSize  = pixOffset + rowSize * SCREEN_H;

  uint8_t hdr[54] = {0};
  hdr[0] = 'B'; hdr[1] = 'M';
  *(uint32_t *)(hdr + 2)  = fileSize;
  *(uint32_t *)(hdr + 10) = pixOffset;
  *(uint32_t *)(hdr + 14) = 40;
  *(uint32_t *)(hdr + 18) = SCREEN_W;
  *(uint32_t *)(hdr + 22) = SCREEN_H;
  *(uint16_t *)(hdr + 26) = 1;
  *(uint16_t *)(hdr + 28) = 24;
  f.write(hdr, 54);

  uint8_t *row = new uint8_t[rowSize]();
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

// ═════════════════════════════════════════════════════════════════
//  APP CALLBACKS
// ═════════════════════════════════════════════════════════════════

void paint_enter() {
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(44, 8);
  tft.print("Malen");
  tft.fillRect(0, P_CANVAS_Y, SCREEN_W, P_TOOLBAR_Y - P_CANVAS_Y, TFT_WHITE);
  p_drawToolbar();
  p_isDown = false;
  p_lastX = p_lastY = -1;
}

void paint_clearCanvas() {
  tft.fillRect(0, P_CANVAS_Y, SCREEN_W, P_TOOLBAR_Y - P_CANVAS_Y, TFT_WHITE);
}

void paint_touch(int x, int y) {
  if (y >= P_TOOLBAR_Y) {
    int cw = SCREEN_W / 8;

    if (y < P_TOOLBAR_Y + P_ROW_H) {
      int idx = x / cw;
      if (idx >= 0 && idx < 8) {
        p_color    = p_palette[idx];
        p_eraserOn = false;
        p_drawToolbar();
      }
    } else {
      int bw = SCREEN_W / 5;
      int ti = x / bw;
      switch (ti) {
        case 0: p_eraserOn = !p_eraserOn; p_drawToolbar(); break;
        case 1: paint_clearCanvas();                  break;
        case 2: p_saveDrawing();                       break;
        case 3: if (p_penSize < 20) p_penSize += 2; p_drawToolbar(); break;
        case 4: if (p_penSize > 1)  p_penSize -= 2; p_drawToolbar(); break;
      }
    }

    p_isDown = false;
    p_lastX = p_lastY = -1;
    return;
  }

  if (y < P_CANVAS_Y) return;

  uint16_t drawColor = p_eraserOn ? TFT_WHITE : p_color;

  if (!p_isDown || p_lastX < 0) {
    tft.fillCircle(x, y, p_penSize, drawColor);
  } else {
    int dx = x - p_lastX;
    int dy = y - p_lastY;
    int steps = max(abs(dx), abs(dy));
    for (int i = 0; i <= steps; i++) {
      int cx = p_lastX + (dx * i) / steps;
      int cy = p_lastY + (dy * i) / steps;
      tft.fillCircle(cx, cy, p_penSize, drawColor);
    }
  }

  p_lastX = x;
  p_lastY = y;
  p_isDown = true;
}

void paint_release() {
  p_isDown = false;
  p_lastX = p_lastY = -1;
}

void paint_loop() {
}

// =====================================================================
// ===========================  APP: UHR  ================================
// =====================================================================
unsigned long clock_lastDraw = 0;

void clock_enter() {
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(44, 8);
  tft.print("Uhr (Laufzeit)");
  clock_lastDraw = 0;
}

void clock_touch(int x, int y) {
  // Diese App reagiert nicht auf Touch im Inhaltsbereich
}

void clock_release() {}

void clock_loop() {
  unsigned long now = millis();
  if (now - clock_lastDraw < 200) return; // alle 200ms aktualisieren
  clock_lastDraw = now;

  unsigned long s = now / 1000;
  int hh = (s / 3600) % 24;
  int mm = (s / 60) % 60;
  int ss = s % 60;

  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);

  tft.fillRect(0, 120, SCREEN_W, 40, TFT_BLACK);
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(4);
  tft.setCursor(20, 130);
  tft.print(buf);
}

// =====================================================================
// ===========================  APP: INFO  ================================
// =====================================================================
void info_enter() {
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(44, 8);
  tft.print("Info");

  tft.setCursor(10, 50);
  tft.setTextSize(1);
  tft.println("ESP32 Touch Menue Demo");
  tft.setCursor(10, 70);
  tft.println("Touch oben links = zurueck");
  tft.setCursor(10, 90);
  tft.println("zum Hauptmenue.");
  tft.setCursor(10, 110);
  tft.println("Neue Apps koennen im");
  tft.setCursor(10, 130);
  tft.println("apps[] Array ergaenzt");
  tft.setCursor(10, 150);
  tft.println("werden.");
}

void info_touch(int x, int y) {}
void info_release() {}
void info_loop() {}
