#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ==================== PINS ====================
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define draw_color TFT_WHITE

bool fill_gab = false;
bool first_touch = true;

// ==================== OBJEKTE ====================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
int x, y;
// ==================== TOUCH ====================
bool getTouch(int &x, int &y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    x = constrain(map(p.x, 200, 3800, 0, 239), 0, 239);
    y = constrain(map(p.y, 200, 3800, 0, 319), 0, 319);

    return true;
  }
  return false;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  // Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Touch
  touchscreenSPI.begin(
    XPT2046_CLK,
    XPT2046_MISO,
    XPT2046_MOSI,
    XPT2046_CS);

  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  // Bootanzeige
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
}

// ==================== LOOP ====================
void loop() {
  draw_point();
}

void draw_point() {
  int old_y = y;
  int old_x = x;
  

  if (getTouch(x, y)) {
    fill_gab = true;
    if (first_touch = true) {
      fill_gab = false;
    }
    tft.fillCircle(x, y, 2, draw_color);
    if (fill_gab == true) {
      tft.drawWideLine(x, y, old_x, old_y, 2, draw_color);
    }
    first_touch = false;
  } else {
    fill_gab = false;
  }
}
