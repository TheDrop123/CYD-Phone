#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include <FS.h>
#include <SPIFFS.h>

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
    if (
      isButtonPressed(
        tx, ty,
        10, 20,
        100, 40)) {
      Serial.println("Calc");
      calculator();
    }

    if (
      isButtonPressed(
        tx, ty,
        130, 20,
        100, 40)) {
      Serial.println("Draw");
      drawing();
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
      showFileSelection();
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
          drawMenu();
          return;  // Rechner beenden
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
      if (x < 40 && y < 28) { drawMenu(); return; }
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
