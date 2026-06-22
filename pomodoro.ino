// ================= STOPPUHR APP =================

void stopwatchApp() {
  drainTouch();
  
  // Modus: 0 = Stoppuhr, 1 = Timer
  int mode = 0;
  
  // ===== STOPPUHR VARIABLEN =====
  bool stopwatchRunning = false;
  unsigned long stopwatchStartTime = 0;
  unsigned long stopwatchElapsedMs = 0;
  std::vector<unsigned long> splits;
  
  // ===== TIMER VARIABLEN =====
  enum TimerState { IDLE, RUNNING, PAUSED, FINISHED };
  TimerState timerState = IDLE;
  unsigned long timerDuration = 60000; // 60 Sekunden Standard
  unsigned long timerRemainingMs = 60000;
  unsigned long timerStartTime = 0;
  unsigned long timerPausedRemaining = 60000;
  
  // ===== GEMEINSAME VARIABLEN =====
  unsigned long lastDisplayUpdate = 0;
  bool uiDirty = true;
  
  auto formatTime = [](unsigned long ms) -> String {
    unsigned long totalSec = ms / 1000;
    unsigned long hours = totalSec / 3600;
    unsigned long minutes = (totalSec % 3600) / 60;
    unsigned long seconds = totalSec % 60;
    unsigned long millis = (ms % 1000) / 10;
    char buf[16];
    if (hours > 0) {
      snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu.%02lu", hours, minutes, seconds, millis);
    } else {
      snprintf(buf, sizeof(buf), "%02lu:%02lu.%02lu", minutes, seconds, millis);
    }
    return String(buf);
  };
  
  auto formatTimeShort = [](unsigned long ms) -> String {
    unsigned long totalSec = ms / 1000;
    unsigned long hours = totalSec / 3600;
    unsigned long minutes = (totalSec % 3600) / 60;
    unsigned long seconds = totalSec % 60;
    char buf[12];
    if (hours > 0) {
      snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    } else {
      snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
    }
    return String(buf);
  };
  
  auto drawUI = [&]() {
    tft.fillScreen(BG_COLOR);
    
    // Header
    tft.fillRect(0, 0, SCREEN_W, 28, HEADER_COLOR);
    tft.setTextColor(TEXT_COLOR, HEADER_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(mode == 0 ? "STOPPUHR" : "TIMER", 120, 14, 2);
    
    // Zurueck-Button
    tft.fillRoundRect(2, 2, 40, 24, 3, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("<", 22, 14, 2);
    
    // Mode Switch Button
    tft.fillRoundRect(180, 2, 55, 24, 3, TFT_BLUE);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.drawString(mode == 0 ? "TIMER" : "STOPP", 207, 14, 1);
    
    if (mode == 0) {
      // ===== STOPPUHR UI =====
      // Haupt-Zeitanzeige
      tft.fillRect(0, 32, SCREEN_W, 60, darkMode ? 0x1082 : 0xC618);
      tft.drawRect(0, 32, SCREEN_W, 60, BORDER_COLOR);
      tft.setTextColor(TFT_YELLOW, darkMode ? 0x1082 : 0xC618);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(formatTime(stopwatchElapsedMs), 120, 62, 4);
      
      // Status-Anzeige
      tft.setTextColor(stopwatchRunning ? TFT_GREEN : TFT_RED, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(stopwatchRunning ? "● LÄUFT" : "■ GESTOPPT", 120, 96, 1);
      
      // Steuerungs-Buttons
      int buttonY = 110;
      int buttonW = 55;
      int spacing = 6;
      int totalW = buttonW * 4 + spacing * 3;
      int startX = (SCREEN_W - totalW) / 2;
      
      drawButton(startX, buttonY, buttonW, 40, stopwatchRunning ? TFT_RED : TFT_GREEN, 
                 stopwatchRunning ? "STOP" : "START");
      drawButton(startX + buttonW + spacing, buttonY, buttonW, 40, TFT_BLUE, "SPLIT");
      drawButton(startX + (buttonW + spacing) * 2, buttonY, buttonW, 40, TFT_ORANGE, "RESET");
      drawButton(startX + (buttonW + spacing) * 3, buttonY, buttonW, 40, TFT_DARKGREY, "MENU");
      
      // Split-Liste
      int splitStartY = 160;
      tft.drawFastHLine(0, splitStartY - 4, SCREEN_W, BORDER_COLOR);
      tft.setTextColor(TEXT_COLOR, BG_COLOR);
      tft.setTextDatum(TL_DATUM);
      
      int maxSplits = min((int)splits.size(), 6);
      int startIdx = max(0, (int)splits.size() - 6);
      
      for (int i = 0; i < maxSplits; i++) {
        int idx = startIdx + i;
        int yPos = splitStartY + i * 26;
        tft.setCursor(8, yPos + 4);
        tft.setTextColor(TFT_LIGHTGREY, BG_COLOR);
        tft.print("#" + String(idx + 1) + " ");
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        unsigned long splitTime = splits[idx];
        tft.print(formatTime(splitTime));
        
        if (idx > 0) {
          unsigned long diff = splitTime - splits[idx - 1];
          tft.setTextColor(TFT_CYAN, BG_COLOR);
          tft.setCursor(150, yPos + 4);
          tft.print("+" + formatTime(diff));
        }
      }
      
    } else {
      // ===== TIMER UI =====
      // Haupt-Zeitanzeige
      uint16_t bgColor = (timerState == FINISHED) ? TFT_RED : (darkMode ? 0x1082 : 0xC618);
      tft.fillRect(0, 32, SCREEN_W, 60, bgColor);
      tft.drawRect(0, 32, SCREEN_W, 60, BORDER_COLOR);
      tft.setTextColor((timerState == FINISHED) ? TFT_WHITE : TFT_YELLOW, bgColor);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(formatTimeShort(timerRemainingMs), 120, 62, 4);
      
      // Status-Anzeige
      String statusText;
      uint16_t statusColor;
      switch(timerState) {
        case IDLE: statusText = "■ BEREIT"; statusColor = TFT_YELLOW; break;
        case RUNNING: statusText = "● LÄUFT"; statusColor = TFT_GREEN; break;
        case PAUSED: statusText = "■ PAUSIERT"; statusColor = TFT_ORANGE; break;
        case FINISHED: statusText = "● ABGELAUFEN!"; statusColor = TFT_RED; break;
      }
      tft.setTextColor(statusColor, BG_COLOR);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(statusText, 120, 96, 1);
      
      // Steuerungs-Buttons (5 Buttons nebeneinander)
      int buttonY = 110;
      int buttonW = 42;
      int spacing = 4;
      int totalW = buttonW * 5 + spacing * 4;
      int startX = (SCREEN_W - totalW) / 2;
      
      // START/PAUSE/WEITER Button
      String startLabel;
      uint16_t startColor;
      if (timerState == IDLE || timerState == FINISHED) {
        startLabel = "START";
        startColor = TFT_GREEN;
      } else if (timerState == RUNNING) {
        startLabel = "PAUSE";
        startColor = TFT_ORANGE;
      } else { // PAUSED
        startLabel = "WEITER";
        startColor = TFT_BLUE;
      }
      drawButton(startX, buttonY, buttonW, 40, startColor, startLabel);
      
      // RESET Button
      drawButton(startX + buttonW + spacing, buttonY, buttonW, 40, TFT_RED, "RESET");
      
      // +1 MIN Button
      drawButton(startX + (buttonW + spacing) * 2, buttonY, buttonW, 40, TFT_CYAN, "+1M");
      
      // -1 MIN Button
      drawButton(startX + (buttonW + spacing) * 3, buttonY, buttonW, 40, TFT_PURPLE, "-1M");
      
      // +10 SEK Button (NEU)
      drawButton(startX + (buttonW + spacing) * 4, buttonY, buttonW, 40, TFT_YELLOW, "+10S");
      
      // Zeit-Einstellung (nur im IDLE Modus sichtbar)
      if (timerState == IDLE || timerState == FINISHED) {
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Tippe +/- um Zeit einzustellen", 120, 165, 1);
        
        // Aktuelle Zeit anzeigen
        tft.setTextColor(TFT_CYAN, BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Aktuell: " + formatTimeShort(timerDuration), 120, 185, 2);
        
        // Fortschrittsbalken (wenn läuft)
      } else if (timerState == RUNNING || timerState == PAUSED) {
        int progressY = 170;
        tft.drawFastHLine(10, progressY, SCREEN_W - 20, BORDER_COLOR);
        float progress = 1.0 - ((float)timerRemainingMs / (float)timerDuration);
        if (progress < 0) progress = 0;
        if (progress > 1) progress = 1;
        int barWidth = (SCREEN_W - 20) * progress;
        tft.fillRect(10, progressY, barWidth, 8, TFT_GREEN);
        tft.drawRect(10, progressY, SCREEN_W - 20, 8, BORDER_COLOR);
        
        // Verbleibende Zeit in Minuten/Sekunden
        tft.setTextColor(TEXT_COLOR, BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Verbleibend: " + formatTimeShort(timerRemainingMs), 120, 195, 1);
      }
    }
    uiDirty = false;
  };
  
  // Initiales Zeichnen
  drawUI();
  
  // Haupt-Loop
  while (true) {
    unsigned long now = millis();
    
    // ===== STOPPUHR UPDATE =====
    if (mode == 0 && stopwatchRunning) {
      stopwatchElapsedMs = now - stopwatchStartTime;
      if (now - lastDisplayUpdate > 50) {
        // Nur Zeitanzeige updaten
        tft.fillRect(0, 32, SCREEN_W, 60, darkMode ? 0x1082 : 0xC618);
        tft.drawRect(0, 32, SCREEN_W, 60, BORDER_COLOR);
        tft.setTextColor(TFT_YELLOW, darkMode ? 0x1082 : 0xC618);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(formatTime(stopwatchElapsedMs), 120, 62, 4);
        lastDisplayUpdate = now;
      }
    }
    
    // ===== TIMER UPDATE =====
    if (mode == 1 && timerState == RUNNING) {
      unsigned long elapsed = now - timerStartTime;
      if (elapsed >= timerPausedRemaining) {
        timerRemainingMs = 0;
        timerState = FINISHED;
        uiDirty = true;
        // Alarm - kurzer Piepton oder Blinken
        for (int i = 0; i < 3; i++) {
          tft.fillRect(0, 32, SCREEN_W, 60, TFT_RED);
          delay(200);
          tft.fillRect(0, 32, SCREEN_W, 60, TFT_WHITE);
          delay(200);
        }
        drawUI();
      } else {
        timerRemainingMs = timerPausedRemaining - elapsed;
        if (now - lastDisplayUpdate > 50) {
          // Nur Zeitanzeige updaten
          uint16_t bgColor = darkMode ? 0x1082 : 0xC618;
          tft.fillRect(0, 32, SCREEN_W, 60, bgColor);
          tft.drawRect(0, 32, SCREEN_W, 60, BORDER_COLOR);
          tft.setTextColor(TFT_YELLOW, bgColor);
          tft.setTextDatum(MC_DATUM);
          tft.drawString(formatTimeShort(timerRemainingMs), 120, 62, 4);
          
          // Fortschrittsbalken updaten
          int progressY = 170;
          tft.drawFastHLine(10, progressY, SCREEN_W - 20, BG_COLOR);
          float progress = 1.0 - ((float)timerRemainingMs / (float)timerDuration);
          if (progress < 0) progress = 0;
          if (progress > 1) progress = 1;
          int barWidth = (SCREEN_W - 20) * progress;
          tft.fillRect(10, progressY, barWidth, 8, TFT_GREEN);
          tft.drawRect(10, progressY, SCREEN_W - 20, 8, BORDER_COLOR);
          
          lastDisplayUpdate = now;
        }
      }
    }
    
    int tx, ty;
    if (!getTouch(tx, ty)) {
      delay(10);
      continue;
    }
    
    // ===== HEADER BUTTONS =====
    if (ty < 28) {
      if (tx < 45) {
        // Zurueck
        drainTouch();
        drawMenu();
        return;
      } else if (tx > 175) {
        // Mode Switch
        drainTouch();
        mode = (mode == 0) ? 1 : 0;
        // Timer-Reset wenn zu Timer wechseln
        if (mode == 1 && timerState == IDLE) {
          timerRemainingMs = timerDuration;
        }
        drawUI();
      }
      delay(150);
      continue;
    }
    
    // ===== STOPPUHR BUTTONS =====
    if (mode == 0 && ty >= 110 && ty < 150) {
      int buttonW = 55;
      int spacing = 6;
      int totalW = buttonW * 4 + spacing * 3;
      int startX = (SCREEN_W - totalW) / 2;
      
      if (tx >= startX && tx < startX + buttonW) {
        // START/STOP
        drainTouch();
        if (stopwatchRunning) {
          stopwatchRunning = false;
          drawUI();
        } else {
          stopwatchRunning = true;
          stopwatchStartTime = millis() - stopwatchElapsedMs;
          drawUI();
        }
      } else if (tx >= startX + buttonW + spacing && tx < startX + (buttonW + spacing) * 2) {
        // SPLIT
        drainTouch();
        if (stopwatchRunning || stopwatchElapsedMs > 0) {
          splits.push_back(stopwatchElapsedMs);
          drawUI();
        }
      } else if (tx >= startX + (buttonW + spacing) * 2 && tx < startX + (buttonW + spacing) * 3) {
        // RESET
        drainTouch();
        stopwatchRunning = false;
        stopwatchElapsedMs = 0;
        splits.clear();
        drawUI();
      } else if (tx >= startX + (buttonW + spacing) * 3 && tx < startX + (buttonW + spacing) * 4) {
        // MENU
        drainTouch();
        drawMenu();
        return;
      }
      delay(200);
      continue;
    }
    
    // ===== TIMER BUTTONS =====
    if (mode == 1 && ty >= 110 && ty < 150) {
      int buttonW = 42;
      int spacing = 4;
      int totalW = buttonW * 5 + spacing * 4;
      int startX = (SCREEN_W - totalW) / 2;
      
      // START/PAUSE/WEITER (Index 0)
      if (tx >= startX && tx < startX + buttonW) {
        drainTouch();
        if (timerState == IDLE || timerState == FINISHED) {
          // START
          if (timerRemainingMs == 0) {
            timerRemainingMs = timerDuration;
          }
          timerState = RUNNING;
          timerPausedRemaining = timerRemainingMs;
          timerStartTime = millis();
          drawUI();
        } else if (timerState == RUNNING) {
          // PAUSE
          timerState = PAUSED;
          timerPausedRemaining = timerRemainingMs;
          drawUI();
        } else if (timerState == PAUSED) {
          // WEITER
          timerState = RUNNING;
          timerStartTime = millis();
          drawUI();
        }
      }
      // RESET (Index 1)
      else if (tx >= startX + buttonW + spacing && tx < startX + (buttonW + spacing) * 2) {
        drainTouch();
        timerState = IDLE;
        timerRemainingMs = timerDuration;
        drawUI();
      }
      // +1 MIN (Index 2)
      else if (tx >= startX + (buttonW + spacing) * 2 && tx < startX + (buttonW + spacing) * 3) {
        drainTouch();
        if (timerState == IDLE || timerState == FINISHED) {
          timerDuration += 60000;
          if (timerDuration > 3600000) timerDuration = 3600000; // Max 1 Stunde
          timerRemainingMs = timerDuration;
          drawUI();
        }
      }
      // -1 MIN (Index 3)
      else if (tx >= startX + (buttonW + spacing) * 3 && tx < startX + (buttonW + spacing) * 4) {
        drainTouch();
        if (timerState == IDLE || timerState == FINISHED) {
          if (timerDuration >= 60000) {
            timerDuration -= 60000;
            if (timerDuration < 1000) timerDuration = 1000; // Min 1 Sekunde
            timerRemainingMs = timerDuration;
            drawUI();
          }
        }
      }
      // +10 SEK (Index 4) - NEU
      else if (tx >= startX + (buttonW + spacing) * 4 && tx < startX + (buttonW + spacing) * 5) {
        drainTouch();
        if (timerState == IDLE || timerState == FINISHED) {
          timerDuration += 10000; // 10 Sekunden
          if (timerDuration > 3600000) timerDuration = 3600000; // Max 1 Stunde
          timerRemainingMs = timerDuration;
          drawUI();
        }
      }
      delay(200);
      continue;
    }
    
    delay(10);
  }
}
