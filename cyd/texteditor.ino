#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Pins für den Touch-Controller des CYD
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Pins für die MicroSD-Karte des CYD
#define SD_CS 5

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

static const uint32_t screenWidth  = 240;
static const uint32_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

lv_obj_t * text_area;
lv_obj_t * keyboard;
lv_obj_t * file_list = NULL; // Liste für die Dateiauswahl

// =====================================================================
// ==================== CUSTOM TASTATUR LAYOUTS ========================
// =====================================================================

// Layout 1: Kleinbuchstaben (Untere Reihe hat "Dateien", rechts das '-')
static const char * kb_map_lowercase[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "-", "\n",
    LV_SYMBOL_UP, "y", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "Dateien", " ", LV_SYMBOL_NEW_LINE, LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kb_ctrl_lowercase[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 2, 1, 1, 1, 1, 1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 3, 4, LV_KEYBOARD_CTRL_BTN_FLAGS | 2, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
};

// Layout 2: Großbuchstaben (Das '-' wird hier automatisch zum '_')
static const char * kb_map_uppercase[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "_", "\n",
    LV_SYMBOL_UP, "Y", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "Dateien", " ", LV_SYMBOL_NEW_LINE, LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kb_ctrl_uppercase[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 2, 1, 1, 1, 1, 1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2,
    LV_KEYBOARD_CTRL_BTN_FLAGS | 3, 4, LV_KEYBOARD_CTRL_BTN_FLAGS | 2, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
};

// =====================================================================
// ==================== SYSTEM FUNKTIONEN ==============================
// =====================================================================

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        data->point.x = map(p.x, 300, 3700, 0, screenWidth);
        data->point.y = map(p.y, 280, 3800, 0, screenHeight);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// =====================================================================
// ==================== SD DATEIEN LOGIK ===============================
// =====================================================================

void save_text_to_sd(const char * text) {
    if(!SD.exists("/texteditor")) SD.mkdir("/texteditor");
    int fileNum = 0;
    String fileName = "/texteditor/text_" + String(fileNum) + ".txt";
    while(SD.exists(fileName)) {
        fileNum++;
        fileName = "/texteditor/text_" + String(fileNum) + ".txt";
    }
    File file = SD.open(fileName, FILE_WRITE);
    if(!file) return;
    file.print(text);
    file.close();
    Serial.printf("Gespeichert unter: %s\n", fileName.c_str());
}

static void file_select_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    const char * file_name = lv_list_get_btn_text(file_list, obj);
    
    String full_path = String(file_name);
    if(!full_path.startsWith("/texteditor/")) {
        full_path = "/texteditor/" + full_path;
    }
    
    File file = SD.open(full_path, FILE_READ);
    if(file) {
        String content = "";
        while(file.available()) {
            content += (char)file.read();
        }
        file.close();
        lv_textarea_set_text(text_area, content.c_str());
    }
    
    if(file_list) {
        lv_obj_del(file_list);
        file_list = NULL;
    }
}

void open_file_browser() {
    if(file_list) return;

    file_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(file_list, 240, 160);
    lv_obj_align(file_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(file_list, 0, 0);

    lv_obj_t * btn_close = lv_list_add_btn(file_list, LV_SYMBOL_CLOSE, " Schliessen");
    lv_obj_add_event_cb(btn_close, [](lv_event_t *e){
        lv_obj_del(file_list);
        file_list = NULL;
    }, LV_EVENT_CLICKED, NULL);

    File root = SD.open("/texteditor");
    if(!root || !root.isDirectory()) {
        lv_list_add_text(file_list, "Keine Dateien gefunden");
        return;
    }

    File file = root.openNextFile();
    while(file) {
        if(!file.isDirectory()) {
            lv_obj_t * btn = lv_list_add_btn(file_list, LV_SYMBOL_FILE, file.name());
            lv_obj_add_event_cb(btn, file_select_event_cb, LV_EVENT_CLICKED, NULL);
        }
        file = root.openNextFile();
    }
}

// =====================================================================
// ==================== TASTATUR EVENT HANDLER ========================
// =====================================================================

static void keyboard_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_READY) {
        save_text_to_sd(lv_textarea_get_text(text_area));
        lv_textarea_set_text(text_area, "");
    }
    
    if(code == LV_EVENT_VALUE_CHANGED) {
        uint16_t btn_id = lv_btnmatrix_get_selected_btn(obj);
        const char * txt = lv_btnmatrix_get_btn_text(obj, btn_id);
        if(txt == NULL) return;

        if(strcmp(txt, "Dateien") == 0) {
            open_file_browser();
        }
    }
}

// =====================================================================
// =============================== SETUP ===============================
// =====================================================================

void setup() {
    Serial.begin(115200);
    if(!SD.begin(SD_CS)) Serial.println("SD-Fehler!");

    tft.begin();
    tft.setRotation(0); 
    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(mySpi);
    ts.setRotation(0); 

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Textfeld (Obere Bildschirmhälfte)
    text_area = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(text_area, 240, 160); 
    lv_obj_align(text_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_textarea_set_placeholder_text(text_area, "Schreibe etwas...");
    lv_obj_set_style_radius(text_area, 0, 0); 
    lv_obj_set_style_border_width(text_area, 0, 0);
    lv_obj_set_style_pad_all(text_area, 8, 0);

    // Tastatur (Untere Bildschirmhälfte)
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, 240, 160);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(keyboard, 0, 0);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    
    // Matrizen zuweisen
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, (const char**)kb_map_lowercase, kb_ctrl_lowercase);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, (const char**)kb_map_uppercase, kb_ctrl_uppercase);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);

    lv_keyboard_set_textarea(keyboard, text_area);
    lv_obj_add_state(text_area, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
}

void loop() {
    lv_tick_inc(5); 
    lv_timer_handler(); 
    delay(5);
}
