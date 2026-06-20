#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

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
// ===========================  APP: MALEN  =============================
// =====================================================================
#define PALETTE_Y 30
#define PALETTE_H 24
#define DRAW_AREA_Y (PALETTE_Y + PALETTE_H)

uint16_t paint_colors[] = { TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_BLACK };
const int paint_colorCount = sizeof(paint_colors) / sizeof(paint_colors[0]);
uint16_t paint_currentColor = TFT_WHITE;

int paint_lastX = -1, paint_lastY = -1;
bool paint_strokeActive = false;

void paint_drawPalette() {
  int swatchW = SCREEN_W / (paint_colorCount + 1); // +1 für "Clear"-Feld
  for (int i = 0; i < paint_colorCount; i++) {
    tft.fillRect(i * swatchW, PALETTE_Y, swatchW, PALETTE_H, paint_colors[i]);
    if (paint_colors[i] == paint_currentColor) {
      tft.drawRect(i * swatchW, PALETTE_Y, swatchW, PALETTE_H, TFT_RED);
      tft.drawRect(i * swatchW + 1, PALETTE_Y + 1, swatchW - 2, PALETTE_H - 2, TFT_RED);
    }
  }
  // Clear-Feld ganz rechts
  int cx = paint_colorCount * swatchW;
  int cw = SCREEN_W - cx;
  tft.fillRect(cx, PALETTE_Y, cw, PALETTE_H, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setCursor(cx + 4, PALETTE_Y + 8);
}

void paint_enter() {
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(44, 8);
  tft.print("Malen");
  paint_drawPalette();
  tft.fillRect(0, DRAW_AREA_Y, SCREEN_W, SCREEN_H - DRAW_AREA_Y, TFT_BLACK);
  paint_strokeActive = false;
}

void paint_clearCanvas() {
  tft.fillRect(0, DRAW_AREA_Y, SCREEN_W, SCREEN_H - DRAW_AREA_Y, TFT_BLACK);
}

void paint_touch(int x, int y) {
  // Touch in der Farbpalette?
  if (y >= PALETTE_Y && y < DRAW_AREA_Y) {
    int swatchW = SCREEN_W / (paint_colorCount + 1);
    int idx = x / swatchW;
    if (idx < paint_colorCount) {
      paint_currentColor = paint_colors[idx];
      paint_drawPalette();
    } else {
      paint_clearCanvas();
    }
    paint_strokeActive = false; // Strich unterbrechen, wenn Palette berührt wurde
    return;
  }

  // Zeichnen im Malbereich
  tft.fillCircle(x, y, 2, paint_currentColor);
  if (paint_strokeActive) {
    tft.drawWideLine(paint_lastX, paint_lastY, x, y, 2, paint_currentColor);
  }
  paint_lastX = x;
  paint_lastY = y;
  paint_strokeActive = true;
}

void paint_release() {
  paint_strokeActive = false;
}

void paint_loop() {
  // Für die Malen-App nicht benötigt
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
