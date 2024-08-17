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
#include <ESPAsyncWebServer.h>
#include <ESP8266TimerInterrupt.h>
#include <include/WiFiState.h>
#include <RTCMemory.h>

#include "configItems.hpp"
#include "HtmlRequests.hpp"
#include "rtcInterface.hpp"

//TODO: see if this can go into a header file when I do the header file cleanup.
void setupDevMode();
void loopDevMode();

//TODO: Find a better place for this
#define AP_IP_ADDR 192,168,30,1
//Sketch specific data types


//used to direct behavior based on the state of the device.
//TODO: clean up commenting
enum devOpMode {
  staDevice, //regular mode. Device is in station mode, it do the normal device functions
  staConfig, //"station mode" meaning on the configured wifi network, but boots to server the configuration pages to allow config updates
  apConfig,  //AP mode config mode. Boot as an AP that can be connected to t in order to get to the config page that way. Config is not erased
  resetConfig, //erase the config settings, "factory reset"
  errorNoFs
};


//globals
AsyncWebServer server(80);
ESP8266Timer ITimer;
RTCMemory<devRtcData> rtcMemIface;
devOpMode BootMode;
bool rtcInit;

//
// Simple debug function to convert the boot mode into a string
//
String bootModeToStr(devOpMode BootMode) {
  switch (BootMode) {
    case staDevice:
      return "staDevice";
    case staConfig:
      return "staConfig";
    case apConfig:
      return "apConfig";
    case resetConfig:
      return "resetConfig";
    case errorNoFs:
      return "errorNoFs";
    default:
      break;
  }
  return "invalid boot mode";
}

//
// Timer ISR. 
// Used for boot mode change command.
// Resets withon a predetermined time window are counted and used to determine a user commanded boot mode change. 
// But if the count is reset at the end of Setup(), it's very hard for a human to use this functionality.
// So this timer ISR creates a 750ms window instead. 
// In battery powered device mode, this could become an issue if the device resets itself too quickly.
// In that case, it may be better to add the reset functionality right before the reset or deep sleep command.
// The process of reading a sensor, restoring the WiFi connection, and reporting the data should hopefully take 
// long enough to allow for reliable reset detection.
//
void IRAM_ATTR TimerHandler()
{
  devRtcData* myRtcData = rtcMemIface.getData();
  myRtcData->unhandledResetCount = 0;
  rtcMemIface.save();
  //This may be bad, but it seems to work OK for now.
  timer1_disable();
}

//
// Setup() sub-function
// This is the vertion of the Setup() function that needs to be called when the 
// ESP8266 is operating in apConfig mode. 
// The device comes up with WiFi in AP mode and runs a webserver to serve the config pages.
// 
void setupApConfigMode()
{
  Serial.println("setupApConfigMode");
  String configEspHostname;
  if (jsonConfig.containsKey("hostname")) {
    configEspHostname = String("config:") + String(jsonConfig["hostname"]);
  } else {
    configEspHostname = String("config:") + WiFi.hostname().c_str();
  }
  WiFi.softAPConfig(IPAddress(AP_IP_ADDR), IPAddress(0,0,0,0), IPAddress(255,255,255,0));
  Serial.print(F("AP hostname: "));
  Serial.println(configEspHostname);
  WiFi.softAP(configEspHostname);

  // setup HTTP server and the HTML requests
  registerHtmlInterfaces();
  server.begin();
}

//
// Setup() sub-function
// This is the vertion of the Setup() function that needs to be called when the 
// ESP8266 is operating in staConfig mode. 
// The device connects to the configured WiFi network but runs the webserver to 
// serve the configuration pages rather than operate an IoT device.
// 
void setupReconfigMode()
{
  Serial.println("setupReconfigMode");
  WiFi.hostname(static_cast<String>(jsonConfig["hostname"]).c_str());
  Serial.print(F("Connecting to "));
  Serial.println(static_cast<String>(jsonConfig["ssid"]));

  WiFi.mode(WIFI_STA);
  WiFi.begin(static_cast<String>(jsonConfig["ssid"]), static_cast<String>(jsonConfig["WiFiPw"]));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.println(F("WiFi connected\nIP address: "));
  Serial.println(WiFi.localIP());

  registerHtmlInterfaces();
  server.begin();
}


//
// There are some common steps needed by all boot modes. Those get performed here.
//
bool commonInit(){
  //devRtcData* myRtcData = rtcMemIface.getData();
  devRtcData* myRtcData = nullptr;
  ITimer.attachInterruptInterval(750000, TimerHandler);
  Serial.println(F("Mount LittleFS"));
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed"));
    BootMode =  errorNoFs;
    return false;
  }

  if(loadConfigFile(CONFIG_FILE)) {
    Serial.println (F("config loaded"));
    BootMode =  staDevice; 
  } else {
    //There is an FS so that's OK, but no config.
    BootMode =  apConfig;
  }


  //
  // Fetch data from RTC memory
  //
  rtcInit = rtcMemIface.begin();
  if(!rtcInit){
    // probably the first boot after a power loss
    Serial.println(F("No RTC data"));
    // often happens if the CRC for the RTC RAM fails which is expected on a first boot. 
    // System tries to restore from flash (which we don't use for this).
    // If that fails, it does a reset of the data area which is what we want. 
    // We can either reset or try tgoe begin again. 
    // Go ahead and try to begin again. IF that fails, I think the best course of action is an
    // error 'halt' and blink the onboard LED.
    if (rtcMemIface.begin()){
      myRtcData = rtcMemIface.getData();
    }
  } else {
    Serial.println(F("reading RTC data"));
    myRtcData = rtcMemIface.getData();
  }
  if (myRtcData != nullptr) {
    //increment the count and save back to RTC RAM
    Serial.print(F("reset count: "));
    Serial.println(myRtcData->unhandledResetCount);
    myRtcData->unhandledResetCount += 1;
    rtcMemIface.save();
    //now see if we hit any of the manual mode override thresholds
    switch (myRtcData->unhandledResetCount) {
      case 0:
      case 1:
        Serial.println(F("no override"));
        Serial.print(F("Boot mode: "));
        Serial.println(bootModeToStr(BootMode));
        break;
      case 2:
        Serial.println(F("reconfig on configed network"));
        if (BootMode == staDevice) {
          BootMode = staConfig;
        } else {
          //just go into normal config mode.
          BootMode = apConfig;
        }
        break;
      case 3:
        Serial.println("return to AP mode, keep config");
        BootMode = apConfig;

        break;
      // Use 4 or more concurrent resets as the signal to wipe
      // the stored configuration.
      // This handles any case where a reset happens while the config is being erased.
      default:
        Serial.println(F("\"factory reset\""));
        BootMode = resetConfig;
        blinkLed(5);
        break;
    }
  }
  return true;
}
void blinkLed(int blinks) {
  for (int i = 0; i < blinks;i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  }
}
void setup() {
  Serial.begin(115200);
  delay(20);
  Serial.println(F("Start setup"));
  pinMode(LED_BUILTIN, OUTPUT);

  commonInit();

  switch (BootMode) {
    case staConfig:
      setupReconfigMode();
      break;
    case staDevice:
      setupDevMode();
      break;
    case apConfig:
      setupApConfigMode();
      break;
    case resetConfig:
      //devConfig.clearConfig();
      eraseConfig(CONFIG_FILE);
      delay (1000);
      ESP.restart();
      delay (1000);
    default:
      Serial.println(F("Invalid boot mode"));
      break;

  }

  Serial.println("Setup done");
}

void loop() {
// check BootMode and do the right loop required based on that.
  if (BootMode == staDevice) {
    loopDevMode();
  }
}
