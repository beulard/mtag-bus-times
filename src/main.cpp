#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define ENABLE_GxEPD2_GFX 0
#include <ArduinoJson.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/Picopixel.h>
#include <GxEPD2_3C.h>
#include <mtag_api.h>

#include "stub_api_response.h"

#define LED 2

// ESP-WROOM-32
// BUSY=15
// RES=2
// DC=4
// CS=16
// SCL=17
// SDA=5

// ESP32-C3 SS=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RST=2,DC=1

/// Globals
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
    GxEPD2_290_C90c(/*CS=*/5,
                    /*DC=*/22,
                    /*RST=*/21,
                    /*BUSY=*/4));

// mtag
MData mdata;

volatile byte ledState = LOW;
volatile bool shouldRefresh = false;
void setLedInterrupt() {
  ledState = !ledState;
  digitalWrite(LED, ledState);
  shouldRefresh = true;
}

/// Draw text centered on x, y
void draw_text(const String& str, int16_t x, int16_t y) {
  int16_t cx, cy;
  uint16_t wx, wy;
  display.getTextBounds(str, 0, 0, &cx, &cy, &wx, &wy);
  Serial.printf("drawing at %d %d, size %d %d\n", cx, cy, wx, wy);
  display.setCursor(x - wx / 2 - cx, y - wy / 2 - cy);
  display.print(str);
}

void setup() {
  Serial.begin(115200);
  // LED
  pinMode(LED, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT);

  // ePaper setup
  display.init();
  Serial.println("display characteristics:");
  Serial.print("fast partial update");
  Serial.println(display.epd2.hasFastPartialUpdate);
  Serial.print("slow partial update");
  Serial.println(display.epd2.hasPartialUpdate);

  // display.setFullWindow();
  Serial.println("display characteristics:");
  Serial.print("fast partial update");
  Serial.println(display.epd2.hasFastPartialUpdate);
  Serial.print("slow partial update");
  Serial.println(display.epd2.hasPartialUpdate);

  display.setRotation(1);
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.fillScreen(GxEPD_BLACK);
  Serial.printf("display width %d display height %d\n", display.width(),
                display.height());
  draw_text("debout", display.width() / 2, display.height() * 1 / 4);
  draw_text("line 2", display.width() / 2, display.height() * 3 / 4);
  draw_text("very very long line", display.width() / 2,
            display.height() * 1 / 2);

  display.setFont(&Picopixel);
  draw_text("hello\nworld", 10, 10);

  // Some random shit
  display.drawCircle(50, 50, 20, GxEPD_RED);
  display.drawRoundRect(10, 10, 40, 70, 5, GxEPD_WHITE);
  display.drawTriangle(250, 50, 225, 75, 275, 75, GxEPD_RED);

  // Connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin("le wifi", "saucisse de strasb0urg");
  // TODO use ?
  // WiFi.setAutoReconnect()

  // TODO use waitForConnectResult instead to avoid infinite try
  Serial.print("Connecting to wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP());

  attachInterrupt(PUSH_BUTTON, setLedInterrupt, RISING);
}

void loop() {
  // Northbound
  // DynamicJsonDocument doc = mdata.getStopTimes(mdata.TREFFORINE_NORD);
  DynamicJsonDocument doc = mdata.getStopTimes(mdata.PALAIS);
  // Get next few stop times for line 19 or 20
  for (size_t i = 0; i < doc.size(); ++i) {
    // Get line
    String id = doc[i]["pattern"]["id"].as<String>();
    Serial.println(String("ID=") + id);
    JsonArray times = doc[i]["times"].as<JsonArray>();
    for (JsonArrayIterator it = times.begin(); it != times.end(); ++it) {
      Serial.println((*it)["realtimeDeparture"].as<String>());
      // Convert to time of day
      // TODO figure out format
    }
  }

  if (shouldRefresh) {
    Serial.println("call display");
    display.display();
    Serial.println("done");
    shouldRefresh = false;
  }
  // TODO
  // esp_sleep_enable_timer_wakeup();
  // WiFi.disconnect();
  // esp_light_sleep_start();

  delay(5000);
}