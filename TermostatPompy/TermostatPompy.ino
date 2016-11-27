#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "OneWire.h"
#include "DallasTemperature.h"
extern "C" {
#include "user_interface.h"
}


//local parameters
#define OUTPUT_PORT 4
#define ONEWIRE_PORT GPIO_ID_PIN(5)

// WiFi parameters
IPAddress address(192, 168, 2, 7);
IPAddress gateway(192, 168, 2, 1);
IPAddress netmask(255, 255, 255, 0);
const char* ssid = "xxxxxxxxx";
const char* password = "xxxxxxxxx";
#define LISTEN_PORT 80
ESP8266WebServer server(LISTEN_PORT);
OneWire oneWire(ONEWIRE_PORT);
DallasTemperature sensors(&oneWire);
unsigned long requestTime(0);
unsigned long lastRun(0);
boolean IsEnabled(true);
boolean IsPumpRunning(false);
float temp0(0.0);
float temp1(0.0);
const float tempDelta(3.0);
const float hysteresis(0.5);
const float maxTemp1(35);
void handleUnknown()

{
  String content = "<html>";
  content += "\n<head>";
  content += "\n<meta charset = \"utf-8\" />";
  content += "\n<title>Sterowanie termostatem pompy</title>";
  content += "\n</head>";
  content += "\n<body>";
  content += "\n<h1>";
  content += "\nPompa CWU";
  content += "\n</h1>";
  content += "\nUżycie:<br/>";
  content += "\n<ul>";
  content += "\n<li>włączenie/wyłączenie termostatu: POST /enable state = ON | OFF</li>";
  content += "\n<li>spawdzenie, czy włączony: GET /enable</li>";
  content += "\n<li>spawdzenie, czy pompa pracuje: GET /pump</li>";
  content += "\n<li>odczyt termometru: GET /temp0 albo /temp1</li>";
  content += "\n</ul>";
  content += "\n</body>";
  content += "\n</html>";

  server.send(200, "text/html", content);
}

void handleStateWrite()
{
  if (server.args() > 0)
  {
    String value = server.arg(0);
    if (value == "ON")
      IsEnabled = true;
    else if (value == "OFF")
      IsEnabled = false;
    else
    {
      server.send(400);
      return;
    }
    server.send(200, "text/plain", IsEnabled ? "ON" : "OFF");
  }
  else
    server.send(400);
}

void handleStateRead()
{
  String message = IsEnabled ? "ON" : "OFF";
  server.send(200, "text/plain", message);
}

void handlePumpStateRead()
{
  String message = IsPumpRunning ? "ON" : "OFF";
  server.send(200, "text/plain", message);
}

void handleTemp0Read()
{
  server.send(200, "text/plain", String(temp0));
}

void handleTemp1Read()
{
  server.send(200, "text/plain", String(temp1));
}

void handleFavIcon()
{
  server.send(404, "text/html", "No favicon.ico");
}

void pumpOn()
{
  digitalWrite(OUTPUT_PORT, HIGH);
  IsPumpRunning = true;
  lastRun = millis();
  debugPrint();
}
void pumpOff()
{
  if ((millis() - lastRun) >= (30 * 1000))
  {
    digitalWrite(OUTPUT_PORT, LOW);
    IsPumpRunning = false;
    debugPrint();
  }
}

void debugPrint()
{
  Serial.print(millis() / 1000);
  Serial.print(" - Pump state: "); Serial.println(IsPumpRunning);
  Serial.print("Temp0: "); Serial.print(temp0);
  Serial.print("\tTemp1: "); Serial.println(temp1); Serial.println();
}

void determinePumpState()
{
  if (!IsEnabled)
    return;
  temp0 = sensors.getTempCByIndex(0);
  temp1 = sensors.getTempCByIndex(1);
  if ((temp0 < 0.0)
      || (temp1 < 0.0))
    return;
  if (IsPumpRunning
      && abs(temp0 - temp1) < (tempDelta - hysteresis))
    pumpOff();

  if (!IsPumpRunning
      && (temp0 - temp1) > (tempDelta + hysteresis)
      && temp1 <= maxTemp1)
    pumpOn();
}
void idleStart()
{  
  if (IsEnabled && !IsPumpRunning)
    pumpOn();
}

void setup(void)
{
  //Start Serial
  Serial.begin(74880);
  //Serial.setDebugOutput(true);
  // assign SD_DATA2 GPIO function
  pinMode(OUTPUT_PORT, OUTPUT);
  digitalWrite(OUTPUT_PORT, LOW);
  pinMode(ONEWIRE_PORT, INPUT_PULLUP);
  sensors.begin();
  // Configure server
  server.on("/enable", HTTP_POST, handleStateWrite);
  server.on("/enable", HTTP_GET, handleStateRead);
  server.on("/pump", HTTP_GET, handlePumpStateRead);
  server.on("/temp0", HTTP_GET, handleTemp0Read);
  server.on("/temp1", HTTP_GET, handleTemp1Read);
  server.on("/favicon.ico", handleFavIcon);
  server.onNotFound(handleUnknown);
  server.begin();
  // Connect to WiFi
  wifi_set_phy_mode(PHY_MODE_11G);
  WiFi.config(address, gateway, netmask);
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(0);
  WiFi.begin(ssid, password);
  //while (WiFi.status() != WL_CONNECTED)
  {
    //delay(3000);
    //Serial.print(".");
  }
  //Serial.println("WiFi connected");
  // Print the IP address
  //Serial.println(WiFi.localIP());
  sensors.requestTemperatures();
  idleStart();
}

void loop() {
  // Handle REST calls
  server.handleClient();
  unsigned long currentTime = millis();
  if (abs(currentTime - requestTime) > 5000)
  {
    determinePumpState();
    sensors.requestTemperatures();
    requestTime = currentTime;
    //WiFi.printDiag(Serial);
  }
  if (currentTime - lastRun > 5 * 60 * 1000)
    idleStart();
}


