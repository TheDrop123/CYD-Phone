#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include <FS.h>


// ============ TOUCH PINS ============
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ============ DISPLAY ============
#define SCREEN_W 240
#define SCREEN_H 320

// ============ TOUCH CALIBRATION ============
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800
//SD
bool sdReady = false;
#define SD_CS 5

// ============ WIFI ============
const char* WIFI_SSID = "Jugendhackt";
const char* WIFI_PASS = "Jug-!hackt";

// ============ TIME SETTINGS ============
// Deutschland:
// Winter: GMT_OFFSET_SEC = 3600
// Sommer: GMT_OFFSET_SEC = 7200

const char* NTP_SERVER = "pool.ntp.org";

long GMT_OFFSET_SEC = 3600;
int DAYLIGHT_OFFSET_SEC = 3600;

// ============ OBJECTS ============
TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ================= BUTTON =================

struct Button {
  int x;
  int y;
  int w;
  int h;
  uint16_t color;
  String text;
};

void drawButton(
  int x,
  int y,
  int w,
  int h,
  uint16_t color,
  String text) {
  tft.fillRoundRect(x, y, w, h, 8, color);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);

  tft.setTextColor(TFT_WHITE, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, x + w / 2, y + h / 2, 2);
}

bool isButtonPressed(
  int tx,
  int ty,
  int x,
  int y,
  int w,
  int h) {
  return (
    tx >= x && tx <= x + w && ty >= y && ty <= y + h);
}

// ================= WIFI =================

void connectWiFi() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Verbinde WLAN...", 120, 150, 2);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WLAN verbunden");

  tft.fillScreen(TFT_BLACK);
  tft.drawString("WLAN verbunden", 120, 150, 2);

  delay(1000);
}

// ================= TIME =================

void initTime() {
  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    NTP_SERVER);

  struct tm timeinfo;

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Hole Uhrzeit...", 120, 150, 2);

  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.println("Warte auf NTP...");
  }

  Serial.println("Zeit synchronisiert");

  tft.fillScreen(TFT_BLACK);
  tft.drawString("Zeit synchronisiert", 120, 150, 2);

  delay(1000);
}

void drawClock() {
  static String lastTime = "";

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
    return;

  char timeBuffer[16];
  char dateBuffer[20];

  strftime(
    timeBuffer,
    sizeof(timeBuffer),
    "%H:%M:%S",
    &timeinfo);

  strftime(
    dateBuffer,
    sizeof(dateBuffer),
    "%d.%m.%Y",
    &timeinfo);

  String currentTime = String(timeBuffer);

  if (currentTime != lastTime) {
    lastTime = currentTime;

    tft.fillRect(
      0,
      278,
      SCREEN_W,
      42,
      TFT_BLACK);

    tft.drawFastHLine(
      0,
      278,
      SCREEN_W,
      TFT_DARKGREY);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(
      TFT_YELLOW,
      TFT_BLACK);

    tft.drawString(
      currentTime,
      SCREEN_W / 2,
      292,
      4);

    tft.setTextColor(
      TFT_WHITE,
      TFT_BLACK);

    tft.drawString(
      String(dateBuffer),
      SCREEN_W / 2,
      312,
      2);
  }
}

// ================= TOUCH =================

bool getTouch(int& x, int& y) {
  if (!touchscreen.tirqTouched())
    return false;

  if (!touchscreen.touched())
    return false;

  TS_Point p = touchscreen.getPoint();

  x = map(
    p.x,
    TOUCH_X_MIN,
    TOUCH_X_MAX,
    0,
    SCREEN_W);

  y = map(
    p.y,
    TOUCH_Y_MIN,
    TOUCH_Y_MAX,
    0,
    SCREEN_H);

  return true;
}

// ================= MENU =================

void drawMenu() {
  tft.fillScreen(TFT_BLACK);

  drawButton(
    10,
    20,
    100,
    40,
    TFT_BLUE,
    "Calc");

  drawButton(
    130,
    20,
    100,
    40,
    TFT_RED,
    "Draw");

  drawButton(
    10,
    80,
    100,
    40,
    TFT_GREEN,
    "Notes");

  drawButton(
    130,
    80,
    100,
    40,
    TFT_CYAN,
    "Chat");

  drawButton(
    10,
    140,
    100,
    40,
    TFT_MAGENTA,
    "Read");

  drawButton(
    130,
    140,
    100,
    40,
    TFT_ORANGE,
    "Settings");

  tft.drawFastHLine(
    0,
    278,
    SCREEN_W,
    TFT_DARKGREY);
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(2);
  initSD();
    touchscreenSPI.begin(
      XPT2046_CLK,
      XPT2046_MISO,
      XPT2046_MOSI,
      XPT2046_CS);

  touchscreen.begin(
    touchscreenSPI);

  touchscreen.setRotation(2);

  connectWiFi();
  initTime();

  drawMenu();
}

// ================= LOOP =================

void loop() {
  drawClock();

  int tx;
  int ty;

  if (getTouch(tx, ty)) {
    if (
      isButtonPressed(
        tx, ty,
        10, 20,
        100, 40)) {
      Serial.println("Calc");
    }

    if (
      isButtonPressed(
        tx, ty,
        130, 20,
        100, 40)) {
      Serial.println("Draw");
    }

    if (
      isButtonPressed(
        tx, ty,
        10, 80,
        100, 40)) {
      Serial.println("Notes");
    }

    if (
      isButtonPressed(
        tx, ty,
        130, 80,
        100, 40)) {
      Serial.println("Chat");
    }

    if (
      isButtonPressed(
        tx, ty,
        10, 140,
        100, 40)) {
      Serial.println("Read");
      showMarkdown("/notizen.md");
    }

    if (
      isButtonPressed(
        tx, ty,
        130, 140,
        100, 40)) {
      Serial.println("Settings");
    }

    delay(200);
  }
}

void initSD() {
  if (SD.begin(SD_CS)) {
    Serial.println("SD OK");
    sdReady = true;
  } else {
    Serial.println("SD Fehler");
    sdReady = false;
  }
}

void listMarkdownFiles()
{
    File root = SD.open("/");

    while(true)
    {
        File file = root.openNextFile();

        if(!file)
            break;

        String name = file.name();

        if(name.endsWith(".md"))
        {
            Serial.println(name);
        }

        file.close();
    }
}

void drawMDLine(String line, int y)
{
    if(line.startsWith("# "))
    {
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(
            line.substring(2),
            5,
            y,
            4
        );
    }
    else if(line.startsWith("## "))
    {
        tft.setTextColor(TFT_CYAN);
        tft.drawString(
            line.substring(3),
            5,
            y,
            2
        );
    }
    else
    {
        tft.setTextColor(TFT_WHITE);
        tft.drawString(
            line,
            5,
            y,
            2
        );
    }
}

void showMarkdown(String path)
{
    String text = readFile(path);

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextWrap(true);

    int y = 0;

    String line = "";

    for (int i = 0; i < text.length(); i++)
    {
        char c = text[i];

        if (c == '\n')
        {
            tft.drawString(
                line,
                5,
                y,
                2
            );

            y += 16;
            line = "";
        }
        else
        {
            line += c;
        }
    }
}

String readFile(String path)
{
    File file = SD.open(path);

    if (!file)
        return "Datei nicht gefunden";

    String content;

    while (file.available())
    {
        content += (char)file.read();
    }

    file.close();

    return content;
}

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>

// ============ TOUCH PINS ============
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ============ DISPLAY ============
#define SCREEN_W 240
#define SCREEN_H 320

// ============ TOUCH CALIBRATION ============
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800

// ============ WIFI ============
const char* WIFI_SSID = "Noah iphone";
const char* WIFI_PASS = "13278965";

// ============ TIME SETTINGS ============
// Deutschland:
// Winter: GMT_OFFSET_SEC = 3600
// Sommer: GMT_OFFSET_SEC = 7200

const char* NTP_SERVER = "pool.ntp.org";

long GMT_OFFSET_SEC = 3600;
int DAYLIGHT_OFFSET_SEC = 3600;

// ============ OBJECTS ============
TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ================= BUTTON =================

struct Button
{
    int x;
    int y;
    int w;
    int h;
    uint16_t color;
    String text;
};

void drawButton(
    int x,
    int y,
    int w,
    int h,
    uint16_t color,
    String text)
{
    tft.fillRoundRect(x, y, w, h, 8, color);
    tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(text, x + w / 2, y + h / 2, 2);
}

bool isButtonPressed(
    int tx,
    int ty,
    int x,
    int y,
    int w,
    int h)
{
    return (
        tx >= x &&
        tx <= x + w &&
        ty >= y &&
        ty <= y + h
    );
}

// ================= WIFI =================

void connectWiFi()
{
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Verbinde WLAN...", 120, 150, 2);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WLAN verbunden");

    tft.fillScreen(TFT_BLACK);
    tft.drawString("WLAN verbunden", 120, 150, 2);

    delay(1000);
}

// ================= TIME =================

void initTime()
{
    configTime(
        GMT_OFFSET_SEC,
        DAYLIGHT_OFFSET_SEC,
        NTP_SERVER
    );

    struct tm timeinfo;

    tft.fillScreen(TFT_BLACK);
    tft.drawString("Hole Uhrzeit...", 120, 150, 2);

    while (!getLocalTime(&timeinfo))
    {
        delay(500);
        Serial.println("Warte auf NTP...");
    }

    Serial.println("Zeit synchronisiert");

    tft.fillScreen(TFT_BLACK);
    tft.drawString("Zeit synchronisiert", 120, 150, 2);

    delay(1000);
}

void drawClock()
{
    static String lastTime = "";

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
        return;

    char timeBuffer[16];
    char dateBuffer[20];

    strftime(
        timeBuffer,
        sizeof(timeBuffer),
        "%H:%M:%S",
        &timeinfo
    );

    strftime(
        dateBuffer,
        sizeof(dateBuffer),
        "%d.%m.%Y",
        &timeinfo
    );

    String currentTime = String(timeBuffer);

    if (currentTime != lastTime)
    {
        lastTime = currentTime;

        tft.fillRect(
            0,
            278,
            SCREEN_W,
            42,
            TFT_BLACK
        );

        tft.drawFastHLine(
            0,
            278,
            SCREEN_W,
            TFT_DARKGREY
        );

        tft.setTextDatum(MC_DATUM);

        tft.setTextColor(
            TFT_YELLOW,
            TFT_BLACK
        );

        tft.drawString(
            currentTime,
            SCREEN_W / 2,
            292,
            4
        );

        tft.setTextColor(
            TFT_WHITE,
            TFT_BLACK
        );

        tft.drawString(
            String(dateBuffer),
            SCREEN_W / 2,
            312,
            2
        );
    }
}

// ================= TOUCH =================

bool getTouch(int &x, int &y)
{
    if (!touchscreen.tirqTouched())
        return false;

    if (!touchscreen.touched())
        return false;

    TS_Point p = touchscreen.getPoint();

    x = map(
        p.x,
        TOUCH_X_MIN,
        TOUCH_X_MAX,
        0,
        SCREEN_W
    );

    y = map(
        p.y,
        TOUCH_Y_MIN,
        TOUCH_Y_MAX,
        0,
        SCREEN_H
    );

    return true;
}

// ================= MENU =================

void drawMenu()
{
    tft.fillScreen(TFT_BLACK);

    drawButton(
        10,
        20,
        100,
        40,
        TFT_BLUE,
        "Calc"
    );

    drawButton(
        130,
        20,
        100,
        40,
        TFT_RED,
        "Draw"
    );

    drawButton(
        10,
        80,
        100,
        40,
        TFT_GREEN,
        "Notes"
    );

    drawButton(
        130,
        80,
        100,
        40,
        TFT_CYAN,
        "Chat"
    );

    drawButton(
        10,
        140,
        100,
        40,
        TFT_MAGENTA,
        "Read"
    );

    drawButton(
        130,
        140,
        100,
        40,
        TFT_ORANGE,
        "Settings"
    );

    tft.drawFastHLine(
        0,
        278,
        SCREEN_W,
        TFT_DARKGREY
    );
}

// ================= SETUP =================

void setup()
{
    Serial.begin(115200);

    tft.init();
    tft.setRotation(2);

    touchscreenSPI.begin(
        XPT2046_CLK,
        XPT2046_MISO,
        XPT2046_MOSI,
        XPT2046_CS
    );

    touchscreen.begin(
        touchscreenSPI
    );

    touchscreen.setRotation(2);

    connectWiFi();
    initTime();

    drawMenu();
}

// ================= LOOP =================

void loop()
{
    drawClock();

    int tx;
    int ty;

    if (getTouch(tx, ty))
    {
        if (
            isButtonPressed(
                tx, ty,
                10, 20,
                100, 40
            )
        )
        {
            Serial.println("Calc");
        }

        if (
            isButtonPressed(
                tx, ty,
                130, 20,
                100, 40
            )
        )
        {
            Serial.println("Draw");
        }

        if (
            isButtonPressed(
                tx, ty,
                10, 80,
                100, 40
            )
        )
        {
            Serial.println("Notes");
        }

        if (
            isButtonPressed(
                tx, ty,
                130, 80,
                100, 40
            )
        )
        {
            Serial.println("Chat");
        }

        if (
            isButtonPressed(
                tx, ty,
                10, 140,
                100, 40
            )
        )
        {
            Serial.println("Read");
        }

        if (
            isButtonPressed(
                tx, ty,
                130, 140,
                100, 40
            )
        )
        {
            Serial.println("Time");
        }

        delay(200);
    }
}
