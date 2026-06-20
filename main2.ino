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
const char* WIFI_SSID = "Noah iphone";
const char* WIFI_PASS = "13278965";

// ============ TIME SETTINGS ============
// Deutschland:
// Winter: GMT_OFFSET_SEC = 3600
// Sommer: GMT_OFFSET_SEC = 7200

const char* NTP_SERVER = "pool.ntp.org";

long GMT_OFFSET_SEC = 3600;
int DAYLIGHT_OFFSET_SEC = 3600;

// ============ FEATURE TOGGLE ============
// Setze auf false um WiFi und Zeit-Sync zu deaktivieren
bool useWiFiTime = false;  // true = WiFi + NTP aktiv, false = kein WiFi + keine Zeit

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
  if (!useWiFiTime) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi deaktiviert", 120, 150, 2);
    delay(1000);
    return;
  }

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
  if (!useWiFiTime) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Zeit-Sync", 120, 140, 2);
    tft.drawString("deaktiviert", 120, 170, 2);
    delay(1000);
    return;
  }

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
  if (!useWiFiTime) {
    // Wenn WiFi/Zeit deaktiviert, zeige nur "Keine Zeit"
    static bool clockDrawn = false;
    if (!clockDrawn) {
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
        TFT_RED,
        TFT_BLACK);

      tft.drawString(
        "Keine Zeit",
        SCREEN_W / 2,
        295,
        4);
      
      clockDrawn = true;
    }
    return;
  }

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

const uint16_t CARD_BG = 0x2104;
const uint16_t CARD_BORDER = 0x39E7;
const uint16_t CARD_HIGHLIGHT = 0x3186;

struct AppButton {
  int x, y, w, h;
  const char* label;
};

void drawAppCard(int x, int y, int w, int h, const char* label, bool highlight) {
  uint16_t bg = highlight ? CARD_HIGHLIGHT : CARD_BG;
  tft.fillRoundRect(x, y, w, h, 8, bg);
  tft.drawRoundRect(x, y, w, h, 8, CARD_BORDER);
  const uint16_t* arr = nullptr;
  if (strcmp(label, "Calc") == 0) arr = icon_calc;
  else if (strcmp(label, "Draw") == 0) arr = icon_draw;
  else if (strcmp(label, "Notes") == 0) arr = icon_notes;
  else if (strcmp(label, "Chat") == 0) arr = icon_chat;
  else if (strcmp(label, "Read") == 0) arr = icon_book;
  else if (strcmp(label, "Settings") == 0) arr = icon_settings;
  else if (strcmp(label, "WebUntis") == 0) arr = icon_untis;
  if (arr) tft.pushImage(x + (w - 40) / 2, y + 4, 40, 40, (uint16_t*)arr);
  tft.setTextColor(0xADB5, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h - 10, 1);
}

void drawMenu() {
  tft.fillScreen(0x0000);

  AppButton apps[] = {
    {10, 12, 108, 66, "Calc"},
    {122, 12, 108, 66, "Draw"},
    {10, 84, 108, 66, "Notes"},
    {122, 84, 108, 66, "Chat"},
    {10, 156, 108, 66, "Read"},
    {122, 156, 108, 66, "Settings"},
    {30, 228, 180, 38, "WebUntis"},
  };
  int n = sizeof(apps) / sizeof(apps[0]);

  for (int i = 0; i < n; i++) {
    auto& b = apps[i];
    drawAppCard(b.x, b.y, b.w, b.h, b.label, false);
  }
}

void pressAnim(int x, int y, int w, int h, const char* label) {
  drawAppCard(x, y, w, h, label, true);
  delay(60);
  drawAppCard(x, y, w, h, label, false);
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

// ================= MD-DATEIEN LISTEN =================

int listMarkdownFiles(String* fileNames, int maxFiles) {
  if (!sdReady) {
    Serial.println("SD nicht bereit");
    return 0;
  }
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("Kann Root nicht öffnen");
    return 0;
  }
  
  int count = 0;
  
  while (true) {
    File file = root.openNextFile();
    
    if (!file)
      break;
    
    String name = file.name();
    
    // Prüfen ob es eine .md Datei ist (Groß-/Kleinschreibung ignorieren)
    if (name.endsWith(".md") || name.endsWith(".MD")) {
      if (count < maxFiles) {
        fileNames[count] = "/" + name;  // Pfad mit / für root
        count++;
        Serial.print("Gefunden: ");
        Serial.println(name);
      }
    }
    
    file.close();
  }
  
  root.close();
  return count;
}

// ================= DATEIEN ANZEIGEN =================

void showFileSelection() {
  if (!sdReady) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SD-Karte nicht", 120, 140, 2);
    tft.drawString("verfuegbar!", 120, 170, 2);
    delay(2000);
    drawMenu();
    return;
  }
  
  String fileNames[20];  // Max 20 Dateien
  int fileCount = listMarkdownFiles(fileNames, 20);
  
  if (fileCount == 0) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Keine .md Dateien", 120, 140, 2);
    tft.drawString("auf SD-Karte", 120, 170, 2);
    tft.drawString("gefunden!", 120, 200, 2);
    delay(2000);
    drawMenu();
    return;
  }
  
  // Dateien auf dem Display anzeigen
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("=== DATEIEN ===", 120, 10, 2);
  
  // Max 6 Dateien anzeigen (wegen Platz)
  int maxDisplay = min(fileCount, 6);
  
  for (int i = 0; i < maxDisplay; i++) {
    // Dateinamen ohne Pfad anzeigen
    String displayName = fileNames[i];
    displayName.replace("/", "");  // / entfernen
    
    // Button für jede Datei
    int yPos = 50 + (i * 40);
    drawButton(
      20,
      yPos,
      200,
      30,
      TFT_BLUE,
      displayName
    );
  }
  
  // Zurück-Button
  drawButton(
    20,
    290,
    200,
    25,
    TFT_RED,
    "Zurueck"
  );
  
  // Auf Touch warten
  while (true) {
    int tx, ty;
    if (getTouch(tx, ty)) {
      // Prüfen ob Zurück-Button gedrückt wurde
      if (isButtonPressed(tx, ty, 20, 290, 200, 25)) {
        drawMenu();
        return;
      }
      
      // Prüfen ob eine Datei ausgewählt wurde
      for (int i = 0; i < maxDisplay; i++) {
        int yPos = 50 + (i * 40);
        if (isButtonPressed(tx, ty, 20, yPos, 200, 30)) {
          Serial.print("Öffne Datei: ");
          Serial.println(fileNames[i]);
          showMarkdown(fileNames[i]);
          return;
        }
      }
    }
    delay(10);
  }
}

String readFile(String path) {
  if (!sdReady) {
    return "SD-Karte nicht verfügbar";
  }
  
  File file = SD.open(path);
  
  if (!file) {
    Serial.print("Kann Datei nicht öffnen: ");
    Serial.println(path);
    return "Datei nicht gefunden";
  }

  String content;

  while (file.available()) {
    content += (char)file.read();
  }

  file.close();
  
  Serial.print("Datei gelesen: ");
  Serial.print(path);
  Serial.print(" (");
  Serial.print(content.length());
  Serial.println(" Bytes)");

  return content;
}

// ================= NEUE FUNKTION: TEXT UMBRUCH =================

void drawWrappedText(String text, int x, int y, int maxWidth, int fontSize) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(false); // Manueller Umbruch
  
  int currentY = y;
  String remaining = text;
  
  while (remaining.length() > 0) {
    // Finde die maximale Anzahl von Zeichen die in eine Zeile passen
    String line = "";
    int i = 0;
    
    while (i < remaining.length()) {
      String testLine = line + remaining[i];
      
      // Prüfe ob die Zeile in den Bildschirm passt
      int textWidth = tft.textWidth(testLine, fontSize);
      
      if (textWidth > maxWidth) {
        // Wenn ein Leerzeichen in der Nähe ist, dort umbrechen
        int lastSpace = line.lastIndexOf(' ');
        if (lastSpace > 0 && lastSpace > line.length() / 2) {
          // Umbrechen beim letzten Leerzeichen
          line = line.substring(0, lastSpace);
          remaining = remaining.substring(lastSpace + 1);
          break;
        } else {
          // Kein gutes Leerzeichen gefunden, aber Zeile ist zu lang
          // Nimm die Zeile wie sie ist (ohne das letzte Zeichen)
          remaining = remaining.substring(i);
          break;
        }
      }
      
      line = testLine;
      i++;
    }
    
    // Wenn wir das Ende erreicht haben
    if (i >= remaining.length()) {
      line = remaining;
      remaining = "";
    }
    
    // Zeile zeichnen
    if (currentY < 260) {
      tft.drawString(line, x, currentY, fontSize);
    }
    
    currentY += (fontSize == 4) ? 22 : 18;
    
    // Wenn zu weit unten, warten auf Touch
    if (currentY > 260) {
      tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Tippen zum", 120, 280, 2);
      tft.drawString("weiter", 120, 300, 2);
      
      int tx, ty;
      while (!getTouch(tx, ty)) {
        delay(10);
      }
      
      // Bildschirm leeren für nächste Seite
      tft.fillRect(0, 0, SCREEN_W, 270, TFT_BLACK);
      currentY = 0;
    }
  }
}

// ================= MARKDOWN DISPLAY =================

void drawMDLine(String line, int y, int& lineHeight) {
  tft.setTextDatum(TL_DATUM);
  
  // Entferne führende Leerzeichen für bessere Darstellung
  line.trim();
  
  if (line.startsWith("# ")) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    drawWrappedText(line.substring(2), 5, y, SCREEN_W - 10, 4);
    lineHeight = 22;
  } else if (line.startsWith("## ")) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    drawWrappedText(line.substring(3), 5, y, SCREEN_W - 10, 2);
    lineHeight = 18;
  } else if (line.startsWith("### ")) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    drawWrappedText(line.substring(4), 10, y, SCREEN_W - 15, 2);
    lineHeight = 18;
  } else if (line.startsWith("- ") || line.startsWith("* ")) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawWrappedText("• " + line.substring(2), 10, y, SCREEN_W - 15, 2);
    lineHeight = 18;
  } else if (line.length() == 0) {
    // Leerzeile - kleiner Abstand
    lineHeight = 8;
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawWrappedText(line, 5, y, SCREEN_W - 10, 2);
    lineHeight = 18;
  }
}

void showMarkdown(String path) {
  if (!sdReady) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("SD-Karte nicht", 120, 140, 2);
    tft.drawString("verfuegbar!", 120, 170, 2);
    delay(2000);
    drawMenu();
    return;
  }

  String content = readFile(path);
  
  if (content == "Datei nicht gefunden" || content == "SD-Karte nicht verfügbar") {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Datei nicht", 120, 140, 2);
    tft.drawString("gefunden!", 120, 170, 2);
    delay(2000);
    drawMenu();
    return;
  }

  tft.fillScreen(TFT_BLACK);
  
  int y = 0;
  String line = "";
  
  // Durch den gesamten Text gehen und Zeilen extrahieren
  for (int i = 0; i < content.length(); i++) {
    char c = content[i];
    
    if (c == '\n') {
      // Zeile zeichnen
      if (y < 260) {
        int lineHeight = 0;
        drawMDLine(line, y, lineHeight);
        y += lineHeight;
      }
      line = "";
      
      // Wenn zu viele Zeilen, pausieren
      if (y > 260) {
        tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Tippen zum", 120, 280, 2);
        tft.drawString("weiter", 120, 300, 2);
        
        // Warten auf Touch
        int tx, ty;
        while (!getTouch(tx, ty)) {
          delay(10);
        }
        
        // Bildschirm leeren für nächste Seite
        tft.fillRect(0, 0, SCREEN_W, 270, TFT_BLACK);
        y = 0;
      }
    } else {
      line += c;
    }
  }
  
  // Letzte Zeile zeichnen (falls vorhanden)
  if (line.length() > 0 && y < 260) {
    int lineHeight = 0;
    drawMDLine(line, y, lineHeight);
    y += lineHeight;
  }
  
  // Zurück-Button am Ende
  tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Tippen zum", 120, 280, 2);
  tft.drawString("zurueck", 120, 300, 2);
  
  // Warten auf Touch zum Zurückkehren
  int tx, ty;
  while (!getTouch(tx, ty)) {
    delay(10);
  }
  
  showFileSelection(); // Zurück zur Dateiauswahl
}

// ================= WIFI HELPER =================

void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
      delay(500); tries++;
    }
  }
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

  // Nur WiFi und Zeit-Sync wenn aktiviert
  if (useWiFiTime) {
    connectWiFi();
    initTime();
  } else {
    // Kurze Meldung dass WiFi/Zeit deaktiviert ist
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi & Zeit", 120, 140, 2);
    tft.drawString("deaktiviert", 120, 170, 2);
    tft.drawString("Starte ohne", 120, 200, 2);
    tft.drawString("Netzwerk...", 120, 230, 2);
    delay(2000);
  }

  drawMenu();
}

// ================= LOOP =================

void loop() {
  drawClock();

  int tx;
  int ty;

  if (getTouch(tx, ty)) {
    if (isButtonPressed(tx, ty, 10, 12, 108, 66)) {
      pressAnim(10, 12, 108, 66, "Calc"); calculator();
    } else if (isButtonPressed(tx, ty, 122, 12, 108, 66)) {
      pressAnim(122, 12, 108, 66, "Draw"); drawing();
    } else if (isButtonPressed(tx, ty, 10, 84, 108, 66)) {
      pressAnim(10, 84, 108, 66, "Notes"); notesApp();
    } else if (isButtonPressed(tx, ty, 122, 84, 108, 66)) {
      pressAnim(122, 84, 108, 66, "Chat"); chatApp();
    } else if (isButtonPressed(tx, ty, 10, 156, 108, 66)) {
      pressAnim(10, 156, 108, 66, "Read"); showFileSelection();
    } else if (isButtonPressed(tx, ty, 122, 156, 108, 66)) {
      pressAnim(122, 156, 108, 66, "Settings"); settingsApp();
    } else if (isButtonPressed(tx, ty, 30, 228, 180, 38)) {
      pressAnim(30, 228, 180, 38, "WebUntis"); webuntisApp();
    }
  }
}

// ================= CALCULATOR FUNCTION =================
// ================= CALCULATOR FUNCTION =================

void calculator() {
  // Lokale Variablen für den Rechner
  String display = "0";
  String input = "";
  float result = 0;
  char lastOperator = ' ';
  bool newNumber = true;
  bool error = false;
  
  // Tasten-Layout Definition
  struct CalcButton {
    String label;
    int x, y, w, h;
    uint16_t color;
  };
  
  // Alle Tasten definieren
  CalcButton buttons[] = {
    // Erste Reihe
    {"C", 5, 70, 50, 40, TFT_RED},
    {"±", 60, 70, 50, 40, TFT_DARKGREY},
    {"√", 115, 70, 50, 40, TFT_DARKGREY},
    {"/", 170, 70, 55, 40, TFT_ORANGE},
    
    // Zweite Reihe
    {"7", 5, 115, 50, 40, TFT_DARKGREY},
    {"8", 60, 115, 50, 40, TFT_DARKGREY},
    {"9", 115, 115, 50, 40, TFT_DARKGREY},
    {"*", 170, 115, 55, 40, TFT_ORANGE},
    
    // Dritte Reihe
    {"4", 5, 160, 50, 40, TFT_DARKGREY},
    {"5", 60, 160, 50, 40, TFT_DARKGREY},
    {"6", 115, 160, 50, 40, TFT_DARKGREY},
    {"-", 170, 160, 55, 40, TFT_ORANGE},
    
    // Vierte Reihe
    {"1", 5, 205, 50, 40, TFT_DARKGREY},
    {"2", 60, 205, 50, 40, TFT_DARKGREY},
    {"3", 115, 205, 50, 40, TFT_DARKGREY},
    {"+", 170, 205, 55, 40, TFT_ORANGE},
    
    // Fünfte Reihe
    {"x²", 5, 250, 50, 40, TFT_DARKGREY},
    {"0", 60, 250, 50, 40, TFT_DARKGREY},
    {".", 115, 250, 50, 40, TFT_DARKGREY},
    {"=", 170, 250, 55, 40, TFT_GREEN},
    
    // Untere Reihe
    {"^", 5, 295, 50, 30, TFT_PURPLE},
    {"1/x", 60, 295, 105, 30, TFT_DARKGREY},
    {"←", 170, 295, 55, 30, TFT_DARKGREY},
    
    // Zurück
    {"Zurueck", 5, 330, 230, 25, TFT_RED}
  };
  
  int numButtons = sizeof(buttons) / sizeof(buttons[0]);
  
  // Funktion zum Zeichnen des Displays
  auto drawDisplay = [&]() {
    tft.fillRect(5, 5, 230, 60, TFT_DARKGREY);
    tft.drawRect(5, 5, 230, 60, TFT_WHITE);
    tft.setTextDatum(TR_DATUM);
    tft.setTextSize(2);
    
    if (error) {
      tft.setTextColor(TFT_RED, TFT_DARKGREY);
      tft.drawString("ERROR", 230, 10, 4);
    } else {
      String displayText = display;
      if (displayText.length() > 15) {
        displayText = displayText.substring(0, 15);
      }
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.drawString(displayText, 230, 10, 4);
      
      if (input.length() > 0) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
        tft.setTextSize(1);
        tft.drawString(input, 230, 45, 2);
      }
    }
  };
  
  // Funktion zum Zeichnen aller Tasten
  auto drawButtons = [&]() {
    for (int i = 0; i < numButtons; i++) {
      tft.fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, buttons[i].color);
      tft.drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, TFT_WHITE);
      tft.setTextColor(TFT_WHITE, buttons[i].color);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(buttons[i].label, buttons[i].x + buttons[i].w/2, buttons[i].y + buttons[i].h/2, 2);
    }
  };
  
  // Funktion für Tastendruck
  auto handleInput = [&](String value) {
    if (error) {
      error = false;
      display = "0";
      input = "";
      result = 0;
      lastOperator = ' ';
      newNumber = true;
    }
    
    if (value == "C") {
      display = "0";
      input = "";
      result = 0;
      lastOperator = ' ';
      newNumber = true;
      error = false;
    }
    else if (value == "±") {
      if (display != "0") {
        if (display.startsWith("-")) {
          display = display.substring(1);
        } else {
          display = "-" + display;
        }
      }
    }
    else if (value == "√") {
      float num = display.toFloat();
      if (num >= 0) {
        display = String(sqrt(num), 6);
        result = display.toFloat();
        newNumber = true;
      } else {
        error = true;
      }
    }
    else if (value == "x²") {
      float num = display.toFloat();
      display = String(num * num, 6);
      result = display.toFloat();
      newNumber = true;
    }
    else if (value == "1/x") {
      float num = display.toFloat();
      if (num != 0) {
        display = String(1.0 / num, 6);
        result = display.toFloat();
        newNumber = true;
      } else {
        error = true;
      }
    }
    else if (value == "=") {
      float currentNum = display.toFloat();
      if (lastOperator != ' ') {
        switch (lastOperator) {
          case '+': result += currentNum; break;
          case '-': result -= currentNum; break;
          case '*': result *= currentNum; break;
          case '/': 
            if (currentNum != 0) result /= currentNum;
            else { error = true; return; }
            break;
          case '^': result = pow(result, currentNum); break;
        }
        display = String(result, 6);
        input = "";
        lastOperator = ' ';
        newNumber = true;
      }
    }
    else if (value == "+" || value == "-" || value == "*" || value == "/" || value == "^") {
      if (lastOperator != ' ' && !newNumber) {
        float currentNum = display.toFloat();
        switch (lastOperator) {
          case '+': result += currentNum; break;
          case '-': result -= currentNum; break;
          case '*': result *= currentNum; break;
          case '/': 
            if (currentNum != 0) result /= currentNum;
            else { error = true; return; }
            break;
          case '^': result = pow(result, currentNum); break;
        }
        display = String(result, 6);
      } else {
        result = display.toFloat();
      }
      lastOperator = value[0];
      input = display + " " + value;
      newNumber = true;
    }
    else if (value == "←") {
      if (display.length() > 1) {
        display = display.substring(0, display.length() - 1);
      } else {
        display = "0";
        newNumber = true;
      }
    }
    else {
      // Zahlen
      if (newNumber) {
        if (value == ".") {
          display = "0.";
        } else {
          display = value;
        }
        newNumber = false;
      } else {
        if (value == ".") {
          // FIX: Verwende indexOf() statt contains()
          if (display.indexOf('.') == -1) {
            display += value;
          }
        } else {
          if (display.length() < 15) {
            if (display == "0" && value != ".") {
              display = value;
            } else {
              display += value;
            }
          }
        }
      }
    }
  };
  
  // Hauptschleife des Rechners
  while (true) {
    // Bildschirm leeren und alles zeichnen
    tft.fillScreen(TFT_BLACK);
    drawDisplay();
    drawButtons();
    
    // Auf Touch warten
    int tx, ty;
    while (!getTouch(tx, ty)) {
      delay(10);
    }
    
    // Prüfen welche Taste gedrückt wurde
    bool buttonPressed = false;
    for (int i = 0; i < numButtons; i++) {
      if (isButtonPressed(tx, ty, buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h)) {
        if (buttons[i].label == "Zurueck") {
          while (getTouch(tx, ty)) { delay(10); }
          drawMenu();
          return;
        }
        handleInput(buttons[i].label);
        buttonPressed = true;
        break;
      }
    }
    
    // Display nach Tastendruck aktualisieren
    if (buttonPressed) {
      drawDisplay();
      delay(200);  // Entprellung
    }
  }
}

// ================= DRAWING =================

void drawing() {
  { int _tx, _ty; while (getTouch(_tx, _ty)) { delay(10); } }

  tft.fillScreen(TFT_WHITE);

  const uint16_t palette[8] = {
    TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE,
    TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_WHITE
  };

  uint16_t color    = TFT_BLACK;
  uint8_t  penSize  = 3;
  bool     eraserOn = false;
  int16_t  lastX    = -1;
  int16_t  lastY    = -1;
  bool     isDown   = false;

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
    int bw = SCREEN_W / 5;
    int by = 294;
    int bh = 26;
    const char* labels[5] = {"RAD", "NEU", "SPEICH", "SZ+", "SZ-"};
    for (int i = 0; i < 5; i++) {
      int bx = i * bw;
      tft.fillRect(bx, by, bw - 1, bh, i == 0 && eraserOn ? TFT_RED : 0x0841);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      tft.setCursor(bx + 3, by + (bh - 8) / 2);
      tft.print(labels[i]);
      if (i == 3) {
        tft.fillCircle(bx + bw - 10, by + bh / 2, penSize, eraserOn ? TFT_WHITE : color);
      }
    }
    tft.drawFastHLine(0, 270, SCREEN_W, TFT_WHITE);
    tft.drawFastHLine(0, 294, SCREEN_W, TFT_DARKGREY);
  };

  auto clearCanvas = [&]() {
    tft.fillRect(0, 30, SCREEN_W, 240, TFT_WHITE);
  };

  auto saveDrawing = [&]() {
    if (!SPIFFS.begin(true)) return;
    String path;
    for (int i = 0; i < 1000; i++) {
      path = "/zeichnung_" + String(i) + ".bmp";
      if (!SPIFFS.exists(path)) break;
    }
    size_t nPixels = SCREEN_W * SCREEN_H;
    uint16_t* fb = new uint16_t[nPixels];
    tft.readRect(0, 0, SCREEN_W, SCREEN_H, fb);
    fs::File f = SPIFFS.open(path, FILE_WRITE);
    if (!f) { delete[] fb; return; }
    uint32_t rowSize = ((SCREEN_W * 3) + 3) & ~3U;
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    *(uint32_t*)(hdr + 2)  = 54 + rowSize * SCREEN_H;
    *(uint32_t*)(hdr + 10) = 54;
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
    delete[] row; delete[] fb; f.close();
    tft.fillRect(0, 0, SCREEN_W, 18, TFT_GREEN);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextSize(1);
    tft.setCursor(4, 4);
    tft.print("Gespeichert " + path);
    delay(1000);
    tft.fillRect(0, 0, SCREEN_W, 18, TFT_WHITE);
  };

  drawToolbar();
  clearCanvas();

  while (true) {
    int x, y;
    if (!getTouch(x, y)) {
      if (isDown) { isDown = false; lastX = lastY = -1; }
      delay(10);
      continue;
    }

    if (y >= 270) {
      if (y < 294) {
        int idx = x / (SCREEN_W / 8);
        if (idx >= 0 && idx < 8) { color = palette[idx]; eraserOn = false; drawToolbar(); }
      } else {
        int ti = x / (SCREEN_W / 5);
        switch (ti) {
          case 0: eraserOn = !eraserOn; drawToolbar(); break;
          case 1: clearCanvas(); break;
          case 2: saveDrawing(); break;
          case 3: if (penSize < 20) penSize += 2; drawToolbar(); break;
          case 4: if (penSize > 1) penSize -= 2; drawToolbar(); break;
        }
      }
      isDown = false; lastX = lastY = -1;
      delay(10);
      continue;
    }

    if (y < 30) {
      // Home/Back: touch top-left to exit
      if (x < 40 && y < 28) {
        while (getTouch(x, y)) { delay(10); }
        drawMenu(); return;
      }
      delay(10);
      continue;
    }

    uint16_t dc = eraserOn ? TFT_WHITE : color;
    if (!isDown || lastX < 0) {
      tft.fillCircle(x, y, penSize, dc);
    } else {
      int dx = x - lastX, dy = y - lastY;
      int steps = max(abs(dx), abs(dy));
      for (int i = 0; i <= steps; i++)
        tft.fillCircle(lastX + (dx * i) / steps, lastY + (dy * i) / steps, penSize, dc);
    }
    lastX = x; lastY = y; isDown = true;
    delay(10);
  }
}

// ================= NOTES APP =================

void notesApp() {
  { int _tx, _ty; while (getTouch(_tx, _ty)) { delay(10); } }

  struct NoteFile {
    String name;
    String path;
  };

  auto refreshNotes = [](std::vector<NoteFile>& list) {
    list.clear();
    if (!SD.begin(SD_CS)) return;
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

  auto drawNoteList = [&](std::vector<NoteFile>& list, int& sel, int& offset) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 30, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("NOTIZEN", 120, 8, 2);

    tft.fillRoundRect(5, 3, 40, 24, 4, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("<", 25, 15, 1);

    tft.fillRoundRect(195, 3, 40, 24, 4, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.drawString("+", 215, 15, 1);

    int y = 38;
    int shown = 0;
    for (int i = offset; i < (int)list.size() && shown < 7; i++) {
      tft.fillRoundRect(5, y, 230, 32, 4, sel == i ? TFT_BLUE : 0x0841);
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(12, y + 8);
      tft.print(list[i].name);
      tft.fillRoundRect(190, y + 4, 40, 24, 3, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("X", 210, y + 16, 1);
      y += 36;
      shown++;
    }
    if (offset > 0) {
      tft.fillTriangle(120, 290, 110, 280, 130, 280, TFT_LIGHTGREY);
    }
    if (offset + 7 < (int)list.size()) {
      tft.fillTriangle(120, 310, 110, 320, 130, 320, TFT_LIGHTGREY);
    }
  };

  std::vector<NoteFile> notes;
  int selected = -1, scrollOff = 0;
  refreshNotes(notes);
  drawNoteList(notes, selected, scrollOff);

  while (true) {
    int tx, ty;
    if (!getTouch(tx, ty)) { delay(10); continue; }

    if (ty < 30) {
      if (tx < 50) { drawMenu(); return; }
      if (tx > 195) {
        // New note
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Name:", 120, 100, 2);
        tft.drawRect(20, 130, 200, 30, TFT_WHITE);

        String newName = "";
        while (true) {
          int kx, ky;
          if (!getTouch(kx, ky)) { delay(10); continue; }
          if (ky > 130 && ky < 160 && kx > 180 && kx < 220) {
            if (newName.length() > 0) {
              String path = "/notes/" + newName + ".txt";
              File nf = SD.open(path, FILE_WRITE);
              if (nf) nf.close();
            }
            break;
          }
          if (ky > 130 && ky < 160 && kx > 20 && kx < 180) {
            char c = (kx - 20) / 20;
            if (c >= 0 && c < 26) newName += (char)('A' + c);
            tft.fillRect(22, 132, 196, 26, TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(30, 138);
            tft.print(newName);
          }
        }
        refreshNotes(notes);
        drawNoteList(notes, selected, scrollOff);
      }
      continue;
    }

    if (ty > 280 && ty < 320) {
      if (tx > 110 && tx < 130) {
        if (ty < 295 && scrollOff > 0) scrollOff--;
        else if (ty >= 295 && scrollOff + 7 < (int)notes.size()) scrollOff++;
        drawNoteList(notes, selected, scrollOff);
      }
      continue;
    }

    int idx = scrollOff + (ty - 38) / 36;
    if (idx >= 0 && idx < (int)notes.size() && ty < 38 + 7 * 36) {
      if (tx > 190) {
        SD.remove(notes[idx].path);
        refreshNotes(notes);
        drawNoteList(notes, selected, scrollOff);
      } else {
        selected = idx;
        // Open & edit
        String content = "";
        File rf = SD.open(notes[selected].path, FILE_READ);
        if (rf) {
          while (rf.available()) content += (char)rf.read();
          rf.close();
        }

        bool editing = true;
        String text = content;
        int kbMode = 0;
        const char* kRows[3] = {"QWERTZUIOP", "ASDFGHJKL", "YXCVBNM"};
        const char* kLow[3] = {"qwertzuiop", "asdfghjkl", "yxcvbnm"};
        const char* kNum[3] = {"1234567890", "-_.,!?;:", "+*\"' /()"};

        while (editing) {
          tft.fillScreen(TFT_BLACK);
          tft.fillRect(0, 0, 240, 24, TFT_DARKGREY);
          tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
          tft.setTextSize(1);
          tft.setCursor(4, 6);
          tft.print(notes[selected].name);

          tft.fillRoundRect(180, 2, 55, 20, 3, TFT_GREEN);
          tft.setTextColor(TFT_WHITE, TFT_GREEN);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("SAVE", 207, 12, 1);

          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.setTextDatum(TL_DATUM);
          int lineY = 28;
          String disp = text;
          if (disp.length() > 130) disp = disp.substring(disp.length() - 130);
          int idx2 = 0;
          while (idx2 < (int)disp.length() && lineY < 160) {
            String line;
            while (idx2 < (int)disp.length() && disp[idx2] != '\n' && line.length() < 34) line += disp[idx2++];
            if (idx2 < (int)disp.length() && disp[idx2] == '\n') idx2++;
            tft.setCursor(4, lineY);
            tft.print(line);
            lineY += 12;
          }
          if ((millis() / 500) % 2 == 0) tft.fillRect(4 + (text.length() % 34) * 6, lineY - 12, 6, 10, TFT_WHITE);

          // Keyboard
          const char** rows = (kbMode == 2) ? kNum : (kbMode == 1) ? kRows : kLow;
          int ky2 = 165;
          tft.fillRect(0, ky2, 240, 155, TFT_BLACK);
          for (int r = 0; r < 3; r++) {
            int len = strlen(rows[r]);
            int kw = 240 / max(len, 10);
            int xo = (r == 1) ? kw / 2 : 0;
            for (int i = 0; i < len; i++) {
              int kx2 = xo + i * kw;
              tft.fillRoundRect(kx2, ky2, kw - 2, 28, 3, 0x0841);
              tft.setTextColor(TFT_WHITE, 0x0841);
              tft.setTextDatum(MC_DATUM);
              tft.drawString(String(rows[r][i]), kx2 + kw / 2, ky2 + 14, 1);
            }
            ky2 += 30;
          }
          tft.fillRoundRect(0, ky2, 50, 28, 3, kbMode == 2 ? TFT_BLUE : 0x0841);
          tft.drawCentreString(kbMode == 2 ? "ABC" : "123", 25, ky2 + 14, 1);
          tft.fillRoundRect(54, ky2, 40, 28, 3, kbMode == 1 ? TFT_BLUE : 0x0841);
          tft.drawCentreString("^", 74, ky2 + 14, 1);
          tft.fillRoundRect(98, ky2, 80, 28, 3, 0x0841);
          tft.drawCentreString("SPACE", 138, ky2 + 14, 1);
          tft.fillRoundRect(182, ky2, 28, 28, 3, 0x0841);
          tft.drawCentreString("<", 196, ky2 + 14, 1);
          tft.fillRoundRect(214, ky2, 26, 28, 3, TFT_GREEN);
          tft.drawCentreString("OK", 227, ky2 + 14, 1);

          while (true) {
            int kx, ky;
            if (!getTouch(kx, ky)) { delay(10); continue; }
            if (ky < 24) {
              if (kx > 175) {
                File sf = SD.open(notes[selected].path, FILE_WRITE);
                if (sf) { sf.print(text); sf.close(); }
              }
              editing = false;
              break;
            }
            if (ky < 165) { delay(10); continue; }

            int rowY2 = 165;
            bool handled = false;
            for (int r = 0; r < 3 && !handled; r++) {
              int len = strlen(rows[r]);
              int kw = 240 / max(len, 10);
              int xo = (r == 1) ? kw / 2 : 0;
              if (ky > rowY2 && ky < rowY2 + 28) {
                int ki = (kx - xo) / kw;
                if (ki >= 0 && ki < len) {
                  text += rows[r][ki];
                  if (kbMode == 1) kbMode = 0;
                }
                handled = true;
              }
              rowY2 += 30;
            }
            if (!handled && ky >= rowY2) {
              if (kx < 50) kbMode = (kbMode == 2) ? 0 : 2;
              else if (kx >= 54 && kx < 94) kbMode = (kbMode == 1) ? 0 : 1;
              else if (kx >= 98 && kx < 178) text += " ";
              else if (kx >= 182 && kx < 210) { if (text.length() > 0) text.remove(text.length() - 1); }
              else if (kx >= 214) text += "\n";
            }
            break;
          }
          delay(100);
        }
        refreshNotes(notes);
        drawNoteList(notes, selected, scrollOff);
      }
    }
    delay(50);
  }
}

// ================= CHAT APP =================

void chatApp() {
  { int _tx, _ty; while (getTouch(_tx, _ty)) { delay(10); } }

  ensureWiFi();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Chat (Nostr)", 120, 20, 2);
  tft.drawString("Verbinde...", 120, 80, 1);

  String pubkey = "", privkey = "";
  String userId = "CYD-User-" + String(random(1000, 9999));
  String groupId = "";

  // Try login via API
  HTTPClient http;
  http.setTimeout(5000);
  String apiBase = "http://149.102.157.124:3001";

  // Register user
  http.begin(apiBase + "/api/register");
  http.addHeader("Content-Type", "application/json");
  String regBody = "{\"username\":\"" + userId + "\"}";
  int code = http.POST(regBody);
  if (code == 200 || code == 201) {
    String resp = http.getString();
    int pk = resp.indexOf("\"pubkey\":\"");
    if (pk > 0) {
      pubkey = resp.substring(pk + 10);
      pubkey = pubkey.substring(0, pubkey.indexOf("\""));
    }
  }
  http.end();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Gruppen...", 120, 20, 2);

  // List groups
  struct Grp { String id, name; };
  std::vector<Grp> groups;
  if (pubkey != "") {
    http.begin(apiBase + "/api/groups?pubkey=" + pubkey);
    int gc = http.GET();
    if (gc == 200) {
      String gResp = http.getString();
      int pos = 0;
      while (true) {
        int si = gResp.indexOf("\"id\":\"", pos);
        if (si < 0) break;
        String gid = gResp.substring(si + 6);
        gid = gid.substring(0, gid.indexOf("\""));
        int sn = gResp.indexOf("\"name\":\"", si);
        String gn = "";
        if (sn > 0) {
          gn = gResp.substring(sn + 8);
          gn = gn.substring(0, gn.indexOf("\""));
        }
        groups.push_back({gid, gn});
        pos = si + 1;
      }
    }
    http.end();
  }

  // If no groups, create one
  if (groups.empty() && pubkey != "") {
    http.begin(apiBase + "/api/groups");
    http.addHeader("Content-Type", "application/json");
    String gBody = "{\"name\":\"CYD-Chat\",\"ownerPubkey\":\"" + pubkey + "\"}";
    if (http.POST(gBody) == 200 || http.POST(gBody) == 201) {
      String gResp = http.getString();
      int si = gResp.indexOf("\"id\":\"");
      if (si > 0) {
        groupId = gResp.substring(si + 6);
        groupId = groupId.substring(0, groupId.indexOf("\""));
      }
    }
    http.end();
  } else if (!groups.empty()) {
    groupId = groups[0].id;
  }

  if (groupId == "") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine Verbindung", 120, 80, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Tippen zum zurueck", 120, 120, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) { delay(10); }
    drawMenu(); return;
  }

  // Chat UI
  std::vector<String> messages;
  String chatInput = "";
  int kbMode = 0;
  const char* kRows[3] = {"QWERTZUIOP", "ASDFGHJKL", "YXCVBNM"};
  const char* kLow[3] = {"qwertzuiop", "asdfghjkl", "yxcvbnm"};
  const char* kNum[3] = {"1234567890", "-_.,!?;:", "+*\"' /()"};
  unsigned long lastPoll = 0;

  auto fetchMessages = [&]() {
    http.begin(apiBase + "/api/messages?groupId=" + groupId);
    if (http.GET() == 200) {
      String mResp = http.getString();
      messages.clear();
      int pos = 0;
      while (true) {
        int sc = mResp.indexOf("\"content\":\"", pos);
        if (sc < 0) break;
        String ct = mResp.substring(sc + 11);
        ct = ct.substring(0, ct.indexOf("\""));
        messages.push_back(ct);
        pos = sc + 1;
      }
    }
    http.end();
  };

  fetchMessages();

  while (true) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 24, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("CHAT", 120, 12, 1);
    tft.fillRoundRect(200, 2, 35, 20, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("X", 217, 12, 1);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    int my = 28;
    int start = max(0, (int)messages.size() - 8);
    for (int i = start; i < (int)messages.size() && my < 155; i++) {
      tft.setCursor(4, my);
      String m = messages[i];
      if (m.length() > 32) m = m.substring(0, 32);
      tft.print(m);
      my += 14;
    }

    // Keyboard
    const char** rows = (kbMode == 2) ? kNum : (kbMode == 1) ? kRows : kLow;
    int ky = 165;
    tft.fillRect(0, ky, 240, 155, TFT_BLACK);
    for (int r = 0; r < 3; r++) {
      int len = strlen(rows[r]);
      int kw = 240 / max(len, 10);
      int xo = (r == 1) ? kw / 2 : 0;
      for (int i = 0; i < len; i++) {
        int kx = xo + i * kw;
        tft.fillRoundRect(kx, ky, kw - 2, 28, 3, 0x0841);
        tft.setTextColor(TFT_WHITE, 0x0841);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(rows[r][i]), kx + kw / 2, ky + 14, 1);
      }
      ky += 30;
    }
    tft.fillRoundRect(0, ky, 50, 28, 3, kbMode == 2 ? TFT_BLUE : 0x0841);
    tft.drawCentreString(kbMode == 2 ? "ABC" : "123", 25, ky + 14, 1);
    tft.fillRoundRect(54, ky, 40, 28, 3, kbMode == 1 ? TFT_BLUE : 0x0841);
    tft.drawCentreString("^", 74, ky + 14, 1);
    tft.fillRoundRect(98, ky, 80, 28, 3, 0x0841);
    tft.drawCentreString("SPACE", 138, ky + 14, 1);
    tft.fillRoundRect(182, ky, 28, 28, 3, 0x0841);
    tft.drawCentreString("<", 196, ky + 14, 1);
    tft.fillRoundRect(214, ky, 26, 28, 3, TFT_GREEN);
    tft.drawCentreString("OK", 227, ky + 14, 1);

    // Input line
    tft.fillRect(0, 156, 240, 9, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(4, 157);
    tft.print(">" + chatInput);

    int tx, ty;
    if (!getTouch(tx, ty)) {
      if (millis() - lastPoll > 5000) {
        fetchMessages();
        lastPoll = millis();
      }
      delay(10);
      continue;
    }

    if (ty < 24 && tx > 195) { drawMenu(); return; }

    if (ty >= 165) {
      int rowY = 165;
      bool handled = false;
      for (int r = 0; r < 3 && !handled; r++) {
        int len = strlen(rows[r]);
        int kw = 240 / max(len, 10);
        int xo = (r == 1) ? kw / 2 : 0;
        if (ty > rowY && ty < rowY + 28) {
          int ki = (tx - xo) / kw;
          if (ki >= 0 && ki < len) { chatInput += rows[r][ki]; if (kbMode == 1) kbMode = 0; }
          handled = true;
        }
        rowY += 30;
      }
      if (!handled && ty >= rowY) {
        if (tx < 50) kbMode = (kbMode == 2) ? 0 : 2;
        else if (tx >= 54 && tx < 94) kbMode = (kbMode == 1) ? 0 : 1;
        else if (tx >= 98 && tx < 178) chatInput += " ";
        else if (tx >= 182 && tx < 210) { if (chatInput.length() > 0) chatInput.remove(chatInput.length() - 1); }
        else if (tx >= 214) {
          if (chatInput.length() > 0) {
            http.begin(apiBase + "/api/messages");
            http.addHeader("Content-Type", "application/json");
            String msgBody = "{\"groupId\":\"" + groupId + "\",\"pubkey\":\"" + pubkey + "\",\"content\":\"" + chatInput + "\"}";
            http.POST(msgBody);
            http.end();
            chatInput = "";
            fetchMessages();
          }
        }
      }
    }
    delay(50);
  }
}

// ================= WEBUNTIS APP =================

void webuntisApp() {
  { int _tx, _ty; while (getTouch(_tx, _ty)) { delay(10); } }

  ensureWiFi();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WebUntis", 120, 15, 2);
  tft.drawString("Stundenplan wird", 120, 100, 1);
  tft.drawString("geladen...", 120, 120, 1);

  // Sync time via NTP
  configTime(3600, 3600, "pool.ntp.org", "time.google.com");
  struct tm ti;
  int timeTries = 0;
  while (!getLocalTime(&ti) && timeTries < 20) {
    delay(500); timeTries++;
  }
  char dateStr[9];
  sprintf(dateStr, "%04d%02d%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);

  WiFiClientSecure client;
  client.setInsecure();

  String host = "coppi-gymnasium.webuntis.com";
  String rpcUrl = "https://" + host + "/WebUntis/jsonrpc.do";

  HTTPClient http;
  http.setTimeout(10000);

  struct Period {
    int start, end;
    String subject, teacher, room;
  };
  std::vector<Period> periods;

  // Helper: POST JSON-RPC and return response body
  auto rpcCall = [&](const String& jsonBody) -> String {
    http.begin(client, rpcUrl);
    http.addHeader("Content-Type", "application/json");
    http.POST(jsonBody);
    String resp = http.getString();
    http.end();
    return resp;
  };

  // Login
  String loginResp = rpcCall("{\"id\":\"1\",\"method\":\"authenticate\",\"params\":{\"user\":\"9b\",\"password\":\"BietL2024!\"},\"jsonrpc\":\"2.0\"}");
  String sessionId = "";
  int si = loginResp.indexOf("\"sessionId\":\"");
  if (si > 0) {
    sessionId = loginResp.substring(si + 13);
    sessionId = sessionId.substring(0, sessionId.indexOf("\""));
  }

  if (sessionId != "") {
    // Find class ID for "9b"
    int classId = 1;
    String klassenResp = rpcCall("{\"id\":\"2\",\"method\":\"getKlassen\",\"params\":{},\"jsonrpc\":\"2.0\"}");
    int ks = 0;
    while (true) {
      int ki = klassenResp.indexOf("\"name\":\"", ks);
      if (ki < 0) break;
      String kn = klassenResp.substring(ki + 8);
      kn = kn.substring(0, kn.indexOf("\""));
      if (kn == "9b") {
        int idi = klassenResp.lastIndexOf("\"id\":", ki);
        if (idi > 0) {
          classId = klassenResp.substring(idi + 5).toInt();
        }
        break;
      }
      ks = ki + 1;
    }

    // Get timetable
    String tj = "{\"id\":\"3\",\"method\":\"getTimetable\",\"params\":{\"options\":{\"element\":{\"id\":" + String(classId) + ",\"type\":1},\"startDate\":\"" + dateStr + "\",\"endDate\":\"" + dateStr + "\"}},\"jsonrpc\":\"2.0\"}";

    http.begin(client, rpcUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Cookie", "JSESSIONID=" + sessionId);
    String resp = "";
    if (http.POST(tj) == 200) resp = http.getString();
    http.end();

    int pos = 0;
    while (true) {
      si = resp.indexOf("\"startTime\":", pos);
      if (si < 0) break;
      int st = resp.substring(si + 12).toInt();
      int ei = resp.indexOf("\"endTime\":", si);
      int en = 0;
      if (ei > 0) en = resp.substring(ei + 10).toInt();

      String subj = "", teach = "", room = "";
      int ss = resp.indexOf("\"subject\"", si);
      if (ss > 0) {
        int sn = resp.indexOf("\"name\":\"", ss);
        if (sn > 0) {
          subj = resp.substring(sn + 8);
          subj = subj.substring(0, subj.indexOf("\""));
        }
      }
      int ts = resp.indexOf("\"teacher\"", si);
      if (ts > 0) {
        int tn = resp.indexOf("\"name\":\"", ts);
        if (tn > 0) {
          teach = resp.substring(tn + 8);
          teach = teach.substring(0, teach.indexOf("\""));
        }
      }
      int rs = resp.indexOf("\"room\"", si);
      if (rs > 0) {
        int rn = resp.indexOf("\"name\":\"", rs);
        if (rn > 0) {
          room = resp.substring(rn + 8);
          room = room.substring(0, room.indexOf("\""));
        }
      }
      periods.push_back({st, en, subj, teach, room});
      pos = si + 1;
    }
  }

  // Display timetable
  tft.fillScreen(TFT_BLACK);

  // Header with WebUntis style
  tft.fillRect(0, 0, 240, 28, 0x1D7C);
  tft.setTextColor(TFT_WHITE, 0x1D7C);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);

  const char* days[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
  char hdr[32];
  sprintf(hdr, "Stundenplan  %s", days[ti.tm_wday]);
  tft.drawString(hdr, 120, 14, 1);

  tft.fillRoundRect(200, 3, 35, 22, 3, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("X", 217, 14, 1);

  if (periods.empty()) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Keine Stunden heute", 120, 150, 2);
    int tx, ty;
    while (!getTouch(tx, ty)) { delay(10); }
    drawMenu(); return;
  }

  int yPos = 32;
  int scroll = 0;
  int maxVis = min((int)periods.size(), 8);

  while (true) {
    tft.fillRect(0, 28, 240, 292, TFT_BLACK);

    for (int i = scroll; i < min(scroll + 8, (int)periods.size()); i++) {
      int y = 32 + (i - scroll) * 34;
      int h = 32;

      tft.fillRoundRect(2, y, 236, h, 3, 0x0841);
      tft.drawRoundRect(2, y, 236, h, 3, TFT_DARKGREY);

      // Time
      char tb[16];
      int sh = periods[i].start / 100;
      int sm = periods[i].start % 100;
      int eh = periods[i].end / 100;
      int em = periods[i].end % 100;
      sprintf(tb, "%02d:%02d-%02d:%02d", sh, sm, eh, em);
      tft.setTextColor(0x1D7C, 0x0841);
      tft.setTextDatum(MC_DATUM);
      tft.setTextSize(1);
      tft.drawString(tb, 42, y + 8, 1);

      // Subject
      tft.setTextColor(TFT_WHITE, 0x0841);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(4, y + 3);
      tft.setTextSize(1);
      tft.print(periods[i].subject);

      // Teacher
      tft.setTextColor(TFT_LIGHTGREY, 0x0841);
      tft.setCursor(4, y + 18);
      tft.print(periods[i].teacher);

      // Room
      tft.setTextColor(TFT_CYAN, 0x0841);
      tft.setTextDatum(TR_DATUM);
      tft.setCursor(235, y + 18);
      tft.print(periods[i].room);

      // Subject accent bar
      tft.fillRect(0, y, 2, h, 0x1D7C);
    }

    // Scroll indicators
    if (scroll > 0) {
      tft.fillTriangle(120, 290, 110, 280, 130, 280, TFT_LIGHTGREY);
    }
    if (scroll + 8 < (int)periods.size()) {
      tft.fillTriangle(120, 310, 110, 320, 130, 320, TFT_LIGHTGREY);
    }

    int tx, ty;
    if (!getTouch(tx, ty)) { delay(10); continue; }

    if (ty < 28 && tx > 195) { drawMenu(); return; }

    // Scroll
    if (ty > 280) {
      if (tx > 110 && tx < 130) {
        if (ty < 295 && scroll > 0) scroll--;
        else if (ty >= 295 && scroll + 8 < (int)periods.size()) scroll++;
      }
    }
    delay(50);
  }
}

// ================= SETTINGS APP =================

void settingsApp() {
  { int _tx, _ty; while (getTouch(_tx, _ty)) { delay(10); } }

  enum State { STATE_MAIN, STATE_SCAN, STATE_PASSWORD };
  State st = STATE_MAIN;

  String selSSID = "";
  String password = "";
  String statusMsg = WiFi.status() == WL_CONNECTED ? "Verbunden: " + String(WiFi.SSID()) : "Nicht verbunden";
  std::vector<String> networks;
  int scroll = 0, selected = -1;
  int kbMode = 0;
  const char* kRows[3] = {"QWERTZUIOP", "ASDFGHJKL", "YXCVBNM"};
  const char* kLow[3] = {"qwertzuiop", "asdfghjkl", "yxcvbnm"};
  const char* kNum[3] = {"1234567890", "-_.,!?;:", "+*\"' /()"};

  while (true) {
    tft.fillScreen(TFT_BLACK);

    if (st == STATE_MAIN) {
      tft.fillRect(0, 0, 240, 28, CARD_BG);
      tft.setTextColor(0xADB5, CARD_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Einstellungen", 120, 14, 1);
      tft.fillRoundRect(200, 3, 35, 22, 4, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString("X", 217, 14, 1);

      tft.setTextColor(0xADB5, TFT_BLACK);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(10, 40);
      tft.print("WiFi: ");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.println(statusMsg);

      tft.fillRoundRect(20, 75, 200, 36, 6, CARD_BG);
      tft.drawRoundRect(20, 75, 200, 36, 6, CARD_BORDER);
      tft.setTextColor(0xADB5, CARD_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Netzwerke scannen", 120, 93, 1);

    } else if (st == STATE_SCAN) {
      tft.fillRect(0, 0, 240, 28, CARD_BG);
      tft.setTextColor(0xADB5, CARD_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Netzwerke", 120, 14, 1);
      tft.fillRoundRect(200, 3, 35, 22, 4, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString("X", 217, 14, 1);

      int y = 35;
      for (int i = scroll; i < min(scroll + 6, (int)networks.size()); i++) {
        uint16_t bg = (selected == i) ? CARD_HIGHLIGHT : CARD_BG;
        tft.fillRoundRect(4, y, 232, 28, 4, bg);
        tft.drawRoundRect(4, y, 232, 28, 4, CARD_BORDER);
        tft.setTextColor(0xADB5, bg);
        tft.setCursor(10, y + 8);
        tft.print(networks[i]);
        y += 32;
      }
      if (scroll > 0) {
        tft.fillTriangle(120, 290, 110, 280, 130, 280, 0xADB5);
      }
      if (scroll + 6 < (int)networks.size()) {
        tft.fillTriangle(120, 310, 110, 320, 130, 320, 0xADB5);
      }
    } else if (st == STATE_PASSWORD) {
      tft.fillRect(0, 0, 240, 28, CARD_BG);
      tft.setTextColor(0xADB5, CARD_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Passwort", 120, 14, 1);
      tft.fillRoundRect(200, 3, 35, 22, 4, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString("X", 217, 14, 1);

      tft.setTextColor(0xADB5, TFT_BLACK);
      tft.setCursor(4, 34);
      tft.print(selSSID.length() > 20 ? selSSID.substring(0, 20) + ".." : selSSID);

      tft.fillRect(4, 50, 232, 14, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(4, 50);
      String disp = password;
      if (disp.length() > 36) disp = disp.substring(disp.length() - 36);
      tft.print(disp);
      if ((millis() / 400) % 2 == 0) tft.drawRect(4 + disp.length() * 6, 50, 6, 12, TFT_WHITE);

      tft.fillRoundRect(80, 70, 80, 24, 4, TFT_GREEN);
      tft.setTextColor(TFT_WHITE, TFT_GREEN);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Verbinden", 120, 82, 1);

      const char** rows = (kbMode == 2) ? kNum : (kbMode == 1) ? kRows : kLow;
      int ky = 100;
      for (int r = 0; r < 3; r++) {
        int len = strlen(rows[r]);
        int kw = 240 / max(len, 10);
        int xo = (r == 1) ? kw / 2 : 0;
        for (int i = 0; i < len; i++) {
          int kx = xo + i * kw;
          tft.fillRoundRect(kx, ky, kw - 2, 26, 3, CARD_BG);
          tft.setTextColor(0xADB5, CARD_BG);
          tft.setTextDatum(MC_DATUM);
          tft.drawString(String(rows[r][i]), kx + kw / 2, ky + 13, 1);
        }
        ky += 28;
      }
      tft.fillRoundRect(0, ky, 50, 28, 3, kbMode == 2 ? CARD_HIGHLIGHT : CARD_BG);
      tft.drawCentreString(kbMode == 2 ? "ABC" : "123", 25, ky + 14, 1);
      tft.fillRoundRect(54, ky, 40, 28, 3, kbMode == 1 ? CARD_HIGHLIGHT : CARD_BG);
      tft.drawCentreString("^", 74, ky + 14, 1);
      tft.fillRoundRect(98, ky, 80, 28, 3, CARD_BG);
      tft.drawCentreString("SPACE", 138, ky + 14, 1);
      tft.fillRoundRect(182, ky, 28, 28, 3, CARD_BG);
      tft.drawCentreString("<", 196, ky + 14, 1);
      tft.fillRoundRect(214, ky, 26, 28, 3, TFT_GREEN);
      tft.drawCentreString("OK", 227, ky + 14, 1);
    }

    int tx, ty;
    if (!getTouch(tx, ty)) { delay(10); continue; }

    if (ty < 28 && tx > 190) { drawMenu(); return; }

    if (st == STATE_MAIN) {
      if (ty >= 75 && ty <= 111 && tx >= 20 && tx <= 220) {
        tft.fillRoundRect(20, 75, 200, 36, 6, CARD_HIGHLIGHT);
        delay(60);
        networks.clear();
        selSSID = "";
        WiFi.scanDelete();
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
          networks.push_back(WiFi.SSID(i));
        }
        scroll = 0; selected = -1;
        st = STATE_SCAN;
      }
    } else if (st == STATE_SCAN) {
      if (ty > 280) {
        if (tx > 110 && tx < 130) {
          if (ty < 295 && scroll > 0) scroll--;
          else if (ty >= 295 && scroll + 6 < (int)networks.size()) scroll++;
        }
        continue;
      }
      int idx = scroll + (ty - 35) / 32;
      if (idx >= 0 && idx < (int)networks.size() && ty >= 35 && ty < 35 + 6 * 32) {
        selected = idx;
        selSSID = networks[idx];
        password = "";
        kbMode = 0;
        st = STATE_PASSWORD;
      }
    } else if (st == STATE_PASSWORD) {
      if (ty >= 70 && ty <= 94 && tx >= 80 && tx <= 160) {
        if (password.length() > 0) {
          tft.fillRoundRect(80, 70, 80, 24, 4, CARD_BG);
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("Verbinde...", 120, 82, 1);
          WiFi.begin(selSSID.c_str(), password.c_str());
          int tries = 0;
          while (WiFi.status() != WL_CONNECTED && tries < 30) {
            delay(500); tries++;
          }
          if (WiFi.status() == WL_CONNECTED) {
            statusMsg = "Verbunden: " + selSSID;
            st = STATE_MAIN;
          } else {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Fehlgeschlagen", 120, 130, 1);
            delay(1000);
            st = STATE_PASSWORD;
          }
        }
        continue;
      }
      if (ty >= 100) {
        const char** rows = (kbMode == 2) ? kNum : (kbMode == 1) ? kRows : kLow;
        int rowY = 100;
        bool handled = false;
        for (int r = 0; r < 3 && !handled; r++) {
          int len = strlen(rows[r]);
          int kw = 240 / max(len, 10);
          int xo = (r == 1) ? kw / 2 : 0;
          if (ty > rowY && ty < rowY + 26) {
            int ki = (tx - xo) / kw;
            if (ki >= 0 && ki < len) {
              password += rows[r][ki];
              if (kbMode == 1) kbMode = 0;
            }
            handled = true;
          }
          rowY += 28;
        }
        if (!handled && ty >= rowY) {
          if (tx < 50) kbMode = (kbMode == 2) ? 0 : 2;
          else if (tx >= 54 && tx < 94) kbMode = (kbMode == 1) ? 0 : 1;
          else if (tx >= 98 && tx < 178) password += " ";
          else if (tx >= 182 && tx < 210) { if (password.length() > 0) password.remove(password.length() - 1); }
          else if (tx >= 214) password += "\n";
        }
      }
    }
    delay(50);
  }
}
