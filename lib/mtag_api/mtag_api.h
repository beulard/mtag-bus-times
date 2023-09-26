/// Get information about a bus stop using the mtag api
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <string_utils.h>

// typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);
// static int on_http_body(http_parser *, const char *at, size_t length) {
//   Serial.printf("we got body !: %s\n", at);
//   return 0;
// }

// static int on_http_header_field(http_parser *parser, const char *at,
//                                 size_t length) {
//   Serial.printf("we got header field !: %s\n", at);
//   Serial.println(length);
//   return 0;
// }

class MData {
private:
  HTTPClient http;
  WiFiClientSecure client;

  String PROTOCOL = "https://";
  String HOST = "data.mobilites-m.fr";

public:
  String PALAIS = "SEM:0910";
  // Arret Trefforine direction Nord (Air Liquide)
  String TREFFORINE_NORD = "SEM:0613";
  // Arret Trefforine direction Sud (Poya)
  String TREFFORINE_SUD = "SEM:0917";

  // Get the api endpoint for a given stop id
  String getEndpoint(const String &stop_id) {
    return String("/api/routers/default/index/stops/") + stop_id +
           String("/stoptimes");
  }

  // Get the full URL for a given stop id
  String getUrl(const String &stop_id) {
    return PROTOCOL + HOST + getEndpoint(stop_id);
  }

  DynamicJsonDocument getStopTimes(const String &stop_id) {
    String url = getUrl(stop_id);
    client.setInsecure();
    if (client.connect(HOST.c_str(), 443)) {
      Serial.println("Connection ok! Sending req...");

      client.printf("GET %s HTTP/1.1\r\n", getEndpoint(stop_id).c_str());
      client.println("Origin: saucisse");
      client.println("Host: " + HOST);
      client.println("Connection: close");
      client.println();
    } else {
      Serial.println("Connection failed");
    }

    // Buffer for the content
    char *content = nullptr;
    // Content length as found in response header
    size_t contentLength = 0;
    bool isJson = false;
    // Next index to write into content buffer (in case the content is split
    // into multiple lines)
    size_t contentIdx = 0;
    // Set to true when we've read past all the headers and the content starts
    bool isContent = false;

    DynamicJsonDocument doc(4096);

    while (client.connected() && !isContent) {
      String line = client.readStringUntil('\n');
      // Serial.printf("length = %d\n", line.length());

      // First line
      if (line.substring(0, 4).equalsIgnoreCase("http")) {
        Serial.print("Status: ");
        Serial.println(line);
      } else if (line.length() == 1) {
        Serial.println("empty line");
        isContent = true;
      } else {
        // Header
        Serial.print("Header: ");
        if (line.substring(0, 14).equalsIgnoreCase("Content-Length")) {
          String sub = line.substring(15);
          sub.trim();
          contentLength = sub.toInt();
          Serial.printf("LENGTH FROM HEADER: %d\n", contentLength);
        } else if (line.substring(0, 12).equalsIgnoreCase("Content-Type")) {
          String sub = line.substring(13);
          sub.trim();
          if (isSubstring("application/json", sub.c_str())) {
            Serial.println("Content type is JSON !");
            isJson = true;
          }
        }
        // Serial.println(line);
      }
    }
    if (contentLength > 0 && isJson) {
      // Content
      DeserializationError err = deserializeJson(doc, client);
      Serial.println(err.c_str());
      if (err.code() == DeserializationError::Ok) {
        Serial.print("Doc size: ");
        Serial.println(doc.size());
        for (size_t i = 0; i < doc.size(); ++i) {
          JsonObject obj = doc[i];
          char buf[2048];
          serializeJsonPretty(obj, buf);
          Serial.println(buf);
          if (doc[i].containsKey("pattern")) {
            Serial.println("Found key 'pattern'");
            JsonObject pattern = doc[i]["pattern"];
          }
          if (doc[i].containsKey("times")) {
            Serial.println("Found key 'times'");
            JsonObject pattern = doc[i]["times"];
          }
        }
      } else {
        Serial.println("Error deserializing JSON content !");
      }

      client.stop();

      return doc;
    }
  }
};