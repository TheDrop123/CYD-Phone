// ====================================================================
//  CYD NOTES  –  Minimal Touch-UI für ESP32 "Cheap Yellow Display"
//  Kein Terminal, nur grafische Oberfläche mit einer Notizen-App.
// ====================================================================
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <vector>

// ==================== PIN DEFINITIONEN (CYD Standard) ====================
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33
#define SD_CS         5

// ==================== HARDWARE OBJEKTE ====================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ==================== THEME ====================
uint16_t BG_COLOR     = TFT_WHITE;
uint16_t TEXT_COLOR   = TFT_BLACK;
uint16_t ACCENT_COLOR = TFT_SKYBLUE;
uint16_t BUTTON_COLOR = TFT_LIGHTGREY;
uint16_t WARNING_COLOR= TFT_RED;
uint16_t SUCCESS_COLOR= TFT_GREEN;

// ==================== APP STATE ====================
enum Screen { SCREEN_HOME, SCREEN_NOTE_LIST, SCREEN_NOTE_EDIT };
Screen currentScreen = SCREEN_HOME;

std::vector<String> noteFiles;     // Dateinamen im /notes Ordner
int    listScrollOffset = 0;
const int LIST_VISIBLE   = 8;

String editingFile   = "";         // aktuell geöffnete Notiz
String editingText    = "";        // Inhalt im Editor
bool   editingDirty    = false;

// einfache Tastatur: 3 Zeilen Buchstaben/Zahlen, Umschalter, Space, Backspace, Enter
const char* kbLower[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
const char* kbUpper[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
const char* kbNums [3] = { "1234567890", "-_/:;()", "+*\"',.?!" };
int kbMode = 0; // 0=lower 1=upper 2=numbers

// ==================== FORWARD DECLARATIONS ====================
bool getTouch(int &x, int &y);
void drawHomeScreen();
void drawNoteList();
void drawNoteEditor(bool full);
void drawKeyboard();
void handleHomeTouch(int x, int y);
void handleNoteListTouch(int x, int y);
void handleEditorTouch(int x, int y);
void refreshNoteFiles();
void saveCurrentNote();
void deleteNote(String filename);

// ====================================================================
//  ZENTRALE TOUCH-FUNKTION
//  Liest den Touchscreen aus und rechnet die Rohdaten in
//  Bildschirm-Pixelkoordinaten um (0..239 x, 0..319 y).
// ====================================================================
bool getTouch(int &x, int &y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = constrain(map(p.x, 200, 3800, 0, 239), 0, 239);
    y = constrain(map(p.y, 200, 3800, 0, 319), 0, 319);
    return true;
  }
  return false;
}

// ====================================================================
//  SETUP – alle Initialisierungen
// ====================================================================
void setup() {
  Serial.begin(115200);

  // --- Display ---
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR);

  // --- Touchscreen ---
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  // --- SD Karte ---
  tft.setCursor(10, 10);
  tft.println("Init SD card...");
  if (SD.begin(SD_CS)) {
    if (!SD.exists("/notes")) SD.mkdir("/notes");
    tft.println("SD OK");
  } else {
    tft.println("SD FAILED - notes can't be saved!");
    delay(1500);
  }

  refreshNoteFiles();
  drawHomeScreen();
}

// ====================================================================
//  LOOP – Touch abfragen, an aktuellen Screen weiterreichen
// ====================================================================
void loop() {
  int tx, ty;
  if (getTouch(tx, ty)) {
    switch (currentScreen) {
      case SCREEN_HOME:      handleHomeTouch(tx, ty);      break;
      case SCREEN_NOTE_LIST: handleNoteListTouch(tx, ty);  break;
      case SCREEN_NOTE_EDIT: handleEditorTouch(tx, ty);    break;
    }
    delay(120); // einfache Entprellung
  }
}

// ====================================================================
//  HOME SCREEN
// ====================================================================
void drawHomeScreen() {
  tft.fillScreen(BG_COLOR);

  tft.fillRect(0, 0, 240, 40, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("CYD HOME", 120, 12, 2);

  // App-Kachel: Notizen
  tft.fillRoundRect(30, 70, 180, 90, 8, BUTTON_COLOR);
  tft.drawRoundRect(30, 70, 180, 90, 8, TEXT_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(3);
  tft.drawCentreString("NOTES", 120, 100, 4);
  tft.setTextSize(1);
  tft.drawCentreString(String(noteFiles.size()) + " gespeichert", 120, 135, 1);

  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawCentreString("Tippe auf eine Kachel zum Oeffnen", 120, 300, 1);
}

void handleHomeTouch(int x, int y) {
  if (x > 30 && x < 210 && y > 70 && y < 160) {
    refreshNoteFiles();
    listScrollOffset = 0;
    currentScreen = SCREEN_NOTE_LIST;
    drawNoteList();
  }
}

// ====================================================================
//  NOTIZ-LISTE
// ====================================================================
void refreshNoteFiles() {
  noteFiles.clear();
  File dir = SD.open("/notes");
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    if (!f.isDirectory()) noteFiles.push_back(String(f.name()));
    f = dir.openNextFile();
  }
  dir.close();
}

void drawNoteList() {
  tft.fillScreen(BG_COLOR);

  // Kopfzeile
  tft.fillRect(0, 0, 240, 40, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("MEINE NOTIZEN", 120, 12, 2);

  // Zurück-Button
  tft.fillRoundRect(5, 5, 30, 30, 4, WARNING_COLOR);
  tft.drawCentreString("<", 20, 12, 2);

  // Neue Notiz Button
  tft.fillRoundRect(195, 5, 40, 30, 4, SUCCESS_COLOR);
  tft.drawCentreString("+", 215, 10, 2);

  // Liste
  int y = 50;
  int shown = 0;
  for (int i = listScrollOffset; i < (int)noteFiles.size() && shown < LIST_VISIBLE; i++) {
    tft.fillRoundRect(10, y, 220, 28, 4, BUTTON_COLOR);
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(1);
    String name = noteFiles[i];
    if (name.length() > 26) name = name.substring(0, 23) + "...";
    tft.setCursor(18, y + 9);
    tft.print(name);

    // Lösch-Symbol
    tft.fillRoundRect(190, y + 3, 22, 22, 3, WARNING_COLOR);
    tft.drawCentreString("X", 201, y + 7, 1);

    y += 32;
    shown++;
  }

  if (noteFiles.empty()) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    tft.drawCentreString("Noch keine Notizen - tippe [+]", 120, 150, 1);
  }

  // Scroll Buttons
  tft.fillTriangle(225, 60, 215, 50, 235, 50, BUTTON_COLOR);
  tft.fillTriangle(225, 300, 215, 290, 235, 290, BUTTON_COLOR);
}

void handleNoteListTouch(int x, int y) {
  // Zurück
  if (x < 35 && y < 35) {
    currentScreen = SCREEN_HOME;
    drawHomeScreen();
    return;
  }

  // Neue Notiz
  if (x > 195 && y < 35) {
    editingFile  = "note_" + String(millis()) + ".txt";
    editingText  = "";
    editingDirty = false;
    currentScreen = SCREEN_NOTE_EDIT;
    drawNoteEditor(true);
    return;
  }

  // Scroll oben
  if (x > 210 && y > 45 && y < 65 && listScrollOffset > 0) {
    listScrollOffset--;
    drawNoteList();
    return;
  }
  // Scroll unten
  if (x > 210 && y > 285 && y < 305 &&
      listScrollOffset + LIST_VISIBLE < (int)noteFiles.size()) {
    listScrollOffset++;
    drawNoteList();
    return;
  }

  // Listeneinträge
  int yPos = 50;
  int shown = 0;
  for (int i = listScrollOffset; i < (int)noteFiles.size() && shown < LIST_VISIBLE; i++) {
    if (y > yPos && y < yPos + 28) {
      if (x > 188 && x < 214) {
        // löschen
        deleteNote(noteFiles[i]);
        refreshNoteFiles();
        drawNoteList();
        return;
      } else {
        // öffnen
        editingFile = noteFiles[i];
        File f = SD.open("/notes/" + editingFile, FILE_READ);
        editingText = "";
        if (f) {
          while (f.available()) editingText += (char)f.read();
          f.close();
        }
        editingDirty = false;
        currentScreen = SCREEN_NOTE_EDIT;
        drawNoteEditor(true);
        return;
      }
    }
    yPos += 32;
    shown++;
  }
}

void deleteNote(String filename) {
  SD.remove("/notes/" + filename);
}

// ====================================================================
//  NOTIZ-EDITOR (Textbereich + einfache Tastatur)
// ====================================================================
void drawNoteEditor(bool full) {
  if (full) tft.fillScreen(BG_COLOR);

  // Kopfzeile
  tft.fillRect(0, 0, 240, 32, ACCENT_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.fillRoundRect(5, 4, 50, 24, 4, WARNING_COLOR);
  tft.drawCentreString("ZURUECK", 30, 12, 1);

  tft.fillRoundRect(185, 4, 50, 24, 4, SUCCESS_COLOR);
  tft.drawCentreString(editingDirty ? "SPEICHERN" : "GESPEICHERT", 210, 12, 1);

  // Textbereich
  tft.fillRect(0, 32, 240, 130, BG_COLOR);
  tft.drawRect(2, 34, 236, 126, TEXT_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(8, 40);

  // einfache Umbruch-Darstellung
  String displayText = editingText;
  if (displayText.length() > 400) {
    displayText = displayText.substring(displayText.length() - 400);
  }
  tft.print(displayText);
  if ((millis() / 500) % 2 == 0) tft.print("_");

  drawKeyboard();
}

void drawKeyboard() {
  tft.fillRect(0, 165, 240, 155, BG_COLOR);
  const char** rows = (kbMode == 0) ? kbLower : (kbMode == 1) ? kbUpper : kbNums;

  int rowY = 165;
  for (int r = 0; r < 3; r++) {
    int len = strlen(rows[r]);
    int keyW = 240 / max(len, 10);
    int xOff = (r == 1) ? keyW / 2 : 0; // mittlere Reihe leicht eingerückt
    for (int i = 0; i < len; i++) {
      int x = xOff + i * keyW;
      tft.fillRoundRect(x, rowY, keyW - 2, 30, 3, BUTTON_COLOR);
      tft.setTextColor(TEXT_COLOR);
      tft.setTextSize(1);
      tft.drawCentreString(String(rows[r][i]), x + keyW / 2, rowY + 11, 1);
    }
    rowY += 32;
  }

  // Funktionsreihe: ABC/123 | SPACE | < (Backspace) | ENTER
  int fy = rowY;
  tft.fillRoundRect(0, fy, 50, 32, 3, (kbMode == 2) ? ACCENT_COLOR : BUTTON_COLOR);
  tft.drawCentreString(kbMode == 2 ? "ABC" : "123", 25, fy + 11, 1);

  tft.fillRoundRect(54, fy, 40, 32, 3, (kbMode == 1) ? ACCENT_COLOR : BUTTON_COLOR);
  tft.drawCentreString("^", 74, fy + 11, 1);

  tft.fillRoundRect(98, fy, 80, 32, 3, BUTTON_COLOR);
  tft.drawCentreString("SPACE", 138, fy + 11, 1);

  tft.fillRoundRect(182, fy, 28, 32, 3, BUTTON_COLOR);
  tft.drawCentreString("<", 196, fy + 11, 1);

  tft.fillRoundRect(214, fy, 26, 32, 3, SUCCESS_COLOR);
  tft.drawCentreString("OK", 227, fy + 11, 1);
}

void handleEditorTouch(int x, int y) {
  // Zurück
  if (x < 60 && y < 32) {
    if (editingDirty) saveCurrentNote();
    refreshNoteFiles();
    currentScreen = SCREEN_NOTE_LIST;
    drawNoteList();
    return;
  }
  // Speichern Button
  if (x > 180 && y < 32) {
    saveCurrentNote();
    drawNoteEditor(false);
    return;
  }

  // Tastatur
  if (y < 165) return; // Texttipp-Bereich: kein Cursorpositionieren in dieser Minimalversion

  const char** rows = (kbMode == 0) ? kbLower : (kbMode == 1) ? kbUpper : kbNums;
  int rowY = 165;
  for (int r = 0; r < 3; r++) {
    int len = strlen(rows[r]);
    int keyW = 240 / max(len, 10);
    int xOff = (r == 1) ? keyW / 2 : 0;
    if (y > rowY && y < rowY + 30) {
      int idx = (x - xOff) / keyW;
      if (idx >= 0 && idx < len) {
        editingText += rows[r][idx];
        editingDirty = true;
        if (kbMode == 1) kbMode = 0; // Shift nur für einen Buchstaben
        drawNoteEditor(true);
        return;
      }
    }
    rowY += 32;
  }

  int fy = rowY;
  if (y > fy && y < fy + 32) {
    if (x < 50) {                       // 123 / ABC
      kbMode = (kbMode == 2) ? 0 : 2;
      drawNoteEditor(true);
    } else if (x >= 54 && x < 94) {     // Shift
      kbMode = (kbMode == 1) ? 0 : 1;
      drawNoteEditor(true);
    } else if (x >= 98 && x < 178) {    // Space
      editingText += " ";
      editingDirty = true;
      drawNoteEditor(true);
    } else if (x >= 182 && x < 210) {   // Backspace
      if (editingText.length() > 0) {
        editingText.remove(editingText.length() - 1);
        editingDirty = true;
      }
      drawNoteEditor(true);
    } else if (x >= 214) {              // Enter -> Zeilenumbruch
      editingText += "\n";
      editingDirty = true;
      drawNoteEditor(true);
    }
  }
}

void saveCurrentNote() {
  File f = SD.open("/notes/" + editingFile, FILE_WRITE);
  if (f) {
    f.print(editingText);
    f.close();
    editingDirty = false;
  }
}
