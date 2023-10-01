#include <Arduino.h>
#include <WiFi.h>

#define ENABLE_GxEPD2_GFX 0
#include <ArduinoJson.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
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
volatile bool shouldRefresh = true;
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
  // Serial.printf("drawing at %d %d, size %d %d\n", cx, cy, wx, wy);
  display.setCursor(x - wx / 2 - cx, y - wy / 2 - cy);
  display.print(str);
}

/// DEPARTURES

struct departure_t {
  uint32_t time = UINT32_MAX;  // departure time (seconds)
  char line[8] = "";           // bus line (19, 20, 50)
};

// Displayed departure times. If they change, the display will be updated.
departure_t departuresLeft[3];
departure_t departuresRight[3];

void convertToHHMMSS(uint32_t time, char* buf, size_t len) {
  int16_t hours = time * 24 / 86400;
  int16_t minutes = time / 60 - hours * 60;
  int16_t seconds = time - minutes * 60 - hours * 60 * 60;
  snprintf(buf, len, "%02d:%02d:%02d", hours % 24, minutes, seconds);
}

enum DisplaySide { LEFT, RIGHT };

void drawDepartureTimes(departure_t departures[3], DisplaySide side) {
  // TODO if all times are 0, then show a message that service is off
  // TODO get partial update working ?
  int xoffset;
  if (side == LEFT) {
    xoffset = 0;
  } else {
    xoffset = display.width() / 2;
  }

  // Turns false if at least one departure is planned
  bool noDeps = true;

  display.setTextColor(GxEPD_RED);
  display.setFont(&FreeMono9pt7b);
  for (size_t i = 0; i < 3; ++i) {
    if (departures[i].time < UINT32_MAX) {
      // Draw line number on left
      display.setCursor(8 + xoffset, 55 + 30 * i);
      display.printf("L%s", departures[i].line);
      noDeps = false;
    }
  }

  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSans12pt7b);
  for (size_t i = 0; i < 3; ++i) {
    if (departures[i].time < UINT32_MAX) {
      char time[16];
      convertToHHMMSS(departures[i].time, time, sizeof(time));

      // Draw time
      display.setCursor(45 + xoffset, 55 + 30 * i);
      // Serial.println(time);
      display.print(time);
    }
  }

  // Handle case where no departures are planned
  if (noDeps) {
    Serial.println("No departures");
    display.setFont(&FreeSans9pt7b);
    draw_text("Pas de", display.width() / 4 + xoffset,
              display.height() / 2 - 5);
    draw_text("service", display.width() / 4 + xoffset,
              display.height() / 2 + 25);
  }
}

// Comparison function for departures (sort in ascending departure time)
int cmpDepartures(const void* a, const void* b) {
  uint32_t ta = ((const departure_t*)a)->time;
  uint32_t tb = ((const departure_t*)b)->time;
  if (ta > tb)
    return 1;
  else if (tb > ta)
    return -1;
  else
    return 0;
}

void sortDepartures(departure_t* deps, size_t sz) {
  qsort(deps, sz, sizeof(departure_t), cmpDepartures);
}

void setup() {
  Serial.begin(115200);
  // LED
  pinMode(LED, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT);

  // ePaper setup
  display.init(115200, true, 50, false);
  display.setRotation(1);

  display.fillScreen(GxEPD_WHITE);

  // Headings
  int dx = 25;  // Additional offset for Nord and Sud
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSerif12pt7b);
  draw_text("Trefforine", display.width() / 2, 15);

  display.setTextColor(GxEPD_RED);
  display.setFont(&FreeSans9pt7b);
  draw_text("Nord", display.width() / 4 - dx, 17);

  display.setTextColor(GxEPD_RED);
  display.setFont(&FreeSans9pt7b);
  draw_text("Sud", 3 * display.width() / 4 + dx, 17);

  display.display();

  display.setTextColor(GxEPD_BLACK);

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
  esp_err_t sleepErr = esp_light_sleep_start();
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  Serial.printf("Woke up because %d! Sleep code: %d\n", reason, sleepErr);
}

void updateDepartureTimes(const String& stop_id,
                          departure_t departures[3],
                          DisplaySide side) {
  DynamicJsonDocument doc = mdata.getStopTimes(stop_id);
  // Next departure times found in the API
  // 2 * 2: take at most two departures for each line
  departure_t newDeps[2 * 2];

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
    size_t nTimes =
        min(2U, times.size());  // Number of departure times, capped at 2
    Serial.printf("Number of departures: %d\n", nTimes);

    // Take at most two departures for each line
    for (size_t depIdx = 0; depIdx < nTimes; ++depIdx) {
      JsonObject timesElement = times[depIdx];
      uint32_t departureTime = timesElement["realtimeDeparture"].as<uint32_t>();

      newDeps[i * 2 + depIdx].time = departureTime;
      strncpy(newDeps[i * 2 + depIdx].line, id.c_str() + lineNameStartIdx,
              lineNameLength);
      newDeps[i * 2 + depIdx].line[lineNameLength] = '\0';

      // Serial.println(lineNameStartIdx);
      // Serial.println(lineNameLength);
      // Serial.println(newDeps[i * 2 + depIdx].line);
    }
  }

  // Find the 3 soonest departures
  sortDepartures(newDeps, sizeof(newDeps) / sizeof(departure_t));

  for (size_t i = 0; i < 3; ++i) {
    Serial.printf("%d on line %s : %u vs %u\n", i, newDeps[i].line,
                  newDeps[i].time, departures[i].time);
    if (newDeps[i].time != departures[i].time) {
      departures[i].time = newDeps[i].time;
      memcpy(departures[i].line, newDeps[i].line, sizeof(departures[i].line));
      shouldRefresh = true;
    }
  }
}

// Update the display with new departure times (3 on the left, 3 on the right)
void drawDepartureTimes(departure_t depLeft[3], departure_t depRight[3]) {
  display.setPartialWindow(0, 35, display.width(), display.height());
  display.firstPage();
  do {
    drawDepartureTimes(depLeft, LEFT);
    // Vertical separation in middle
    display.drawLine(display.width() / 2, 30, display.width() / 2,
                     display.height() - 10, GxEPD_BLACK);
    drawDepartureTimes(depRight, RIGHT);
  } while (display.nextPage());
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

    updateDepartureTimes(mdata.TREFFORINE_NORD, departuresLeft, LEFT);
    updateDepartureTimes(mdata.TREFFORINE_SUD, departuresRight, RIGHT);

    if (shouldRefresh) {
      Serial.println("call display");
      drawDepartureTimes(departuresLeft, departuresRight);
      shouldRefresh = false;
      Serial.println("done");
    }
  }

  goToSleep(60000000);
}