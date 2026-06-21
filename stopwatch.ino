#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SD.h>
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include "icons.h"

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

// ============ SD ============
bool sdReady = false;
#define SD_CS 5

// ============ WIFI ============
String WIFI_SSID = "Noah iphone";  // jetzt aenderbar (vorher const char*)
String WIFI_PASS = "13278965";  // jetzt aenderbar (vorher const char*)

// ============ TIME SETTINGS ============
const char* NTP_SERVER = "pool.ntp.org";
long GMT_OFFSET_SEC = 3600;
int DAYLIGHT_OFFSET_SEC = 3600;

// ============ FEATURE TOGGLES ============
bool useWiFiTime = false;
bool darkMode = false;  // Dark Mode: true = dunkel, false = hell

// ============ GLOBALE STRUCTS ============
struct FMEntry {
  String name;
  bool isDir;
};

struct RenderedLine {
  String text;
  uint16_t color;
  int fontSize;
  int xOffset;
  int height;
};

// ============ THEME COLORS ============
// Werden von applyTheme() gesetzt und ueberall verwendet
uint16_t BG_COLOR = TFT_BLACK;
uint16_t TEXT_COLOR = TFT_WHITE;
uint16_t PANEL_COLOR = 0x0841;  // dunkelgrau
uint16_t HEADER_COLOR = TFT_DARKGREY;
uint16_t BORDER_COLOR = TFT_DARKGREY;

void applyTheme() {
  if (darkMode) {
    BG_COLOR = TFT_BLACK;
    TEXT_COLOR = TFT_WHITE;
    PANEL_COLOR = 0x1082;
    HEADER_COLOR = 0x2104;
    BORDER_COLOR = TFT_DARKGREY;
  } else {
    BG_COLOR = 0xFFFF;  // weiss
    TEXT_COLOR = TFT_BLACK;
    PANEL_COLOR = 0xC618;   // hellgrau
    HEADER_COLOR = 0x8410;  // mittelgrau
    BORDER_COLOR = 0x8410;
  }
}

// ============ SETTINGS SPEICHERN / LADEN ============
#define SETTINGS_PATH "/settings.cfg"

void saveSettings() {
  if (!sdReady) return;
  File f = SD.open(SETTINGS_PATH, FILE_WRITE);
  if (!f) return;
  f.printf("wifi=%d\n", useWiFiTime ? 1 : 0);
  f.printf("dark=%d\n", darkMode ? 1 : 0);
  f.printf("gmt=%ld\n", GMT_OFFSET_SEC);
  f.printf("ssid=%s\n", WIFI_SSID.c_str());
  f.printf("pass=%s\n", WIFI_PASS.c_str());
  f.close();
  Serial.println("Einstellungen gespeichert");
}

void loadSettings() {
  if (!sdReady) return;
  if (!SD.exists(SETTINGS_PATH)) return;
  File f = SD.open(SETTINGS_PATH, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("wifi=")) useWiFiTime = line.substring(5).toInt() != 0;
    else if (line.startsWith("dark=")) darkMode = line.substring(5).toInt() != 0;
    else if (line.startsWith("gmt=")) GMT_OFFSET_SEC = line.substring(4).toInt();
    else if (line.startsWith("ssid=")) WIFI_SSID = line.substring(5);
    else if (line.startsWith("pass=")) WIFI_PASS = line.substring(5);
  }
  f.close();
  applyTheme();
  Serial.println("Einstellungen geladen");
}

// ============ OBJECTS ============
TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Forward declarations
void calculator();
void drawing();
void notesApp();
void chatApp();
void showFileSelection();
void stundenplanApp();
void settingsApp();
void fileManagerApp();
void wifiScanAndConfigure();
void drawMenu();
void ensureWiFi();
void initSD();
bool getTouch(int& x, int& y);

// ================= BUTTON =================

void drawButton(int x, int y, int w, int h, uint16_t color, String text) {
  tft.fillRoundRect(x, y, w, h, 8, color);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(text, x + w / 2, y + h / 2, 2);
}

bool isButtonPressed(int tx, int ty, int x, int y, int w, int h) {
  return (tx >= x && tx <= x + w && ty >= y && ty <= y + h);
}

void drainTouch() {
  int _tx, _ty;
  while (getTouch(_tx, _ty)) delay(10);
}

// ================= WIFI =================

void connectWiFi() {
  if (!useWiFiTime) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi deaktiviert", 120, 150, 2);
    delay(1000);
    return;
  }
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Verbinde WLAN...", 120, 150, 2);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    tries++;
  }
  tft.fillScreen(BG_COLOR);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, BG_COLOR);
    tft.drawString("WLAN verbunden", 120, 150, 2);
  } else {
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.drawString("WLAN fehlgeschlagen", 120, 150, 2);
  }
  delay(1000);
}

// ================= TIME =================

void initTime() {
  if (!useWiFiTime) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Zeit-Sync deaktiviert", 120, 155, 2);
    delay(1000);
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Kein WLAN - keine Zeit", 120, 155, 2);
    delay(1000);
    return;
  }
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm timeinfo;
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Hole Uhrzeit...", 120, 150, 2);
  int tries = 0;
  while (!getLocalTime(&timeinfo) && tries < 20) {
    delay(500);
    tries++;
  }
  tft.fillScreen(BG_COLOR);
  if (tries < 20) {
    tft.setTextColor(TFT_GREEN, BG_COLOR);
    tft.drawString("Zeit synchronisiert", 120, 150, 2);
  } else {
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.drawString("Zeit-Sync fehlgeschlagen", 120, 150, 2);
  }
  delay(1000);
}

void drawClock() {
  static bool lastDarkMode = !darkMode;
  static bool lastUseWiFiTime = !useWiFiTime;
  static String lastTime = "";

  if (!useWiFiTime) {
    if (lastUseWiFiTime != useWiFiTime || lastDarkMode != darkMode) {
      tft.fillRect(0, 278, SCREEN_W, 42, BG_COLOR);
      tft.drawFastHLine(0, 278, SCREEN_W, BORDER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, BG_COLOR);
      tft.drawString("Keine Zeit", SCREEN_W / 2, 295, 4);
      lastUseWiFiTime = useWiFiTime;
      lastDarkMode = darkMode;
      lastTime = "";
    }
    return;
  }
  struct tm timeinfo;
  // WICHTIG: kurzes Timeout (5ms) statt Standard-5000ms!
  // Sonst blockiert drawClock() bis zu 5 Sekunden lang, wenn die Zeit
  // (noch) nicht synchron ist -> Touch-Eingaben werden in der Zeit
  // "verschluckt" und die UI wirkt extrem traege.
  if (!getLocalTime(&timeinfo, 5)) {
    if (lastUseWiFiTime != useWiFiTime || lastDarkMode != darkMode) {
      tft.fillRect(0, 278, SCREEN_W, 42, BG_COLOR);
      tft.drawFastHLine(0, 278, SCREEN_W, BORDER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, BG_COLOR);
      tft.drawString("Keine Zeit", SCREEN_W / 2, 295, 4);
      lastUseWiFiTime = useWiFiTime;
      lastDarkMode = darkMode;
    }
    return;
  }
  char timeBuffer[16], dateBuffer[20];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d.%m.%Y", &timeinfo);
  String currentTime = String(timeBuffer);
  if (currentTime != lastTime || lastUseWiFiTime != useWiFiTime || lastDarkMode != darkMode) {
    lastTime = currentTime;
    lastUseWiFiTime = useWiFiTime;
    lastDarkMode = darkMode;
    tft.fillRect(0, 278, SCREEN_W, 42, BG_COLOR);
    tft.drawFastHLine(0, 278, SCREEN_W, BORDER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, BG_COLOR);
    tft.drawString(currentTime, SCREEN_W / 2, 292, 4);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.drawString(String(dateBuffer), SCREEN_W / 2, 312, 2);
  }
}

// ================= TOUCH =================

bool getTouch(int& x, int& y) {
  if (!touchscreen.tirqTouched()) return false;
  if (!touchscreen.touched()) return false;
  TS_Point p = touchscreen.getPoint();
  x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  return true;
}

// ================= MENU ICONS =================

void drawMenuIcon(int x, int y, const char* name) {
  const uint16_t* arr = nullptr;
  if (strcmp(name, "Calc") == 0) arr = icon_calc;
  else if (strcmp(name, "Draw") == 0) arr = icon_draw;
  else if (strcmp(name, "Notes") == 0) arr = icon_notes;
  else if (strcmp(name, "Chat") == 0) arr = icon_chat;
  else if (strcmp(name, "Read") == 0) arr = icon_book;
  else if (strcmp(name, "Settings") == 0) arr = icon_settings;
  else if (strcmp(name, "Stundenplan") == 0) arr = icon_untis;
  else if (strcmp(name, "Stoppuhr") == 0) {
    // Einfaches Stoppuhr-Icon zeichnen (Uhr + Start/Stop)
    tft.fillCircle(x + 14, y + 18, 10, TFT_WHITE);
    tft.drawCircle(x + 14, y + 18, 10, TFT_BLACK);
    tft.drawLine(x + 14, y + 18, x + 14, y + 10, TFT_BLACK);
    tft.drawLine(x + 14, y + 18, x + 20, y + 18, TFT_BLACK);
    return;
  }
  if (arr) tft.pushImage(x + 2, y + 6, 24, 24, (uint16_t*)arr, TFT_BLACK);
}

// ================= MENU =================

struct AppButton {
  int x, y, w, h;
  uint16_t color;
  const char* label;
};

static const AppButton MENU_APPS[] = {
  { 10, 20, 100, 50, TFT_BLUE, "Calc" },
  { 130, 20, 100, 50, TFT_RED, "Draw" },
  { 10, 80, 100, 50, TFT_GREEN, "Notes" },
  { 130, 80, 100, 50, TFT_CYAN, "Chat" },
  { 10, 140, 100, 50, TFT_MAGENTA, "Read" },
  { 130, 140, 100, 50, TFT_ORANGE, "Settings" },
  { 40, 200, 160, 40, 0x07E0, "Stundenplan" },
  { 40, 245, 160, 40, TFT_PURPLE, "Stoppuhr" }  // NEU
};

static const int MENU_APPS_COUNT = sizeof(MENU_APPS) / sizeof(MENU_APPS[0]);

void drawMenu() {
  applyTheme();
  tft.fillScreen(BG_COLOR);
  for (int i = 0; i < MENU_APPS_COUNT; i++) {
    const AppButton& b = MENU_APPS[i];
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.color);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, darkMode ? TFT_WHITE : TFT_DARKGREY);
    drawMenuIcon(b.x + 4, b.y + 5, b.label);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(b.label, b.x + b.w / 2, b.y + b.h - 12, 2);
  }
  tft.drawFastHLine(0, 250, SCREEN_W, BORDER_COLOR);
}

// ================= SD CARD =================

void initSD() {
  if (SD.begin(SD_CS)) {
    Serial.println("SD OK");
    sdReady = true;
  } else {
    Serial.println("SD Fehler");
    sdReady = false;
  }
}

// ================= FILE LISTING =================

String readFile(String path) {
  if (!sdReady) return "SD-Karte nicht verfuegbar";
  File f = SD.open(path, FILE_READ);
  if (!f) return "Datei nicht gefunden";
  String content = "";
  while (f.available()) content += (char)f.read();
  f.close();
  return content;
}

int listMarkdownFiles(String* fileNames, int maxCount) {
  if (!sdReady) return 0;
  int count = 0;
  File root = SD.open("/");
  if (!root) return 0;
  File file = root.openNextFile();
  while (file && count < maxCount) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.endsWith(".txt") || name.endsWith(".TXT")) {
        fileNames[count++] = "/" + name;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return count;
}

// ================= TXT READER =================



// Bricht einen String in Displayzeilen auf und haengt sie an `out` an.
// Gibt die Gesamthoehe der erzeugten Zeilen zurueck.
void wrapToLines(String text, int xOffset, int maxWidth, int fontSize,
                 uint16_t color, std::vector<RenderedLine>& out) {
  int lineHeight = (fontSize == 4) ? 26 : (fontSize == 3) ? 22
                                                          : 16;
  String remaining = text;
  int safety = 0;
  while (remaining.length() > 0 && safety++ < 200) {
    String line = "";
    int i = 0;
    bool broke = false;
    while (i < (int)remaining.length()) {
      String test = line + remaining[i];
      if (tft.textWidth(test, fontSize) > maxWidth) {
        int sp = line.lastIndexOf(' ');
        if (sp > 0 && sp > (int)line.length() / 2) {
          line = line.substring(0, sp);
          remaining = remaining.substring(sp + 1);
        } else if (line.length() == 0) {
          line = test;
          remaining = remaining.substring(i + 1);
        } else {
          remaining = remaining.substring(i);
        }
        broke = true;
        break;
      }
      line = test;
      i++;
    }
    if (!broke) {
      line = remaining;
      remaining = "";
    }
    RenderedLine rl;
    rl.text = line;
    rl.color = color;
    rl.fontSize = fontSize;
    rl.xOffset = xOffset;
    rl.height = lineHeight;
    out.push_back(rl);
  }
}

// Wandelt eine Rohzeile in RenderedLines um (Markdown-Formatierung)
void parseTXTLine(String raw, std::vector<RenderedLine>& out) {
  raw.trim();
  if (raw.length() == 0) {
    RenderedLine blank;
    blank.text = "";
    blank.color = BG_COLOR;
    blank.fontSize = 1;
    blank.xOffset = 0;
    blank.height = 8;
    out.push_back(blank);
    return;
  }
  int fontSize = 2;
  uint16_t color = TEXT_COLOR;
  int xOffset = 5;
  int maxWidth = SCREEN_W - 10;
  if (raw.startsWith("# ")) {
    fontSize = 4;
    color = TFT_YELLOW;
    raw = raw.substring(2);
  } else if (raw.startsWith("## ")) {
    fontSize = 3;
    color = TFT_CYAN;
    raw = raw.substring(3);
  } else if (raw.startsWith("### ")) {
    fontSize = 2;
    color = TFT_GREEN;
    raw = raw.substring(4);
    xOffset = 10;
    maxWidth = SCREEN_W - 15;
  } else if (raw.startsWith("- ") || raw.startsWith("* ")) {
    raw = "• " + raw.substring(2);
    xOffset = 10;
    maxWidth = SCREEN_W - 15;
  } else if (raw.startsWith("   ") || raw.startsWith("\t")) {
    color = TFT_LIGHTGREY;
    xOffset = 15;
    maxWidth = SCREEN_W - 20;
  }
  wrapToLines(raw, xOffset, maxWidth, fontSize, color, out);
}

// Zeichnet den sichtbaren Bereich neu (Zeilen scrollOffset .. passend in CONTENT_H)
void renderPage(const std::vector<RenderedLine>& lines, int scrollOffset,
                int contentY, int contentH) {
  tft.fillRect(0, contentY, SCREEN_W, contentH, BG_COLOR);
  int y = contentY;
  for (int i = scrollOffset; i < (int)lines.size(); i++) {
    if (y + lines[i].height > contentY + contentH) break;
    if (lines[i].text.length() > 0) {
      tft.setTextColor(lines[i].color, BG_COLOR);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(lines[i].text, lines[i].xOffset, y, lines[i].fontSize);
    }
    y += lines[i].height;
  }
}

void showTXTFile(String path) {
  drainTouch();
  if (!sdReady) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SD-Karte nicht verfuegbar", 120, 155, 2);
    delay(2000);
    drawMenu();
    return;
  }
  String content = readFile(path);
  if (content == "Datei nicht gefunden" || content == "SD-Karte nicht verfuegbar") {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Datei nicht gefunden!", 120, 155, 2);
    delay(2000);
    showFileSelection();
    return;
  }

  // Dateiname fuer Header
  String fileName = path;
  fileName.replace("/", "");
  if (fileName.endsWith(".txt") || fileName.endsWith(".TXT"))
    fileName = fileName.substring(0, fileName.length() - 4);

  // ---- Alle Zeilen vorberechnen ----
  std::vector<RenderedLine> lines;
  String rawLine = "";
  for (int i = 0; i <= (int)content.length(); i++) {
    char c = (i < (int)content.length()) ? content[i] : '\n';
    if (c == '\n') {
      parseTXTLine(rawLine, lines);
      rawLine = "";
    } else {
      rawLine += c;
    }
  }

  // Layout-Konstanten
  const int HEADER_H = 24;  // Kopfzeile
  const int FOOTER_H = 46;  // Fusszeile mit Buttons
  const int CONTENT_Y = HEADER_H;
  const int CONTENT_H = SCREEN_H - HEADER_H - FOOTER_H;  // 250px

  // Berechne wie viele Zeilen auf eine Seite passen
  // (fuer Scroll-Step: immer eine Displayhoehe weiter)
  // Scroll-Step = Anzahl Zeilen, die in CONTENT_H passen
  auto linesPerPage = [&](int from) -> int {
    int h = 0, count = 0;
    for (int i = from; i < (int)lines.size(); i++) {
      h += lines[i].height;
      if (h > CONTENT_H) break;
      count++;
    }
    return max(count, 1);
  };

  int scrollOffset = 0;
  int totalLines = lines.size();

  auto drawUI = [&]() {
    tft.fillScreen(BG_COLOR);
    // Header
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(fileName, 120, HEADER_H / 2, 2);
    tft.drawFastHLine(0, HEADER_H, SCREEN_W, BORDER_COLOR);

    // Footer-Trennlinie
    tft.drawFastHLine(0, SCREEN_H - FOOTER_H, SCREEN_W, BORDER_COLOR);

    // Scroll-Buttons (gross, gut tippbar)
    // [ << ZURUECK ]  [ /\ ]  [ \/ ]
    // Zurueck: 0..119, Hoch: 120..179, Runter: 180..239
    tft.fillRoundRect(2, SCREEN_H - FOOTER_H + 6, 114, 36, 6, TFT_RED);
    tft.fillRoundRect(120, SCREEN_H - FOOTER_H + 6, 56, 36, 6, TFT_DARKGREY);
    tft.fillRoundRect(180, SCREEN_H - FOOTER_H + 6, 56, 36, 6, TFT_DARKGREY);

    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("< Zurueck", 59, SCREEN_H - FOOTER_H + 24, 2);

    // Pfeil rauf
    tft.fillTriangle(148, SCREEN_H - FOOTER_H + 16,
                     138, SCREEN_H - FOOTER_H + 32,
                     158, SCREEN_H - FOOTER_H + 32,
                     scrollOffset > 0 ? TFT_WHITE : TFT_DARKGREY);

    // Pfeil runter
    tft.fillTriangle(208, SCREEN_H - FOOTER_H + 32,
                     198, SCREEN_H - FOOTER_H + 16,
                     218, SCREEN_H - FOOTER_H + 16,
                     scrollOffset + linesPerPage(scrollOffset) < totalLines ? TFT_WHITE : TFT_DARKGREY);

    // Seitenanzeige
    tft.setTextColor(TFT_LIGHTGREY, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(scrollOffset + 1) + "/" + String(totalLines),
                   120, SCREEN_H - FOOTER_H + 2, 1);

    // Inhalt
    renderPage(lines, scrollOffset, CONTENT_Y, CONTENT_H);
  };

  drawUI();

  while (true) {
    int tx, ty;
    if (!getTouch(tx, ty)) {
      delay(10);
      continue;
    }

    int footerY = SCREEN_H - FOOTER_H;

    if (ty >= footerY) {
      if (tx < 120) {
        // Zurueck
        drainTouch();
        showFileSelection();
        return;
      } else if (tx < 180) {
        // Scroll hoch
        if (scrollOffset > 0) {
          int step = linesPerPage(scrollOffset);
          scrollOffset = max(0, scrollOffset - step);
          drawUI();
        }
      } else {
        // Scroll runter
        int step = linesPerPage(scrollOffset);
        if (scrollOffset + step < totalLines) {
          scrollOffset += step;
          drawUI();
        }
      }
      delay(200);
    }
  }
}

void showFileSelection() {
  drainTouch();
  if (!sdReady) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SD-Karte nicht verfuegbar!", 120, 155, 2);
    delay(2000);
    drawMenu();
    return;
  }
  String fileNames[20];
  int fileCount = listMarkdownFiles(fileNames, 20);
  if (fileCount == 0) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_YELLOW, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Keine .txt Dateien", 120, 140, 2);
    tft.drawString("auf SD-Karte gefunden!", 120, 170, 2);
    delay(2000);
    drawMenu();
    return;
  }
  tft.fillScreen(BG_COLOR);
  tft.fillRect(0, 0, SCREEN_W, 22, HEADER_COLOR);
  tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("=== DATEIEN ===", 120, 11, 2);
  int maxDisplay = min(fileCount, 6);
  for (int i = 0; i < maxDisplay; i++) {
    String displayName = fileNames[i];
    displayName.replace("/", "");
    if (displayName.endsWith(".txt") || displayName.endsWith(".TXT"))
      displayName = displayName.substring(0, displayName.length() - 4);
    int yPos = 32 + (i * 40);
    drawButton(20, yPos, 200, 30, TFT_BLUE, displayName);
  }
  drawButton(20, 290, 200, 25, TFT_RED, "Zurueck");
  while (true) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      if (isButtonPressed(tx, ty, 20, 290, 200, 25)) {
        drainTouch();
        drawMenu();
        return;
      }
      for (int i = 0; i < maxDisplay; i++) {
        int yPos = 32 + (i * 40);
        if (isButtonPressed(tx, ty, 20, yPos, 200, 30)) {
          drainTouch();
          showTXTFile(fileNames[i]);
          return;
        }
      }
      delay(150);
    }
    delay(10);
  }
}

// ================= WIFI HELPER =================

void ensureWiFi() {
  if (!useWiFiTime) return;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
      delay(500);
      tries++;
    }
  }
}

// ================= VIRTUELLE TASTATUR =================

void getKeyboardLayout(int mode, const char* rows[3][12], int counts[3]) {
  static const char* low0[] = { "q", "w", "e", "r", "t", "z", "u", "i", "o", "p" };
  static const char* low1[] = { "a", "s", "d", "f", "g", "h", "j", "k", "l" };
  static const char* low2[] = { "y", "x", "c", "v", "b", "n", "m" };
  static const char* up0[] = { "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P" };
  static const char* up1[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
  static const char* up2[] = { "Y", "X", "C", "V", "B", "N", "M" ,"\n"};
  static const char* num0[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
  static const char* num1[] = { "-", "_", ".", ",", "!", "?", ";", ":" };
  static const char* num2[] = { "+", "*", "\"", "'", "/", "(", ")" };
  const char** r0;
  const char** r1;
  const char** r2;
  int c0, c1, c2;
  switch (mode) {
    case 1:
      r0 = up0;
      c0 = 10;
      r1 = up1;
      c1 = 9;
      r2 = up2;
      c2 = 7;
      break;
    case 2:
      r0 = num0;
      c0 = 10;
      r1 = num1;
      c1 = 8;
      r2 = num2;
      c2 = 7;
      break;
    default:
      r0 = low0;
      c0 = 10;
      r1 = low1;
      c1 = 9;
      r2 = low2;
      c2 = 7;
      break;
  }
  for (int i = 0; i < c0; i++) rows[0][i] = r0[i];
  for (int i = 0; i < c1; i++) rows[1][i] = r1[i];
  for (int i = 0; i < c2; i++) rows[2][i] = r2[i];
  counts[0] = c0;
  counts[1] = c1;
  counts[2] = c2;
}

void drawKeyboard(int kbMode, int yOffset) {
  const char* rows[3][12];
  int counts[3];
  getKeyboardLayout(kbMode, rows, counts);
  tft.fillRect(0, yOffset, 240, 320 - yOffset, darkMode ? TFT_BLACK : 0xC618);
  int ky2 = yOffset;
  for (int r = 0; r < 3; r++) {
    int len = counts[r];
    int kw = 240 / max(len, 10);
    int xo = (r == 1) ? kw / 2 : 0;
    for (int i = 0; i < len; i++) {
      int kx2 = xo + i * kw;
      tft.fillRoundRect(kx2, ky2, kw - 2, 28, 3, PANEL_COLOR);
      tft.setTextColor(TEXT_COLOR, PANEL_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(rows[r][i], kx2 + kw / 2, ky2 + 14, 1);
    }
    ky2 += 30;
  }
  tft.fillRoundRect(0, ky2, 50, 28, 3, kbMode == 2 ? TFT_BLUE : PANEL_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawCentreString(kbMode == 2 ? "ABC" : "123", 25, ky2 + 14, 1);
  tft.fillRoundRect(54, ky2, 40, 28, 3, kbMode == 1 ? TFT_BLUE : PANEL_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawCentreString("^", 74, ky2 + 14, 1);
  tft.fillRoundRect(98, ky2, 80, 28, 3, PANEL_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawCentreString("SPACE", 138, ky2 + 14, 1);
  tft.fillRoundRect(182, ky2, 28, 28, 3, PANEL_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawCentreString("<", 196, ky2 + 14, 1);
  tft.fillRoundRect(214, ky2, 26, 28, 3, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("OK", 227, ky2 + 14, 1);
}

int handleKeyboardTouch(int tx, int ty, String& text, int& kbMode, int yOffset) {
  if (ty < yOffset) return 0;
  const char* rows[3][12];
  int counts[3];
  getKeyboardLayout(kbMode, rows, counts);
  int ky2 = yOffset;
  for (int r = 0; r < 3; r++) {
    int len = counts[r];
    int kw = 240 / max(len, 10);
    int xo = (r == 1) ? kw / 2 : 0;
    if (ty >= ky2 && ty < ky2 + 28) {
      int ki = (tx - xo) / kw;
      if (ki >= 0 && ki < len) {
        text += rows[r][ki];
        if (kbMode == 1) kbMode = 0;
        return 1;
      }
      return 0;
    }
    ky2 += 30;
  }
  if (ty >= ky2 && ty < ky2 + 28) {
    if (tx < 50) {
      kbMode = (kbMode == 2) ? 0 : 2;
      return 1;
    } else if (tx >= 54 && tx < 94) {
      kbMode = (kbMode == 1) ? 0 : 1;
      return 1;
    } else if (tx >= 98 && tx < 178) {
      text += " ";
      return 1;
    } else if (tx >= 182 && tx < 210) {
      if (text.length() > 0) text.remove(text.length() - 1);
      return 1;
    } else if (tx >= 214) {
      return 2;
    }
  }
  return 0;
}

String virtualKeyboardInput(String title, String initial, int maxLen) {
  drainTouch();
  String text = initial;
  int kbMode = 0;
  while (true) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, 240, 24, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(4, 8);
    tft.print(title);
    tft.fillRoundRect(180, 2, 56, 20, 3, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FERTIG", 208, 12, 1);
    tft.drawRect(8, 32, 224, 30, BORDER_COLOR);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(14, 41);
    tft.print(text);
    if ((millis() / 500) % 2 == 0) {
      int cw = tft.textWidth(text, 1) + 2;
      tft.fillRect(14 + cw, 38, 2, 16, TEXT_COLOR);
    }
    drawKeyboard(kbMode, 165);
    int kx, ky;
    while (!getTouch(kx, ky)) delay(10);
    if (ky < 24 && kx > 175) {
      drainTouch();
      return text;
    }
    int res = handleKeyboardTouch(kx, ky, text, kbMode, 165);
    if (res == 2) {
      drainTouch();
      return text;
    }
    if (maxLen > 0 && (int)text.length() > maxLen) text = text.substring(0, maxLen);
    delay(150);
  }
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(2);
  initSD();
  loadSettings();
  applyTheme();
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);
  if (useWiFiTime) {
    connectWiFi();
    initTime();
  } else {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi & Zeit deaktiviert", 120, 155, 2);
    tft.drawString("Starte ohne Netzwerk...", 120, 185, 2);
    delay(1500);
  }
  drawMenu();
}

// ================= LOOP =================

void loop() {
  drawClock();
  int tx, ty;
  if (getTouch(tx, ty)) {
    for (int i = 0; i < MENU_APPS_COUNT; i++) {
      const AppButton& b = MENU_APPS[i];
      if (isButtonPressed(tx, ty, b.x, b.y, b.w, b.h)) {
        Serial.println(b.label);
        drainTouch();
        if (strcmp(b.label, "Calc") == 0) calculator();
        else if (strcmp(b.label, "Draw") == 0) drawing();
        else if (strcmp(b.label, "Notes") == 0) notesApp();
        else if (strcmp(b.label, "Chat") == 0) chatApp();
        else if (strcmp(b.label, "Read") == 0) showFileSelection();
        else if (strcmp(b.label, "Settings") == 0) settingsApp();
        else if (strcmp(b.label, "Stundenplan") == 0) stundenplanApp();
        else if (strcmp(b.label, "Stoppuhr") == 0) stopwatchApp();  // NEU
        break;
      }
    }
  }
}

// ================= CALCULATOR =================

void calculator() {
  String display = "0";
  String input = "";
  float result = 0;
  char lastOperator = ' ';
  bool newNumber = true;
  bool error = false;

  struct CalcButton {
    String label;
    int x, y, w, h;
    uint16_t color;
  };

  // BUGFIX: "Zurueck"-Button liegt jetzt vollstaendig auf dem Bildschirm
  // (y=305, h=22 -> y+h=327 war ausserhalb SCREEN_H=320 → jetzt h=22 at y=296)
  CalcButton buttons[] = {
    { "C", 5, 70, 50, 35, TFT_RED },
    { "+/-", 60, 70, 50, 35, TFT_DARKGREY },
    { "Wr.", 115, 70, 50, 35, TFT_DARKGREY },
    { "/", 170, 70, 55, 35, TFT_ORANGE },
    { "7", 5, 110, 50, 35, TFT_DARKGREY },
    { "8", 60, 110, 50, 35, TFT_DARKGREY },
    { "9", 115, 110, 50, 35, TFT_DARKGREY },
    { "*", 170, 110, 55, 35, TFT_ORANGE },
    { "4", 5, 150, 50, 35, TFT_DARKGREY },
    { "5", 60, 150, 50, 35, TFT_DARKGREY },
    { "6", 115, 150, 50, 35, TFT_DARKGREY },
    { "-", 170, 150, 55, 35, TFT_ORANGE },
    { "1", 5, 190, 50, 35, TFT_DARKGREY },
    { "2", 60, 190, 50, 35, TFT_DARKGREY },
    { "3", 115, 190, 50, 35, TFT_DARKGREY },
    { "+", 170, 190, 55, 35, TFT_ORANGE },
    { "x2", 5, 230, 50, 30, TFT_DARKGREY },
    { "0", 60, 230, 50, 30, TFT_DARKGREY },
    { ".", 115, 230, 50, 30, TFT_DARKGREY },
    { "=", 170, 230, 55, 30, TFT_GREEN },
    { "^", 5, 264, 50, 28, TFT_PURPLE },
    { "1/x", 60, 264, 100, 28, TFT_DARKGREY },
    { "<--", 165, 264, 65, 28, TFT_DARKGREY },
    // BUGFIX: Zurueck ist jetzt y=295, h=22 → y+h=317 < 320 (auf dem Screen)
    { "Zurueck", 5, 295, 230, 22, TFT_RED },
  };
  int numButtons = sizeof(buttons) / sizeof(buttons[0]);

  auto drawDisplay = [&]() {
    tft.fillRect(5, 5, 230, 60, TFT_DARKGREY);
    tft.drawRect(5, 5, 230, 60, TFT_WHITE);
    tft.setTextDatum(TR_DATUM);
    tft.setTextSize(1);
    if (error) {
      tft.setTextColor(TFT_RED, TFT_DARKGREY);
      tft.drawString("ERROR", 230, 10, 4);
    } else {
      String displayText = display;
      if (displayText.length() > 15) displayText = displayText.substring(0, 15);
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.drawString(displayText, 230, 10, 4);
      if (input.length() > 0) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
        tft.drawString(input, 230, 45, 2);
      }
    }
  };

  auto drawButtons = [&]() {
    for (int i = 0; i < numButtons; i++) {
      tft.fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, buttons[i].color);
      tft.drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, TFT_WHITE);
      tft.setTextColor(TFT_WHITE, buttons[i].color);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(buttons[i].label,
                     buttons[i].x + buttons[i].w / 2,
                     buttons[i].y + buttons[i].h / 2, 2);
    }
  };

  auto applyPendingOp = [&](float currentNum) -> bool {
    switch (lastOperator) {
      case '+': result += currentNum; return true;
      case '-': result -= currentNum; return true;
      case '*': result *= currentNum; return true;
      case '/':
        if (currentNum != 0) {
          result /= currentNum;
          return true;
        }
        error = true;
        return false;
      case '^': result = pow(result, currentNum); return true;
    }
    return true;
  };

  auto handleInput = [&](String value) {
    if (error) {
      error = false;
      display = "0";
      input = "";
      result = 0;
      lastOperator = ' ';
      newNumber = true;
      if (value == "C") return;
    }
    if (value == "C") {
      display = "0";
      input = "";
      result = 0;
      lastOperator = ' ';
      newNumber = true;
      error = false;
    } else if (value == "+/-") {
      if (display != "0") {
        if (display.startsWith("-")) display = display.substring(1);
        else display = "-" + display;
      }
    } else if (value == "Wr.") {
      float num = display.toFloat();
      if (num >= 0) {
        display = String(sqrt(num), 6);
        result = display.toFloat();
        newNumber = true;
      } else error = true;
    } else if (value == "x2") {
      float num = display.toFloat();
      display = String(num * num, 6);
      result = display.toFloat();
      newNumber = true;
    } else if (value == "1/x") {
      float num = display.toFloat();
      if (num != 0) {
        display = String(1.0 / num, 6);
        result = display.toFloat();
        newNumber = true;
      } else error = true;
    } else if (value == "=") {
      float currentNum = display.toFloat();
      if (lastOperator != ' ') {
        if (applyPendingOp(currentNum)) {
          display = String(result, 4);
          input = "";
          lastOperator = ' ';
          newNumber = true;
        }
      }
    } else if (value == "+" || value == "-" || value == "*" || value == "/" || value == "^") {
      if (lastOperator != ' ' && !newNumber) {
        float currentNum = display.toFloat();
        if (!applyPendingOp(currentNum)) return;
        display = String(result, 4);
      } else {
        result = display.toFloat();
      }
      lastOperator = value[0];
      input = display + " " + value;
      newNumber = true;
    } else if (value == "<--") {
      if (!newNumber && display.length() > 0) {
        display.remove(display.length() - 1);
        if (display.length() == 0 || display == "-") display = "0";
      }
    } else {
      if (newNumber) {
        if (value == ".") display = "0.";
        else display = value;
        newNumber = false;
      } else {
        if (value == ".") {
          if (display.indexOf('.') == -1) display += value;
        } else {
          if (display.length() < 15) {
            if (display == "0" && value != ".") display = value;
            else display += value;
          }
        }
      }
    }
  };

  drainTouch();
  while (true) {
    tft.fillScreen(TFT_BLACK);
    drawDisplay();
    drawButtons();
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    bool buttonPressed = false;
    for (int i = 0; i < numButtons; i++) {
      if (isButtonPressed(tx, ty, buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h)) {
        if (buttons[i].label == "Zurueck") {
          drainTouch();
          drawMenu();
          return;
        }
        handleInput(buttons[i].label);
        buttonPressed = true;
        break;
      }
    }
    if (buttonPressed) {
      drawDisplay();
      delay(180);
    }
  }
}

// ================= DRAWING =================

void drawing() {
  drainTouch();
  tft.fillScreen(TFT_WHITE);
  const uint16_t palette[8] = {
    TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE,
    TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE
  };
  uint16_t color = TFT_BLACK;
  uint8_t penSize = 3;
  bool eraserOn = false;
  int16_t lastX = -1, lastY = -1;
  bool isDown = false;

  auto drawHeader = [&]() {
    tft.fillRect(0, 0, SCREEN_W, 28, TFT_WHITE);
    tft.fillRoundRect(2, 2, 50, 24, 4, TFT_RED);
    tft.drawRoundRect(2, 2, 50, 24, 4, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ZURUECK", 27, 14, 1);
    tft.drawFastHLine(0, 28, SCREEN_W, TFT_BLACK);
  };

  auto drawToolbar = [&]() {
    int cw = SCREEN_W / 8;
    for (int i = 0; i < 8; i++) {
      int x = i * cw;
      tft.fillRect(x, 270, cw - 1, 24, palette[i]);
      if (palette[i] == color) {
        tft.drawRect(x + 1, 271, cw - 3, 22, eraserOn ? TFT_RED : TFT_WHITE);
        tft.drawRect(x + 2, 272, cw - 5, 20, eraserOn ? TFT_RED : TFT_WHITE);
      }
    }
    int bw = SCREEN_W / 5, by = 294, bh = 26;
    const char* labels[5] = { "RAD", "NEU", "RAUS", "SZ+", "SZ-" };
    for (int i = 0; i < 5; i++) {
      int bx = i * bw;
      tft.fillRect(bx, by, bw - 1, bh, (i == 0 && eraserOn) ? TFT_RED : 0x0841);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(bx + 3, by + (bh - 8) / 2);
      tft.print(labels[i]);
      if (i == 3) tft.fillCircle(bx + bw - 10, by + bh / 2, penSize, eraserOn ? TFT_WHITE : color);
    }
    tft.drawFastHLine(0, 270, SCREEN_W, TFT_WHITE);
    tft.drawFastHLine(0, 294, SCREEN_W, TFT_DARKGREY);
  };

  auto clearCanvas = [&]() {
    tft.fillRect(0, 30, SCREEN_W, 240, TFT_WHITE);
  };

  drawHeader();
  drawToolbar();
  clearCanvas();

  while (true) {
    int x, y;
    if (!getTouch(x, y)) {
      if (isDown) {
        isDown = false;
        lastX = lastY = -1;
      }
      delay(10);
      continue;
    }
    if (y < 28) {
      if (isButtonPressed(x, y, 2, 2, 50, 24)) {
        drainTouch();
        drawMenu();
        return;
      }
      delay(10);
      continue;
    }
    if (y >= 270 && y < 294) {
      int idx = x / (SCREEN_W / 8);
      if (idx >= 0 && idx < 8) {
        color = palette[idx];
        eraserOn = false;
        drawToolbar();
      }
      isDown = false;
      lastX = lastY = -1;
      delay(150);
      continue;
    }
    if (y >= 294) {
      int ti = x / (SCREEN_W / 5);
      switch (ti) {
        case 0:
          eraserOn = !eraserOn;
          drawToolbar();
          break;
        case 1: clearCanvas(); break;
        case 2:
          drainTouch();
          drawMenu();
          return;
        case 3:
          if (penSize < 20) penSize += 2;
          drawToolbar();
          break;
        case 4:
          if (penSize > 1) penSize -= 2;
          drawToolbar();
          break;
      }
      isDown = false;
      lastX = lastY = -1;
      delay(150);
      continue;
    }
    uint16_t dc = eraserOn ? TFT_WHITE : color;
    if (!isDown || lastX < 0) {
      tft.fillCircle(x, y, penSize, dc);
    } else {
      int dx = x - lastX, dy = y - lastY;
      int steps = max(abs(dx), abs(dy));
      if (steps == 0) steps = 1;
      for (int i = 0; i <= steps; i++)
        tft.fillCircle(lastX + (dx * i) / steps, lastY + (dy * i) / steps, penSize, dc);
    }
    lastX = x;
    lastY = y;
    isDown = true;
    delay(10);
  }
}

// ================= NOTES APP =================

void notesApp() {
  drainTouch();
  struct NoteFile {
    String name;
    String path;
  };

  auto refreshNotes = [](std::vector<NoteFile>& list) {
    list.clear();
    if (!sdReady) return;
    if (!SD.exists("/notes")) SD.mkdir("/notes");
    File dir = SD.open("/notes");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        NoteFile n;
        n.name = String(f.name());
        int dot = n.name.lastIndexOf('.');
        if (dot > 0) n.name = n.name.substring(0, dot);
        n.path = "/notes/" + String(f.name());
        list.push_back(n);
      }
      f.close();
      f = dir.openNextFile();
    }
    dir.close();
  };

  auto drawNoteList = [&](std::vector<NoteFile>& list, int sel, int offset) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, 240, 30, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("NOTIZEN", 120, 8, 2);
    tft.fillRoundRect(5, 3, 40, 24, 4, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("<", 25, 15, 1);
    tft.fillRoundRect(195, 3, 40, 24, 4, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.drawString("+", 215, 15, 1);
    if (list.empty()) {
      tft.setTextColor(TFT_LIGHTGREY, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Keine Notizen", 120, 150, 2);
      tft.drawString("Tippe + zum Erstellen", 120, 175, 1);
      return;
    }
    int y = 38, shown = 0;
    for (int i = offset; i < (int)list.size() && shown < 7; i++) {
      tft.fillRoundRect(5, y, 230, 32, 4, sel == i ? TFT_BLUE : PANEL_COLOR);
      tft.setTextColor(TEXT_COLOR);
      tft.setCursor(12, y + 8);
      tft.print(list[i].name);
      tft.fillRoundRect(190, y + 4, 40, 24, 3, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("X", 210, y + 16, 1);
      y += 36;
      shown++;
    }
  };

  auto editNote = [&](NoteFile& note) {
    String text = "";
    File rf = SD.open(note.path, FILE_READ);
    if (rf) {
      while (rf.available()) text += (char)rf.read();
      rf.close();
    }
    int kbMode = 0;
    while (true) {
      tft.fillScreen(BG_COLOR);
      tft.fillRect(0, 0, 240, 24, HEADER_COLOR);
      tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
      tft.setTextSize(1);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(4, 6);
      tft.print(note.name);
      tft.fillRoundRect(180, 2, 55, 20, 3, TFT_GREEN);
      tft.setTextColor(TFT_WHITE, TFT_GREEN);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("SPEICHERN", 207, 12, 1);
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.setTextDatum(TL_DATUM);
      int lineY = 28;
      String disp = text;
      if (disp.length() > 600) disp = disp.substring(disp.length() - 600);
      int idx2 = 0;
      while (idx2 < (int)disp.length() && lineY < 160) {
        String line;
        while (idx2 < (int)disp.length() && disp[idx2] != '\n' && line.length() < 34) line += disp[idx2++];
        if (idx2 < (int)disp.length() && disp[idx2] == '\n') idx2++;
        tft.setCursor(4, lineY);
        tft.print(line);
        lineY += 12;
      }
      if ((millis() / 500) % 2 == 0 && lineY < 160) {
        int cursorCol = text.length() % 34;
        tft.fillRect(4 + cursorCol * 6, lineY - 12, 6, 10, TEXT_COLOR);
      }
      drawKeyboard(kbMode, 165);
      int kx, ky;
      while (!getTouch(kx, ky)) delay(10);
      if (ky < 24) {
        if (kx > 175) {
          File sf = SD.open(note.path, FILE_WRITE);
          if (sf) {
            sf.print(text);
            sf.close();
          }
        }
        drainTouch();
        return;
      }
      handleKeyboardTouch(kx, ky, text, kbMode, 165);
      delay(130);
    }
  };

  std::vector<NoteFile> notes;
  int selected = -1, scrollOff = 0;
  refreshNotes(notes);
  drawNoteList(notes, selected, scrollOff);

  while (true) {
    int tx, ty;
    if (!getTouch(tx, ty)) {
      delay(10);
      continue;
    }
    if (ty < 30) {
      if (tx < 50) {
        drainTouch();
        drawMenu();
        return;
      }
      if (tx > 195) {
        String newName = virtualKeyboardInput("Neuer Notizname:", "", 24);
        newName.trim();
        if (newName.length() > 0) {
          String path = "/notes/" + newName + ".txt";
          File nf = SD.open(path, FILE_WRITE);
          if (nf) nf.close();
        }
        refreshNotes(notes);
        drawNoteList(notes, selected, scrollOff);
      }
      delay(150);
      continue;
    }
    int idx = scrollOff + (ty - 38) / 36;
    if (idx >= 0 && idx < (int)notes.size() && ty < 38 + 7 * 36) {
      if (tx > 190) {
        SD.remove(notes[idx].path);
        refreshNotes(notes);
        if (scrollOff > 0 && scrollOff >= (int)notes.size()) scrollOff = max(0, (int)notes.size() - 1);
        drawNoteList(notes, selected, scrollOff);
      } else {
        selected = idx;
        drainTouch();
        editNote(notes[selected]);
        refreshNotes(notes);
        drawNoteList(notes, selected, scrollOff);
      }
      delay(150);
    }
  }
}

// ================= CHAT APP =================

// ================= CHAT APP =================

void chatApp() {
  drainTouch();
  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Kein WLAN", 120, 100, 2);
    tft.drawString("Tippen zum Zurueck", 120, 140, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    drainTouch(); drawMenu(); return;
  }
  String pubkey = "", privkey = "", username = "";
  HTTPClient http; http.setTimeout(5000);
  String apiBase = "http://149.102.157.124:3001";
  if (SPIFFS.begin(true)) {
    if (SPIFFS.exists("/chat.json")) {
      File f = SPIFFS.open("/chat.json", FILE_READ);
      if (f) {
        String s = f.readString(); f.close();
        int u = s.indexOf("\"username\":\"");
        if (u > 0) { int e = s.indexOf("\"", u+12); if (e > u+12) username = s.substring(u+12, e); }
        int p = s.indexOf("\"pubkey\":\"");
        if (p > 0) { int e = s.indexOf("\"", p+10); if (e > p+10) pubkey = s.substring(p+10, e); }
        int r = s.indexOf("\"privkey\":\"");
        if (r > 0) { int e = s.indexOf("\"", r+11); if (e > r+11) privkey = s.substring(r+11, e); }
      }
    }
  }
  int state = (pubkey == "") ? -1 : 0;
  String dmTarget = "", dmPubkey = "", currentGroupId = "", currentGroupName = "";
  bool isDM = false;
  while (true) {
    if (state == -1) {
      tft.fillScreen(BG_COLOR);
      tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
      tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("CHAT LOGIN", 120, 14, 2);
      drawButton(30, 60, 180, 40, TFT_GREEN, "REGISTRIEREN");
      drawButton(30, 120, 180, 40, TFT_BLUE, "LOGIN");
      drawButton(30, 200, 180, 30, TFT_RED, "Zurueck");
      int tx, ty;
      while (!getTouch(tx, ty)) delay(10);
      drainTouch();
      if (isButtonPressed(tx, ty, 30, 60, 180, 40)) {
        username = virtualKeyboardInput("Neuer Benutzername:", "", 30);
        username.trim(); if (username == "") continue;
        tft.fillScreen(BG_COLOR);
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Registriere...", 120, 100, 2);
        http.begin(apiBase + "/api/register");
        http.addHeader("Content-Type", "application/json");
        int code = http.POST("{\"username\":\"" + username + "\",\"displayName\":\"" + username + "\"}");
        if (code == 200 || code == 201) {
          String resp = http.getString();
          int pk = resp.indexOf("\"pubkey\":\"");
          if (pk > 0) pubkey = resp.substring(pk+10, resp.indexOf("\"", pk+10));
          int pr = resp.indexOf("\"privkey\":\"");
          if (pr > 0) privkey = resp.substring(pr+11, resp.indexOf("\"", pr+11));
        }
        http.end();
        if (pubkey != "") {
          File f = SPIFFS.open("/chat.json", FILE_WRITE);
          if (f) { f.print("{\"username\":\"" + username + "\",\"pubkey\":\"" + pubkey + "\",\"privkey\":\"" + privkey + "\"}"); f.close(); }
          state = 0;
        } else {
          tft.fillScreen(BG_COLOR);
          tft.setTextColor(TFT_RED, BG_COLOR);
          tft.drawString("Fehler bei Registrierung", 120, 100, 2);
          tft.drawString("Tippen zum Zurueck", 120, 140, 1);
          while (!getTouch(tx, ty)) delay(10);
          drainTouch(); drawMenu(); return;
        }
      } else if (isButtonPressed(tx, ty, 30, 120, 180, 40)) {
        username = virtualKeyboardInput("Dein Benutzername:", "", 30);
        username.trim(); if (username == "") continue;
        tft.fillScreen(BG_COLOR);
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.drawString("Logge ein...", 120, 100, 2);
        http.begin(apiBase + "/api/login");
        http.addHeader("Content-Type", "application/json");
        int code = http.POST("{\"username\":\"" + username + "\"}");
        if (code == 200) {
          String resp = http.getString();
          int pk = resp.indexOf("\"pubkey\":\"");
          if (pk > 0) pubkey = resp.substring(pk+10, resp.indexOf("\"", pk+10));
          int pr = resp.indexOf("\"privkey\":\"");
          if (pr > 0) privkey = resp.substring(pr+11, resp.indexOf("\"", pr+11));
        }
        http.end();
        if (pubkey != "") {
          File f = SPIFFS.open("/chat.json", FILE_WRITE);
          if (f) { f.print("{\"username\":\"" + username + "\",\"pubkey\":\"" + pubkey + "\",\"privkey\":\"" + privkey + "\"}"); f.close(); }
          state = 0;
        } else {
          tft.fillScreen(BG_COLOR);
          tft.setTextColor(TFT_RED, BG_COLOR);
          tft.drawString("Benutzer nicht gefunden", 120, 100, 2);
          tft.drawString("Tippen zum Zurueck", 120, 140, 1);
          while (!getTouch(tx, ty)) delay(10);
          drainTouch(); drawMenu(); return;
        }
      } else if (isButtonPressed(tx, ty, 30, 200, 180, 30)) {
        drainTouch(); drawMenu(); return;
      }
      delay(150);
    } else if (state == 0) {
      tft.fillScreen(BG_COLOR);
      tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
      tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("CHAT: " + username, 120, 14, 2);
      drawButton(20, 45, 200, 35, TFT_CYAN, "Private Nachricht");
      drawButton(20, 90, 200, 35, TFT_GREEN, "Gruppenchat");
      drawButton(20, 190, 90, 30, TFT_RED, "Logout");
      drawButton(130, 190, 100, 30, TFT_DARKGREY, "Zurueck");
      int tx, ty;
      while (!getTouch(tx, ty)) delay(10);
      drainTouch();
      if (isButtonPressed(tx, ty, 20, 45, 200, 35)) state = 1;
      else if (isButtonPressed(tx, ty, 20, 90, 200, 35)) state = 2;
      else if (isButtonPressed(tx, ty, 20, 190, 90, 30)) { pubkey = ""; privkey = ""; username = ""; SPIFFS.remove("/chat.json"); state = -1; }
      else if (isButtonPressed(tx, ty, 130, 190, 100, 30)) { drainTouch(); drawMenu(); return; }
      delay(150);
    } else if (state == 1) {
      std::vector<String> userNames;
      std::vector<String> userPubkeys;
      tft.fillScreen(BG_COLOR);
      tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
      tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BENUTZER", 120, 14, 2);
      tft.fillRoundRect(2, 2, 45, 24, 3, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString("<", 24, 14, 2);
      tft.fillRoundRect(190, 2, 45, 24, 3, TFT_CYAN);
      tft.setTextColor(TFT_WHITE, TFT_CYAN);
      tft.drawString("Suche", 212, 14, 1);
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Lade Benutzer...", 120, 120, 2);
      http.begin(apiBase + "/api/users");
      if (http.GET() == 200) {
        String resp = http.getString(); int pos = 0;
        while (true) {
          int un = resp.indexOf("\"username\":\"", pos);
          if (un < 0) break;
          String u = resp.substring(un+11); u = u.substring(0, u.indexOf("\""));
          int pk = resp.indexOf("\"pubkey\":\"", un);
          if (pk > 0) {
            String pkv = resp.substring(pk+10); pkv = pkv.substring(0, pkv.indexOf("\""));
            if (u != username) { userNames.push_back(u); userPubkeys.push_back(pkv); }
          } else {
            if (u != username) userNames.push_back(u);
          }
          pos = un + 1;
        }
      }
      http.end();
      int scroll = 0;
      while (true) {
        tft.fillRect(0, 30, SCREEN_W, SCREEN_H - 60, BG_COLOR);
        if (userNames.empty()) {
          tft.setTextColor(TFT_RED, BG_COLOR);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("Keine Benutzer gefunden", 120, 130, 2);
          tft.drawString("WLAN/Server prüfen", 120, 160, 1);
        }
        int n = min((int)userNames.size() - scroll, 6);
        for (int i = 0; i < n; i++) {
          int y = 34 + i * 36;
          tft.fillRoundRect(4, y, 232, 32, 4, PANEL_COLOR);
          tft.setTextColor(TEXT_COLOR, PANEL_COLOR);
          tft.setTextDatum(TL_DATUM);
          tft.setCursor(12, y + 8);
          tft.print(userNames[scroll + i]);
        }
        int by = SCREEN_H - 52;
        tft.fillRect(0, by, SCREEN_W, 50, BG_COLOR);
        tft.drawFastHLine(0, by, SCREEN_W, BORDER_COLOR);
        tft.fillRoundRect(10, by + 6, 90, 36, 4, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("HOCH", 55, by + 24, 2);
        tft.fillRoundRect(120, by + 6, 90, 36, 4, TFT_DARKGREY);
        tft.drawString("RUNTER", 165, by + 24, 2);
        int tx, ty;
        while (!getTouch(tx, ty)) delay(10);
        if (ty < 28) {
          if (tx < 55) { drainTouch(); state = 0; break; }
          if (tx > 180) {
            String searchName = virtualKeyboardInput("Benutzername (od. Teil):", "", 30);
            searchName.trim(); if (searchName == "") continue;
            searchName.toLowerCase();
            int found = -1;
            for (int i = 0; i < (int)userNames.size(); i++) {
              String un = userNames[i]; un.toLowerCase();
              if (un.indexOf(searchName) >= 0) { found = i; break; }
            }
            if (found >= 0) {
              int i = found;
              dmTarget = userNames[i];
              dmPubkey = (i < (int)userPubkeys.size()) ? userPubkeys[i] : "";
              isDM = true;
              state = 4; break;
            }
            if (state != 4) {
              tft.fillScreen(BG_COLOR);
              tft.setTextColor(TFT_RED, BG_COLOR);
              tft.setTextDatum(MC_DATUM);
              tft.drawString("Benutzer nicht gefunden", 120, 100, 2);
              tft.drawString("Tippen zum Zurueck", 120, 140, 1);
              while (!getTouch(tx, ty)) delay(10);
              drainTouch(); continue;
            }
            break;
          }
          delay(150); continue;
        }
        if (ty >= 34 && ty < 34 + 6 * 36) {
          int idx = scroll + (ty - 34) / 36;
          if (idx < (int)userNames.size()) {
            dmTarget = userNames[idx];
            dmPubkey = (idx < (int)userPubkeys.size()) ? userPubkeys[idx] : "";
            isDM = true;
            state = 4; break;
          }
        }
        if (ty >= by && ty < by + 50) {
          if (tx < 115) { if (scroll > 0) scroll--; }
          else { if (scroll + 6 < (int)userNames.size()) scroll++; }
        }
        delay(130);
      }
    } else if (state == 2) {
      struct GEntry { String id, name; bool isPrivate; };
      std::vector<GEntry> groups;
      tft.fillScreen(BG_COLOR);
      tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
      tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("GRUPPEN", 120, 14, 2);
      tft.fillRoundRect(2, 2, 45, 24, 3, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString("<", 24, 14, 2);
      tft.fillRoundRect(130, 2, 50, 24, 3, TFT_GREEN);
      tft.setTextColor(TFT_WHITE, TFT_GREEN);
      tft.drawString("+Pub", 155, 14, 1);
      tft.fillRoundRect(185, 2, 50, 24, 3, TFT_ORANGE);
      tft.setTextColor(TFT_WHITE, TFT_ORANGE);
      tft.drawString("+Priv", 210, 14, 1);
      http.begin(apiBase + "/api/groups?pubkey=" + pubkey);
      if (http.GET() == 200) {
        String resp = http.getString(); int pos = 0;
        while (true) {
          int si = resp.indexOf("\"id\":\"", pos);
          if (si < 0) break;
          String gid = resp.substring(si+6); gid = gid.substring(0, gid.indexOf("\""));
          int sn = resp.indexOf("\"name\":\"", si);
          if (sn > 0) {
            String gn = resp.substring(sn+8); gn = gn.substring(0, gn.indexOf("\""));
            if (!gn.startsWith("DM:")) {
              GEntry e; e.id = gid; e.name = gn;
              int sp = resp.indexOf("\"pin\":\"", si);
              e.isPrivate = (sp > 0 && sp < resp.indexOf("}", si));
              groups.push_back(e);
            }
          }
          pos = si + 1;
        }
      }
      http.end();
      int scroll = 0;
      while (true) {
        tft.fillRect(0, 30, SCREEN_W, SCREEN_H - 60, BG_COLOR);
        int n = min((int)groups.size() - scroll, 6);
        for (int i = 0; i < n; i++) {
          int y = 34 + i * 36;
          tft.fillRoundRect(4, y, 232, 32, 4, groups[scroll + i].isPrivate ? 0x780F : PANEL_COLOR);
          tft.setTextColor(TEXT_COLOR, groups[scroll + i].isPrivate ? 0x780F : PANEL_COLOR);
          tft.setTextDatum(TL_DATUM);
          tft.setCursor(12, y + 8);
          String gn = groups[scroll + i].name;
          if (groups[scroll + i].isPrivate) gn = "[P] " + gn;
          tft.print(gn);
        }
        int by = SCREEN_H - 52;
        tft.fillRect(0, by, SCREEN_W, 50, BG_COLOR);
        tft.drawFastHLine(0, by, SCREEN_W, BORDER_COLOR);
        tft.fillRoundRect(10, by + 6, 90, 36, 4, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("HOCH", 55, by + 24, 2);
        tft.fillRoundRect(120, by + 6, 110, 36, 4, TFT_ORANGE);
        tft.drawString("BEITRETEN", 175, by + 24, 2);
        int tx, ty;
        while (!getTouch(tx, ty)) delay(10);
        if (ty < 28) {
          if (tx < 55) { drainTouch(); state = 0; break; }
          if (tx > 120) {
            bool isPrivate = (tx < 185);
            String gname = virtualKeyboardInput("Gruppenname:", "", 30);
            gname.trim(); if (gname == "") continue;
            String pin = "";
            if (isPrivate) {
              pin = virtualKeyboardInput("PIN (0=oeffentlich):", "0", 10);
              pin.trim(); if (pin == "") pin = "0";
            }
            String jsonBody = "{\"name\":\"" + gname + "\",\"ownerPubkey\":\"" + pubkey + "\"";
            if (isPrivate && pin != "0") jsonBody += ",\"pin\":\"" + pin + "\"";
            jsonBody += "}";
            http.begin(apiBase + "/api/groups");
            http.addHeader("Content-Type", "application/json");
            int code = http.POST(jsonBody);
            if (code == 200 || code == 201) {
              String resp = http.getString();
              int si = resp.indexOf("\"id\":\"");
              if (si > 0) { currentGroupId = resp.substring(si+6); currentGroupId = currentGroupId.substring(0, currentGroupId.indexOf("\"")); currentGroupName = gname; isDM = false; }
              http.end();
              if (currentGroupId != "") {
                tft.fillScreen(BG_COLOR);
                tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
                tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
                tft.setTextDatum(MC_DATUM);
                tft.drawString("MITGLIEDER EINLADEN", 120, 14, 2);
                std::vector<String> allUsers, allPubkeys;
                http.begin(apiBase + "/api/users");
                if (http.GET() == 200) {
                  String uResp = http.getString(); int pos = 0;
                  while (true) {
                    int un = uResp.indexOf("\"username\":\"", pos);
                    if (un < 0) break;
                    String uname = uResp.substring(un+11); uname = uname.substring(0, uname.indexOf("\""));
                    int upk = uResp.indexOf("\"pubkey\":\"", un);
                    if (upk > 0) { String pkv = uResp.substring(upk+10); pkv = pkv.substring(0, pkv.indexOf("\"")); allUsers.push_back(uname); allPubkeys.push_back(pkv); }
                    pos = un + 1;
                  }
                }
                http.end();
                int uscroll = 0;
                while (true) {
                  tft.fillRect(0, 30, SCREEN_W, SCREEN_H - 60, BG_COLOR);
                  int nu = min((int)allUsers.size() - uscroll, 6);
                  for (int ui = 0; ui < nu; ui++) {
                    int uy = 34 + ui * 36;
                    tft.fillRoundRect(4, uy, 232, 32, 4, PANEL_COLOR);
                    tft.setTextColor(TEXT_COLOR, PANEL_COLOR);
                    tft.setTextDatum(TL_DATUM);
                    tft.setCursor(12, uy + 8);
                    tft.print(allUsers[uscroll + ui]);
                  }
                  int uby = SCREEN_H - 52;
                  tft.fillRect(0, uby, SCREEN_W, 50, BG_COLOR);
                  tft.drawFastHLine(0, uby, SCREEN_W, BORDER_COLOR);
                  tft.fillRoundRect(10, uby + 6, 90, 36, 4, TFT_DARKGREY);
                  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
                  tft.setTextDatum(MC_DATUM);
                  tft.drawString("HOCH", 55, uby + 24, 2);
                  tft.fillRoundRect(120, uby + 6, 110, 36, 4, TFT_GREEN);
                  tft.drawString("FERTIG", 175, uby + 24, 2);
                  int utx, uty;
                  while (!getTouch(utx, uty)) delay(10);
                  if (uty >= 34 && uty < 34 + 6 * 36) {
                    int uidx = uscroll + (uty - 34) / 36;
                    if (uidx < (int)allUsers.size() && uidx < (int)allPubkeys.size() && allPubkeys[uidx] != pubkey) {
                      http.begin(apiBase + "/api/groups/" + currentGroupId + "/invite");
                      http.addHeader("Content-Type", "application/json");
                      http.POST("{\"pubkey\":\"" + allPubkeys[uidx] + "\"}");
                      http.end();
                      tft.fillRoundRect(4, 34 + (uidx - uscroll) * 36, 232, 32, 4, TFT_GREEN);
                      tft.setTextColor(TFT_WHITE, TFT_GREEN);
                      tft.setTextDatum(TL_DATUM);
                      tft.setCursor(12, 34 + (uidx - uscroll) * 36 + 8);
                      tft.print(allUsers[uidx] + " ✓");
                    }
                    delay(200); continue;
                  }
                  if (uty >= uby && uty < uby + 50) {
                    if (utx < 115) { if (uscroll > 0) uscroll--; }
                    else { state = 4; break; }
                    continue;
                  }
                  if (uty < 28) { state = 4; break; }
                  delay(130);
                }
                break;
              }
            }
            http.end();
            if (currentGroupId != "") { state = 4; break; }
          } else { delay(150); continue; }
        }
        if (ty >= by && ty < by + 50) {
          if (tx < 115) { if (scroll > 0) scroll--; continue; }
          if (tx >= 115) {
            String gid = virtualKeyboardInput("Gruppen-ID:", "", 36);
            gid.trim(); if (gid == "") continue;
            String pin2 = virtualKeyboardInput("PIN (0=kein PIN):", "0", 10);
            pin2.trim(); if (pin2 == "") pin2 = "0";
            String joinBody = "{\"pubkey\":\"" + pubkey + "\"";
            if (pin2 != "0") joinBody += ",\"pin\":\"" + pin2 + "\"";
            joinBody += "}";
            http.begin(apiBase + "/api/groups/" + gid + "/join");
            http.addHeader("Content-Type", "application/json");
            int jcode = http.POST(joinBody);
            if (jcode == 200 || jcode == 201) {
              String jresp = http.getString();
              int ji = jresp.indexOf("\"id\":\"");
              if (ji > 0) { currentGroupId = jresp.substring(ji+6); currentGroupId = currentGroupId.substring(0, currentGroupId.indexOf("\"")); }
              int jn = jresp.indexOf("\"name\":\"");
              if (jn > 0) { currentGroupName = jresp.substring(jn+8); currentGroupName = currentGroupName.substring(0, currentGroupName.indexOf("\"")); }
              isDM = false; http.end(); state = 4; break;
            }
            http.end();
            delay(150); continue;
          }
        }
        if (ty >= 34 && ty < 34 + 6 * 36) {
          int idx = scroll + (ty - 34) / 36;
          if (idx < (int)groups.size()) { currentGroupId = groups[idx].id; currentGroupName = groups[idx].name; isDM = false; state = 4; break; }
        }
        delay(130);
      }
    } else if (state == 3) {
      if (dmPubkey == "") {
        http.begin(apiBase + "/api/users");
        if (http.GET() == 200) {
          String uResp = http.getString(); int pos = 0;
          while (true) {
            int un = uResp.indexOf("\"username\":\"", pos);
            if (un < 0) break;
            String u = uResp.substring(un+11); u = u.substring(0, u.indexOf("\""));
            int pk = uResp.indexOf("\"pubkey\":\"", un);
            if (pk > 0) { String pkv = uResp.substring(pk+10); pkv = pkv.substring(0, pkv.indexOf("\"")); if (u == dmTarget) { dmPubkey = pkv; break; } }
            pos = un + 1;
          }
        }
        http.end();
      }
      if (dmPubkey == "") { state = 1; continue; }
      isDM = true; state = 4;
    } else if (state == 4) {
      std::vector<String> messages;
      String chatInput = ""; int kbMode = 0;
      unsigned long lastPoll = 0; bool dirty = true;
      auto fetchMessages = [&]() {
        if (isDM && dmPubkey != "") {
          http.begin(apiBase + "/api/messages?pubkey=" + pubkey + "&otherPubkey=" + dmPubkey);
        } else {
          http.begin(apiBase + "/api/messages?groupId=" + currentGroupId);
        }
        if (http.GET() == 200) {
          String mResp = http.getString(); messages.clear(); int pos = 0;
          while (true) {
            int sc = mResp.indexOf("\"content\":\"", pos);
            if (sc < 0) break;
            String ct = mResp.substring(sc+11); ct = ct.substring(0, ct.indexOf("\"")); messages.push_back(ct);
            pos = sc + 1;
          }
        }
        http.end();
      };
      fetchMessages(); dirty = true;
      while (true) {
        if (dirty) {
          tft.fillScreen(BG_COLOR);
          tft.fillRect(0, 0, 240, 28, HEADER_COLOR);
          tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
          tft.setTextDatum(TL_DATUM);
          tft.setCursor(4, 8);
          String h = isDM ? dmTarget : currentGroupName;
          if (h.length() > 16) h = h.substring(0, 15) + "~";
          tft.print(h);
          tft.fillRoundRect(178, 2, 28, 24, 3, TFT_RED);
          tft.setTextColor(TFT_WHITE, TFT_RED);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("X", 192, 14, 1);
          tft.fillRoundRect(208, 2, 28, 24, 3, TFT_ORANGE);
          tft.setTextColor(TFT_WHITE, TFT_ORANGE);
          tft.drawString("+", 222, 14, 1);
          tft.setTextColor(TEXT_COLOR, BG_COLOR);
          tft.setTextDatum(TL_DATUM);
          int my = 30, start = max(0, (int)messages.size() - 7);
          for (int i = start; i < (int)messages.size() && my < 158; i++) {
            tft.setCursor(4, my);
            String m = messages[i];
            int colon = m.indexOf(':');
            if (colon > 0 && colon < 15) {
              String snd = m.substring(0, colon);
              String rst = m.substring(colon + 1);
              tft.setTextColor(TFT_YELLOW, BG_COLOR); tft.print(snd + ":");
              tft.setTextColor(TEXT_COLOR, BG_COLOR);
              if (rst.length() > 28) rst = rst.substring(0, 28);
              tft.print(rst);
            } else {
              if (m.length() > 34) m = m.substring(0, 34);
              tft.setTextColor(TEXT_COLOR, BG_COLOR);
              tft.print(m);
            }
            my += 16;
          }
          tft.fillRect(0, 156, 240, 10, BG_COLOR);
          tft.setTextColor(TFT_CYAN, BG_COLOR);
          tft.setTextDatum(TL_DATUM); tft.setCursor(4, 158);
          String si = chatInput;
          if (si.length() > 38) si = si.substring(si.length() - 38);
          tft.print(">" + si);
          drawKeyboard(kbMode, 165);
          dirty = false;
        }
        int tx, ty;
        if (!getTouch(tx, ty)) {
          if (millis() - lastPoll > 2000) {
            int oldSz = messages.size();
            fetchMessages();
            if ((int)messages.size() != oldSz) dirty = true;
            lastPoll = millis();
          }
          delay(10); continue;
        }
        if (ty < 28) {
          if (tx > 200) {
            String target = virtualKeyboardInput("Benutzer einladen:", "", 30);
            target.trim(); if (target == "") { dirty = true; delay(150); continue; }
            http.begin(apiBase + "/api/users");
            if (http.GET() == 200) {
              String uResp = http.getString(); int pos = 0;
              while (true) {
                int un = uResp.indexOf("\"username\":\"", pos);
                if (un < 0) break;
                String u = uResp.substring(un+11); u = u.substring(0, u.indexOf("\""));
                int pk = uResp.indexOf("\"pubkey\":\"", un);
                if (pk > 0) { String pkv = uResp.substring(pk+10); pkv = pkv.substring(0, pkv.indexOf("\"")); if (u == target) { http.begin(apiBase + "/api/groups/" + currentGroupId + "/invite"); http.addHeader("Content-Type", "application/json"); http.POST("{\"pubkey\":\"" + pkv + "\"}"); http.end(); break; } }
                pos = un + 1;
              }
            }
            http.end();
            dirty = true;
          } else if (tx > 170) { drainTouch(); state = 0; break; }
          delay(150); continue;
        }
        if (ty >= 165) {
          int res = handleKeyboardTouch(tx, ty, chatInput, kbMode, 165);
          if (res == 2 && chatInput.length() > 0) {
            chatInput.replace("\\", "\\\\"); chatInput.replace("\"", "\\\"");
            http.begin(apiBase + "/api/messages");
            http.addHeader("Content-Type", "application/json");
            if (isDM && dmPubkey != "") {
              http.POST("{\"toPubkey\":\"" + dmPubkey + "\",\"senderPubkey\":\"" + pubkey + "\",\"senderPrivkey\":\"" + privkey + "\",\"content\":\"" + username + ":" + chatInput + "\"}");
            } else {
              http.POST("{\"groupId\":\"" + currentGroupId + "\",\"senderPubkey\":\"" + pubkey + "\",\"senderPrivkey\":\"" + privkey + "\",\"content\":\"" + username + ":" + chatInput + "\"}");
            }
            http.end(); chatInput = ""; fetchMessages(); dirty = true;
          } else if (res == 1) dirty = true;
        }
        delay(20);
      }
    }
    delay(10);
  }
}

// ================= STUNDENPLAN APP =================
// Ruft WebUntis-Timetable ueber API ab und zeigt einen Tag mit Navigation.

String extractField(String json, String key) {
  int k = json.indexOf("\"" + key + "\":");
  if (k < 0) return "";
  k += key.length() + 4;
  while (k < (int)json.length() && json[k] == ' ') k++;
  if (k >= (int)json.length()) return "";
  if (json[k] == '"') {
    k++; String r = "";
    while (k < (int)json.length() && json[k] != '"') { if (json[k] == '\\') { k++; if (k < (int)json.length()) r += json[k]; } else r += json[k]; k++; }
    return r;
  } else if (json[k] == '{') {
    int n = json.indexOf("\"name\":\"", k);
    if (n < 0) return "";
    n += 8; String r = "";
    while (n < (int)json.length() && json[n] != '"') r += json[n++];
    return r;
  } else {
    String r = "";
    while (k < (int)json.length() && json[k] >= '0' && json[k] <= '9') { r += json[k]; k++; }
    return r;
  }
}

void stundenplanApp() {
  tft.fillScreen(BG_COLOR);
  tft.println("HOLE PLAN");
  drainTouch();
  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Kein WLAN - Stundentabelle", 120, 100, 2);
    tft.drawString("nicht verfuegbar", 120, 130, 2);
    tft.drawString("Tippen zum Zurueck", 120, 170, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    drainTouch(); drawMenu(); return;
  }
  HTTPClient http; http.setTimeout(10000);
  String apiBase = "http://149.102.157.124:3001";
  struct tm timeinfo;
  bool hasTime = useWiFiTime && getLocalTime(&timeinfo);
  int curYear = 2026, curMon = 6, curDay = 15, curWDay = 1;
  if (hasTime) {
    curYear = timeinfo.tm_year + 1900; curMon = timeinfo.tm_mon + 1;
    curDay = timeinfo.tm_mday; curWDay = timeinfo.tm_wday;
    if (curWDay == 0) curWDay = 7;  // Sonntag = 7
  }
  struct Period { String subject, start, end, teacher, room; };
  std::vector<Period> weekDays[7];
  int cachedMonY = 0, cachedMonM = 0, cachedMonD = 0;
  
  // viewDayOffset: 0=Montag, 1=Dienstag, 2=Mittwoch, 3=Donnerstag, 4=Freitag
  int viewDayOffset = curWDay - 1;
  if (viewDayOffset < 0 || viewDayOffset > 6) viewDayOffset = 0;
  if (viewDayOffset > 4) viewDayOffset = 0;  // Auf Montag setzen wenn Wochenende

  auto fetchWeek = [&](int y, int m, int d) -> bool {
    for (int i = 0; i < 7; i++) weekDays[i].clear();
    cachedMonY = y; cachedMonM = m; cachedMonD = d;
    int endY = y, endM = m, endD = d + 6;
    int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) dim[1] = 29;
    if (endD > dim[endM - 1]) { endD -= dim[endM - 1]; endM++; if (endM > 12) { endM = 1; endY++; } }
    char url[120];
    snprintf(url, sizeof(url), "%s/webuntis/timetable?start=%04d-%02d-%02d&end=%04d-%02d-%02d", apiBase.c_str(), y, m, d, endY, endM, endD);
    http.begin(url);
    if (http.GET() != 200) { http.end(); return false; }
    String resp = http.getString(); http.end();
    int weekDates[7];
    for (int i = 0; i < 7; i++) {
      int cy = y, cm = m, cd = d + i;
      if (cd > dim[cm - 1]) { cd -= dim[cm - 1]; cm++; if (cm > 12) { cm = 1; cy++; } }
      weekDates[i] = cy * 10000 + cm * 100 + cd;
    }
    int pp = resp.indexOf("\"periods\":");
    if (pp < 0) return false;
    pp = resp.indexOf('[', pp);
    if (pp < 0) return false;
    int depth = 1, ep = pp + 1;
    while (depth > 0 && ep < (int)resp.length()) {
      if (resp[ep] == '[') depth++;
      else if (resp[ep] == ']') depth--;
      ep++;
    }
    String arr = resp.substring(pp, ep);
    int pos = 0;
    while (true) {
      int ob = arr.indexOf('{', pos);
      int cb = arr.indexOf('}', pos);
      if (ob < 0 || cb < 0 || cb <= ob) break;
      String pj = arr.substring(ob, cb + 1);
      String ds = extractField(pj, "date");
      int pd = ds.toInt();
      for (int di = 0; di < 7; di++) {
        if (pd == weekDates[di]) {
          Period p;
          p.subject = extractField(pj, "subject");
          String st = extractField(pj, "startTime");
          String et = extractField(pj, "endTime");
          if (st.length() == 3) st = "0" + st;
          if (st.length() >= 4) p.start = st.substring(0, 2) + ":" + st.substring(2); else p.start = st;
          if (et.length() == 3) et = "0" + et;
          if (et.length() >= 4) p.end = et.substring(0, 2) + ":" + et.substring(2); else p.end = et;
          p.teacher = extractField(pj, "teacher");
          p.room = extractField(pj, "room");
          if (p.subject.length() > 0) weekDays[di].push_back(p);
          break;
        }
      }
      pos = cb + 1;
    }
    return true;
  };

  // Aktuelle Woche berechnen (Montag der aktuellen Woche)
  int mondayDay = curDay - (curWDay - 1);
  int mondayMon = curMon, mondayYear = curYear;
  int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if ((mondayYear % 4 == 0 && mondayYear % 100 != 0) || mondayYear % 400 == 0) dim[1] = 29;
  
  // Korrektur: Wenn mondayDay < 1, dann ist der Montag im Vormonat
  while (mondayDay < 1) {
    mondayMon--;
    if (mondayMon < 1) {
      mondayMon = 12;
      mondayYear--;
      // Dim-Array für neues Jahr neu berechnen
      if ((mondayYear % 4 == 0 && mondayYear % 100 != 0) || mondayYear % 400 == 0) dim[1] = 29;
      else dim[1] = 28;
    }
    mondayDay += dim[mondayMon - 1];
  }
  
  if (!fetchWeek(mondayYear, mondayMon, mondayDay)) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Fehler beim Laden", 120, 100, 2);
    tft.drawString("Tippen zum Zurueck", 120, 140, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    drainTouch(); drawMenu(); return;
  }
  
  static const char* DOW[] = {"Mo","Di","Mi","Do","Fr","Sa","So"};
  
  // Wenn wir uns im Wochenende befinden, automatisch zur nächsten Woche wechseln
  if (viewDayOffset > 4) {
    viewDayOffset = 0;  // Montag der nächsten Woche
    // Nächste Woche berechnen
    mondayDay += 7;
    if (mondayDay > dim[mondayMon - 1]) {
      mondayDay -= dim[mondayMon - 1];
      mondayMon++;
      if (mondayMon > 12) {
        mondayMon = 1;
        mondayYear++;
        if ((mondayYear % 4 == 0 && mondayYear % 100 != 0) || mondayYear % 400 == 0) dim[1] = 29;
        else dim[1] = 28;
      }
    }
    fetchWeek(mondayYear, mondayMon, mondayDay);
  }

  int periodScroll = 0;
  while (true) {
    int vy = cachedMonY, vm = cachedMonM, vd = cachedMonD + viewDayOffset;
    int dim2[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if ((vy % 4 == 0 && vy % 100 != 0) || vy % 400 == 0) dim2[1] = 29;
    if (vd > dim2[vm - 1]) { vd -= dim2[vm - 1]; vm++; if (vm > 12) { vm = 1; vy++; } }
    
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    char hdr[32]; snprintf(hdr, sizeof(hdr), "%s %02d.%02d.%04d", DOW[viewDayOffset], vd, vm, vy);
    tft.drawString("STUNDENPLAN", 120, 14, 2);
    tft.fillRoundRect(180, 2, 55, 24, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("X", 207, 14, 1);
    
    // Day header
    tft.fillRect(0, 28, SCREEN_W, 22, darkMode ? 0x2104 : TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(hdr), 120, 39, 2);

    // Periods
    int yPos = 56;
    int maxPeriodY = 245;
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(TL_DATUM);
    auto& periods = weekDays[viewDayOffset];
    if (periods.empty()) {
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Keine Stunden", 120, 150, 2);
    } else {
      int maxVis = (maxPeriodY - 56) / 40;
      for (int i = periodScroll; i < (int)periods.size() && yPos < maxPeriodY; i++) {
        auto& p = periods[i];
        tft.fillRoundRect(4, yPos, 232, 36, 4, PANEL_COLOR);
        tft.setTextColor(TFT_WHITE, PANEL_COLOR);
        tft.setTextDatum(TL_DATUM);
        tft.setCursor(8, yPos + 2);
        tft.print(p.start + "-" + p.end);
        tft.setCursor(70, yPos + 2);
        tft.setTextColor(TFT_YELLOW, PANEL_COLOR);
        String subj = p.subject; if (subj.length() > 12) subj = subj.substring(0, 11);
        tft.print(subj);
        if (p.room.length() > 0) {
          tft.setTextColor(TFT_LIGHTGREY, PANEL_COLOR);
          tft.setCursor(8, yPos + 20);
          tft.print("Raum: " + p.room);
        }
        tft.setTextColor(TFT_CYAN, PANEL_COLOR);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(">", 230, yPos + 18, 1);
        yPos += 40;
      }
      if (periodScroll > 0) { tft.fillTriangle(120, 50, 114, 58, 126, 58, TFT_LIGHTGREY); }
      if (periodScroll + maxVis < (int)periods.size()) { tft.fillTriangle(120, maxPeriodY - 2, 114, maxPeriodY - 10, 126, maxPeriodY - 10, TFT_LIGHTGREY); }
    }
    
    // Bottom navigation
    int by = SCREEN_H - 34;
    tft.drawFastHLine(0, by, SCREEN_W, BORDER_COLOR);
    tft.fillRect(0, by + 1, SCREEN_W, 33, BG_COLOR);
    tft.fillTriangle(20, by + 24, 30, by + 14, 30, by + 34, TFT_WHITE);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(hdr), 120, by + 24, 2);
    tft.fillTriangle(220, by + 24, 210, by + 14, 210, by + 34, TFT_WHITE);
    
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    
    if (ty < 28 && tx > 175) { drainTouch(); drawMenu(); return; }
    
    // Period scroll arrows
    if (!periods.empty() && ty >= 48 && ty < 245) {
      int maxVis = (maxPeriodY - 56) / 40;
      if (tx > 100 && tx < 140) {
        if (ty < 60 && periodScroll > 0) { periodScroll--; delay(200); continue; }
        if (ty >= maxPeriodY - 14 && ty < maxPeriodY && periodScroll + maxVis < (int)periods.size()) { periodScroll++; delay(200); continue; }
      }
    }
    
    // Bottom bar navigation
    if (ty >= by) {
      if (tx < 40) {
        // Rückwärts blättern - Mo-Fr Begrenzung
        viewDayOffset--;
        if (viewDayOffset < 0) {
          // Zur vorherigen Woche (Freitag der Vorwoche)
          viewDayOffset = 4;
          // Montag der Vorwoche berechnen
          mondayDay = cachedMonD - 7;
          mondayMon = cachedMonM; 
          mondayYear = cachedMonY;
          while (mondayDay < 1) {
            mondayMon--;
            if (mondayMon < 1) {
              mondayMon = 12;
              mondayYear--;
              if ((mondayYear % 4 == 0 && mondayYear % 100 != 0) || mondayYear % 400 == 0) dim[1] = 29;
              else dim[1] = 28;
            }
            mondayDay += dim[mondayMon - 1];
          }
          fetchWeek(mondayYear, mondayMon, mondayDay);
        }
        delay(200); continue;
      } else if (tx > 200) {
        // Vorwärts blättern - Mo-Fr Begrenzung
        viewDayOffset++;
        if (viewDayOffset > 4) {
          // Zur nächsten Woche (Montag der nächsten Woche)
          viewDayOffset = 0;
          mondayDay = cachedMonD + 7;
          mondayMon = cachedMonM; 
          mondayYear = cachedMonY;
          if ((mondayYear % 4 == 0 && mondayYear % 100 != 0) || mondayYear % 400 == 0) dim[1] = 29;
          else dim[1] = 28;
          if (mondayDay > dim[mondayMon - 1]) {
            mondayDay -= dim[mondayMon - 1];
            mondayMon++;
            if (mondayMon > 12) {
              mondayMon = 1;
              mondayYear++;
            }
          }
          fetchWeek(mondayYear, mondayMon, mondayDay);
        }
        delay(200); continue;
      }
    }
    
    // Tap period for details
    int maxVis = (maxPeriodY - 56) / 40;
    if (ty >= 56 && ty < maxPeriodY && !periods.empty()) {
      int idx = periodScroll + (ty - 56) / 40;
      if (idx >= 0 && idx < (int)periods.size()) {
        drainTouch();
        auto& p = periods[idx];
        tft.fillScreen(BG_COLOR);
        tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
        tft.setTextColor(TFT_YELLOW, HEADER_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(p.subject), 120, 14, 2);
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setTextDatum(TL_DATUM);
        int ly = 50;
        tft.setCursor(10, ly); tft.print("Zeit: " + p.start + " - " + p.end); ly += 30;
        if (p.teacher.length() > 0) { tft.setCursor(10, ly); tft.print("Lehrer: " + p.teacher); ly += 30; }
        if (p.room.length() > 0) { tft.setCursor(10, ly); tft.print("Raum: " + p.room); ly += 30; }
        tft.setTextColor(TFT_LIGHTGREY, BG_COLOR);
        tft.setCursor(10, 220);
        tft.print("Tippen zum Schliessen");
        while (!getTouch(tx, ty)) delay(10);
        drainTouch();
      }
    }
    delay(130);
  }
}

// ================= DATEI-MANAGER =================
// Erreichbar ueber Einstellungen. Ermoeglicht:
// - Ordner und Dateien browsen
// - Neue Datei / neuen Ordner erstellen
// - Datei umbenennen
// - Datei / Ordner loeschen
// - Datei verschieben (Ausschneiden + Ziel-Ordner waehlen)

int listDir(String path, std::vector<FMEntry>& entries) {
  entries.clear();
  if (!sdReady) return 0;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  File f = dir.openNextFile();
  while (f) {
    FMEntry e;
    e.name = String(f.name());
    e.isDir = f.isDirectory();
    entries.push_back(e);
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  return entries.size();
}

// Einfacher Text-Editor fuer kleine Dateien im Datei-Manager
void fmEditFile(String path) {
  String text = "";
  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    if (f) {
      while (f.available()) text += (char)f.read();
      f.close();
    }
  }
  int kbMode = 0;
  while (true) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, 240, 24, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    String fname = path;
    fname = fname.substring(fname.lastIndexOf('/') + 1);
    if (fname.length() > 18) fname = fname.substring(0, 18);
    tft.setCursor(4, 6);
    tft.print(fname);
    tft.fillRoundRect(168, 2, 66, 20, 3, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SPEICHERN", 201, 12, 1);
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(TL_DATUM);
    int lineY = 28;
    String disp = text;
    if (disp.length() > 600) disp = disp.substring(disp.length() - 600);
    int idx2 = 0;
    while (idx2 < (int)disp.length() && lineY < 160) {
      String line;
      while (idx2 < (int)disp.length() && disp[idx2] != '\n' && line.length() < 34) line += disp[idx2++];
      if (idx2 < (int)disp.length() && disp[idx2] == '\n') idx2++;
      tft.setCursor(4, lineY);
      tft.print(line);
      lineY += 12;
    }
    if ((millis() / 500) % 2 == 0 && lineY < 160) {
      int cc = text.length() % 34;
      tft.fillRect(4 + cc * 6, lineY - 12, 6, 10, TEXT_COLOR);
    }
    drawKeyboard(kbMode, 165);
    int kx, ky;
    while (!getTouch(kx, ky)) delay(10);
    if (ky < 24 && kx > 162) {
      File sf = SD.open(path, FILE_WRITE);
      if (sf) {
        sf.print(text);
        sf.close();
      }
      drainTouch();
      return;
    }
    if (ky < 24) {
      drainTouch();
      return;
    }
    handleKeyboardTouch(kx, ky, text, kbMode, 165);
    delay(130);
  }
}

void fileManagerApp() {
  drainTouch();
  if (!sdReady) {
    tft.fillScreen(BG_COLOR);
    tft.setTextColor(TFT_RED, BG_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SD-Karte nicht", 120, 140, 2);
    tft.drawString("verfuegbar!", 120, 170, 2);
    delay(2000);
    return;
  }

  String currentPath = "/";
  String cutPath = "";  // Ausgeschnittene Datei (fuer Verschieben)
  int scrollOffset = 0;

  // Maximale sichtbare Eintraege
  const int MAX_VISIBLE = 6;
  // Hoehe einer Zeile in Pixeln
  const int ROW_H = 36;

  while (true) {
    std::vector<FMEntry> entries;
    listDir(currentPath, entries);

    // Screen zeichnen
    tft.fillScreen(BG_COLOR);
    // Header
    tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(TL_DATUM);
    String headerPath = currentPath;
    if (headerPath.length() > 18) headerPath = ".." + headerPath.substring(headerPath.length() - 16);
    tft.drawString(headerPath, 4, 8, 1);

    // Zurueck-Button (links)
    tft.fillRoundRect(2, 2, 30, 24, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("<", 17, 14, 2);

    // Neue Datei (mitte)
    tft.fillRoundRect(96, 2, 46, 24, 3, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("+Dat", 119, 14, 1);

    // Neuer Ordner (rechts davon)
    tft.fillRoundRect(146, 2, 46, 24, 3, TFT_CYAN);
    tft.setTextColor(TFT_BLACK, TFT_CYAN);
    tft.drawString("+Ord", 169, 14, 1);

    // Einfuegen-Button (wenn etwas ausgeschnitten)
    if (cutPath != "") {
      tft.fillRoundRect(196, 2, 40, 24, 3, TFT_ORANGE);
      tft.setTextColor(TFT_WHITE, TFT_ORANGE);
      tft.drawString("EINF", 216, 14, 1);
    }

    // Eintraege
    int visibleCount = min((int)entries.size() - scrollOffset, MAX_VISIBLE);
    for (int i = 0; i < visibleCount; i++) {
      int idx = i + scrollOffset;
      int y = 32 + i * ROW_H;
      const FMEntry& e = entries[idx];
      uint16_t rowColor = e.isDir ? 0x0410 : PANEL_COLOR;  // Ordner leicht gruen
      tft.fillRoundRect(2, y, 196, ROW_H - 2, 4, rowColor);
      tft.setTextColor(TEXT_COLOR, rowColor);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(28, y + 4);
      String displayName = e.name;
      if (displayName.length() > 18) displayName = displayName.substring(0, 17) + "~";
      tft.print((e.isDir ? "[" : " ") + displayName + (e.isDir ? "]" : ""));

      // Typ-Icon (einfach)
      tft.setTextColor(e.isDir ? TFT_YELLOW : TFT_CYAN, rowColor);
      tft.setCursor(6, y + 4);
      tft.print(e.isDir ? "D" : "F");

      // Aktions-Knopf (3 kleine Buttons: Bearbeiten/Oeffnen | Umbenennen | Loeschen)
      tft.fillRoundRect(200, y, 37, (ROW_H - 2) / 3, 3, TFT_GREEN);
      tft.fillRoundRect(200, y + 12, 37, (ROW_H - 2) / 3, 3, TFT_BLUE);
      tft.fillRoundRect(200, y + 24, 37, (ROW_H - 2) / 3, 3, TFT_RED);
      tft.setTextColor(TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(e.isDir ? "OPN" : "EDI", 218, y + 5, 1);
      tft.drawString("REN", 218, y + 17, 1);
      tft.drawString("DEL", 218, y + 29, 1);
    }

    // Scroll-Pfeile
    if (scrollOffset > 0) {
      tft.fillTriangle(120, 250, 110, 260, 130, 260, TFT_LIGHTGREY);
    }
    if (scrollOffset + MAX_VISIBLE < (int)entries.size()) {
      tft.fillTriangle(120, 248, 110, 240, 130, 240, TFT_LIGHTGREY);
    }

    // Touch warten
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);

    // Header-Bereich
    if (ty < 28) {
      // Zurueck
      if (tx < 35) {
        drainTouch();
        if (currentPath == "/") return;  // Ganz raus
        // Eine Ebene hoch
        int slash = currentPath.lastIndexOf('/');
        if (slash > 0) currentPath = currentPath.substring(0, slash);
        else currentPath = "/";
        scrollOffset = 0;
        continue;
      }
      // Neue Datei
      if (tx >= 96 && tx < 142) {
        drainTouch();
        String newName = virtualKeyboardInput("Dateiname:", "", 24);
        newName.trim();
        if (newName.length() > 0) {
          String fullPath = currentPath;
          if (!fullPath.endsWith("/")) fullPath += "/";
          fullPath += newName;
          File nf = SD.open(fullPath, FILE_WRITE);
          if (nf) { nf.close(); }
          // Direkt in Editor
          fmEditFile(fullPath);
        }
        continue;
      }
      // Neuer Ordner
      if (tx >= 146 && tx < 192) {
        drainTouch();
        String newName = virtualKeyboardInput("Ordner-Name:", "", 24);
        newName.trim();
        if (newName.length() > 0) {
          String fullPath = currentPath;
          if (!fullPath.endsWith("/")) fullPath += "/";
          fullPath += newName;
          SD.mkdir(fullPath);
        }
        continue;
      }
      // Einfuegen (Verschieben)
      if (tx >= 196 && cutPath != "") {
        drainTouch();
        if (cutPath != "") {
          String fname = cutPath.substring(cutPath.lastIndexOf('/') + 1);
          String dest = currentPath;
          if (!dest.endsWith("/")) dest += "/";
          dest += fname;
          // SD hat kein rename ueber Verzeichnisgrenzen, daher kopieren + loeschen
          File src = SD.open(cutPath, FILE_READ);
          File dst = SD.open(dest, FILE_WRITE);
          if (src && dst) {
            while (src.available()) dst.write(src.read());
            src.close();
            dst.close();
            SD.remove(cutPath);
          }
          cutPath = "";
        }
        continue;
      }
      delay(150);
      continue;
    }

    // Scroll-Bereich
    if (ty >= 245 && ty < 265) {
      if (ty < 255 && scrollOffset + MAX_VISIBLE < (int)entries.size()) scrollOffset++;
      else if (ty >= 255 && scrollOffset > 0) scrollOffset--;
      delay(150);
      continue;
    }

    // Eintrags-Bereich
    int rowIdx = (ty - 32) / ROW_H + scrollOffset;
    if (rowIdx >= 0 && rowIdx < (int)entries.size() && ty >= 32 && ty < 32 + MAX_VISIBLE * ROW_H) {
      int rowY = 32 + (rowIdx - scrollOffset) * ROW_H;
      String entryPath = currentPath;
      if (!entryPath.endsWith("/")) entryPath += "/";
      entryPath += entries[rowIdx].name;

      if (tx >= 200) {
        // Aktions-Buttons
        int subY = ty - rowY;
        drainTouch();

        if (subY < 12) {
          // Oeffnen / Bearbeiten
          if (entries[rowIdx].isDir) {
            currentPath = entryPath;
            scrollOffset = 0;
          } else {
            fmEditFile(entryPath);
          }
        } else if (subY < 24) {
          // Umbenennen
          String oldName = entries[rowIdx].name;
          String newName = virtualKeyboardInput("Umbenennen:", oldName, 24);
          newName.trim();
          if (newName.length() > 0 && newName != oldName) {
            String newPath = currentPath;
            if (!newPath.endsWith("/")) newPath += "/";
            newPath += newName;
            // SD rename nutzen (gleiche Partition)
            SD.rename(entryPath, newPath);
          }
        } else {
          // Loeschen
          tft.fillScreen(BG_COLOR);
          tft.setTextColor(TEXT_COLOR, BG_COLOR);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("Wirklich loeschen?", 120, 100, 2);
          tft.drawString(entries[rowIdx].name, 120, 130, 1);
          drawButton(20, 170, 90, 36, TFT_RED, "JA");
          drawButton(130, 170, 90, 36, TFT_GREEN, "NEIN");
          int cx, cy;
          while (!getTouch(cx, cy)) delay(10);
          drainTouch();
          if (isButtonPressed(cx, cy, 20, 170, 90, 36)) {
            if (entries[rowIdx].isDir) SD.rmdir(entryPath);
            else SD.remove(entryPath);
          }
        }
      } else if (tx < 200) {
        // Normales Tippen = Ordner oeffnen oder Datei auswaehlen
        drainTouch();
        if (entries[rowIdx].isDir) {
          currentPath = entryPath;
          scrollOffset = 0;
        } else {
          // Schnell-Aktions-Menu: Bearbeiten oder Ausschneiden
          tft.fillScreen(BG_COLOR);
          tft.setTextColor(TEXT_COLOR, BG_COLOR);
          tft.setTextDatum(MC_DATUM);
          tft.drawString(entries[rowIdx].name, 120, 80, 2);
          drawButton(20, 120, 200, 36, TFT_GREEN, "Bearbeiten");
          drawButton(20, 165, 200, 36, TFT_ORANGE, "Ausschneiden (Verschb.)");
          drawButton(20, 210, 200, 36, TFT_RED, "Abbrechen");
          int cx, cy;
          while (!getTouch(cx, cy)) delay(10);
          drainTouch();
          if (isButtonPressed(cx, cy, 20, 120, 200, 36)) {
            fmEditFile(entryPath);
          } else if (isButtonPressed(cx, cy, 20, 165, 200, 36)) {
            cutPath = entryPath;
          }
        }
      }
    }
    delay(150);
  }
}

// ================= WLAN SCAN & PASSWORT-EINGABE =================
// Scannt verfuegbare WLAN-Netzwerke, zeigt sie als Liste (mit Signalstaerke),
// und fragt nach Antippen eines Netzwerks per virtueller Tastatur das Passwort ab.
// Fallback: manuelle SSID/Passwort-Eingabe ueber "MANU."-Button.

void wifiScanAndConfigure() {
  drainTouch();
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Suche WLANs...", 120, 150, 2);

  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();

  std::vector<String> ssids;
  std::vector<int> rssis;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool dup = false;
    for (size_t j = 0; j < ssids.size(); j++)
      if (ssids[j] == s) {
        dup = true;
        break;
      }
    if (dup) continue;
    ssids.push_back(s);
    rssis.push_back(WiFi.RSSI(i));
  }
  WiFi.scanDelete();

  int scrollOff = 0;
  const int ROW_H = 36;
  const int MAX_VISIBLE = 6;

  while (true) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, 240, 28, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WLAN AUSWAEHLEN", 120, 14, 2);
    tft.fillRoundRect(2, 2, 40, 24, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("<", 22, 14, 2);
    tft.fillRoundRect(186, 2, 52, 24, 3, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString("MANU.", 212, 14, 1);

    if (ssids.empty()) {
      tft.setTextColor(TFT_YELLOW, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Keine Netzwerke gefunden", 120, 150, 2);
    }

    int visibleCount = min((int)ssids.size() - scrollOff, MAX_VISIBLE);
    for (int i = 0; i < visibleCount; i++) {
      int idx = i + scrollOff;
      int y = 32 + i * ROW_H;
      tft.fillRoundRect(4, y, 232, ROW_H - 2, 4, PANEL_COLOR);
      tft.setTextColor(TEXT_COLOR, PANEL_COLOR);
      tft.setTextDatum(TL_DATUM);
      String name = ssids[idx];
      if (name.length() > 21) name = name.substring(0, 20) + "~";
      tft.setCursor(8, y + 10);
      tft.print(name);
      // Signalstaerke als kleine Balken (4 Stufen)
      int bars = rssis[idx] > -55 ? 4 : rssis[idx] > -65 ? 3
                                      : rssis[idx] > -75 ? 2
                                                         : 1;
      for (int b = 0; b < 4; b++) {
        uint16_t bc = b < bars ? TFT_GREEN : TFT_DARKGREY;
        int bh = 6 + b * 4;
        tft.fillRect(196 + b * 8, y + (ROW_H - 2) - bh - 4, 5, bh, bc);
      }
    }
    if (scrollOff > 0) tft.fillTriangle(120, 252, 110, 262, 130, 262, TFT_LIGHTGREY);
    if (scrollOff + MAX_VISIBLE < (int)ssids.size()) tft.fillTriangle(120, 250, 110, 240, 130, 240, TFT_LIGHTGREY);

    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);

    if (ty < 28) {
      if (tx < 42) {
        drainTouch();
        return;
      }
      if (tx >= 186) {
        drainTouch();
        String newSSID = virtualKeyboardInput("WLAN-Name (SSID):", WIFI_SSID, 32);
        newSSID.trim();
        if (newSSID.length() > 0) {
          String newPass = virtualKeyboardInput("WLAN-Passwort:", "", 63);
          WIFI_SSID = newSSID;
          WIFI_PASS = newPass;
          saveSettings();
          tft.fillScreen(BG_COLOR);
          tft.setTextColor(TFT_GREEN, BG_COLOR);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("WLAN-Daten gespeichert", 120, 150, 2);
          delay(1200);
        }
        return;
      }
      delay(150);
      continue;
    }

    if (ty >= 245 && ty < 265) {
      if (ty < 255 && scrollOff + MAX_VISIBLE < (int)ssids.size()) scrollOff++;
      else if (ty >= 255 && scrollOff > 0) scrollOff--;
      delay(150);
      continue;
    }

    int rowIdx = (ty - 32) / ROW_H + scrollOff;
    if (rowIdx >= 0 && rowIdx < (int)ssids.size() && ty < 32 + MAX_VISIBLE * ROW_H) {
      drainTouch();
      String chosenSSID = ssids[rowIdx];
      String newPass = virtualKeyboardInput("Passwort fuer " + chosenSSID + ":", "", 63);
      WIFI_SSID = chosenSSID;
      WIFI_PASS = newPass;
      saveSettings();
      tft.fillScreen(BG_COLOR);
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Verbinde...", 120, 150, 2);
      useWiFiTime = true;
      saveSettings();
      connectWiFi();
      if (WiFi.status() == WL_CONNECTED) initTime();
      return;
    }
    delay(150);
  }
}

// ================= SETTINGS APP =================

void settingsApp() {
  drainTouch();
  while (true) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(0, 0, 240, 28, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("EINSTELLUNGEN", 120, 14, 2);

    drawButton(10, 34, 220, 30, useWiFiTime ? TFT_GREEN : TFT_RED,
               useWiFiTime ? "WiFi/Zeit: AN" : "WiFi/Zeit: AUS");

    drawButton(10, 68, 220, 30, darkMode ? 0x1082 : 0xC618,
               darkMode ? "Dark Mode: AN" : "Dark Mode: AUS");

    // Status-Anzeige
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(12, 104);
    tft.print("WiFi: ");
    tft.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, BG_COLOR);
    tft.print(WiFi.status() == WL_CONNECTED ? "Verbunden" : "Getrennt");
    tft.setTextColor(TEXT_COLOR, BG_COLOR);
    tft.setCursor(12, 118);
    tft.print("SD: ");
    tft.setTextColor(sdReady ? TFT_GREEN : TFT_RED, BG_COLOR);
    tft.print(sdReady ? "Bereit" : "Nicht gefunden");

    int tzHour = GMT_OFFSET_SEC / 3600;
    String tzLabel = "Zeitzone: UTC" + String(tzHour >= 0 ? "+" : "") + String(tzHour);
    drawButton(10, 136, 220, 30, TFT_BLUE, tzLabel);

    drawButton(10, 170, 220, 30, TFT_CYAN, "WiFi neu verbinden");
    drawButton(10, 204, 220, 30, TFT_PURPLE, "WLAN scannen / aendern");
    drawButton(10, 238, 220, 30, TFT_MAGENTA, "SD-Karte neu laden");
    drawButton(10, 272, 100, 34, 0x4208, "Datei-Manager");
    drawButton(120, 272, 110, 34, TFT_RED, "Zurueck");

    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);

    if (isButtonPressed(tx, ty, 10, 34, 220, 30)) {
      useWiFiTime = !useWiFiTime;
      applyTheme();
      if (useWiFiTime) {
        connectWiFi();
        initTime();
      } else WiFi.disconnect(true);
      saveSettings();

    } else if (isButtonPressed(tx, ty, 10, 68, 220, 30)) {
      darkMode = !darkMode;
      applyTheme();
      saveSettings();

    } else if (isButtonPressed(tx, ty, 10, 136, 220, 30)) {
      int tz = GMT_OFFSET_SEC / 3600;
      tz++;
      if (tz > 12) tz = -12;
      GMT_OFFSET_SEC = (long)tz * 3600L;
      saveSettings();
      if (useWiFiTime) initTime();

    } else if (isButtonPressed(tx, ty, 10, 170, 220, 30)) {
      ensureWiFi();
      if (useWiFiTime) connectWiFi();

    } else if (isButtonPressed(tx, ty, 10, 204, 220, 30)) {
      drainTouch();
      wifiScanAndConfigure();

    } else if (isButtonPressed(tx, ty, 10, 238, 220, 30)) {
      initSD();

    } else if (isButtonPressed(tx, ty, 10, 272, 100, 34)) {
      drainTouch();
      fileManagerApp();

    } else if (isButtonPressed(tx, ty, 120, 272, 110, 34)) {
      drainTouch();
      drawMenu();
      return;
    }
    delay(180);
  }
}
// ================= STOPPUHR APP =================

void stopwatchApp() {
  drawButton(10, 170, 220, 30, TFT_CYAN, "Start");
    drawButton(10, 204, 220, 30, TFT_PURPLE, "Stop");
    drawButton(10, 238, 220, 30, TFT_MAGENTA, "Reset");

     if (isButtonPressed(tx, ty, 10, 170, 220, 30)) {
      
      

    } else if (isButtonPressed(tx, ty, 10, 204, 220, 30)) {
      

    } else if (isButtonPressed(tx, ty, 10, 238, 220, 30)) {
      
    }
      

}
