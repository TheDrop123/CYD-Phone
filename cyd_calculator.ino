/*
  ============================================================
  CYD TASCHENRECHNER (Touch + Tastaturlayout)
  ============================================================
  - Einfacher Rechner mit Touch-Tastatur
  - Unterstützt: +, -, *, /
  - Dezimalpunkte und Clear-Funktion
  - Verwendet die gleiche getTouch() Funktion wie andere Apps
  ============================================================
*/

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ==================== PIN DEFINITIONEN ====================
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

// ==================== DISPLAY ====================
#define SCREEN_W   240
#define SCREEN_H   320
#define TOOLBAR_H  32
#define CONTENT_Y  TOOLBAR_H

// ==================== BUTTON LAYOUT ====================
#define BUTTON_WIDTH 55
#define BUTTON_HEIGHT 45
#define BUTTON_PADDING 3
#define BUTTON_START_X 3
#define BUTTON_START_Y 100

// ==================== FARBEN ====================
#define COL_BG      TFT_BLACK
#define COL_TEXT    TFT_WHITE
#define COL_BUTTON  0x2945      // Dunkelblau
#define COL_BUTTON_PRESS 0x4E8F // Heller
#define COL_TEXT_BTN TFT_WHITE
#define COL_OPERATOR TFT_RED
#define COL_EQUALS   TFT_GREEN
#define COL_CLEAR    TFT_MAGENTA
#define COL_DISPLAY  0x1082     // Navy Display-Hintergrund

// ==================== OBJEKTE ====================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ==================== CALCULATOR STATE ====================
String displayValue = "0";
float firstNumber = 0;
float secondNumber = 0;
String currentOperator = "";
bool newNumber = true;
bool needsRedraw = true;

// ==================== BUTTON STRUCT ====================
struct Button {
  int x, y, w, h;
  String label;
  uint16_t color;
};

Button buttons[17];
int buttonCount = 0;

// ==================== TOUCH FUNCTION ====================
bool getTouch(int &x, int &y) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = constrain(map(p.x, 200, 3800, 0, SCREEN_W - 1), 0, SCREEN_W - 1);
    y = constrain(map(p.y, 200, 3800, 0, SCREEN_H - 1), 0, SCREEN_H - 1);
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
  tft.fillScreen(COL_BG);
  
  // Touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);
  
  Serial.println("CYD Calculator initialized");
  
  initializeButtons();
  drawCalculator();
}

// ==================== LOOP ====================
void loop() {
  if (needsRedraw) {
    drawCalculator();
    needsRedraw = false;
  }
  
  int x, y;
  if (getTouch(x, y)) {
    handleTouchInput(x, y);
    delay(150);
  }
}

// ==================== INITIALIZE BUTTONS ====================
void initializeButtons() {
  int x = BUTTON_START_X;
  int y = BUTTON_START_Y;
  int cols = 4;
  
  String labels[] = {"7", "8", "9", "/", 
                     "4", "5", "6", "*", 
                     "1", "2", "3", "-", 
                     "0", ".", "=", "+"};
  
  buttonCount = 0;
  for (int i = 0; i < 16; i++) {
    buttons[i].x = x + (i % cols) * (BUTTON_WIDTH + BUTTON_PADDING);
    buttons[i].y = y + (i / cols) * (BUTTON_HEIGHT + BUTTON_PADDING);
    buttons[i].w = BUTTON_WIDTH;
    buttons[i].h = BUTTON_HEIGHT;
    buttons[i].label = labels[i];
    
    if (labels[i] == "/" || labels[i] == "*" || labels[i] == "-" || labels[i] == "+") {
      buttons[i].color = COL_OPERATOR;
    } else if (labels[i] == "=") {
      buttons[i].color = COL_EQUALS;
    } else {
      buttons[i].color = COL_BUTTON;
    }
    buttonCount++;
  }
  
  // Clear button - spans full width
  buttons[16].x = BUTTON_START_X;
  buttons[16].y = y + 4 * (BUTTON_HEIGHT + BUTTON_PADDING);
  buttons[16].w = SCREEN_W - 6;
  buttons[16].h = BUTTON_HEIGHT;
  buttons[16].label = "C";
  buttons[16].color = COL_CLEAR;
  buttonCount++;
}

// ==================== DRAW CALCULATOR ====================
void drawCalculator() {
  tft.fillScreen(COL_BG);
  
  // Toolbar
  tft.fillRect(0, 0, SCREEN_W, TOOLBAR_H, 0x4208);
  tft.setTextColor(TFT_WHITE, 0x4208);
  tft.setTextSize(2);
  tft.drawCentreString("CALCULATOR", SCREEN_W / 2, 6, 2);
  
  // Display area
  tft.fillRect(5, 45, SCREEN_W - 10, 40, COL_DISPLAY);
  tft.drawRect(5, 45, SCREEN_W - 10, 40, COL_TEXT);
  
  // Display text
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(2);
  tft.setCursor(15, 55);
  
  String disp = displayValue;
  if (disp.length() > 15) disp = disp.substring(disp.length() - 15);
  tft.print(disp);
  
  // Draw all buttons
  for (int i = 0; i < buttonCount; i++) {
    drawButton(i, false);
  }
}

// ==================== DRAW BUTTON ====================
void drawButton(int index, boolean pressed) {
  Button btn = buttons[index];
  uint16_t bgColor = pressed ? COL_BUTTON_PRESS : btn.color;
  
  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 3, bgColor);
  tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 3, COL_TEXT);
  
  // Draw text centered
  uint16_t textColor = (bgColor == COL_OPERATOR || bgColor == COL_EQUALS || bgColor == COL_CLEAR) 
                        ? TFT_WHITE : COL_TEXT_BTN;
  tft.setTextColor(textColor);
  tft.setTextSize(2);
  
  int16_t textX = btn.x + btn.w / 2;
  int16_t textY = btn.y + btn.h / 2 - 8;
  
  tft.drawCentreString(btn.label, textX, textY, 2);
}

// ==================== HANDLE TOUCH ====================
void handleTouchInput(int x, int y) {
  for (int i = 0; i < buttonCount; i++) {
    Button btn = buttons[i];
    
    if (x >= btn.x && x <= btn.x + btn.w && 
        y >= btn.y && y <= btn.y + btn.h) {
      
      // Visual feedback
      drawButton(i, true);
      delay(100);
      
      // Handle button press
      handleButtonPress(btn.label);
      needsRedraw = true;
      Serial.println(displayValue);
      return;
    }
  }
}

// ==================== HANDLE BUTTON PRESS ====================
void handleButtonPress(String label) {
  if (label == "C") {
    // Clear all
    displayValue = "0";
    firstNumber = 0;
    secondNumber = 0;
    currentOperator = "";
    newNumber = true;
  }
  else if (label == "=" && currentOperator != "") {
    // Calculate result
    secondNumber = displayValue.toFloat();
    float result = performCalculation(firstNumber, secondNumber, currentOperator);
    displayValue = String(result, 6);  // 6 decimal places
    currentOperator = "";
    newNumber = true;
  }
  else if (label == "+" || label == "-" || label == "*" || label == "/") {
    // Operator pressed
    if (currentOperator != "" && !newNumber) {
      // Previous operation exists, calculate it first
      secondNumber = displayValue.toFloat();
      float result = performCalculation(firstNumber, secondNumber, currentOperator);
      displayValue = String(result, 6);
      firstNumber = result;
    } else {
      firstNumber = displayValue.toFloat();
    }
    currentOperator = label;
    newNumber = true;
  }
  else if (label == ".") {
    // Decimal point
    if (!displayValue.contains(".")) {
      displayValue += ".";
    }
  }
  else {
    // Number pressed
    if (newNumber) {
      displayValue = label;
      newNumber = false;
    } else {
      if (displayValue == "0") {
        displayValue = label;
      } else {
        displayValue += label;
      }
    }
  }
}

// ==================== PERFORM CALCULATION ====================
float performCalculation(float num1, float num2, String operation) {
  if (operation == "+") return num1 + num2;
  if (operation == "-") return num1 - num2;
  if (operation == "*") return num1 * num2;
  if (operation == "/") return num2 != 0 ? num1 / num2 : 0;
  return 0;
}
