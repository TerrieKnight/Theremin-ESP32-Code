#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPASyncWebServer.h>
#include <WebSocketsServer.h>

//Globals
#define TOUCH_CS 5    
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

const int analogPin = 34;  // GPIO34 = ADC1 Chan 6

//Webserver Setup
const char* ssid = "Boneca-Ambalabu";
const char* password = "Siddiqui2025";

int interval = 250;                                  // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0;                     // we use the "millis()" command for time reference and this will output an unsigned long

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

SPI.begin(18, 19, 23);  // SCLK, MISO, MOSI
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS);

// LVGL Display and Input Handles
static lv_display_t *disp;
static lv_indev_t *indev;

// Display Flush Callback
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// Touch Read Callback 
void detectButtonTouch(lv_indev_t *indev, lv_indev_data_t *data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    
    // You may need to calibrate these values
    int x = map(p.x, 200, 3900, 1, SCREEN_WIDTH);
    int y = map(p.y, 300, 3800, 1, SCREEN_HEIGHT);

    data->state = LV_INDEV_STATE_PRESSED;

    data->point.x = x;
    data->point.y = y;
    Serial.print("X = ");
    Serial.print(x);
    Serial.print(" | Y  = ");
    Serial.print(y);
    Serial.println();
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  delay(100);
}

void setup() {
  Serial.begin(115200);

  //Setup for webserver
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

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

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html");
  });

  server.serveStatic("/", SPIFFS, "/");

  //server.onNotFound([](AsyncWebServerRequest *request) {
  //  request->send(404, "text/plain", "File not found.");
  //});

  webSocket.begin();
  //webSocket.onEvent(webSocketEvent);

  server.begin();


  SPI.begin(18, 19, 23);  // SCLK, MISO, MOSI

  // Init screen and touch
  tft.begin();
  tft.setRotation(3);
  ts.begin();
  ts.setRotation(3);

  // Init LVGL
  lv_init();

  // Allocate display buffer
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * SCREEN_WIDTH * 40, MALLOC_CAP_DMA);
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf, NULL, SCREEN_WIDTH * 40, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Register touchscreen input
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, detectButtonTouch);

  //Create Text
  lv_obj_t * text = lv_label_create(lv_screen_active());
  lv_label_set_text(text, "Hey Buddy");
  lv_obj_align(text, LV_ALIGN_TOP_LEFT, 0, 0);

  //Create Button
  lv_obj_t *btn = lv_button_create(lv_screen_active());
  lv_obj_center(btn);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, "Click Me");
  lv_obj_center(label);

  // Button Event
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      Serial.println("Button Pressed!");
    }
  }, LV_EVENT_CLICKED, NULL);
}

//Loop
void loop() {
  lv_timer_handler();  // LVGL rendering
  lv_tick_inc(5);
  delay(5);

  webSocket.loop();                                       // Update function for the webSockets 
  unsigned long now = millis();                           // read out the current "time" ("millis()" gives the time in ms since the ESP started)
  if ((unsigned long)(now - previousMillis) > interval) { // check if "interval" ms has passed since last time the clients were updated
    previousMillis = now;                                 // reset previousMillis

    int sensorValue = analogRead(analogPin);
    float voltage = (sensorValue / 4095.0) * 3.3;

    //sendJson("graph_update", voltage);
  }
}

