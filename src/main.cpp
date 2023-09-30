#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#define ENABLE_GxEPD2_GFX 0
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/Org_01.h>
#include <Fonts/Picopixel.h>
#include <GxEPD2_3C.h>
#include <mtag_api.h>

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

/// DEPARTURES

struct departure_t {
  int time = -1;      // departure time (seconds)
  char line[8] = "";  // bus line (19, 20, 50)
};

// Displayed departure times. If they change, the display will be updated.
departure_t departures[3];

void convertToHHMMSS(uint32_t time, char* buf, size_t len) {
  int16_t hours = time * 24 / 86400;
  int16_t minutes = time / 60 - hours * 60;
  int16_t seconds = time - minutes * 60 - hours * 60 * 60;
  snprintf(buf, len, "%02d:%02d:%02d", hours % 24, minutes, seconds);
}

void drawDepartureTimes() {
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(8, 20);
  display.print("hello world");

  // TODO if all times are 0, then show a message that service is off
  // TODO get partial update working ?

  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_RED);
  for (size_t i = 0; i < 3; ++i) {
    if (departures[i].time >= 0) {
      // Draw line number on left
      display.setCursor(8, 55 + 30 * i);
      display.printf("L%s", departures[i].line);
    }
  }

  display.setFont(&FreeSans12pt7b);
  display.setTextColor(GxEPD_BLACK);
  for (size_t i = 0; i < 3; ++i) {
    if (departures[i].time >= 0) {
      char time[16];
      convertToHHMMSS(departures[i].time, time, sizeof(time));

      // Draw time
      display.setCursor(45, 55 + 30 * i);
      // Serial.println(time);
      display.print(time);
    }
  }
}

void sortDepartures(departure_t* deps, size_t sz) {
  for (size_t i = 0; i < sz; ++i) {
    departure_t tmp;
    memcpy(&tmp, &deps[i], sizeof(tmp));
    for (size_t j = i + 1; j < sz; ++j) {
      if (tmp.time < deps[j].time) {
        printf("Swapping departures: %d=(L%s t%d) <-> %d=(L%s t%d)\n", i,
               deps[i].line, deps[i].time, j, deps[j].line, deps[j].time);
        // Swap deps[i] and deps[j]
        memcpy(&deps[i], &deps[j], sizeof(tmp));
        memcpy(&deps[j], &tmp, sizeof(tmp));
      }
    }
  }
}

///

void setup() {
  Serial.begin(115200);
  // LED
  pinMode(LED, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT);

  // ePaper setup
  display.init(115200, true, 50, false);
  display.setRotation(1);
  display.drawLine(display.width() / 2, 10, display.width() / 2,
                   display.height() - 10, GxEPD_BLACK);
  // Serial.println("display characteristics:");
  // Serial.print("fast partial update");
  // Serial.println(display.epd2.hasFastPartialUpdate);
  // Serial.print("slow partial update");
  // Serial.println(display.epd2.hasPartialUpdate);

  // // display.setFullWindow();
  // Serial.println("display characteristics:");
  // Serial.print("fast partial update");
  // Serial.println(display.epd2.hasFastPartialUpdate);
  // Serial.print("slow partial update");
  // Serial.println(display.epd2.hasPartialUpdate);
  // display.setRotation(1);
  display.setCursor(display.width() / 2 + 8, 45);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&Org_01);
  display.println("Right side !");

  attachInterrupt(PUSH_BUTTON, setLedInterrupt, RISING);
}

void goToSleep(uint64_t time_us) {
  Serial.println("Activate timer");
  esp_err_t timerErr = esp_sleep_enable_timer_wakeup(time_us);
  if (timerErr != ESP_OK) {
    Serial.println("Unable to set wakeup timer");
    return;
  }
  Serial.println("Wifi disconnect");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("Going to sleep...");
  Serial.flush();
  // Serial.end();
  esp_err_t sleepErr = esp_light_sleep_start();
  // Serial.begin(115200);
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  Serial.printf("Woke up because %d! Sleep code: %d\n", reason, sleepErr);
}

void loop() {
  // Connect to the wifi
  Serial.print("Connecting to wifi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin("le wifi", "saucisse de strasb0urg");
  // Wait 10s for connection, otherwise abort
  uint8_t result = WiFi.waitForConnectResult(10000);
  Serial.printf("Connect result %d\n", result);
  if (result == WL_CONNECTED) {
    Serial.print("Connected at ");
    Serial.println(WiFi.localIP());
    // Northbound
    // DynamicJsonDocument doc = mdata.getStopTimes(mdata.TREFFORINE_NORD);
    DynamicJsonDocument doc = mdata.getStopTimes(mdata.PALAIS);
    // Next departure times found in the API
    // * 2: take at most two departures for each line
    departure_t newDeps[doc.size() * 2];
    for (size_t i = 0; i < doc.size() * 2; ++i)
      Serial.printf("%d\n", newDeps[i].time);

    // Get next few stop times for line 19 or 20
    for (size_t i = 0; i < doc.size(); ++i) {
      // Get line
      String id = doc[i]["pattern"]["id"].as<String>();
      Serial.println(String("ID=") + id);
      // Bus line is the second field of id, e.g. id="SEM:C5:0:1671003832"
      // -> line = C5
      size_t lineNameStartIdx, lineNameLength;
      getStringSplit(id.c_str(), ':', 1, &lineNameStartIdx, &lineNameLength);

      // Iterate over departure times
      JsonArray times = doc[i]["times"].as<JsonArray>();

      // Take at most two departures for each line, then find the soonest 3
      // of the bunch
      for (size_t depIdx = 0; depIdx < 2; ++depIdx) {
        JsonObject timesElement = times[depIdx];
        uint32_t departureTime =
            timesElement["realtimeDeparture"].as<uint32_t>();
        Serial.println(departureTime);
        // Convert to time of day
        char hhmmss[16];
        // convertToHHMMSS(departureTime, hhmmss, sizeof(hhmmss));
        // Serial.printf("Departing at %s\n", hhmmss);
        newDeps[i * 2 + depIdx].time = departureTime;
        strncpy(newDeps[i * 2 + depIdx].line, id.c_str() + lineNameStartIdx,
                lineNameLength);
        newDeps[i * 2 + depIdx].line[lineNameLength] = '\0';
        Serial.println(lineNameStartIdx);
        Serial.println(lineNameLength);
        Serial.println(newDeps[i * 2 + depIdx].line);
      }
    }

    // Find the soonest departures TODO
    sortDepartures(newDeps, doc.size() * 2);
    for (size_t i = 0; i < 3; ++i) {
      Serial.printf("%d vs %d\n", newDeps[i].time, departures[i].time);
      if (newDeps[i].time != departures[i].time) {
        departures[i].time = newDeps[i].time;
        memcpy(departures[i].line, newDeps[i].line, sizeof(departures[i].line));
        shouldRefresh = true;
      }
    }

    // TODO partial update testing -> does it work at all?

    if (shouldRefresh) {
      Serial.println("call display");
      // display.setPartialWindow(0, 0, display.width() / 2, display.height());
      display.fillScreen(GxEPD_WHITE);
      display.firstPage();
      do {
        display.drawLine(display.width() / 2, 10, display.width() / 2,
                         display.height() - 10, GxEPD_BLACK);
        drawDepartureTimes();
        display.setCursor(display.width() / 2 + 8, 45);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&Org_01);
        display.println("Right side !");
      } while (display.nextPage());
      // display.display();
      Serial.println("done");
      shouldRefresh = false;
    }
  }

  goToSleep(5000000);
}