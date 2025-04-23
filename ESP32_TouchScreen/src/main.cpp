// ESP32 Touch Screen Version 3.0//

// bring in needed libraries 
#include <TFT_eSPI.h>
#include <cstdint>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <WiFi.h>
#include <math.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <SPIFFS.h>
#include <ESPASyncWebServer.h>
#include <WebSocketsServer.h>

// Read pin for esp32 data theremin data
const int analogPin = 34;  // GPIO34 = ADC1 Chan 6
const char* gui_ver = "ThereminOS Ver. 3.0";

//Webserver Setup
const char* ssid = "Boneca-Ambalabu";
const char* password = "Siddiqui2025";

int interval = 250;                                  // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0;                     // we use the "millis()" command for time reference and this will output an unsigned long
uint8_t broadcastAddress[] = {0x70, 0xb8, 0xf6, 0x5c, 0x9e, 0x7c};
//70:b8:f6:5c:9e:7c
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
lv_obj_t* harm_buttons[10];
    
void lv_create_harmonics_gui(void);
void lv_create_equalizer_gui(void);
void lv_create_frequency_gui(void);
void lv_create_main_gui(void);
void sendSettingsUpdate();
String settingsToJson();

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 13  // T_DIN
#define XPT2046_MISO 19  // T_OUT
#define XPT2046_CLK 14   // T_CLK
#define XPT2046_CS 33    // T_CS
    
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
    
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
    
// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;
int freq = 3000;
String note = "";

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

static lv_obj_t * slider_label1;
static lv_obj_t * slider_label2;
static lv_obj_t * slider_label3;
lv_obj_t * screen;
lv_obj_t * screen_hrmncs;
lv_obj_t * screen_eqlzr;
lv_obj_t * screen_freq_sel;
lv_obj_t * btn_label;
lv_obj_t * eqlzr_btn;
lv_obj_t * hrmncs_btn;
lv_obj_t * ext_btn;
lv_obj_t * frqncy_sel_btn;
lv_obj_t * rst_btn;
lv_obj_t * sin_btn;
lv_obj_t * tri_btn;
lv_obj_t * sqr_btn;

  //Enum lol thanks chat
  //70:b8:f6:5c:cc:c0 MAC ADDRESS
  //70:b8:f6:5c:9e:7c Terra MAC ADDRESS
  typedef enum {
    MAIN_SCREEN,
    HARM_SCREEN,
    EQ_SCREEN,
    FREQ_SCREEN,
    RST
  } ts_data_t;

  typedef enum {
    low_t,
    mid_t,
    high_t
  } eq_id_t;

  typedef enum {
    BONE,
    B2,
    B3,
    B4,
    B5,
    B6,
    B7,
    B8,
    B9,
    BTEN,
    SINE_WAVE,
    TRI_WAVE,
    SQR_WAVE
  } harm_and_wave_data_t;

  typedef struct struct_message {
    String msg;
  } struct_message;

  String success;
  esp_now_peer_info_t peerInfo;

  //info to transfer to other ESP32
  int low;
  int mid;
  int high;
  bool harm_data[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  bool wave_data[3] = {1, 0, 0};
  int last_low_val = 6;   // Default center value (you’re using range 0–12)
  int last_mid_val = 6;
  int last_high_val = 6;
  bool last_wave_data[3] = {1, 0, 0};
  bool last_harm_data[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


  struct settings_struct {
    int l;
    int m;
    int h;
    bool h_data[10];
    bool w_data[3];
  };

  settings_struct settings;

  void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status ==0){
      success = "Delivery Success :)";
    }
    else{
      success = "Delivery Fail :(";
    }
  }

  // If logging is enabled, it will inform the user about what is happening in the library
  void log_print(lv_log_level_t level, const char * buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
  }
  
  // Get the Touchscreen data
  void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
    // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
    if(touchscreen.touched()) {
      // Get Touchscreen points
      TS_Point p = touchscreen.getPoint();

      // Calibrate Touchscreen points with map function to the correct width and height
      x = map(p.x, 3950, 200, 0, SCREEN_WIDTH);
      y = map(p.y, 3900, 150, 0, SCREEN_HEIGHT);
      z = p.z;

      data->state = LV_INDEV_STATE_PRESSED;
  
      // Set the coordinates
      data->point.x = x;
      data->point.y = y;
    }
    else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  
  // Callback that prints the current slider value on the TFT display and Serial Monitor for debugging purposes
  static void slider_event_callback(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*) lv_event_get_target(e);
    eq_id_t id = (eq_id_t)(uintptr_t)lv_event_get_user_data(e);
    int val = lv_slider_get_value(slider) - 6;
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)lv_slider_get_value(slider));
    switch(id) {
      case low_t: 
        last_low_val = val; 
        break;
      case mid_t: 
        last_mid_val = val; 
        break;
      case high_t: 
        last_high_val = val; 
        break;
    }
    sendSettingsUpdate();
  }

  static void event_handler_btn(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      ts_data_t screen_sel = (ts_data_t) (uintptr_t) lv_event_get_user_data(e);
      switch(screen_sel) {
        case MAIN_SCREEN:
        if (!screen) screen = lv_obj_create(NULL);
          lv_scr_load(screen);
          lv_obj_clean(screen);
          lv_create_main_gui();
          break;
        case HARM_SCREEN:
          lv_scr_load(screen_hrmncs);
          lv_obj_clean(screen_hrmncs);
          lv_create_harmonics_gui();
          break;
        case EQ_SCREEN:
          lv_scr_load(screen_eqlzr);
          lv_obj_clean(screen_eqlzr);
          lv_create_equalizer_gui();
          break;
        case FREQ_SCREEN:
          lv_scr_load(screen_freq_sel);
          lv_obj_clean(screen_freq_sel);
          lv_create_frequency_gui();
          break;
        case RST:
          for (int i = 0; i < 10; i++) {
            harm_data[i] = 0;
            // Clear toggle state
            lv_obj_clear_state(harm_buttons[i], LV_STATE_CHECKED);
            // Reset color (back to blue)
            lv_obj_set_style_bg_color(harm_buttons[i], lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          }
          break;
      }
    }
    sendSettingsUpdate();
  }

  void toggle_handler_btn(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t*) lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
      LV_UNUSED(obj);
      harm_and_wave_data_t hrm_sel = (harm_and_wave_data_t) (uintptr_t) lv_event_get_user_data(e);
      switch(hrm_sel) {
        case BONE:
          harm_data[0] ^= 1;
          last_harm_data[0] = harm_data[0];
          break;
        case B2:
          harm_data[1] ^= 1;
          last_harm_data[1] = harm_data[1];
          break;
        case B3:
          harm_data[2] ^= 1;
          last_harm_data[2] = harm_data[2];
          break;
        case B4:
          harm_data[3] ^= 1;
          last_harm_data[3] = harm_data[3];
          break;
        case B5:
          harm_data[4] ^= 1;
          last_harm_data[4] = harm_data[4];
          break;
        case B6:
          harm_data[5] ^= 1;
          last_harm_data[5] = harm_data[5];
          break;
        case B7:
          harm_data[6] ^= 1;
          last_harm_data[6] = harm_data[6];
          break;
        case B8:
          harm_data[7] ^= 1;
          last_harm_data[7] = harm_data[7];
          break;
        case B9:
          harm_data[8] ^= 1;
          last_harm_data[8] = harm_data[8];
          break;
        case BTEN:
          harm_data[9] ^= 1;
          last_harm_data[9] = harm_data[9];
          break;
        case SINE_WAVE:
          wave_data[0] = 1;
          wave_data[1] = 0;
          wave_data[2] = 0;
          last_wave_data[0] = 1;
          last_wave_data[1] = 0;
          last_wave_data[2] = 0;
          lv_obj_clear_state(sqr_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(sqr_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          lv_obj_clear_state(tri_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(tri_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          break;
      case TRI_WAVE:
        wave_data[0] = 0;
        wave_data[1] = 1;
        wave_data[2] = 0;
        last_wave_data[0] = 0;
        last_wave_data[1] = 1;
        last_wave_data[2] = 0;
          lv_obj_clear_state(sin_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(sin_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          lv_obj_clear_state(sqr_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(sqr_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          break;
      case SQR_WAVE:
        wave_data[0] = 0;
        wave_data[1] = 0;
        wave_data[2] = 1;
        last_wave_data[0] = 0;
        last_wave_data[1] = 0;
        last_wave_data[2] = 1;
          lv_obj_clear_state(sin_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(sin_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          lv_obj_clear_state(tri_btn, LV_STATE_CHECKED);
          lv_obj_set_style_bg_color(tri_btn, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
          break;
      }
    }
    sendSettingsUpdate();
  }

  void lv_create_harmonics_gui(void) {
    for (int i = 0; i < 10; i++) {
      harm_buttons[i] = NULL;
    }
    // Create a Button (hrmncs_btn)
    ext_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(ext_btn, event_handler_btn, LV_EVENT_ALL, (void *)MAIN_SCREEN);
    lv_obj_align(ext_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_remove_flag(ext_btn, LV_OBJ_FLAG_PRESS_LOCK);
  
    btn_label = lv_label_create(ext_btn);
    lv_label_set_text(btn_label, "X");
    lv_obj_center(btn_label);

    lv_obj_t * hrm = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(hrm, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text(hrm, "Harmonics");
    lv_obj_set_width(hrm, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_height(hrm, 60);
    lv_obj_set_style_text_align(hrm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hrm, LV_ALIGN_CENTER, 0, -50);

    for (int i = 0; i < 10; i++) {
      harm_buttons[i] = lv_button_create(lv_screen_active());
      harm_and_wave_data_t btn_id;
      btn_id = (harm_and_wave_data_t)i;
      lv_obj_add_event_cb(harm_buttons[i], toggle_handler_btn, LV_EVENT_ALL, (void *)btn_id);
      if (i < 5) {
        lv_obj_align(harm_buttons[i], LV_ALIGN_CENTER, -80 + i * 40, -30);
      } else {
        lv_obj_align(harm_buttons[i], LV_ALIGN_CENTER, -80 + (i - 5) * 40, 30);
      }
      lv_obj_add_flag(harm_buttons[i], LV_OBJ_FLAG_CHECKABLE);
      if (last_harm_data[i]) {
        lv_obj_add_state(harm_buttons[i], LV_STATE_CHECKED);
      }
      lv_obj_set_height(harm_buttons[i], LV_SIZE_CONTENT);
      btn_label = lv_label_create(harm_buttons[i]);
      char btn_txt[3];  // make sure the buffer is large enough
      snprintf(btn_txt, sizeof(btn_txt), "%d", i + 1);
      lv_label_set_text(btn_label, btn_txt); 
      lv_obj_center(btn_label);
    }
    
    lv_obj_t * ts_ver = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ts_ver, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text(ts_ver, gui_ver);
    lv_obj_set_width(ts_ver, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_style_text_align(ts_ver, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ts_ver, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    rst_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(rst_btn, event_handler_btn, LV_EVENT_ALL, (void *)RST);
    lv_obj_align(rst_btn, LV_ALIGN_CENTER, 20, 80);
    lv_obj_remove_flag(ext_btn, LV_OBJ_FLAG_PRESS_LOCK);

    btn_label = lv_label_create(rst_btn);
    lv_label_set_text(btn_label, "Reset");
    lv_obj_center(btn_label);
  }

  void lv_create_equalizer_gui(void) {
    // Create a Button (hrmncs_btn)
    ext_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(ext_btn, event_handler_btn, LV_EVENT_ALL, (void *)MAIN_SCREEN);
    lv_obj_align(ext_btn, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_remove_flag(ext_btn, LV_OBJ_FLAG_PRESS_LOCK);
  
    btn_label = lv_label_create(ext_btn);
    lv_label_set_text(btn_label, "X");
    lv_obj_center(btn_label);

    // Create a slider aligned in the center bottom of the TFT display
    lv_obj_t * slider = lv_slider_create(lv_screen_active());
    lv_obj_align(slider, LV_ALIGN_CENTER, -100, 0);
    lv_obj_set_size(slider, 20, 150);
    lv_obj_add_event_cb(slider, slider_event_callback, LV_EVENT_RELEASED, (void *)low_t);
    lv_slider_set_range(slider, 0, 12);
    lv_slider_set_value(slider, last_low_val, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider, 2000, 0);

    slider_label1 = lv_label_create(lv_screen_active());
    lv_label_set_text(slider_label1, "Low");
    lv_obj_align_to(slider_label1, slider, LV_ALIGN_CENTER, -50, 0);

    lv_obj_t * slider2 = lv_slider_create(lv_screen_active());
    lv_obj_align(slider2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(slider2, 20, 150);
    lv_obj_add_event_cb(slider2, slider_event_callback, LV_EVENT_RELEASED, (void *)mid_t);
    lv_slider_set_range(slider2, 0, 12);
    lv_slider_set_value(slider2, last_mid_val, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider2, 2000, 0);
    
    slider_label2 = lv_label_create(lv_screen_active());
    lv_label_set_text(slider_label2, "Mid");
    lv_obj_align_to(slider_label2, slider2, LV_ALIGN_CENTER, -50, 0);

    lv_obj_t * slider3 = lv_slider_create(lv_screen_active());
    lv_obj_align(slider3, LV_ALIGN_CENTER, 100, 0);
    lv_obj_set_size(slider3, 20, 150);
    lv_obj_add_event_cb(slider3, slider_event_callback, LV_EVENT_RELEASED, (void *)high_t);
    lv_slider_set_range(slider3, 0, 12);
    lv_slider_set_value(slider3, last_high_val, LV_ANIM_OFF);
    lv_obj_set_style_anim_duration(slider3, 2000, 0);

    // Create a label below the slider to display the current slider value
    slider_label3 = lv_label_create(lv_screen_active());
    lv_label_set_text(slider_label3, "High");
    lv_obj_align_to(slider_label3, slider3, LV_ALIGN_CENTER, -60, 0);

    lv_obj_t * ts_ver = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ts_ver, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text(ts_ver, gui_ver);
    lv_obj_set_width(ts_ver, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_style_text_align(ts_ver, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ts_ver, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }

  void lv_create_frequency_gui(void) { 
      // Create a Button (hrmncs_btn)
      ext_btn = lv_button_create(lv_screen_active());
      lv_obj_add_event_cb(ext_btn, event_handler_btn, LV_EVENT_ALL, (void *)MAIN_SCREEN);
      lv_obj_align(ext_btn, LV_ALIGN_TOP_LEFT, 0, 0);
      lv_obj_remove_flag(ext_btn, LV_OBJ_FLAG_PRESS_LOCK);

      btn_label = lv_label_create(ext_btn);
      lv_label_set_text(btn_label, "X");
      lv_obj_center(btn_label);

      lv_obj_t * freq_sel_txt = lv_label_create(lv_screen_active());
      lv_label_set_long_mode(freq_sel_txt, LV_LABEL_LONG_WRAP);    // Breaks the long lines
      lv_label_set_text(freq_sel_txt, "Select a Wave Type:");
      lv_obj_set_width(freq_sel_txt, 150);    // Set smaller width to make the lines wrap
      lv_obj_set_style_text_align(freq_sel_txt, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(freq_sel_txt, LV_ALIGN_CENTER, 0, -20);

      sin_btn = lv_button_create(lv_screen_active());
      lv_obj_add_event_cb(sin_btn, toggle_handler_btn, LV_EVENT_ALL, (void *)SINE_WAVE);
      lv_obj_align(sin_btn, LV_ALIGN_CENTER, -110, 20);
      lv_obj_add_flag(sin_btn, LV_OBJ_FLAG_CHECKABLE);
      if (last_wave_data[0]) lv_obj_add_state(sin_btn, LV_STATE_CHECKED);
      lv_obj_set_height(sin_btn, LV_SIZE_CONTENT);

      btn_label = lv_label_create(sin_btn);
      lv_label_set_text(btn_label, "Sin");
      lv_obj_center(btn_label);

      tri_btn = lv_button_create(lv_screen_active());
      lv_obj_add_event_cb(tri_btn, toggle_handler_btn, LV_EVENT_ALL, (void *)TRI_WAVE);
      lv_obj_align(tri_btn, LV_ALIGN_CENTER, 0, 20);
      lv_obj_add_flag(tri_btn, LV_OBJ_FLAG_CHECKABLE);
      if (last_wave_data[1]) lv_obj_add_state(tri_btn, LV_STATE_CHECKED);
      lv_obj_set_height(tri_btn, LV_SIZE_CONTENT);

      btn_label = lv_label_create(tri_btn);
      lv_label_set_text(btn_label, "Tri");
      lv_obj_center(btn_label);

      sqr_btn = lv_button_create(lv_screen_active());
      lv_obj_add_event_cb(sqr_btn, toggle_handler_btn, LV_EVENT_ALL, (void *)SQR_WAVE);
      lv_obj_align(sqr_btn, LV_ALIGN_CENTER, 110, 20);
      lv_obj_add_flag(sqr_btn, LV_OBJ_FLAG_CHECKABLE);
      if (last_wave_data[2]) lv_obj_add_state(sqr_btn, LV_STATE_CHECKED);
      lv_obj_set_height(sqr_btn, LV_SIZE_CONTENT);

      btn_label = lv_label_create(sqr_btn);
      lv_label_set_text(btn_label, "Sqr");
      lv_obj_center(btn_label);

      lv_obj_t * ts_ver = lv_label_create(lv_screen_active());
      lv_label_set_long_mode(ts_ver, LV_LABEL_LONG_WRAP);    // Breaks the long lines
      lv_label_set_text(ts_ver, gui_ver);
      lv_obj_set_width(ts_ver, 150);    // Set smaller width to make the lines wrap
      lv_obj_set_style_text_align(ts_ver, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_align(ts_ver, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }

  void lv_create_main_gui(void) {
    lv_obj_t * display_freq = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(display_freq, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    /*
    PUT THIS INTO LOOP ALONG WITH NOTE TEXT EVENTUALLY
    char freq_text[32];  // make sure the buffer is large enough
    snprintf(freq_text, sizeof(freq_text), "Frequency | %d Hz", freq);
    lv_label_set_text(display_freq, freq_text);
    */
    lv_label_set_text(display_freq, "Frequency | X Hz");
    lv_obj_set_width(display_freq, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_height(display_freq, 60);
    lv_obj_set_style_text_align(display_freq, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(display_freq, LV_ALIGN_CENTER, 0, -110);

    lv_obj_t * display_note = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(display_note, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text(display_note, "Note | X");
    lv_obj_set_width(display_note, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_style_text_align(display_note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(display_note, LV_ALIGN_CENTER, 0, -90);

    lv_obj_t * ts_ver = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(ts_ver, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text(ts_ver, gui_ver);
    lv_obj_set_width(ts_ver, 150);    // Set smaller width to make the lines wrap
    lv_obj_set_style_text_align(ts_ver, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ts_ver, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    // Create a Button (hrmncs_btn)
    hrmncs_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(hrmncs_btn, event_handler_btn, LV_EVENT_ALL, (void *)HARM_SCREEN);
    lv_obj_align(hrmncs_btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_remove_flag(hrmncs_btn, LV_OBJ_FLAG_PRESS_LOCK);
  
    btn_label = lv_label_create(hrmncs_btn);
    lv_label_set_text(btn_label, "Harmonics");
    lv_obj_center(btn_label);
  
    eqlzr_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(eqlzr_btn, event_handler_btn, LV_EVENT_ALL, (void *)EQ_SCREEN);
    lv_obj_align(eqlzr_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(eqlzr_btn, LV_OBJ_FLAG_PRESS_LOCK);
  
    btn_label = lv_label_create(eqlzr_btn);
    lv_label_set_text(btn_label, "Equalizer");
    lv_obj_center(btn_label);

    frqncy_sel_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(frqncy_sel_btn, event_handler_btn, LV_EVENT_ALL, (void *)FREQ_SCREEN);
    lv_obj_align(frqncy_sel_btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_remove_flag(frqncy_sel_btn, LV_OBJ_FLAG_PRESS_LOCK);
  
    btn_label = lv_label_create(frqncy_sel_btn);
    lv_label_set_text(btn_label, "Frequency Select");
    lv_obj_center(btn_label);
  }

  void setup() {
    String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.begin(115200);
    Serial.println(LVGL_Arduino); 

    last_low_val = 6;   // Default center value (you’re using range 0–12)
    last_mid_val = 6;
    last_high_val = 6;

    // Start LVGL
    lv_init();
    // Register print function for debugging
    lv_log_register_print_cb(log_print);
  
    // Start the SPI for the touchscreen and init the touchscreen
    touchscreenSPI.begin(XPT2046_CLK, 19, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin();
    // Set the Touchscreen rotation in landscape mode
    // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 0: touchscreen.setRotation(0);
    touchscreen.setRotation(2);
    // Create a display object
    lv_display_t * disp;
    // Initialize the TFT display using the TFT_eSPI library
    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
      
    // Initialize an LVGL input device object (Touchscreen)
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    // Set the callback function to read Touchscreen input
    lv_indev_set_read_cb(indev, touchscreen_read);
    void * scrn_sel = (void *) 1;
    Serial.printf("%d", scrn_sel);

    screen = lv_obj_create(NULL);
    screen_hrmncs = lv_obj_create(NULL);
    screen_eqlzr = lv_obj_create(NULL);
    screen_freq_sel = lv_obj_create(NULL);

    lv_scr_load(screen);
    // Function to draw the GUI (text, buttons and sliders)
    lv_create_main_gui();

    //Setup for webserver
if (!SPIFFS.begin(true)) {
  Serial.println("SPIFFS Mount Failed");
  return;
}

WiFi.mode(WIFI_AP);
Serial.println(WiFi.softAP(ssid, password) ? "\nWiFi AP Ready!" : "\nWiFi AP Failed!");
Serial.print("IP address = ");
Serial.println(WiFi.softAPIP());

server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
/*
server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
  int sensorValue = analogRead(analogPin);
  float voltage = (sensorValue / 4095.0) * 3.3;
  String json = "{\"Voltage\": " + String(voltage,3) + "}";
  request->send(200, "application/json", json);
});
*/
server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
  String json = settingsToJson();
  request->send(200, "application/json", json);
});

server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/index.html");
});
//server.onNotFound([](AsyncWebServerRequest *request) {
//  request->send(404, "text/plain", "File not found.");
//});

webSocket.begin();
//webSocket.onEvent(webSocketEvent);

server.begin();

if (esp_now_init() != ESP_OK) {
  Serial.println("Error initializing ESP-NOW");
  return;
}

// Once ESPNow is successfully Init, we will register for Send CB to
// get the status of Trasnmitted packet
esp_now_register_send_cb(OnDataSent);

// Register peer
memcpy(peerInfo.peer_addr, broadcastAddress, 6);
peerInfo.channel = 0;  
peerInfo.encrypt = false;

// Add peer        
if (esp_now_add_peer(&peerInfo) != ESP_OK){
  Serial.println("Failed to add peer");
  return;
} else {
  Serial.println("Added Peer Successfully!!");
}
}
  
void loop() {
  //Data to send to other ESP32
  settings.l = last_low_val;
  settings.m = last_mid_val;
  settings.h = last_high_val;
  for (int i = 0; i < 10; i++) {
    settings.h_data[i] = harm_data[i];
  }
  for (int i = 0; i < 3; i++) {
    settings.w_data[i] = wave_data[i];
  }
  //Serial.printf("%d %d %d", last_low_val, last_mid_val, last_high_val);
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &settings, sizeof(settings));
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  } else {
    Serial.println("Error sending data");
  }
  

  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
  
  webSocket.loop();                                       // Update function for the webSockets 
  unsigned long now = millis();                           // read out the current "time" ("millis()" gives the time in ms since the ESP started)
  if ((unsigned long)(now - previousMillis) > interval) { // check if "interval" ms has passed since last time the clients were updated
    previousMillis = now;                                 // reset previousMillis

    int sensorValue = analogRead(analogPin);
    float voltage = (sensorValue / 4095.0) * 3.3;

    //sendJson("graph_update", voltage);
  }
}

String settingsToJson() {
  String json = "{";
  json += "\"l\":" + String(settings.l) + ",";
  json += "\"m\":" + String(settings.m) + ",";
  json += "\"h\":" + String(settings.h) + ",";

  json += "\"h_data\":[";
  for (int i = 0; i < 10; i++) {
    json += String(settings.h_data[i]);
    if (i < 9) json += ",";
  }
  json += "],";

  json += "\"w_data\":[";
  for (int i = 0; i < 3; i++) {
    json += String(settings.w_data[i]);
    if (i < 2) json += ",";
  }
  json += "]";

  json += "}";
  return json;
}

void sendSettingsUpdate() {
JsonDocument doc;
doc["l"] = settings.l;
doc["m"] = settings.m;
doc["h"] = settings.h;

JsonArray h_array = doc["harmonics"].to<JsonArray>();
for (int i = 0; i < 10; i++) {
  h_array.add(settings.h_data[i]);
}

JsonArray w_array = doc["waveform"].to<JsonArray>();
for (int i = 0; i < 3; i++) {
  w_array.add(settings.w_data[i]);
}

String jsonStr;
serializeJson(doc, jsonStr);
webSocket.broadcastTXT(jsonStr);
}