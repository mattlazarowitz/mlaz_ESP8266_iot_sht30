/*
MIT License

Copyright (c) 2024 Matthew Lazarowitz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h> //needed by configItems.hpp

#include <AsyncMqttClient.h> //consider AsyncMQTT_Generic which is based on this.
#include <include/WiFiState.h>
#include <RTCMemory.h>

#include <Wire.h>
#include "SHT31.h"
//#include <AsyncMqttClient.h>

#include "configItems.hpp"
#include "rtcInterface.hpp"

//
// defines that simply make times easier to use
//
#define FIVE_SECONDS_IN_MILLS 5000
#define TEN_SECONDS_IN_MILLS 10000
#define THIRTY_SECONDS_IN_MILLS 30000

#define TEN_SECONDS_IN_MICRO 10e6
#define THIRTY_SECONDS_IN_MICRO 30e6
#define ONE_MINUTE_IN_MICRO 60e6
#define FIVE_MINUTES_IN_MICRO 30e7
#define TEN_MINUTES_IN_MICRO 60e7
#define FIFTEEN_MINUTES_IN_MICRO 90e7
#define THIRTY_MINUTES_IN_MICRO 18e8


AsyncMqttClient mqttClient;
//
// Sensor specific definitions
//
#define SHT31_ADDRESS   0x44

SHT31 sht;
int topicsPublished = 0;
int topicsToPublish = 0;
unsigned long loopMillis;

//
// This is used to help indicate when it is safe to go to deep sleep and end the sleep-wake cycle.
//
void onMqttPublish(uint16_t packetId) {
  topicsPublished++;
}

bool MqttConnectWithTimeout(unsigned long Timeout){
  unsigned long currMillis;
  unsigned long  MqttStartMillis = millis();
  mqttClient.connect();
  do {
    Serial.print("`");
    currMillis = millis();
    if (currMillis - MqttStartMillis > Timeout) {
      Serial.printf("\n\rMQTT connect timeout (%d > %d)\n\r",currMillis - MqttStartMillis, Timeout);
      return false;
    }
    delay (100);
  } while (!mqttClient.connected());

  Serial.printf("Connected to MQTT in %d millis\n\r",currMillis - MqttStartMillis);
  return true;
}


//
// Device mode worker function. 
// This is used to try and restore a saved WiFi connection.
// Performing this operation leads to faster wifi connections and reduced WiFi power draw.
// TODO: Add connection timeout.
// TODO: Consider increasing sleep times in the event of a connection timeout.
//
void DevModeWifi(devRtcData* data) {
  const char* config_ssid = jsonConfig["ssid"];
  const char* config_pw = jsonConfig["WiFiPw"];
  const char* config_hostname = jsonConfig["hostname"];
  boolean isConnectionRestored = false;
  if (data != nullptr) {
    Serial.println("trying to restore WiFi state");
    String SsidStr = (char*)data->state.state.fwconfig.ssid;
    if(SsidStr.equals(config_ssid)){
      Serial.print("saved state matches config, restoring connection to ");
      Serial.println(config_ssid);
      if (WiFi.resumeFromShutdown(data->state)) {
        isConnectionRestored = true;
      }
    }
  }
  if (!isConnectionRestored) {
    Serial.print("regular wifi connection: ");
    Serial.printf("%s\r\n",static_cast<String>(jsonConfig["ssid"]));
    WiFi.persistent(false);
    Serial.print("setting hostname: ");
    Serial.println(config_hostname);
    WiFi.hostname(config_hostname);
    WiFi.mode(WIFI_STA);
    Serial.println("wifi.begin()");
    WiFi.begin(config_ssid, config_pw);
    if (data != nullptr) {
      data->state.state.fwconfig.ssid[0] = 0;
    }
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
  Serial.println(WiFi.localIP());
}

//may not need this. Here mostly for reference.
void devModeEnd(devRtcData* data) {
    if (data != nullptr) {
      WiFi.shutdown(data->state);
      rtcMemIface.save();
      delay (10);
    } else {
      WiFi.disconnect( true );
      delay(1);
    }
}


//
// Setup() sub-function
// This is the vertion of the Setup() function that needs to be called when the 
// ESP8266 is operating in staDevice mode.
//
// The flow for these sensors is as follows as they utilize deep sleep:
// 1) bring up infrastructure for the sensors
// 2) read the sensors
// 3) try to restore a wifi connection
// 4) establish a new connection if restore fails
// 5) connect to MQTT
// 6) send data
// 7) deep sleep

void setupDevMode()
{
  float temp_c;
  float relativeHumidity;
  //IPAddress MqttIp = MqttIp.fromString(static_cast<String>(jsonConfig["MqttIp"]));
  IPAddress MqttIp;
  uint16_t MqttPort = 1883; //make configuable?

  Wire.begin();
  Wire.setClock(100000);
  sht.begin();

  uint16_t stat = sht.readStatus();
  Serial.print ("SHT sensor status: ");
  Serial.print(stat, HEX);
  Serial.println();
  sht.read();
  temp_c = sht.getTemperature();
  relativeHumidity = sht.getHumidity();
  Serial.print("temperature: ");
  Serial.print(temp_c);
  Serial.print(" humidity: ");
  Serial.println(relativeHumidity);

  //Setup MQTT stuff that doesn't need wifi to set up.
  //Check if a username an PW have been provided
  if (jsonConfig.containsKey("MqttUser") && jsonConfig["MqttUser"].size() > 0 &&
      jsonConfig.containsKey("MqttPw") && jsonConfig["MqttPw"].size() > 0
  ) {
    mqttClient.setCredentials(jsonConfig["MqttUser"], jsonConfig["MqttPw"]);
  }
  //mqttClient.setClientId //consider doing this
  mqttClient.onPublish(onMqttPublish);
  MqttIp.fromString(static_cast<String>(jsonConfig["MqttIp"]));
  mqttClient.setServer(MqttIp, MqttPort);


  Serial.println("dev mode connect to wifi");
  MqttConnectWithTimeout(10000);

  topicsToPublish = 2; //adjust based on the number of topics 
  mqttClient.publish(jsonConfig["MqttTempTopic"], 1, false, String(temp_c).c_str());
  mqttClient.publish(jsonConfig["MqttHumTopic"], 1, false, String(relativeHumidity).c_str());
  loopMillis = millis();
}



void loopDevMode()
{
  unsigned long currMillis;
  unsigned int DbgSleepTime;
  
  if (topicsPublished >= topicsToPublish) {
    devRtcData* myRtcData = rtcMemIface.getData();
    Serial.println("topics published, sleeping");
    //don't worry about resetting variables, that will happen when the ESP wakes
    mqttClient.disconnect(false);
    devModeEnd(myRtcData);
    ESP.deepSleep(ONE_MINUTE_IN_MICRO,WAKE_RF_DEFAULT);
  }
  currMillis = millis();

  //timed out. Don't burn battery.
  if (currMillis - loopMillis > FIVE_SECONDS_IN_MILLS) {
    devRtcData* myRtcData = rtcMemIface.getData();
    Serial.printf("Timeout waiting to publish (infra issues?) (%d published)\r\n",topicsPublished);
    devModeEnd(myRtcData);
    ESP.deepSleep(ONE_MINUTE_IN_MICRO,WAKE_RF_DEFAULT);
  }
  delay (50); 
}

















