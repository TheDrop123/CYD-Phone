/*
  ============================================================
  CYD NOTIZEN-APP  (Touch + Stift + Text, kein Terminal)
  ------------------------------------------------------------
  Idee:
   - Eine einzige Funktion (getTouch) liest den Touchscreen aus
     und rechnet die Rohwerte direkt in Bildschirm-Pixel-
     koordinaten (0..239 / 0..319) um. Alle Screens benutzen
     nur diese eine Funktion.
   - Statt eines Terminals gibt es eine einfache Touch-UI mit
     vier Bildschirmen (Start, Ordner waehlen, Notizen-Liste,
     Editor).
   - Eine Notiz besteht aus zwei Dateien mit gleichem Namen:
       <name>.txt   -> getippter Text
       <name>.bmp   -> gemaltes Bild (echtes 1-Bit-BMP)
     Beim Oeffnen werden beide geladen und im Editor
     UEBEREINANDER dargestellt (Zeichnung im Hintergrund,
     Text darueber).
   - Der Speicherordner auf der SD-Karte wird per UI gewaehlt
     bzw. neu angelegt und in /config.txt gemerkt (bleibt also
     nach einem Neustart erhalten).

  Pins unten ggf. an dein CYD-Board anpassen.
  ============================================================
*/

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <vector>

// ==================== PIN DEFINITIONEN ====================
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33
#define SD_CS         5

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ==================== FARBEN (einfaches helles Theme) ======
#define COL_BG      TFT_WHITE
#define COL_TEXT    TFT_BLACK
#define COL_BTN     TFT_LIGHTGREY
#define COL_BTN_ON  TFT_ORANGE
#define COL_ACCENT  TFT_SKYBLUE
#define COL_DANGER  TFT_RED
#define COL_OK      TFT_GREEN
#define COL_INK     TFT_BLACK

// ==================== LAYOUT ====================
#define SCREEN_W   240
#define SCREEN_H   320
#define TOOLBAR_H  32
#define CONTENT_Y  TOOLBAR_H
#define KB_H       96
#define KB_Y       (SCREEN_H - KB_H)        // = 224, Grenze Inhalt/unterer Bereich
#define CANVAS_W   SCREEN_W
#define CANVAS_H   (KB_Y - CONTENT_Y)        // = 192

// ==================== ZEICHEN-PUFFER (1 Bit pro Pixel) =====
#define CANVAS_ROW_BYTES ((CANVAS_W + 7) / 8)              // 30 Byte/Zeile
uint8_t canvasBuf[CANVAS_ROW_BYTES * CANVAS_H];             // ~5,6 KB RAM

// ==================== GLOBALER ZUSTAND ======================
String currentDir       = "/";   // aktuell gewaehlter Speicherordner
String browsePath       = "/";   // Pfad waehrend des Ordner-Browsens
String currentNoteName  = "";    // Name der offenen Notiz (ohne Endung)
String noteText         = "";    // Textinhalt der offenen Notiz
bool   canvasDirty      = false; // seit letztem Speichern veraendert
bool   needsRedraw      = true;
int    listScroll       = 0;

bool kbShift   = false;
bool kbSymbols = false;

enum Screen { SCR_HOME, SCR_FOLDER, SCR_NOTES, SCR_EDITOR };
Screen currentScreen = SCR_HOME;

enum EditMode { MODE_PEN, MODE_TEXT };
EditMode editMode = MODE_PEN;

// ==================== FORWARD DECLARATIONS ====================
bool   getTouch(int &x, int &y);
void   drawButton(int x, int y, int w, int h, String label, uint16_t color);
void   drawKeyboard(int topY);
String keyboardHit(int tx, int ty, int topY);
String askText(String title, String defaultText = "");

void   clearCanvas();
void   setPixel(int x, int y, bool ink);
bool   getPixel(int x, int y);
void   paintBrush(int cx, int cy, int radius);
void   drawCanvasArea();
void   drawWrappedText(String text, int x, int y, int maxWidth);

void   writeLE16(File &f, uint16_t v);
void   writeLE32(File &f, uint32_t v);
uint16_t readLE16(File &f);
uint32_t readLE32(File &f);
bool   saveCanvasBMP(String path);
bool   loadCanvasBMP(String path);

String parentPath(String p);
void   listSubdirs(String path, std::vector<String> &dirs);
void   listNoteNames(String path, std::vector<String> &names);

void   newNote(String name);
void   openNote(String name);
void   saveNote();
void   deleteNote(String name);

void   loadConfig();
void   saveConfig();

void   drawHome();
void   drawFolderScreen();
void   drawNotesScreen();
void   drawEditorScreen();
void   handleHomeTouch(int x, int y);
void   handleFolderTouch(int x, int y);
void   handleNotesTouch(int x, int y);
void   handleEditorTouch(int x, int y);

// ============================================================
//  ZENTRALE TOUCH-FUNKTION
//  Liest den Touchscreen aus und liefert direkt
//  Bildschirm-Pixelkoordinaten zurueck. Alle Screens nutzen
//  ausschliesslich diese eine Funktion.
// ============================================================
bool getTouch(int &x, int &y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = constrain(map(p.x, 200, 3800, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    y = constrain(map(p.y, 200, 3800, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
    return true;
  }
  return false;
}

// ============================================================
//  SETUP – alle Initialisierungen an einem Ort
// ============================================================
void setup() {
  Serial.begin(115200);

  // Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_TEXT, COL_BG);

  // Touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  // SD-Karte
  if (!SD.begin(SD_CS)) {
    tft.setCursor(10, 10);
    tft.println("SD-Karte nicht gefunden!");
    tft.println("Bitte Karte einlegen und");
    tft.println("Geraet neu starten.");
    while (true) delay(1000);
  }

  loadConfig();    // zuletzt gewaehlten Ordner laden (falls vorhanden)
  clearCanvas();

  currentScreen = SCR_HOME;
  needsRedraw = true;
}

// ============================================================
//  LOOP – einfache Zustandsmaschine fuer die UI
// ============================================================
void loop() {
  if (needsRedraw) {
    switch (currentScreen) {
      case SCR_HOME:   drawHome();          break;
      case SCR_FOLDER: drawFolderScreen();  break;
      case SCR_NOTES:  drawNotesScreen();   break;
      case SCR_EDITOR: drawEditorScreen();  break;
    }
    needsRedraw = false;
  }

  int tx, ty;
  if (getTouch(tx, ty)) {
    switch (currentScreen) {
      case SCR_HOME:   handleHomeTouch(tx, ty);   break;
      case SCR_FOLDER: handleFolderTouch(tx, ty); break;
      case SCR_NOTES:  handleNotesTouch(tx, ty);  break;
      case SCR_EDITOR: handleEditorTouch(tx, ty); break;
    }
    delay(120); // einfache Entprellung
  }
}

// ============================================================
//  UI-HILFSFUNKTIONEN
// ============================================================
void drawButton(int x, int y, int w, int h, String label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 4, color);
  tft.drawRoundRect(x, y, w, h, 4, COL_TEXT);
  uint16_t txtCol = (color == COL_BTN || color == COL_BTN_ON) ? COL_TEXT : TFT_WHITE;
  tft.setTextColor(txtCol, color);
  tft.setTextSize(1);
  tft.drawCentreString(label, x + w / 2, y + h / 2 - 4, 1);
}

// kleines wiederverwendbares Keyboard, wird unten an "topY" gezeichnet
const char* kbLower[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
const char* kbUpper[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
const char* kbSym[3]   = { "1234567890", "-_.,!?:;'", "()/@#%&" };

void drawKeyboard(int topY) {
  tft.fillRect(0, topY, SCREEN_W, SCREEN_H - topY, COL_BG);
  const char** rows = kbSymbols ? kbSym : (kbShift ? kbUpper : kbLower);
  int rowH = (SCREEN_H - topY) / 4;

  // Reihe 1 (10 Tasten)
  for (int i = 0; i < 10; i++) {
    int x = i * 24;
    tft.fillRoundRect(x, topY, 23, rowH - 2, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.drawCentreString(String(rows[0][i]), x + 12, topY + rowH / 2 - 4, 1);
  }

  // Reihe 2 (9 Tasten, eingerueckt)
  int y2 = topY + rowH;
  for (int i = 0; i < 9; i++) {
    int x = 12 + i * 24;
    tft.fillRoundRect(x, y2, 23, rowH - 2, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.drawCentreString(String(rows[1][i]), x + 12, y2 + rowH / 2 - 4, 1);
  }

  // Reihe 3: [SHIFT] zxcvbnm [DEL]
  int y3 = topY + 2 * rowH;
  tft.fillRoundRect(0, y3, 34, rowH - 2, 3, kbShift ? COL_BTN_ON : COL_BTN);
  tft.setTextColor(COL_TEXT, COL_BTN);
  tft.drawCentreString("^", 17, y3 + rowH / 2 - 4, 1);
  for (int i = 0; i < 7; i++) {
    int x = 36 + i * 24;
    tft.fillRoundRect(x, y3, 23, rowH - 2, 3, COL_BTN);
    tft.drawCentreString(String(rows[2][i]), x + 12, y3 + rowH / 2 - 4, 1);
  }
  tft.fillRoundRect(206, y3, 34, rowH - 2, 3, COL_DANGER);
  tft.setTextColor(TFT_WHITE, COL_DANGER);
  tft.drawCentreString("<-", 223, y3 + rowH / 2 - 4, 1);

  // Reihe 4: [123/ABC] [LEERTASTE] [OK]
  int y4 = topY + 3 * rowH;
  tft.fillRoundRect(0, y4, 50, rowH - 2, 3, kbSymbols ? COL_BTN_ON : COL_BTN);
  tft.setTextColor(COL_TEXT, COL_BTN);
  tft.drawCentreString(kbSymbols ? "ABC" : "123", 25, y4 + rowH / 2 - 4, 1);

  tft.fillRoundRect(52, y4, 120, rowH - 2, 3, COL_BTN);
  tft.drawCentreString("LEERTASTE", 112, y4 + rowH / 2 - 4, 1);

  tft.fillRoundRect(174, y4, 66, rowH - 2, 3, COL_OK);
  tft.setTextColor(TFT_WHITE, COL_OK);
  tft.drawCentreString("OK", 207, y4 + rowH / 2 - 4, 1);
}

// liefert: "\b"=Backspace  "\n"=Enter/OK  "\t"=Shift  "\s"=123/ABC
//          " "=Leerzeichen  oder das getippte Zeichen, "" = kein Treffer
String keyboardHit(int tx, int ty, int topY) {
  int rowH = (SCREEN_H - topY) / 4;
  const char** rows = kbSymbols ? kbSym : (kbShift ? kbUpper : kbLower);

  if (ty < topY || ty >= topY + 4 * rowH) return "";
  int row = (ty - topY) / rowH;

  if (row == 0) {
    int i = tx / 24;
    if (i >= 0 && i < 10) return String(rows[0][i]);
  } else if (row == 1) {
    int i = (tx - 12) / 24;
    if (i >= 0 && i < 9) return String(rows[1][i]);
  } else if (row == 2) {
    if (tx < 34) return "\t";
    if (tx > 206) return "\b";
    int i = (tx - 36) / 24;
    if (i >= 0 && i < 7) return String(rows[2][i]);
  } else if (row == 3) {
    if (tx < 50) return "\s";
    if (tx >= 52 && tx < 172) return " ";
    if (tx >= 174) return "\n";
  }
  return "";
}

// Modales Eingabefeld (fuer Ordner-/Notiznamen). Blockiert, bis OK/Abbrechen.
String askText(String title, String defaultText) {
  String text = defaultText;
  bool done = false, cancelled = false;
  kbShift = false; kbSymbols = false;

  while (!done) {
    tft.fillScreen(COL_BG);
    tft.fillRect(0, 0, SCREEN_W, TOOLBAR_H, COL_ACCENT);
    tft.setTextColor(TFT_WHITE, COL_ACCENT);
    tft.setTextSize(1);
    tft.drawCentreString(title, SCREEN_W / 2, 12, 1);

    tft.fillRect(10, 46, SCREEN_W - 20, 30, COL_BTN);
    tft.drawRect(10, 46, SCREEN_W - 20, 30, COL_TEXT);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.setTextSize(2);
    String disp = text;
    if (disp.length() > 16) disp = disp.substring(disp.length() - 16);
    tft.setCursor(15, 54);
    tft.print(disp + "_");

    drawButton(10, 88, 100, 32, "OK", COL_OK);
    drawButton(130, 88, 100, 32, "Abbrechen", COL_DANGER);

    drawKeyboard(KB_Y);

    int tx, ty;
    bool touched = false;
    while (!touched) { if (getTouch(tx, ty)) touched = true; delay(10); }

    if (ty > 88 && ty < 120) {
      if (tx < 110) { done = true; }
      else if (tx > 130 && tx < 230) { cancelled = true; done = true; }
    } else if (ty >= KB_Y) {
      String key = keyboardHit(tx, ty, KB_Y);
      if (key == "\b") { if (text.length() > 0) text.remove(text.length() - 1); }
      else if (key == "\t") { kbShift = !kbShift; }
      else if (key == "\s") { kbSymbols = !kbSymbols; }
      else if (key == " ") { text += " "; }
      else if (key == "\n") { done = true; }
      else if (key.length() == 1) { text += key; }
    }
    delay(120);
  }

  needsRedraw = true;
  return cancelled ? "" : text;
}

// ============================================================
//  ZEICHEN-PUFFER (Stift-Ebene, 1 Bit pro Pixel)
// ============================================================
void clearCanvas() {
  memset(canvasBuf, 0, sizeof(canvasBuf));
  canvasDirty = true;
}

void setPixel(int x, int y, bool ink) {
  if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
  int idx = y * CANVAS_ROW_BYTES + (x / 8);
  uint8_t mask = 0x80 >> (x % 8);
  if (ink) canvasBuf[idx] |= mask;
  else      canvasBuf[idx] &= ~mask;
}

bool getPixel(int x, int y) {
  if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return false;
  int idx = y * CANVAS_ROW_BYTES + (x / 8);
  uint8_t mask = 0x80 >> (x % 8);
  return (canvasBuf[idx] & mask) != 0;
}

void paintBrush(int cx, int cy, int radius) {
  for (int dy = -radius; dy <= radius; dy++)
    for (int dx = -radius; dx <= radius; dx++)
      if (dx * dx + dy * dy <= radius * radius)
        setPixel(cx + dx, cy + dy, true);
  canvasDirty = true;
}

// Zeichnet Stift-Ebene + Text-Ebene UEBEREINANDER in den Inhaltsbereich
void drawCanvasArea() {
  tft.fillRect(0, CONTENT_Y, CANVAS_W, CANVAS_H, COL_BG);
  for (int y = 0; y < CANVAS_H; y++) {
    for (int x = 0; x < CANVAS_W; x++) {
      if (getPixel(x, y)) tft.drawPixel(x, CONTENT_Y + y, COL_INK);
    }
  }
  if (noteText.length() > 0) {
    drawWrappedText(noteText, 4, CONTENT_Y + 4, CANVAS_W - 8);
  }
}

void drawWrappedText(String text, int x, int y, int maxWidth) {
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  int maxChars = maxWidth / 6;
  int lineHeight = 10;
  int curY = y;
  int start = 0;

  while (start < (int)text.length()) {
    int nl = text.indexOf('\n', start);
    String para = (nl == -1) ? text.substring(start) : text.substring(start, nl);

    while (para.length() > 0) {
      String line;
      if ((int)para.length() <= maxChars) { line = para; para = ""; }
      else {
        int breakAt = para.lastIndexOf(' ', maxChars);
        if (breakAt <= 0) breakAt = maxChars;
        line = para.substring(0, breakAt);
        para = para.substring(breakAt);
        para.trim();
      }
      if (curY + lineHeight > CONTENT_Y + CANVAS_H) return;
      tft.setCursor(x, curY);
      tft.print(line);
      curY += lineHeight;
    }
    if (nl == -1) break;
    start = nl + 1;
  }
}

// ============================================================
//  BMP LESEN / SCHREIBEN (1-Bit Bitmap, fuer das gemalte Bild)
// ============================================================
void writeLE16(File &f, uint16_t v) { f.write(v & 0xFF); f.write((v >> 8) & 0xFF); }
void writeLE32(File &f, uint32_t v) {
  f.write(v & 0xFF); f.write((v >> 8) & 0xFF);
  f.write((v >> 16) & 0xFF); f.write((v >> 24) & 0xFF);
}
uint16_t readLE16(File &f) { uint8_t b[2]; f.read(b, 2); return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
uint32_t readLE32(File &f) {
  uint8_t b[4]; f.read(b, 4);
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

bool saveCanvasBMP(String path) {
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;

  int rowBytesBMP = ((CANVAS_W + 31) / 32) * 4;     // BMP-Zeilen auf 4 Byte ausgerichtet
  int pad = rowBytesBMP - CANVAS_ROW_BYTES;
  uint32_t imageSize = (uint32_t)rowBytesBMP * CANVAS_H;
  uint32_t dataOffset = 14 + 40 + 8;                // FileHeader+InfoHeader+2 Palette-Eintraege
  uint32_t fileSize = dataOffset + imageSize;

  // BITMAPFILEHEADER
  f.write('B'); f.write('M');
  writeLE32(f, fileSize);
  writeLE32(f, 0);
  writeLE32(f, dataOffset);

  // BITMAPINFOHEADER
  writeLE32(f, 40);
  writeLE32(f, CANVAS_W);
  writeLE32(f, CANVAS_H);
  writeLE16(f, 1);
  writeLE16(f, 1);       // 1 Bit/Pixel
  writeLE32(f, 0);
  writeLE32(f, imageSize);
  writeLE32(f, 0); writeLE32(f, 0);
  writeLE32(f, 2);       // 2 Farben
  writeLE32(f, 0);

  // Palette: Index 0 = weiss, Index 1 = schwarz
  f.write(255); f.write(255); f.write(255); f.write(0);
  f.write(0);   f.write(0);   f.write(0);   f.write(0);

  // Pixel-Daten, BMP-Standard: von unten nach oben
  uint8_t padBytes[4] = { 0, 0, 0, 0 };
  for (int y = CANVAS_H - 1; y >= 0; y--) {
    f.write(&canvasBuf[y * CANVAS_ROW_BYTES], CANVAS_ROW_BYTES);
    if (pad > 0) f.write(padBytes, pad);
  }

  f.close();
  return true;
}

bool loadCanvasBMP(String path) {
  clearCanvas();
  if (!SD.exists(path)) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  uint8_t sig[2]; f.read(sig, 2);
  if (sig[0] != 'B' || sig[1] != 'M') { f.close(); return false; }

  readLE32(f); readLE32(f);
  uint32_t dataOffset = readLE32(f);
  readLE32(f);                       // Header-Groesse
  int32_t w = (int32_t)readLE32(f);
  int32_t h = (int32_t)readLE32(f);
  readLE16(f);                       // Planes
  uint16_t bpp = readLE16(f);
  for (int i = 0; i < 5; i++) readLE32(f);

  // Es werden nur Bilder unterstuetzt, die diese App selbst erzeugt hat
  if (bpp != 1 || w != CANVAS_W || h > CANVAS_H) { f.close(); return false; }

  int rowBytesBMP = ((w + 31) / 32) * 4;
  f.seek(dataOffset);

  uint8_t rowBuf[40];   // reicht fuer rowBytesBMP (32 Byte bei 240px Breite)
  for (int i = 0; i < h; i++) {
    f.read(rowBuf, rowBytesBMP);
    int internalY = h - 1 - i;       // BMP ist bottom-up, Puffer top-down
    memcpy(&canvasBuf[internalY * CANVAS_ROW_BYTES], rowBuf, CANVAS_ROW_BYTES);
  }

  f.close();
  canvasDirty = false;
  return true;
}

// ============================================================
//  DATEI-/ORDNERVERWALTUNG
// ============================================================
String parentPath(String p) {
  if (p == "/" || p.length() == 0) return "/";
  if (p.endsWith("/")) p.remove(p.length() - 1);
  int slash = p.lastIndexOf('/');
  if (slash <= 0) return "/";
  return p.substring(0, slash);
}

void listSubdirs(String path, std::vector<String> &dirs) {
  dirs.clear();
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      String n = String(entry.name());
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      dirs.push_back(n);
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void listNoteNames(String path, std::vector<String> &names) {
  names.clear();
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String n = String(entry.name());
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (n.endsWith(".txt")) names.push_back(n.substring(0, n.length() - 4));
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}

void loadConfig() {
  if (SD.exists("/config.txt")) {
    File f = SD.open("/config.txt", FILE_READ);
    if (f) {
      String line = f.readStringUntil('\n');
      line.replace("\r", "");
      if (line.length() > 0) currentDir = line;
      f.close();
    }
  }
  if (!SD.exists(currentDir)) currentDir = "/";
}

void saveConfig() {
  SD.remove("/config.txt");
  File f = SD.open("/config.txt", FILE_WRITE);
  if (f) { f.println(currentDir); f.close(); }
}

// ============================================================
//  NOTIZ: ANLEGEN / OEFFNEN / SPEICHERN / LOESCHEN
// ============================================================
void newNote(String name) {
  currentNoteName = name;
  noteText = "";
  clearCanvas();
  editMode = MODE_PEN;
  currentScreen = SCR_EDITOR;
  needsRedraw = true;
}

void openNote(String name) {
  currentNoteName = name;
  String base = currentDir;
  if (!base.endsWith("/")) base += "/";

  noteText = "";
  File tf = SD.open(base + name + ".txt", FILE_READ);
  if (tf) { while (tf.available()) noteText += (char)tf.read(); tf.close(); }

  loadCanvasBMP(base + name + ".bmp");   // legt leere Zeichnung an, falls keine .bmp existiert

  editMode = MODE_PEN;
  currentScreen = SCR_EDITOR;
  needsRedraw = true;
}

void saveNote() {
  if (currentNoteName.length() == 0) return;
  String base = currentDir;
  if (!base.endsWith("/")) base += "/";

  SD.remove(base + currentNoteName + ".txt");
  File tf = SD.open(base + currentNoteName + ".txt", FILE_WRITE);
  if (tf) { tf.print(noteText); tf.close(); }

  saveCanvasBMP(base + currentNoteName + ".bmp");
  canvasDirty = false;
}

void deleteNote(String name) {
  String base = currentDir;
  if (!base.endsWith("/")) base += "/";
  SD.remove(base + name + ".txt");
  SD.remove(base + name + ".bmp");
}

// ============================================================
//  SCREEN: START
// ============================================================
void drawHome() {
  tft.fillScreen(COL_BG);
  tft.fillRect(0, 0, SCREEN_W, TOOLBAR_H, COL_ACCENT);
  tft.setTextColor(TFT_WHITE, COL_ACCENT);
  tft.setTextSize(2);
  tft.drawCentreString("NOTIZEN", SCREEN_W / 2, 6, 2);

  drawButton(20, 60, 200, 45, "Neue Notiz", COL_OK);
  drawButton(20, 115, 200, 45, "Notizen oeffnen", COL_BTN);
  drawButton(20, 170, 200, 45, "Ordner waehlen", COL_BTN);

  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 230);
  tft.print("Aktueller Ordner:");
  tft.setCursor(10, 245);
  String disp = currentDir;
  if (disp.length() > 38) disp = "..." + disp.substring(disp.length() - 35);
  tft.print(disp);
}

void handleHomeTouch(int x, int y) {
  if (y > 60 && y < 105) {
    String name = askText("Name der Notiz");
    if (name.length() > 0) newNote(name);
  } else if (y > 115 && y < 160) {
    currentScreen = SCR_NOTES; listScroll = 0; needsRedraw = true;
  } else if (y > 170 && y < 215) {
    browsePath = currentDir;
    currentScreen = SCR_FOLDER; listScroll = 0; needsRedraw = true;
  }
}

// ============================================================
//  SCREEN: ORDNER WAEHLEN
// ============================================================
void drawFolderScreen() {
  tft.fillScreen(COL_BG);
  tft.fillRect(0, 0, SCREEN_W, TOOLBAR_H, COL_ACCENT);
  tft.setTextColor(TFT_WHITE, COL_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(4, 12);
  String disp = browsePath;
  if (disp.length() > 20) disp = "..." + disp.substring(disp.length() - 17);
  tft.print(disp);

  drawButton(124, 2, 24, 28, "^", COL_BTN);    // Liste hoch
  drawButton(152, 2, 24, 28, "v", COL_BTN);    // Liste runter
  drawButton(200, 2, 36, 28, "<-", COL_DANGER); // zurueck zur Startseite

  std::vector<String> dirs;
  listSubdirs(browsePath, dirs);
  bool hasUp = (browsePath != "/");

  int y = CONTENT_Y + 4;
  int rowH = 26;

  if (hasUp) {
    tft.fillRoundRect(5, y, 230, rowH - 4, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.drawCentreString(".. (eine Ebene hoch)", 120, y + 5, 1);
    y += rowH;
  }

  for (int i = listScroll; i < (int)dirs.size() && y < KB_Y - rowH; i++) {
    tft.fillRoundRect(5, y, 230, rowH - 4, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.drawCentreString(dirs[i], 120, y + 5, 1);
    y += rowH;
  }

  drawButton(5,   KB_Y + 4, 110, 36, "Neuer Ordner", COL_BTN);
  drawButton(125, KB_Y + 4, 110, 36, "Auswaehlen", COL_OK);
}

void handleFolderTouch(int x, int y) {
  if (y < TOOLBAR_H) {
    if (x >= 124 && x < 148) { if (listScroll > 0) listScroll--; needsRedraw = true; return; }
    if (x >= 152 && x < 176) { listScroll++; needsRedraw = true; return; }
    if (x >= 200) { currentScreen = SCR_HOME; needsRedraw = true; return; }
  }

  if (y >= KB_Y + 4 && y < KB_Y + 40) {
    if (x < 115) {
      String name = askText("Ordnername");
      if (name.length() > 0) {
        String np = browsePath;
        if (!np.endsWith("/")) np += "/";
        SD.mkdir(np + name);
      }
    } else {
      currentDir = browsePath;
      saveConfig();
      currentScreen = SCR_HOME;
    }
    needsRedraw = true;
    return;
  }

  std::vector<String> dirs;
  listSubdirs(browsePath, dirs);
  bool hasUp = (browsePath != "/");
  int rowY = CONTENT_Y + 4;
  int rowH = 26;

  if (hasUp && y >= rowY && y < rowY + rowH) {
    browsePath = parentPath(browsePath);
    listScroll = 0; needsRedraw = true; return;
  }
  if (hasUp) rowY += rowH;

  int idx = listScroll + (y - rowY) / rowH;
  if (y >= rowY && idx >= 0 && idx < (int)dirs.size()) {
    String np = browsePath;
    if (!np.endsWith("/")) np += "/";
    browsePath = np + dirs[idx];
    listScroll = 0; needsRedraw = true;
  }
}

// ============================================================
//  SCREEN: NOTIZEN-LISTE
// ============================================================
void drawNotesScreen() {
  tft.fillScreen(COL_BG);
  tft.fillRect(0, 0, SCREEN_W, TOOLBAR_H, COL_ACCENT);
  tft.setTextColor(TFT_WHITE, COL_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(4, 12);
  tft.print("Notizen");

  drawButton(124, 2, 24, 28, "^", COL_BTN);
  drawButton(152, 2, 24, 28, "v", COL_BTN);
  drawButton(200, 2, 36, 28, "<-", COL_DANGER);

  std::vector<String> names;
  listNoteNames(currentDir, names);

  int y = CONTENT_Y + 4;
  int rowH = 30;
  for (int i = listScroll; i < (int)names.size() && y < KB_Y - rowH; i++) {
    tft.fillRoundRect(5, y, 170, rowH - 4, 3, COL_BTN);
    tft.setTextColor(COL_TEXT, COL_BTN);
    tft.drawCentreString(names[i], 90, y + 8, 1);

    tft.fillRoundRect(180, y, 55, rowH - 4, 3, COL_DANGER);
    tft.setTextColor(TFT_WHITE, COL_DANGER);
    tft.drawCentreString("Loeschen", 207, y + 8, 1);
    y += rowH;
  }

  if (names.size() == 0) {
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setCursor(10, CONTENT_Y + 20);
    tft.print("Keine Notizen in diesem Ordner.");
  }
}

void handleNotesTouch(int x, int y) {
  if (y < TOOLBAR_H) {
    if (x >= 124 && x < 148) { if (listScroll > 0) listScroll--; needsRedraw = true; return; }
    if (x >= 152 && x < 176) { listScroll++; needsRedraw = true; return; }
    if (x >= 200) { currentScreen = SCR_HOME; needsRedraw = true; return; }
  }

  std::vector<String> names;
  listNoteNames(currentDir, names);
  int rowY = CONTENT_Y + 4;
  int rowH = 30;
  int idx = listScroll + (y - rowY) / rowH;

  if (y >= rowY && idx >= 0 && idx < (int)names.size()) {
    if (x >= 180) { deleteNote(names[idx]); needsRedraw = true; }
    else { openNote(names[idx]); }
  }
}

// ============================================================
//  SCREEN: EDITOR (Stift + Text, UEBEREINANDER dargestellt)
// ============================================================
void drawEditorScreen() {
  tft.fillScreen(COL_BG);

  // Werkzeugleiste
  drawButton(0,   0, 50, TOOLBAR_H, editMode == MODE_PEN  ? "STIFT*" : "STIFT", editMode == MODE_PEN  ? COL_BTN_ON : COL_BTN);
  drawButton(52,  0, 50, TOOLBAR_H, editMode == MODE_TEXT ? "TEXT*"  : "TEXT",  editMode == MODE_TEXT ? COL_BTN_ON : COL_BTN);
  drawButton(104, 0, 60, TOOLBAR_H, "SPEICHERN", COL_OK);
  drawButton(166, 0, 36, TOOLBAR_H, "X", COL_DANGER);     // Zeichnung loeschen
  drawButton(204, 0, 36, TOOLBAR_H, "<-", COL_BTN);       // zurueck zur Liste

  drawCanvasArea();   // Stift-Ebene + Text-Ebene uebereinander

  if (editMode == MODE_TEXT) {
    drawKeyboard(KB_Y);
  } else {
    tft.fillRect(0, KB_Y, SCREEN_W, KB_H, COL_BG);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextSize(1);
    tft.setCursor(10, KB_Y + 15);
    tft.print("Zum Zeichnen auf die Flaeche tippen.");
    tft.setCursor(10, KB_Y + 35);
    tft.print("Oben auf TEXT wechseln zum Schreiben.");
  }
}

void handleEditorTouch(int x, int y) {
  // Werkzeugleiste
  if (y < TOOLBAR_H) {
    if (x < 50)        { editMode = MODE_PEN;  needsRedraw = true; }
    else if (x < 102)  { editMode = MODE_TEXT; needsRedraw = true; }
    else if (x < 164)  { saveNote(); needsRedraw = true; }
    else if (x < 202)  { clearCanvas(); needsRedraw = true; }
    else                { currentScreen = SCR_NOTES; listScroll = 0; needsRedraw = true; }
    return;
  }

  // Inhaltsbereich (Stift oder reines Anzeigefeld im Textmodus)
  if (y >= CONTENT_Y && y < KB_Y) {
    if (editMode == MODE_PEN) {
      paintBrush(x, y - CONTENT_Y, 2);
      tft.fillCircle(x, y, 2, COL_INK);   // direktes Feedback ohne kompletten Neuaufbau
    }
    return;
  }

  // Tastatur (nur im Textmodus aktiv)
  if (editMode == MODE_TEXT && y >= KB_Y) {
    String key = keyboardHit(x, y, KB_Y);
    if (key == "\b")      { if (noteText.length() > 0) noteText.remove(noteText.length() - 1); needsRedraw = true; }
    else if (key == "\t") { kbShift = !kbShift; needsRedraw = true; }
    else if (key == "\s") { kbSymbols = !kbSymbols; needsRedraw = true; }
    else if (key == " ")  { noteText += " "; needsRedraw = true; }
    else if (key == "\n") { noteText += "\n"; needsRedraw = true; }
    else if (key.length() == 1) { noteText += key; needsRedraw = true; }
  }
}
