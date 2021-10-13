#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

#define MyApiKey "" // TODO: API Key is displayed 
#define MySSID "" // TODO: Wifi network SSID
#define MyWifiPassword "" // TODO: Wifi network password
#define API_ENDPOINT ""

#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

uint8_t TERRAZA_PIN = D1; // Terraza relay
uint8_t LUZ_TV_PIN = D2; // Luz TV relay
uint8_t LUZ_CLOSET_PIN = D3; // Luz closet relay

//Save last states
bool lastStateCloset = LOW;
bool lastStateTV = LOW;
bool lastStateTerraza = LOW;

void turnOn(String deviceId) {
  if (deviceId == "60de**********ef48") // Luz Terraza relay ON
  {
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    digitalWrite(TERRAZA_PIN, HIGH);
    lastStateTerraza = HIGH;
  }
  else if (deviceId == "60de4ae********5cef56") //Luz TV relay ON
  {
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    digitalWrite(LUZ_TV_PIN, HIGH);
    lastStateTV = HIGH;
  }
  else if (deviceId == "60de4b*********d5cef5e") // Luz closet relay ON
  {
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    digitalWrite(LUZ_CLOSET_PIN, HIGH);
    lastStateCloset = HIGH;
  }
  else
  {
    Serial.print("Turn on for unknow device id: ");
    Serial.println(deviceId);
  }

}

void turnOff(String deviceId) {
  if (deviceId == "60de4a94f********f48") // Luz Terraza relay OFF
  {
    Serial.print("Turn OFF device id: ");
    Serial.println(deviceId);
    digitalWrite(TERRAZA_PIN, LOW);
    lastStateTerraza = LOW;
  }
  else if (deviceId == "60de4ae*********d5cef56") // Luz TV relay OFF
  {
    Serial.print("Turn off device id: ");
    Serial.println(deviceId);
    digitalWrite(LUZ_TV_PIN, LOW);
    lastStateTV = LOW;
  }
  else if (deviceId == "60de4b0********ef5e") // Luz closet relay OFF
  {
    Serial.print("Turn OFF device id: ");
    Serial.println(deviceId);
    digitalWrite(LUZ_CLOSET_PIN, LOW);
    lastStateCloset = LOW;
  }
  else
  {
    Serial.print("Turn off for unknown device id: ");
    Serial.println(deviceId);
  } 
  
}

void turnOnAll() {
  digitalWrite(TERRAZA_PIN, HIGH);
  digitalWrite(LUZ_TV_PIN, HIGH);
  digitalWrite(LUZ_CLOSET_PIN, HIGH);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type)
  {
  case WStype_DISCONNECTED:
    isConnected = false;
    Serial.printf("[WSc] Webservice disconnected AWS!\n");
    turnOnAll();
    break;
  case WStype_CONNECTED:
    isConnected = true;
    Serial.printf("[WSc] Service connected to AWS at url: %s\n", payload);
    Serial.printf("Waiting for commands ...\n");
    break;
  case WStype_TEXT: {
    Serial.printf("[WSc] get text: %s\n", payload);
    // *Example payloads*
    // For Light device type
    // {"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-powercontroller.html
    // {"deviceId": xxxx, "action": "AdjustBrightness", value: 3} // https://developer.amazon.com/docs/device-apis/alexa-brightnesscontroller.html
    // {"deviceId": xxxx, "action": "setBrightness", value: 42} // https://developer.amazon.com/docs/device-apis/alexa-brightnesscontroller.html
    // {"deviceId": xxxx, "action": "SetColor", value: {"hue": 350.5,  "saturation": 0.7138, "brightness": 0.6501}} // https://developer.amazon.com/docs/device-apis/alexa-colorcontroller.html
    // {"deviceId": xxxx, "action": "DecreaseColorTemperature"} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html
    // {"deviceId": xxxx, "action": "IncreaseColorTemperature"} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html
    // {"deviceId": xxxx, "action": "SetColorTemperature", value: 2200} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html

    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, (char*)payload);
    String deviceId = jsonBuffer["deviceId"];
    String action = jsonBuffer["action"];

    if (action == "setPowerState") { //Switch or Light
      String value = jsonBuffer["value"];
      if (value == "ON")
      {
        turnOn(deviceId);
      } 
      else
      {
        turnOff(deviceId);
      }     
    }
    // Code for RGB control
  }
    break;
  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    break;

  default:
    break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TERRAZA_PIN, OUTPUT);
  pinMode(LUZ_TV_PIN, OUTPUT);
  pinMode(LUZ_CLOSET_PIN, OUTPUT);
  int connectionCounter = 0;

  WiFiMulti.addAP(MySSID, MyWifiPassword);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(MySSID);

  // Waiting for WiFi Connect
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    if(connectionCounter == 0){
      turnOnAll();
    }
    delay(500);
    Serial.print(".");
  }
  if (WiFiMulti.run() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  // server address, port and URL
  webSocket.begin("com", 80, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);

  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets
}

void loop() {
  webSocket.loop();

  if (isConnected)
  {
    uint64_t now = millis();
    // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night
    if ((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL)
    {
      heartbeatTimestamp = now;
      webSocket.sendTXT("H");
    }    
  }
  else
  {
    digitalWrite(TERRAZA_PIN, lastStateTerraza);
    digitalWrite(LUZ_TV_PIN, lastStateTV);
    digitalWrite(LUZ_CLOSET_PIN, lastStateCloset);
  }
}