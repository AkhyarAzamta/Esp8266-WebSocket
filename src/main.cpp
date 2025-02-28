#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

// Ganti dengan kredensial WiFi kamu
const char *ssid = "Akhyar-Azamta";
const char *password = "Azamta12345";

const int ledPin = 2; // Sesuaikan dengan pin LED yang digunakan

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// JSON untuk menyimpan status LED
JSONVar ledState;

bool isLedOn = false;

// Inisialisasi LittleFS
void initFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("Gagal mount LittleFS");
  }
  else
  {
    Serial.println("LittleFS berhasil dimount");
  }
}

// Inisialisasi WiFi
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("\nWiFi terhubung! IP: " + WiFi.localIP().toString());
}

// Mengirim status LED ke semua client WebSocket
void notifyClients()
{
  ledState["led"] = isLedOn ? "ON" : "OFF";
  String jsonString = JSON.stringify(ledState);
  ws.textAll(jsonString);
}

// Menangani pesan dari WebSocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    String message = String((char *)data).substring(0, len);
    Serial.print("Pesan WebSocket diterima: ");
    Serial.println(message);

    if (message == "LED_ON")
    {
      digitalWrite(ledPin, LOW); // LED menyala
      isLedOn = true;
    }
    else if (message == "LED_OFF")
    {
      digitalWrite(ledPin, HIGH); // LED mati
      isLedOn = false;
    }

    notifyClients(); // Kirim status terbaru ke client
  }
}

// Event handler untuk WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("Client WebSocket #%u terhubung dari %s\n", client->id(), client->remoteIP().toString().c_str());
    notifyClients(); // Kirim status LED ke client baru
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("Client WebSocket #%u terputus\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  }
}

// Inisialisasi WebSocket
void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup()
{
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // LED awalnya mati

  initWiFi();
  initFS();
  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });

  server.serveStatic("/", LittleFS, "/");

  server.begin();
}

void loop()
{
  ws.cleanupClients();
}
