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
const char* WIFI_SSID = "Jugendhackt";
const char* WIFI_PASS = "Jug-!hackt";

// ============ TIME SETTINGS ============
// Deutschland:
// Winter: GMT_OFFSET_SEC = 3600
// Sommer: GMT_OFFSET_SEC = 7200

const char* NTP_SERVER = "pool.ntp.org";

long GMT_OFFSET_SEC = 3600;
int DAYLIGHT_OFFSET_SEC = 3600;

// ============ FEATURE TOGGLE ============
// Kann jetzt auch zur Laufzeit ueber die Settings-App umgeschaltet werden.
bool useWiFiTime = false;  // true = WiFi + NTP aktiv, false = kein WiFi + keine Zeit

// ============ OBJECTS ============
TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Forward declarations (Apps + Helfer)
void calculator();
void drawing();
void notesApp();
void chatApp();
void showFileSelection();
void showMarkdown(String path);
void webuntisApp();
void settingsApp();
void drawMenu();
void ensureWiFi();
void initSD();
bool getTouch(int& x, int& y);

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

// Zieht alle gerade anstehenden Touch-Events leer.
// Verhindert, dass ein Tipp, der ein Menu/App OEFFNET, sofort danach
// im neuen Bildschirm als zweiter Tipp interpretiert wird (Ursache vieler
// "spontaner" Fehlnavigationen / Abstuerze durch falsch interpretierte Taps).
void drainTouch() {
  int _tx, _ty;
  while (getTouch(_tx, _ty)) {
    delay(10);
  }
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

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WLAN verbunden");

    tft.fillScreen(TFT_BLACK);
    tft.drawString("WLAN verbunden", 120, 150, 2);
  } else {
    Serial.println();
    Serial.println("WLAN Zeitueberschreitung");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.drawString("WLAN fehlgeschlagen", 120, 150, 2);
  }

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

  if (WiFi.status() != WL_CONNECTED) {
    // Ohne WLAN macht ein NTP-Sync keinen Sinn und wuerde sonst endlos warten.
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Kein WLAN -", 120, 140, 2);
    tft.drawString("Zeit nicht verfuegbar", 120, 170, 2);
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

  int tries = 0;
  while (!getLocalTime(&timeinfo) && tries < 20) {
    delay(500);
    Serial.println("Warte auf NTP...");
    tries++;
  }

  if (tries < 20) {
    Serial.println("Zeit synchronisiert");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Zeit synchronisiert", 120, 150, 2);
  } else {
    Serial.println("NTP Zeitueberschreitung");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.drawString("Zeit-Sync fehlgeschlagen", 120, 150, 2);
  }

  delay(1000);
}

void drawClock() {
  static bool lastUseWiFiTime = !useWiFiTime;  // erzwingt initiales Neuzeichnen
  static String lastTime = "";

  if (!useWiFiTime) {
    if (lastUseWiFiTime != useWiFiTime) {
      tft.fillRect(0, 278, SCREEN_W, 42, TFT_BLACK);
      tft.drawFastHLine(0, 278, SCREEN_W, TFT_DARKGREY);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Keine Zeit", SCREEN_W / 2, 295, 4);
      lastUseWiFiTime = useWiFiTime;
      lastTime = "";
    }
    return;
  }

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    if (lastUseWiFiTime != useWiFiTime) {
      tft.fillRect(0, 278, SCREEN_W, 42, TFT_BLACK);
      tft.drawFastHLine(0, 278, SCREEN_W, TFT_DARKGREY);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("Keine Zeit", SCREEN_W / 2, 295, 4);
      lastUseWiFiTime = useWiFiTime;
    }
    return;
  }

  char timeBuffer[16];
  char dateBuffer[20];

  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), "%d.%m.%Y", &timeinfo);

  String currentTime = String(timeBuffer);

  if (currentTime != lastTime || lastUseWiFiTime != useWiFiTime) {
    lastTime = currentTime;
    lastUseWiFiTime = useWiFiTime;

    tft.fillRect(0, 278, SCREEN_W, 42, TFT_BLACK);
    tft.drawFastHLine(0, 278, SCREEN_W, TFT_DARKGREY);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(currentTime, SCREEN_W / 2, 292, 4);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(String(dateBuffer), SCREEN_W / 2, 312, 2);
  }
}

// ================= TOUCH =================

bool getTouch(int& x, int& y) {
  if (!touchscreen.tirqTouched())
    return false;

  if (!touchscreen.touched())
    return false;

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
  else if (strcmp(name, "WebUntis") == 0) arr = icon_untis;
  if (arr) tft.pushImage(x + 2, y + 6, 24, 24, (uint16_t*)arr, TFT_BLACK);
}

// ================= MENU =================

struct AppButton {
  int x, y, w, h;
  uint16_t color;
  const char* label;
};

// Menu-Layout zentral definiert, damit drawMenu() und loop() garantiert
// dieselben Koordinaten verwenden (vorher waren die Werte dupliziert und
// liefen Gefahr, beim Anpassen auseinanderzulaufen).
static const AppButton MENU_APPS[] = {
  { 10, 20, 100, 50, TFT_BLUE, "Calc" },
  { 130, 20, 100, 50, TFT_RED, "Draw" },
  { 10, 80, 100, 50, TFT_GREEN, "Notes" },
  { 130, 80, 100, 50, TFT_CYAN, "Chat" },
  { 10, 140, 100, 50, TFT_MAGENTA, "Read" },
  { 130, 140, 100, 50, TFT_ORANGE, "Settings" },
  { 40, 200, 160, 40, 0x07E0, "WebUntis" },
};
static const int MENU_APPS_COUNT = sizeof(MENU_APPS) / sizeof(MENU_APPS[0]);

void drawMenu() {
  tft.fillScreen(TFT_BLACK);

  for (int i = 0; i < MENU_APPS_COUNT; i++) {
    const AppButton& b = MENU_APPS[i];
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, b.color);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, TFT_WHITE);
    drawMenuIcon(b.x + 4, b.y + 5, b.label);
    tft.setTextColor(TFT_WHITE, b.color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(b.label, b.x + b.w / 2, b.y + b.h - 12, 1);
  }

  tft.drawFastHLine(0, 250, SCREEN_W, TFT_DARKGREY);
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
    Serial.println("Kann Root nicht oeffnen");
    return 0;
  }

  int count = 0;

  while (true) {
    File file = root.openNextFile();

    if (!file)
      break;

    String name = file.name();

    if (name.endsWith(".md") || name.endsWith(".MD")) {
      if (count < maxFiles) {
        fileNames[count] = "/" + name;
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
  drainTouch();

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

  String fileNames[20];
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

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("=== DATEIEN ===", 120, 10, 2);

  int maxDisplay = min(fileCount, 6);

  for (int i = 0; i < maxDisplay; i++) {
    String displayName = fileNames[i];
    displayName.replace("/", "");

    int yPos = 50 + (i * 40);
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
        int yPos = 50 + (i * 40);
        if (isButtonPressed(tx, ty, 20, yPos, 200, 30)) {
          Serial.print("Oeffne Datei: ");
          Serial.println(fileNames[i]);
          drainTouch();
          showMarkdown(fileNames[i]);
          return;
        }
      }
      delay(150);
    }
    delay(10);
  }
}

String readFile(String path) {
  if (!sdReady) {
    return "SD-Karte nicht verfuegbar";
  }

  File file = SD.open(path);

  if (!file) {
    Serial.print("Kann Datei nicht oeffnen: ");
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

// ================= TEXT UMBRUCH =================

void drawWrappedText(String text, int x, int y, int maxWidth, int fontSize) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(false);  // Manueller Umbruch

  int currentY = y;
  String remaining = text;

  // Sicherheitsbremse: verhindert eine theoretisch unendliche Schleife,
  // falls maxWidth so klein ist, dass nicht einmal ein einzelnes Zeichen
  // hineinpasst (vorher konnte das den ganzen Bildschirm einfrieren).
  int safetyCounter = 0;

  while (remaining.length() > 0 && safetyCounter < 500) {
    safetyCounter++;

    String line = "";
    int i = 0;
    bool brokeOnSpace = false;

    while (i < (int)remaining.length()) {
      String testLine = line + remaining[i];

      int textWidth = tft.textWidth(testLine, fontSize);

      if (textWidth > maxWidth) {
        int lastSpace = line.lastIndexOf(' ');
        if (lastSpace > 0 && lastSpace > (int)line.length() / 2) {
          line = line.substring(0, lastSpace);
          remaining = remaining.substring(lastSpace + 1);
          brokeOnSpace = true;
          break;
        } else if (line.length() == 0) {
          // Selbst ein einzelnes Zeichen ist schon zu breit (oder die Zeile
          // ist noch leer) -> erzwinge trotzdem mindestens ein Zeichen,
          // sonst kommt remaining nie voran.
          line = testLine;
          remaining = remaining.substring(i + 1);
          brokeOnSpace = true;
          break;
        } else {
          remaining = remaining.substring(i);
          brokeOnSpace = true;
          break;
        }
      }

      line = testLine;
      i++;
    }

    if (!brokeOnSpace) {
      line = remaining;
      remaining = "";
    }

    if (currentY < 260) {
      tft.drawString(line, x, currentY, fontSize);
    }

    currentY += (fontSize == 4) ? 22 : 18;

    if (currentY > 260 && remaining.length() > 0) {
      tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Tippen zum", 120, 280, 2);
      tft.drawString("weiter", 120, 300, 2);

      int tx, ty;
      while (!getTouch(tx, ty)) {
        delay(10);
      }
      drainTouch();

      tft.fillRect(0, 0, SCREEN_W, 270, TFT_BLACK);
      currentY = 0;
    }
  }
}

// ================= MARKDOWN DISPLAY =================

void drawMDLine(String line, int y, int& lineHeight) {
  tft.setTextDatum(TL_DATUM);

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
    drawWrappedText("- " + line.substring(2), 10, y, SCREEN_W - 15, 2);
    lineHeight = 18;
  } else if (line.length() == 0) {
    lineHeight = 8;
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawWrappedText(line, 5, y, SCREEN_W - 10, 2);
    lineHeight = 18;
  }
}

void showMarkdown(String path) {
  drainTouch();

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

  if (content == "Datei nicht gefunden" || content == "SD-Karte nicht verfuegbar") {
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

  for (int i = 0; i < (int)content.length(); i++) {
    char c = content[i];

    if (c == '\n') {
      if (y < 260) {
        int lineHeight = 0;
        drawMDLine(line, y, lineHeight);
        y += lineHeight;
      }
      line = "";

      if (y > 260) {
        tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Tippen zum", 120, 280, 2);
        tft.drawString("weiter", 120, 300, 2);

        int tx, ty;
        while (!getTouch(tx, ty)) {
          delay(10);
        }
        drainTouch();

        tft.fillRect(0, 0, SCREEN_W, 270, TFT_BLACK);
        y = 0;
      }
    } else {
      line += c;
    }
  }

  if (line.length() > 0 && y < 260) {
    int lineHeight = 0;
    drawMDLine(line, y, lineHeight);
  }

  tft.drawFastHLine(0, 270, SCREEN_W, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Tippen zum", 120, 280, 2);
  tft.drawString("zurueck", 120, 300, 2);

  int tx, ty;
  while (!getTouch(tx, ty)) {
    delay(10);
  }
  drainTouch();

  showFileSelection();
}

// ================= WIFI HELPER =================

void ensureWiFi() {
  if (!useWiFiTime) return;
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
      delay(500);
      tries++;
    }
  }
}

// ================= GEMEINSAME VIRTUELLE TASTATUR =================
// Eine einzige, getestete Tastatur-Implementierung fuer alle Apps
// (Notizen-Name, Notizen-Editor, Chat). Vorher hatte jede App ihre eigene
// Kopie, jede mit eigenen, leicht unterschiedlichen Bugs (z.B. die
// "Neuer Notizname"-Eingabe hatte ueberhaupt keine sichtbaren Tasten und
// konnte nur Buchstaben A-H erzeugen).
//
// Modi: 0 = Kleinbuchstaben, 1 = Grossbuchstaben, 2 = Zahlen/Symbole

void getKeyboardLayout(int mode, const char* rows[3][12], int counts[3]) {
  static const char* low0[] = { "q", "w", "e", "r", "t", "z", "u", "i", "o", "p" };
  static const char* low1[] = { "a", "s", "d", "f", "g", "h", "j", "k", "l" };
  static const char* low2[] = { "y", "x", "c", "v", "b", "n", "m" };

  static const char* up0[] = { "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P" };
  static const char* up1[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
  static const char* up2[] = { "Y", "X", "C", "V", "B", "N", "M" };

  static const char* num0[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
  static const char* num1[] = { "-", "_", ".", ",", "!", "?", ";", ":" };
  static const char* num2[] = { "+", "*", "\"", "'", "/", "(", ")" };

  const char** r0;
  const char** r1;
  const char** r2;
  int c0, c1, c2;

  switch (mode) {
    case 1:
      r0 = up0; c0 = 10;
      r1 = up1; c1 = 9;
      r2 = up2; c2 = 7;
      break;
    case 2:
      r0 = num0; c0 = 10;
      r1 = num1; c1 = 8;
      r2 = num2; c2 = 7;
      break;
    default:
      r0 = low0; c0 = 10;
      r1 = low1; c1 = 9;
      r2 = low2; c2 = 7;
      break;
  }

  for (int i = 0; i < c0; i++) rows[0][i] = r0[i];
  for (int i = 0; i < c1; i++) rows[1][i] = r1[i];
  for (int i = 0; i < c2; i++) rows[2][i] = r2[i];
  counts[0] = c0;
  counts[1] = c1;
  counts[2] = c2;
}

// Zeichnet die 3 Buchstaben/Zahlen-Reihen plus Funktionsreihe (123/ABC,
// Shift, Leertaste, Loeschen, OK) ab y = yOffset.
void drawKeyboard(int kbMode, int yOffset) {
  const char* rows[3][12];
  int counts[3];
  getKeyboardLayout(kbMode, rows, counts);

  tft.fillRect(0, yOffset, 240, 320 - yOffset, TFT_BLACK);

  int ky2 = yOffset;
  for (int r = 0; r < 3; r++) {
    int len = counts[r];
    int kw = 240 / max(len, 10);
    int xo = (r == 1) ? kw / 2 : 0;
    for (int i = 0; i < len; i++) {
      int kx2 = xo + i * kw;
      tft.fillRoundRect(kx2, ky2, kw - 2, 28, 3, 0x0841);
      tft.setTextColor(TFT_WHITE, 0x0841);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(rows[r][i], kx2 + kw / 2, ky2 + 14, 1);
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
}

// Verarbeitet einen Touch innerhalb der Tastatur (yOffset .. Bildschirmende).
// Rueckgabewert: 0 = Touch lag ausserhalb der Tastatur / nichts passiert,
//                1 = Eingabe verarbeitet (Zeichen/Space/Backspace/Modus),
//                2 = OK/Enter wurde gedrueckt.
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
        if (kbMode == 1) kbMode = 0;  // Shift ist "single shot"
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

// Stellt ein Textfeld + die obige Tastatur dar und gibt den fertigen Text
// zurueck, sobald OK gedrueckt wird. Wird fuer den Notiz-Namen und fuer
// einzeilige Eingaben verwendet.
String virtualKeyboardInput(String title, String initial, int maxLen) {
  drainTouch();
  String text = initial;
  int kbMode = 0;

  while (true) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 24, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(4, 8);
    tft.print(title);

    tft.fillRoundRect(180, 2, 56, 20, 3, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FERTIG", 208, 12, 1);

    tft.drawRect(8, 32, 224, 30, TFT_WHITE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(14, 41);
    tft.print(text);
    if ((millis() / 500) % 2 == 0) {
      int cw = tft.textWidth(text, 1) + 2;
      tft.fillRect(14 + cw, 38, 2, 16, TFT_WHITE);
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
    if (maxLen > 0 && (int)text.length() > maxLen) {
      text = text.substring(0, maxLen);
    }
    delay(150);
  }
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(2);

  initSD();

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);

  if (useWiFiTime) {
    connectWiFi();
    initTime();
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi & Zeit", 120, 140, 2);
    tft.drawString("deaktiviert", 120, 170, 2);
    tft.drawString("Starte ohne", 120, 200, 2);
    tft.drawString("Netzwerk...", 120, 230, 2);
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
        else if (strcmp(b.label, "WebUntis") == 0) webuntisApp();
        break;
      }
    }
    delay(150);
  }
}

// ================= CALCULATOR FUNCTION =================

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

    { "x2", 5, 230, 50, 35, TFT_DARKGREY },
    { "0", 60, 230, 50, 35, TFT_DARKGREY },
    { ".", 115, 230, 50, 35, TFT_DARKGREY },
    { "=", 170, 230, 55, 35, TFT_GREEN },

    { "^", 5, 270, 50, 30, TFT_PURPLE },
    { "1/x", 60, 270, 105, 30, TFT_DARKGREY },
    { "<--", 170, 270, 55, 30, TFT_DARKGREY },

    { "Zurueck", 5, 305, 230, 25, TFT_RED }
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

  auto drawButtons = [&]() {
    for (int i = 0; i < numButtons; i++) {
      tft.fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, buttons[i].color);
      tft.drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, TFT_WHITE);
      tft.setTextColor(TFT_WHITE, buttons[i].color);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(buttons[i].label, buttons[i].x + buttons[i].w / 2, buttons[i].y + buttons[i].h / 2, 2);
    }
  };

  auto applyPendingOp = [&](float currentNum) -> bool {
    switch (lastOperator) {
      case '+': result += currentNum; return true;
      case '-': result -= currentNum; return true;
      case '*': result *= currentNum; return true;
      case '/':
        if (currentNum != 0) { result /= currentNum; return true; }
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
      // BUGFIX: vorher wurde hier auf den UTF8-Sonderzeichen-String "+/-"
      // (Pluszeichen) verglichen, der niemals zum Button-Label "+/-" passte
      // -> die Vorzeichen-Taste hat nie funktioniert.
      if (display != "0") {
        if (display.startsWith("-")) {
          display = display.substring(1);
        } else {
          display = "-" + display;
        }
      }
    } else if (value == "Wr.") {
      float num = display.toFloat();
      if (num >= 0) {
        display = String(sqrt(num), 6);
        result = display.toFloat();
        newNumber = true;
      } else {
        error = true;
      }
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
      } else {
        error = true;
      }
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

  drainTouch();

  while (true) {
    tft.fillScreen(TFT_BLACK);
    drawDisplay();
    drawButtons();

    int tx, ty;
    while (!getTouch(tx, ty)) {
      delay(10);
    }

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
  int16_t lastX = -1;
  int16_t lastY = -1;
  bool isDown = false;

  // BUGFIX: Vorher gab es keine sichtbare Schaltflaeche, um die Draw-App zu
  // verlassen (es gab nur eine unsichtbare Touch-Zone oben links) und der
  // untere "X"-Knopf rief eine Funktion auf, die nichts tat (toter Code
  // nach einem fruehen "return"). Dadurch blieb man in der Zeichen-App
  // haengen und musste das Geraet neu starten. Jetzt gibt es zwei
  // funktionierende, sichtbare Wege zurueck ins Menu.
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
    int bw = SCREEN_W / 5;
    int by = 294;
    int bh = 26;
    const char* labels[5] = { "RAD", "NEU", "RAUS", "SZ+", "SZ-" };
    for (int i = 0; i < 5; i++) {
      int bx = i * bw;
      tft.fillRect(bx, by, bw - 1, bh, (i == 0 && eraserOn) ? TFT_RED : 0x0841);
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

    // Kopfbereich: Zurueck-Knopf
    if (y < 28) {
      if (isButtonPressed(x, y, 2, 2, 50, 24)) {
        drainTouch();
        drawMenu();
        return;
      }
      delay(10);
      continue;
    }

    // Unterer Werkzeugbereich (Farbpalette)
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

    // Unterer Werkzeugbereich (Funktionstasten)
    if (y >= 294) {
      int ti = x / (SCREEN_W / 5);
      switch (ti) {
        case 0:
          eraserOn = !eraserOn;
          drawToolbar();
          break;
        case 1:
          clearCanvas();
          break;
        case 2:
          // "RAUS" -> sauber zurueck ins Hauptmenu
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

    // Zeichenflaeche
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

    if (list.empty()) {
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Keine Notizen", 120, 150, 2);
      tft.drawString("Tippe '+' zum Erstellen", 120, 175, 1);
      return;
    }

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

  auto editNote = [&](NoteFile& note) {
    String text = "";
    File rf = SD.open(note.path, FILE_READ);
    if (rf) {
      while (rf.available()) text += (char)rf.read();
      rf.close();
    }

    int kbMode = 0;

    while (true) {
      tft.fillScreen(TFT_BLACK);
      tft.fillRect(0, 0, 240, 24, TFT_DARKGREY);
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(4, 6);
      tft.print(note.name);

      tft.fillRoundRect(180, 2, 55, 20, 3, TFT_GREEN);
      tft.setTextColor(TFT_WHITE, TFT_GREEN);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("SPEICHERN", 207, 12, 1);

      tft.setTextColor(TFT_WHITE, TFT_BLACK);
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
        tft.fillRect(4 + cursorCol * 6, lineY - 12, 6, 10, TFT_WHITE);
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
        // BUGFIX: Vorher gab es hier nur ein unsichtbares 20px-Raster, das
        // zudem auf den falschen UTF8-Vergleich "char c=(kx-20)/20" stiess
        // (Bereich 20..180 / 20px = nur 8 Felder fuer 26 Buchstaben ->
        // Buchstaben I-Z waren nie erreichbar). Jetzt: vollwertige Tastatur.
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

    if (ty > 280 && ty < 320) {
      if (tx > 110 && tx < 130) {
        if (ty < 295 && scrollOff > 0) scrollOff--;
        else if (ty >= 295 && scrollOff + 7 < (int)notes.size()) scrollOff++;
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

void chatApp() {
  drainTouch();
  ensureWiFi();

  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Kein WLAN", 120, 100, 2);
    tft.drawString("Tippen zum zurueck", 120, 140, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    drainTouch();
    drawMenu();
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Chat", 120, 20, 2);
  tft.drawString("Verbinde...", 120, 80, 1);

  String pubkey = "", privkey = "", username = "", groupId = "";

  // Load saved identity from SPIFFS
  if (SPIFFS.begin(true)) {
    if (SPIFFS.exists("/chat.json")) {
      File f = SPIFFS.open("/chat.json", FILE_READ);
      if (f) {
        String s = f.readString();
        f.close();
        int u = s.indexOf("\"username\":\"");
        if (u > 0) { int e = s.indexOf("\"", u + 12); if (e > u + 12) username = s.substring(u + 12, e); }
        int p = s.indexOf("\"pubkey\":\"");
        if (p > 0) { int e = s.indexOf("\"", p + 10); if (e > p + 10) pubkey = s.substring(p + 10, e); }
        int r = s.indexOf("\"privkey\":\"");
        if (r > 0) { int e = s.indexOf("\"", r + 11); if (e > r + 11) privkey = s.substring(r + 11, e); }
      }
    }
  }

  HTTPClient http;
  http.setTimeout(5000);
  String apiBase = "http://149.102.157.124:3001";

  if (pubkey == "") {
    // Register new user
    username = "CYD-User-" + String(random(1000, 9999));
    http.begin(apiBase + "/api/register");
    http.addHeader("Content-Type", "application/json");
    String regBody = "{\"username\":\"" + username + "\",\"displayName\":\"" + username + "\"}";
    int code = http.POST(regBody);
    if (code == 200 || code == 201) {
      String resp = http.getString();
      int pk = resp.indexOf("\"pubkey\":\"");
      if (pk > 0) pubkey = resp.substring(pk + 10, resp.indexOf("\"", pk + 10));
      int pr = resp.indexOf("\"privkey\":\"");
      if (pr > 0) privkey = resp.substring(pr + 11, resp.indexOf("\"", pr + 11));
    }
    http.end();
    if (pubkey != "") {
      SPIFFS.begin(true);
      File f = SPIFFS.open("/chat.json", FILE_WRITE);
      if (f) {
        f.print("{\"username\":\""); f.print(username);
        f.print("\",\"pubkey\":\""); f.print(pubkey);
        f.print("\",\"privkey\":\""); f.print(privkey);
        f.print("\"}");
        f.close();
      }
    }
  } else {
    // Recover existing identity
    http.begin(apiBase + "/api/login");
    http.addHeader("Content-Type", "application/json");
    String loginBody = "{\"username\":\"" + username + "\"}";
    int code = http.POST(loginBody);
    if (code == 200) {
      String resp = http.getString();
      int pk = resp.indexOf("\"pubkey\":\"");
      if (pk > 0) pubkey = resp.substring(pk + 10, resp.indexOf("\"", pk + 10));
      int pr = resp.indexOf("\"privkey\":\"");
      if (pr > 0) privkey = resp.substring(pr + 11, resp.indexOf("\"", pr + 11));
    }
    http.end();
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Gruppen...", 120, 20, 2);

  // Find "CYD-Chat" group
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
        if (sn > 0) {
          String gn = gResp.substring(sn + 8);
          gn = gn.substring(0, gn.indexOf("\""));
          if (gn == "CYD-Chat") { groupId = gid; break; }
        }
        pos = si + 1;
      }
    }
    http.end();

    // Create group if not found
    if (groupId == "") {
      http.begin(apiBase + "/api/groups");
      http.addHeader("Content-Type", "application/json");
      String gBody = "{\"name\":\"CYD-Chat\",\"ownerPubkey\":\"" + pubkey + "\"}";
      int gcode = http.POST(gBody);
      if (gcode == 200 || gcode == 201) {
        String gResp = http.getString();
        int si = gResp.indexOf("\"id\":\"");
        if (si > 0) {
          groupId = gResp.substring(si + 6);
          groupId = groupId.substring(0, groupId.indexOf("\""));
        }
      }
      http.end();
    }
  }

  if (groupId == "") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Keine Verbindung", 120, 80, 2);
    tft.drawString("Tippen zum zurueck", 120, 120, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    drainTouch();
    drawMenu();
    return;
  }

  std::vector<String> messages;
  String chatInput = "";
  int kbMode = 0;
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

    drawKeyboard(kbMode, 165);

    tft.fillRect(0, 156, 240, 9, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(4, 157);
    String shownInput = chatInput;
    if (shownInput.length() > 38) shownInput = shownInput.substring(shownInput.length() - 38);
    tft.print(">" + shownInput);

    int tx, ty;
    if (!getTouch(tx, ty)) {
      if (millis() - lastPoll > 5000) {
        fetchMessages();
        lastPoll = millis();
      }
      delay(10);
      continue;
    }

    if (ty < 24 && tx > 195) {
      drainTouch();
      drawMenu();
      return;
    }

    if (ty >= 165) {
      int res = handleKeyboardTouch(tx, ty, chatInput, kbMode, 165);
      if (res == 2) {
        if (chatInput.length() > 0) {
          chatInput.replace("\\", "\\\\");
          chatInput.replace("\"", "\\\"");
          http.begin(apiBase + "/api/messages");
          http.addHeader("Content-Type", "application/json");
          String msgBody = "{\"groupId\":\"" + groupId + "\",\"senderPubkey\":\"" + pubkey + "\",\"senderPrivkey\":\"" + privkey + "\",\"content\":\"" + chatInput + "\"}";
          http.POST(msgBody);
          http.end();
          chatInput = "";
          fetchMessages();
        }
      }
    }
    delay(130);
  }
}

// ================= WEBUNTIS APP =================
// Neu: Es gibt jetzt zuerst ein Wochentag-Menu (Mo - Sa), bevor der
// Stundenplan geladen wird. Vorher wurde immer nur "heute" angezeigt.

struct UntisPeriod {
  int start, end;
  String subject, subjectLong, teacher, room;
};

// Liefert das Datum (YYYYMMDD) fuer einen gewuenschten Wochentag (1=Montag .. 7=Sonntag)
// der aktuellen Woche, ausgehend vom heutigen Datum.
bool computeDateForWeekday(int targetDow, char* outDateStr) {
  if (!useWiFiTime) return false;

  struct tm ti;
  if (!getLocalTime(&ti)) return false;

  int curDow = ti.tm_wday;                      // 0=So..6=Sa
  int curDowMon = (curDow == 0) ? 7 : curDow;    // 1=Mo..7=So
  int diffDays = targetDow - curDowMon;

  time_t now = mktime(&ti);
  time_t target = now + (time_t)diffDays * 86400L;

  struct tm tt;
  localtime_r(&target, &tt);

  sprintf(outDateStr, "%04d%02d%02d", tt.tm_year + 1900, tt.tm_mon + 1, tt.tm_mday);
  return true;
}

// Laedt und zeigt den Stundenplan fuer einen Wochentag (1=Mo..7=So) an.
// Rueckgabe: true = ganz zum Hauptmenu zurueck, false = zurueck zur Tagesauswahl.
bool showTimetableForDay(int dayOfWeek, const char* dayName) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WebUntis", 120, 15, 2);
  tft.drawString("Lade " + String(dayName) + "...", 120, 100, 1);

  char dateStr[9];
  bool haveDate = computeDateForWeekday(dayOfWeek, dateStr);
  if (!haveDate) {
    // Ohne Zeit-Sync kann kein verlaessliches Datum berechnet werden.
    strcpy(dateStr, "20250101");
  }

  HTTPClient http;
  http.setTimeout(10000);

  std::vector<UntisPeriod> periods;

  if (WiFi.status() == WL_CONNECTED) {
    char apiDate[11];
    sprintf(apiDate, "%c%c%c%c-%c%c-%c%c", dateStr[0], dateStr[1], dateStr[2], dateStr[3], dateStr[4], dateStr[5], dateStr[6], dateStr[7]);
    String url = String("http://149.102.157.124:3001/webuntis/timetable?start=") + apiDate + "&end=" + apiDate;

    http.begin(url);
    int code = http.GET();
    if (code == 200) {
      String resp = http.getString();
      int pos = 0;
      while (true) {
        int si = resp.indexOf("\"startTime\":", pos);
        if (si < 0) break;
        int st = resp.substring(si + 12).toInt();
        int ei = resp.indexOf("\"endTime\":", si);
        int en = 0;
        if (ei > 0) en = resp.substring(ei + 10).toInt();

        String subj = "", subjLong = "", teach = "", room = "";
        int ss = resp.indexOf("\"subjects\"", si);
        if (ss > 0) {
          int sn = resp.indexOf("\"name\":\"", ss);
          if (sn > 0) {
            subj = resp.substring(sn + 8);
            subj = subj.substring(0, subj.indexOf("\""));
          }
          int sl = resp.indexOf("\"longName\":\"", ss);
          if (sl > 0) {
            subjLong = resp.substring(sl + 12);
            subjLong = subjLong.substring(0, subjLong.indexOf("\""));
          }
        }
        int ts = resp.indexOf("\"teachers\"", si);
        if (ts > 0) {
          int tn = resp.indexOf("\"name\":\"", ts);
          if (tn > 0) {
            teach = resp.substring(tn + 8);
            teach = teach.substring(0, teach.indexOf("\""));
          }
        }
        int rs = resp.indexOf("\"rooms\"", si);
        if (rs > 0) {
          int rn = resp.indexOf("\"name\":\"", rs);
          if (rn > 0) {
            room = resp.substring(rn + 8);
            room = room.substring(0, room.indexOf("\""));
          }
        }
        periods.push_back({ st, en, subj, subjLong, teach, room });
        pos = si + 1;
      }
    }
    http.end();
  }

  // Group periods by start+end time
  struct TimeSlot { int start, end; std::vector<UntisPeriod> periods; };
  std::vector<TimeSlot> slots;
  for (auto& p : periods) {
    bool found = false;
    for (auto& s : slots) { if (s.start == p.start && s.end == p.end) { s.periods.push_back(p); found = true; break; } }
    if (!found) slots.push_back({p.start, p.end, {p}});
  }

  // Precompute heights
  std::vector<int> heights;
  for (auto& s : slots) {
    if (s.periods.size() == 1) heights.push_back(56);
    else heights.push_back(14 + (int)s.periods.size() * 13 + 6);
  }

  tft.fillScreen(TFT_BLACK);

  // Header: "Montag, 22.06.2026"
  String dStr = String(dateStr);
  String hdr = String(dayName) + ", " + dStr.substring(6,8) + "." + dStr.substring(4,6) + "." + dStr.substring(0,4);
  tft.fillRect(0, 0, 240, 28, 0x1D7C);
  tft.setTextColor(TFT_WHITE, 0x1D7C);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(hdr, 120, 14, 2);

  tft.fillRoundRect(200, 3, 35, 22, 3, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("X", 217, 14, 1);

  if (slots.empty()) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    if (WiFi.status() != WL_CONNECTED) tft.drawString("Kein WLAN", 120, 140, 2);
    else tft.drawString("Keine Stunden", 120, 140, 2);
    tft.drawString("Tippen zum zurueck", 120, 180, 1);
    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);
    bool exitAll = (ty < 28 && tx > 195);
    drainTouch();
    return exitAll;
  }

  int scrollSlot = 0;

  while (true) {
    tft.fillRect(0, 28, 240, 252, TFT_BLACK);
    int y = 32;
    int si = scrollSlot;
    while (si < (int)slots.size() && y < 275) {
      auto& s = slots[si];
      int sh = heights[si];
      int drawEnd = min(y + sh, 275);

      char tb[20];
      sprintf(tb, "%02d:%02d - %02d:%02d", s.start / 100, s.start % 100, s.end / 100, s.end % 100);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextDatum(TL_DATUM);
      tft.setCursor(4, y);
      tft.print(tb);

      if (s.periods.size() == 1) {
        auto& p = s.periods[0];
        if (y + 14 < drawEnd) {
          tft.setTextColor(0x1D7C, TFT_BLACK);
          tft.setCursor(4, y + 14); tft.print("Teacher: ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.teacher);
        }
        if (y + 26 < drawEnd) {
          tft.setTextColor(0x1D7C, TFT_BLACK);
          tft.setCursor(4, y + 26); tft.print("Subject: ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.subject);
          if (p.subjectLong.length() > 0) {
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.print(" (");
            tft.print(p.subjectLong);
            tft.print(")");
          }
        }
        if (y + 40 < drawEnd) {
          tft.setTextColor(0x1D7C, TFT_BLACK);
          tft.setCursor(4, y + 40); tft.print("Room: ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.room);
        }
      } else {
        for (int pi = 0; pi < (int)s.periods.size() && y + 14 + pi * 13 < drawEnd; pi++) {
          auto& p = s.periods[pi];
          int ly = y + 14 + pi * 13;
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
          tft.setCursor(4, ly); tft.print(String(pi + 1) + ": ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.teacher);
          tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
          tft.print(" | ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.subject);
          tft.setTextColor(TFT_CYAN, TFT_BLACK);
          tft.print(" | ");
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          tft.print(p.room);
        }
      }
      y += sh;
      si++;
    }

    if (scrollSlot > 0) tft.fillTriangle(120, 285, 110, 278, 130, 278, TFT_LIGHTGREY);
    if (si < (int)slots.size()) tft.fillTriangle(120, 275, 110, 282, 130, 282, TFT_LIGHTGREY);

    int tx, ty;
    if (!getTouch(tx, ty)) { delay(10); continue; }

    if (ty < 28 && tx > 195) { drainTouch(); return true; }
    if (ty < 28) { drainTouch(); return false; }

    if (ty > 270) {
      if (ty < 285 && scrollSlot > 0) scrollSlot--;
      else if (ty >= 285 && si < (int)slots.size()) scrollSlot++;
      delay(150);
    }
    delay(150);
  }
}

void webuntisApp() {
  drainTouch();
  ensureWiFi();

  const char* dayNames[7] = { "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag" };

  while (true) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 28, 0x1D7C);
    tft.setTextColor(TFT_WHITE, 0x1D7C);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Tag waehlen", 110, 14, 2);

    tft.fillRoundRect(200, 3, 35, 22, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("X", 217, 14, 1);

    for (int i = 0; i < 6; i++) {  // Montag - Samstag
      int y = 36 + i * 38;
      drawButton(15, y, 210, 32, TFT_BLUE, dayNames[i]);
    }

    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);

    if (ty < 28 && tx > 195) {
      drainTouch();
      drawMenu();
      return;
    }

    bool dayPicked = false;
    for (int i = 0; i < 6; i++) {
      int y = 36 + i * 38;
      if (isButtonPressed(tx, ty, 15, y, 210, 32)) {
        drainTouch();
        bool exitAll = showTimetableForDay(i + 1, dayNames[i]);
        drainTouch();
        if (exitAll) {
          drawMenu();
          return;
        }
        dayPicked = true;
        break;
      }
    }
    if (!dayPicked) delay(150);
  }
}

// ================= SETTINGS APP =================

void settingsApp() {
  drainTouch();

  while (true) {
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 240, 28, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("EINSTELLUNGEN", 120, 14, 2);

    drawButton(10, 38, 220, 36, useWiFiTime ? TFT_GREEN : TFT_RED,
               useWiFiTime ? "WiFi/Zeit: AN" : "WiFi/Zeit: AUS");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(12, 82);
    tft.print("WiFi: ");
    tft.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.print(WiFi.status() == WL_CONNECTED ? "Verbunden" : "Getrennt");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(12, 96);
    tft.print("SD-Karte: ");
    tft.setTextColor(sdReady ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.print(sdReady ? "Bereit" : "Nicht gefunden");

    drawButton(10, 118, 220, 36, GMT_OFFSET_SEC == 7200 ? TFT_ORANGE : TFT_BLUE,
               GMT_OFFSET_SEC == 7200 ? "Zeitzone: Sommer" : "Zeitzone: Winter");

    drawButton(10, 166, 220, 36, TFT_CYAN, "WiFi neu verbinden");
    drawButton(10, 214, 220, 36, TFT_MAGENTA, "SD-Karte neu laden");
    drawButton(10, 262, 220, 36, TFT_RED, "Zurueck");

    int tx, ty;
    while (!getTouch(tx, ty)) delay(10);

    if (isButtonPressed(tx, ty, 10, 38, 220, 36)) {
      useWiFiTime = !useWiFiTime;
      if (useWiFiTime) {
        connectWiFi();
        initTime();
      } else {
        WiFi.disconnect(true);
      }
    } else if (isButtonPressed(tx, ty, 10, 118, 220, 36)) {
      if (GMT_OFFSET_SEC == 7200) {
        GMT_OFFSET_SEC = 3600;
      } else {
        GMT_OFFSET_SEC = 7200;
      }
      DAYLIGHT_OFFSET_SEC = 3600;
      if (useWiFiTime) initTime();
    } else if (isButtonPressed(tx, ty, 10, 166, 220, 36)) {
      ensureWiFi();
      if (useWiFiTime) connectWiFi();
    } else if (isButtonPressed(tx, ty, 10, 214, 220, 36)) {
      initSD();
    } else if (isButtonPressed(tx, ty, 10, 262, 220, 36)) {
      drainTouch();
      drawMenu();
      return;
    }
    delay(180);
  }
}
