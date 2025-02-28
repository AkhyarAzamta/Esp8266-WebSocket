#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Ganti dengan kredensial WiFi kamu
const char *ssid = "Akhyar-Azamta";
const char *password = "Azamta12345";

const int ledPin = 2; // Sesuaikan dengan pin LED yang digunakan

// Web Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// JSON untuk menyimpan status LED & Alarm
JSONVar ledState;
JSONVar alarmState;

bool isLedOn = false;
int alarmHour = -1;
int alarmMinute = -1;
bool alarmSet = false;

// Inisialisasi NTP untuk mendapatkan waktu
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000); // GMT+7 (25200 detik)

// Inisialisasi LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("Gagal mounting LittleFS!");
  } else {
    Serial.println("LittleFS berhasil dimounting.");
  }
}

// Inisialisasi WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("\nTerhubung dengan IP: " + WiFi.localIP().toString());
}

// Kirim status LED & Alarm ke client
void notifyClients() {
  ledState["led"] = isLedOn ? "ON" : "OFF";
  alarmState["hour"] = alarmHour;
  alarmState["minute"] = alarmMinute;
  alarmState["set"] = alarmSet ? "ON" : "OFF";

  String jsonString = JSON.stringify(ledState);
  ws.textAll(jsonString);
}

// Handle pesan dari WebSocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String message = String((char *)data).substring(0, len);
    Serial.println("Pesan diterima: " + message);

    if (message == "LED_ON") {
      digitalWrite(ledPin, LOW); // LED menyala
      isLedOn = true;
    } else if (message == "LED_OFF") {
      digitalWrite(ledPin, HIGH); // LED mati
      isLedOn = false;
    } else if (message.startsWith("SET_ALARM:")) {
      int separator = message.indexOf(':');
      String timePart = message.substring(separator + 1);
      separator = timePart.indexOf(':');
      alarmHour = timePart.substring(0, separator).toInt();
      alarmMinute = timePart.substring(separator + 1).toInt();
      alarmSet = true;
      Serial.printf("Alarm diset ke %02d:%02d\n", alarmHour, alarmMinute);
    } else if (message == "CLEAR_ALARM") {
      alarmSet = false;
      alarmHour = -1;
      alarmMinute = -1;
      Serial.println("Alarm dihapus");
    }

    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u terhubung dari %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void checkAlarm() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  Serial.printf("Waktu Sekarang: %02d:%02d\n", currentHour, currentMinute);

  if (alarmSet && currentHour == alarmHour && currentMinute == alarmMinute) {
    Serial.println("Alarm berbunyi! LED dinyalakan.");
    digitalWrite(ledPin, LOW); // Nyalakan LED
    isLedOn = true;
    notifyClients();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // LED awalnya mati

  initWiFi();
  initFS();
  initWebSocket();

  timeClient.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();
}

void loop() {
  ws.cleanupClients();
  checkAlarm();
  delay(1000); // Cek alarm setiap detik
}
